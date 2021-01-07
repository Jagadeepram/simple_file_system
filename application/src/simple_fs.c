#include <stdlib.h>
#include <string.h>
#include "boards.h"
#include "app_timer.h"
#include "sdk_config.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_error.h"
#include "app_error.h"
#include "simple_fs.h"
#include "crc16.h"

#define PAGE_LEN (4096)
#define FOLDER(FILE_ID)  ((FILE_ID)>>16)

/** Page status */
#define NEW_PAGE       (0xFF)
#define ACTIVE_PAGE    (0x7F)
#define OLD_PAGE       (0x07)
#define END_PAGE       (0x03)
#define OBSOLETE_PAGE  (0x00)

/** File status */
#define NEW_FILE       (0xFF)
#define ACTIVE_FILE    (0x0F)
#define OLD_FILE       (0x00)

static sfs_parameters_t * sfs_param;

typedef enum
{
    SFS_SEARCH_NONE = 0,
    /** Search for free space */
    SFS_SEARCH_FREE_SPACE,
    /** Search for last written file */
    SFS_SEARCH_LAST_FILE_ADDR,
    /** Search for a particular FILE */
    SFS_SEARCH_FILE_ID,
} sfs_search_type_t;

static sfs_status_t sfs_search(sfs_search_type_t search, sfs_file_info_t *file_info);
static sfs_status_t sfs_search_page(uint32_t page_addr, sfs_search_type_t search, sfs_file_info_t *file_info);

sfs_status_t sfs_init(sfs_parameters_t *sfs_parameters)
{
    uint8_t i;

    sfs_param = sfs_parameters;

    /** Initialize last written_address to zero */
    for (i = 0; i < sfs_param->nbr_folders; i++)
    {
        sfs_param->sfs_folder_info[i].last_written_address = 0;
    }
    /* TODO: perform address check for data and garbage collection address */
    return SFS_STATUS_NONE;
}

sfs_status_t sfs_uninit(void)
{
    return SFS_STATUS_NONE;
}

static sfs_status_t sfs_search_page(uint32_t addr, sfs_search_type_t search, sfs_file_info_t *file_info)
{
    sfs_status_t status = SFS_STATUS_NONE;
    uint8_t page_state;
    sfs_file_header_t file_header;
    sfs_file_info_t last_file;
    uint32_t end_addr = addr + PAGE_LEN;
    uint8_t is_active_file = 0;
    uint32_t start_addr = addr;

    if (sfs_param->mem_read(addr++, &page_state, sizeof(page_state)) != 0)
    {
        return SFS_STATUS_DRIVER_ERROR;
    }

    /** Do not process the obsolete page */
    if ((page_state == OBSOLETE_PAGE) ||
        /** Do not search for new space in the old page */
        (search == SFS_SEARCH_FREE_SPACE && page_state == OLD_PAGE))
    {
        return SFS_STATUS_NONE;
    }

    do
    {
        if (sfs_param->mem_read(addr, (uint8_t*)&file_header, sizeof(file_header)) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }

        /** Exit the loop on end of the page */
        if (file_header.status == END_PAGE)
        {
            break;
        }

        /** Look for space to store a new file */
        if ((search == SFS_SEARCH_FREE_SPACE) && (file_header.status == NEW_FILE))
        {
            uint32_t file_len = file_info->file_header.data_len + sizeof(sfs_file_header_t);
            uint32_t rem_len = end_addr - addr;

            /** Write a file only if it fits in this page, else declare the page is full */
            if (rem_len >= file_len)
            {
                /** Change page status to ACTIVE */
                if (page_state == NEW_PAGE)
                {
                    page_state = ACTIVE_PAGE;
                    if (sfs_param->mem_write(start_addr, &page_state, sizeof(page_state)) != 0)
                    {
                        return SFS_STATUS_DRIVER_ERROR;
                    }
                }
                file_info->address = addr;
            }
            else
            {
                /** Declare the page is full by marking it as old page, indicating that no free space available for new file */
                page_state = OLD_PAGE;
                if (sfs_param->mem_write(start_addr, &page_state, sizeof(page_state)) != 0)
                {
                    return SFS_STATUS_DRIVER_ERROR;
                }
                /** Declare the page is full by marking end of page status in the following address */
                page_state = END_PAGE;
                if (sfs_param->mem_write(addr, &page_state, sizeof(page_state)) != 0)
                {
                    return SFS_STATUS_DRIVER_ERROR;
                }
            }
            return SFS_STATUS_NONE;
        }

        /** Look for a particular file */
        if ((search == SFS_SEARCH_FILE_ID) && (file_header.status == ACTIVE_FILE)
                && (file_header.file_id == file_info->file_header.file_id))
        {
            file_info->file_header = file_header;
            file_info->address = addr;
            return SFS_STATUS_NONE;
        }

        /** Look for a particular file */
        if ((search == SFS_SEARCH_FILE_ID) && (file_header.status == NEW_FILE))
        {
            return SFS_STATUS_FILE_NOT_FOUND;
        }

        /** Look for the last file */
        if ((search == SFS_SEARCH_LAST_FILE_ADDR) && (file_header.status == NEW_FILE))
        {
            *file_info = last_file;
            return SFS_STATUS_NONE;
        }

        if ((file_header.status == ACTIVE_FILE) && (!is_active_file))
        {
            is_active_file = 1;
        }

        last_file.file_header = file_header;
        last_file.address = addr;
        addr += sizeof(sfs_file_header_t) + file_header.data_len;

    } while (addr < end_addr);

    /** Declare a page as obsolete when it has no active file */
    if (is_active_file == 0)
    {
        page_state = OBSOLETE_PAGE;
        if (sfs_param->mem_write(start_addr, &page_state, sizeof(page_state)) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }
    }

    return status;
}

static sfs_status_t sfs_search(sfs_search_type_t search, sfs_file_info_t *file_info)
{
    uint16_t folder_id = FOLDER(file_info->file_header.file_id) - 1;
    uint32_t addr;
    uint32_t end_addr;
    sfs_status_t status = SFS_STATUS_NONE;

    if (folder_id >= sfs_param->nbr_folders)
    {
        /** Wrong folder ID. This folder doesn't exist */
        return SFS_STATUS_WRONG_FOLDER;
    }

    addr = sfs_param->sfs_folder_info[folder_id].start_address;
    end_addr = addr + sfs_param->sfs_folder_info[folder_id].folder_len;
    /** Set address to zero before the search */
    file_info->address = 0;

    for (; addr < end_addr; addr += PAGE_LEN)
    {
        status = sfs_search_page(addr, search, file_info);
        if ((file_info->address != 0) || (status != SFS_STATUS_NONE))
        {
            /** Search object found or error, end the loop */
            return status;;
        }
    }

    if ((search == SFS_SEARCH_FREE_SPACE) && (status == SFS_STATUS_NONE))
    {
        /** TODO: Trigger Garbage collection for SFS_STATUS_NO_SPACE */
        /** TODO: Call the same function again to find new space after garbage collection */
    }

    return status;
}

sfs_status_t sfs_write_file(uint32_t file_id, uint8_t *data, uint32_t data_len)
{
    sfs_status_t status;
    sfs_file_info_t file_info;
    uint32_t old_addr;

    file_info.file_header.file_id = file_id;
    file_info.file_header.data_len = data_len;
    /** Search for an existing file */
    status = sfs_search(SFS_SEARCH_FILE_ID, &file_info);
    old_addr = file_info.address;

    /** Search for new space */
    status = sfs_search(SFS_SEARCH_FREE_SPACE, &file_info);
    if (status == SFS_STATUS_NONE)
    {
        file_info.file_header.status = ACTIVE_FILE;
        file_info.file_header.crc16 = crc16_compute(data, data_len, NULL);
        /** Write the header */
        if (sfs_param->mem_write(file_info.address, (uint8_t *)&file_info.file_header, sizeof(sfs_file_header_t)) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }
        /** Write the data */
        file_info.address += sizeof(sfs_file_header_t);
        if (sfs_param->mem_write(file_info.address, data, data_len) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }
    }

    /** Inactivate the old file */
    if (old_addr)
    {
        file_info.file_header.status = OLD_FILE;
        /** Write the header */
        if (sfs_param->mem_write(old_addr, (uint8_t *)&file_info.file_header.status, sizeof(file_info.file_header.status)) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }
    }

    return status;
}

sfs_status_t sfs_read_file_info(sfs_file_info_t *file_info)
{
    return sfs_search(SFS_SEARCH_FILE_ID, file_info);;
}

sfs_status_t sfs_read_file_data(sfs_file_info_t *file_info, uint8_t *data, uint32_t data_len)
{
    file_info->address += sizeof(sfs_file_header_t);

    if (data_len != file_info->file_header.data_len)
    {
        return SFS_STATUS_FILE_LEN_MISMATCH;
    }

    if (sfs_param->mem_read(file_info->address, data, data_len) != 0)
    {
        return SFS_STATUS_DRIVER_ERROR;
    }

    if (crc16_compute(data, data_len, NULL) != file_info->file_header.crc16)
    {
        return SFS_STATUS_CRC_ERROR;
    }

    return SFS_STATUS_NONE;
}

sfs_status_t sfs_read_file(uint32_t file_id, uint8_t *data, uint32_t data_len)
{
    sfs_status_t status;
    sfs_file_info_t file_info;

    file_info.file_header.file_id = file_id;
    file_info.file_header.data_len = data_len;

    /** Search for the file */
    status = sfs_search(SFS_SEARCH_FILE_ID, &file_info);

    if (status == SFS_STATUS_NONE && file_info.address)
    {
        status = sfs_read_file_data(&file_info, data, data_len);
    }
    else
    {
        status = SFS_STATUS_READ_ERROR;
    }

    return status;
}

