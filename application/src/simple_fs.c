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
static sfs_status_t sfs_perform_gc(sfs_folder_info_t *folder_info, uint32_t *nbr_of_fresh_pages_after_gc);


static sfs_status_t sfs_perform_gc(sfs_folder_info_t *folder_info, uint32_t *nbr_of_fresh_pages_after_gc)
{
    return SFS_STATUS_SUCCESS;
}

static sfs_status_t sfs_search_page(uint32_t addr, sfs_search_type_t search, sfs_file_info_t *file_info)
{
    sfs_status_t status = SFS_STATUS_BLANK;
    uint8_t page_state;
    sfs_file_header_t file_header;
    sfs_file_info_t last_file;
    uint32_t start_addr = MOD_DIV(addr, PAGE_LEN);
    uint32_t end_addr = start_addr + PAGE_LEN;
    /** Assign non zero value if the page contains an active file */
    uint8_t is_active_file = 0;


    /** Initialize last written address to zero */
    last_file.address = 0;
    if (sfs_param->mem_read(start_addr, &page_state, sizeof(page_state)) != 0)
    {
        return SFS_STATUS_DRIVER_ERROR;
    }

    /** Do not process the obsolete page */
    if ((page_state == OBSOLETE_PAGE) ||
        /** Do not search for new space in the old page */
        (search == SFS_SEARCH_FREE_SPACE && page_state == OLD_PAGE))
    {
        return SFS_STATUS_BLANK;
    }

    if (start_addr == addr)
    {
        /** addr is equal to the start of the page, then increment by 1 */
        addr++;
    }

    while (addr < end_addr)
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
                return SFS_STATUS_SUCCESS;
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
                return SFS_STATUS_BLANK;
            }
        }
        /** Look for a particular file */
        else if (search == SFS_SEARCH_FILE_ID)
        {
            if ((file_header.status == ACTIVE_FILE)
                && (file_header.file_id == file_info->file_header.file_id))
            {
                file_info->file_header = file_header;
                file_info->address = addr;
                return SFS_STATUS_SUCCESS;
            }
            /** Look for a particular file */
            else if (file_header.status == NEW_FILE)
            {
                return SFS_STATUS_FILE_NOT_FOUND;
            }
        }
        /** Look for the last written file */
        else if ((search == SFS_SEARCH_LAST_FILE_ADDR) && (file_header.status == NEW_FILE))
        {
            *file_info = last_file;
            return SFS_STATUS_SUCCESS;
        }

        if (file_header.status == ACTIVE_FILE)
        {
            if (!is_active_file)
            {
                is_active_file = 1;
            }
            last_file.file_header = file_header;
            last_file.address = addr;
        }
        addr += sizeof(sfs_file_header_t) + file_header.data_len;
    }

    /** Declare a page as obsolete when it has no active file */
    if (is_active_file == 0)
    {
        page_state = OBSOLETE_PAGE;
        if (sfs_param->mem_write(start_addr, &page_state, sizeof(page_state)) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }
    }

    if (search == SFS_SEARCH_LAST_FILE_ADDR)
    {
        /** Assign last file info at the end of page search */
        *file_info = last_file;
    }

    return status;
}

static sfs_status_t sfs_search(sfs_search_type_t search, sfs_file_info_t *file_info)
{
    uint16_t folder_id = FOLDER(file_info->file_header.file_id) - 1;
    uint32_t addr;
    uint32_t end_addr;
    uint32_t last_written_address;
    uint32_t nbr_of_fresh_pages_after_gc = 0;
    sfs_status_t status = SFS_STATUS_BLANK;

    if (folder_id >= sfs_param->nbr_folders)
    {
        /** Wrong folder ID. This folder doesn't exist */
        return SFS_STATUS_WRONG_FOLDER;
    }

    addr = sfs_param->sfs_folder_info[folder_id].start_address;
    end_addr = addr + sfs_param->sfs_folder_info[folder_id].folder_len;
    last_written_address = sfs_param->sfs_folder_info[folder_id].last_written_address;
    /** Set address to zero before the search */
    file_info->address = 0;

    /** Update search address from last written file while finding new space */
    if ((search == SFS_SEARCH_FREE_SPACE) && (last_written_address != 0))
    {
        addr = last_written_address;
    }

    for (; (addr < end_addr) && (status == SFS_STATUS_BLANK);)
    {
        status = sfs_search_page(addr, search, file_info);
        addr = MOD_DIV((addr+PAGE_LEN), PAGE_LEN);
    }

    if (status == SFS_STATUS_BLANK)
    {
        if (file_info->address == 0)
        {
            if (search == SFS_SEARCH_FREE_SPACE)
            {
                /** Run garbage collection when unable to find free space */
                status = sfs_perform_gc(&sfs_param->sfs_folder_info[folder_id], &nbr_of_fresh_pages_after_gc);
                if (status == SFS_STATUS_SUCCESS)
                {
                    if (nbr_of_fresh_pages_after_gc > 0)
                    {
                        sfs_file_info_t temp_file_info = *file_info;
                        /** Find last written space */
                        sfs_search(SFS_SEARCH_LAST_FILE_ADDR, &temp_file_info);
                        /** Find next available address to write */
                        status = sfs_search(search, file_info);
                    }
                    else
                    {
                        /* return FS_STATUS_NO_SPACE if no fresh data pages are found after garbage collection */
                        status = SFS_STATUS_NO_SPACE;
                    }
                }
            }
            else if (search == SFS_SEARCH_FILE_ID)
            {
                /** Assign file not found after going through all the pages */
                status = SFS_STATUS_FILE_NOT_FOUND;
            }
        }
        if (search == SFS_SEARCH_LAST_FILE_ADDR)
        {
            /** Store last written address into the folder info */
            sfs_param->sfs_folder_info[folder_id].last_written_address = file_info->address;
        }
    }

    return status;
}

sfs_status_t sfs_write_file(uint32_t file_id, uint8_t *data, uint32_t data_len)
{
    sfs_status_t status;
    sfs_file_info_t file_info;
    uint32_t old_addr;
    uint16_t folder_id = FOLDER(file_id) - 1;

    file_info.file_header.file_id = file_id;
    file_info.file_header.data_len = data_len;
    /** Search for an existing file */
    status = sfs_search(SFS_SEARCH_FILE_ID, &file_info);
    old_addr = file_info.address;

    /** Search for new space */
    status = sfs_search(SFS_SEARCH_FREE_SPACE, &file_info);
    if (status == SFS_STATUS_SUCCESS)
    {
        file_info.file_header.status = ACTIVE_FILE;
        file_info.file_header.crc16 = crc16_compute(data, data_len, NULL);
        sfs_param->sfs_folder_info[folder_id].last_written_address = file_info.address;
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

    return SFS_STATUS_SUCCESS;
}

sfs_status_t sfs_read_file(uint32_t file_id, uint8_t *data, uint32_t data_len)
{
    sfs_status_t status;
    sfs_file_info_t file_info;

    file_info.file_header.file_id = file_id;
    file_info.file_header.data_len = data_len;

    /** Search for the file */
    status = sfs_search(SFS_SEARCH_FILE_ID, &file_info);

    if (status == SFS_STATUS_SUCCESS)
    {
        status = sfs_read_file_data(&file_info, data, data_len);
    }

    return status;
}

sfs_status_t sfs_init(sfs_parameters_t *sfs_parameters)
{
    uint8_t i;

    sfs_param = sfs_parameters;

    /** Initialize last written_address to zero */
    for (i = 0; i < sfs_param->nbr_folders; i++)
    {
        sfs_param->sfs_folder_info[i].last_written_address = 0;
    }
    /* TODO: Find the last written address for all folders */
    /* TODO: perform address check for data and garbage collection address */
    return SFS_STATUS_SUCCESS;
}

sfs_status_t sfs_uninit(void)
{
    return SFS_STATUS_SUCCESS;
}
