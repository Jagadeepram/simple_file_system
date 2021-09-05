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

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#define FOLDER(FILE_ID)  ((FILE_ID)>>16)

/** Page status */
/** A fresh page where no data exits */
#define NEW_PAGE       (0xFF)
/** A page where active data and some free space exits*/
#define ACTIVE_PAGE    (0x7F)
/** A page where at least one active data and no free space exits*/
#define OLD_PAGE       (0x07)
/** A page where no active data and no free space exits*/
#define OBSOLETE_PAGE  (0x00)

/** File status */
/** A space is available for new file */
#define NEW_FILE       (0xFF)
/** An partially active file */
#define PARTIAL_FILE   (0x3F)
/** An active file is found */
#define ACTIVE_FILE    (0x0F)
/** End of page, no more data exits beyond this address in a page */
#define END_PAGE       (0x03)
/** An invalidated file is found */
#define OLD_FILE       (0x00)

static sfs_parameters_t *sfs_param;

typedef enum
{
    SFS_SEARCH_NONE = 0,
    /** Search for free space */
    SFS_SEARCH_FREE_SPACE,
    /** Search for last written file */
    SFS_SEARCH_LAST_FILE_ADDR,
    /** Search for a particular FILE */
    SFS_SEARCH_FILE_ID,
    /** Search for a partially written FILE */
    SFS_SEARCH_PARTIAL_FILE_ID
} sfs_search_type_t;

static sfs_status_t sfs_search(sfs_search_type_t search, sfs_file_info_t *file_info);
static sfs_status_t sfs_search_page(uint32_t addr, uint32_t start_addr, sfs_search_type_t search, sfs_file_info_t *file_info, uint32_t page_len);
static sfs_status_t sfs_perform_gc(sfs_file_info_t *file_info, uint32_t *nbr_of_fresh_pages_after_gc);
static sfs_status_t sfs_move_active_files_to_gc_pages(uint32_t *address, uint32_t folder_start_address, uint32_t folder_end_address,
                                                      uint32_t page_size, uint32_t gc_start_address, uint32_t gc_size);
static sfs_status_t sfs_copy_data(uint32_t source_address, uint32_t dest_address, uint32_t data_len);
static sfs_status_t sfs_update_data_pages(uint32_t folder_start_address, uint32_t folder_end_address, uint32_t page_size, uint32_t gc_start_address,
                                          uint32_t gc_size);

static sfs_status_t sfs_copy_data(uint32_t source_address, uint32_t dest_address, uint32_t data_len)
{
    uint8_t data[DATA_TRANSFER_SIZE];
//uint8_t temp[DATA_TRANSFER_SIZE];
    uint32_t len;

    while (data_len > 0)
    {
        if (data_len >= DATA_TRANSFER_SIZE)
        {
            len = DATA_TRANSFER_SIZE;
        }
        else
        {
            len = data_len;
        }

        if (sfs_param->mem_read(source_address, data, len) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }

        if (sfs_param->mem_write(dest_address, data, len) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }

//if (sfs_param->mem_read(dest_address, temp, len) != 0)
//{
//    return SFS_STATUS_DRIVER_ERROR;
//}
//
//if (memcmp(data, temp, len) != 0)
//{
//    return SFS_STATUS_DRIVER_ERROR;
//}

        source_address += len;
        dest_address += len;
        data_len -= len;
    }
    return SFS_STATUS_SUCCESS;
}

static sfs_status_t sfs_move_active_files_to_gc_pages(uint32_t *address, uint32_t folder_start_address, uint32_t folder_end_address,
                                                      uint32_t page_size, uint32_t gc_address, uint32_t gc_size)
{
    uint32_t page_start_address;
    uint32_t gc_start_address = gc_address;
    uint32_t gc_end_address = gc_address + gc_size;
    sfs_status_t status = SFS_STATUS_BLANK;
    uint8_t page_state;
    sfs_file_header_t file_header;

    /** Copy active files to erased GC pages */
    /** Iterate through all folder pages */
    while ((*address < folder_end_address) && (gc_address < gc_end_address) && (status == SFS_STATUS_BLANK))
    {
        *address = PAGE_START_ADDR((*address), folder_start_address, page_size);
        page_start_address = *address;

        /** Read the page status */
        if (sfs_param->mem_read(*address, &page_state, sizeof(page_state)) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }

        if (page_state == OBSOLETE_PAGE)
        {
            if (sfs_param->mem_erase(*address, page_size))
            {
                return SFS_STATUS_DRIVER_ERROR;
            }
            *address += page_size;
            continue;
        }
        else if (page_state == NEW_PAGE)
        {
            /** All active files are collected */
            status = SFS_STATUS_SUCCESS;
        }

        *address += sizeof(page_state);
        /** Iterate through all files in a folder page */
        while ((*address < (page_start_address + page_size)) && (gc_address < gc_end_address) && (status == SFS_STATUS_BLANK))
        {
            if (sfs_param->mem_read(*address, (uint8_t*) &file_header, sizeof(file_header)) != 0)
            {
                return SFS_STATUS_DRIVER_ERROR;
            }

            if (file_header.status == OLD_FILE)
            {
                *address += (sizeof(file_header) + file_header.data_len);
            }
            else if (file_header.status == END_PAGE || file_header.status == NEW_FILE)
            {
                *address = PAGE_START_ADDR((*address), folder_start_address, page_size);
                if (sfs_param->mem_erase(*address, page_size))
                {
                    return SFS_STATUS_DRIVER_ERROR;
                }
//                /** Make the current page as obsolete as reaching end of page */
//                /** The update should be done at the starting address of this page */
//                page_state = OBSOLETE_PAGE;
//                *address = PAGE_START_ADDR((*address), folder_start_address, page_size);
//                //*address = MOD_DIV((*address), page_size);
//                if (sfs_param->mem_write(*address, &page_state, sizeof(page_state)) != 0)
//                {
//                 return SFS_STATUS_DRIVER_ERROR;
//                }

                /** Update address to the next page */
                *address += page_size;

                if (file_header.status == NEW_FILE)
                {
                    /** All active files are collected */
                    status = SFS_STATUS_SUCCESS;
                    break;
                }
            }
            else if (file_header.status == ACTIVE_FILE || file_header.status == PARTIAL_FILE)
            {
                uint32_t next_gc_address = (gc_address + sizeof(file_header) + file_header.data_len);
                /** Check if the active file fits into the current garbage page */
                if (PAGE_START_ADDR(next_gc_address, gc_start_address, page_size) == PAGE_START_ADDR(gc_address, gc_start_address, page_size))
                {
                    /** The current active file fits into the garbage page, so copy it to the garbage page */
                    /** Update the page as active */
                    if (PAGE_START_ADDR(gc_address, gc_start_address, page_size) == gc_address)
                    {
                        if (sfs_param->mem_erase(gc_address, page_size))
                        {
                            return SFS_STATUS_DRIVER_ERROR;
                        }
                        page_state = ACTIVE_PAGE;
                        if (sfs_param->mem_write(gc_address, &page_state, sizeof(page_state)))
                        {
                            return SFS_STATUS_DRIVER_ERROR;
                        }
                        gc_address += sizeof(page_state);
                    }
                    /** Copy the active file to garbage collection page */
                    if (sfs_copy_data(*address, gc_address, (sizeof(file_header) + file_header.data_len)) != SFS_STATUS_SUCCESS)
                    {
                        return SFS_STATUS_DRIVER_ERROR;
                    }
                    /** Update the status as OLD file */
                    file_header.status = OLD_FILE;
                    if (sfs_param->mem_write(*address, (uint8_t*) &file_header.status, sizeof(file_header.status)) != 0)
                    {
                        return SFS_STATUS_DRIVER_ERROR;
                    }
                    gc_address += (sizeof(file_header) + file_header.data_len);
                    *address += (sizeof(file_header) + file_header.data_len);
                }
                else
                {
                    /** Data crossing a GC page */
                    /** Mark it as end of the page and copy this file starting from the next page */
                    file_header.status = END_PAGE;
                    if (sfs_param->mem_write(gc_address, (uint8_t*) &file_header.status, sizeof(file_header.status)) != 0)
                    {
                        return SFS_STATUS_DRIVER_ERROR;
                    }
                    /** Go to the beginning of the page and mark it as old */
                    gc_address = PAGE_START_ADDR(gc_address, gc_start_address, page_size);
                    page_state = OLD_PAGE;
                    if (sfs_param->mem_write(gc_address, &page_state, sizeof(page_state)) != 0)
                    {
                        return SFS_STATUS_DRIVER_ERROR;
                    }
                    gc_address += page_size;
                }
            }
            else
            {
                /** Should not reach here */
                status = SFS_STATUS_GARBAGE_ERROR;
                break;
            }
        }
    }

    return status;
}

static sfs_status_t sfs_update_data_pages(uint32_t folder_start_address, uint32_t folder_end_address, uint32_t page_size, uint32_t gc_start_address,
                                          uint32_t gc_size)
{
    uint32_t gc_end_address = gc_start_address + gc_size;
    uint8_t gc_page_state, data_page_state;
    sfs_status_t status = SFS_STATUS_BLANK;
//    uint32_t start_address = folder_start_address;
//    uint32_t end_address = folder_end_address;

//    while (start_address < end_address)
//    {
//        /** Read the data page status */
//        if (sfs_param->mem_read(start_address, &data_page_state, sizeof(data_page_state)) != 0)
//        {
//            return SFS_STATUS_DRIVER_ERROR;
//        }
//        if (data_page_state == OBSOLETE_PAGE)
//        {
//            /** Erase obsolete pages */
//            if (sfs_param->mem_erase(start_address, page_size) != 0)
//            {
//                return SFS_STATUS_DRIVER_ERROR;
//            }
//        }
//        start_address += page_size;
//    }

    while ((gc_start_address < gc_end_address) && (folder_start_address < folder_end_address))
    {
        /** Read the data page status */
        if (sfs_param->mem_read(folder_start_address, &data_page_state, sizeof(data_page_state)) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }

        if (data_page_state == NEW_PAGE)
        {
            /** Read the GC page status */
            if (sfs_param->mem_read(gc_start_address, &gc_page_state, sizeof(gc_page_state)) != 0)
            {
                return SFS_STATUS_DRIVER_ERROR;
            }

            if (gc_page_state != NEW_PAGE)
            {
                /** Copy contents of GC page to the data page */
                status = sfs_copy_data(gc_start_address, folder_start_address, page_size);
                if (status != SFS_STATUS_SUCCESS)
                {
                    return SFS_STATUS_DRIVER_ERROR;
                }
            }
            gc_start_address += page_size;
        }
        /** Move to the next page */
        folder_start_address += page_size;
    }
    return status;
}

static sfs_status_t sfs_find_nbr_of_free_pages(uint32_t start_address, uint32_t end_address, uint32_t page_size, uint32_t *nbr_pages)
{
    uint8_t page_state;
    *nbr_pages = 0;

    while (start_address < end_address)
    {
        if (sfs_param->mem_read(start_address, &page_state, sizeof(page_state)) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }
        if ((page_state == NEW_PAGE) || (page_state == ACTIVE_PAGE))
        {
            *nbr_pages += 1;
        }
        start_address += page_size;
    }

    return SFS_STATUS_SUCCESS;
}

static sfs_status_t sfs_perform_gc(sfs_file_info_t *file_info, uint32_t *nbr_of_fresh_pages_after_gc)
{
    uint16_t folder_id = FOLDER(file_info->file_header.file_id) - 1;

    uint32_t folder_start_address = sfs_param->sfs_folder_info[folder_id].start_address;
    uint32_t folder_end_address = folder_start_address + sfs_param->sfs_folder_info[folder_id].folder_len;
    uint32_t folder_page_size = sfs_param->sfs_folder_info[folder_id].page_len;

    uint32_t address = folder_start_address;
    sfs_status_t gc_status;
    sfs_status_t status;
    sfs_file_info_t temp_file_info;

    while (address < folder_end_address)
    {
        /** Copy active files from folders to GC pages */
        gc_status = sfs_move_active_files_to_gc_pages(&address, folder_start_address, folder_end_address, folder_page_size, sfs_param->gc_address,
                                                      sfs_param->gc_len);

        if ((gc_status == SFS_STATUS_SUCCESS) || (gc_status == SFS_STATUS_BLANK))
        {
            status = sfs_update_data_pages(folder_start_address, folder_end_address, folder_page_size, sfs_param->gc_address, sfs_param->gc_len);
        }
        else
        {
            /** Something went wrong, end the garbage collection */
            status = gc_status;
            break;
        }

        if ((gc_status == SFS_STATUS_SUCCESS) || (address >= folder_end_address))
        {
            /** Find last written space */
            temp_file_info = *file_info;
            status = sfs_search(SFS_SEARCH_LAST_FILE_ADDR, &temp_file_info);
            if (status == SFS_STATUS_SUCCESS)
            {
                sfs_param->sfs_folder_info[folder_id].last_written_address = temp_file_info.address;
                /** Calculate the number of blank pages available */
                status = sfs_find_nbr_of_free_pages(folder_start_address, folder_end_address, folder_page_size, nbr_of_fresh_pages_after_gc);
            }
            break;
        }
    }

    return status;
}

static sfs_status_t sfs_search_page(uint32_t addr, uint32_t start_addr, sfs_search_type_t search, sfs_file_info_t *file_info, uint32_t page_len)
{
    sfs_status_t status = SFS_STATUS_BLANK;
    uint8_t page_state;
    sfs_file_header_t file_header;
    sfs_file_info_t last_file;
    uint32_t end_addr;
    /** Assign non zero value if the page contains an active file */
    uint8_t is_active_file = 0;

    /** Initialize last written address to zero */
    last_file.address = 0;
    /** Find the start address of the page */
    start_addr = PAGE_START_ADDR(addr, start_addr, page_len);
    /** Find the end address of the page */
    end_addr = start_addr + page_len;
    /** Read the page status */
    if (sfs_param->mem_read(start_addr, &page_state, sizeof(page_state)) != 0)
    {
        return SFS_STATUS_DRIVER_ERROR;
    }

    /** Do not process the obsolete page */
    /** Do not search for new space in the old page */
    if ((page_state == OBSOLETE_PAGE) || (search == SFS_SEARCH_FREE_SPACE && page_state == OLD_PAGE))
    {
        return SFS_STATUS_BLANK;
    }

    if (start_addr == addr)
    {
        /** addr is equal to the start of the page, then increment by 1.
         * Sometimes the addr is taken from the last written address and it
         * should not be incremented in that case.*/
        addr += sizeof(page_state);
    }

    while (addr < end_addr)
    {
        if (sfs_param->mem_read(addr, (uint8_t*) &file_header, sizeof(file_header)) != 0)
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
            uint32_t file_len = file_info->file_header.data_len + sizeof(sfs_file_header_t) + sizeof(page_state);
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
                file_info->file_header.status = file_header.status;
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
        else if (search == SFS_SEARCH_FILE_ID || search == SFS_SEARCH_PARTIAL_FILE_ID)
        {
            if (((file_header.status == ACTIVE_FILE && search == SFS_SEARCH_FILE_ID)
                    || (file_header.status == PARTIAL_FILE && search == SFS_SEARCH_PARTIAL_FILE_ID))
                    && (file_header.file_id == file_info->file_header.file_id))
            {
                /** Read partial file */
                file_info->file_header = file_header;
                file_info->address = addr;
                return SFS_STATUS_SUCCESS;
            }
            /** Reach end of file search, file not found */
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
        else
        {
            switch (file_header.status)
            {
                case NEW_FILE:
                case PARTIAL_FILE:
                case ACTIVE_FILE:
                case END_PAGE:
                case OLD_FILE:
                    break;
                default:
                    /** Should not reach here */
                    return SFS_STATUS_INTERNAL_ERROR;
            }
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
        /** Assign the last file info at the end of page search */
        *file_info = last_file;
    }

    return status;
}

static sfs_status_t sfs_search(sfs_search_type_t search, sfs_file_info_t *file_info)
{
    uint16_t folder_id = FOLDER(file_info->file_header.file_id) - 1;
    uint32_t addr, start_addr;
    uint32_t end_addr;
    uint32_t last_written_address;
    uint32_t nbr_of_fresh_pages_after_gc = 0;
    uint32_t page_len;
    sfs_status_t status = SFS_STATUS_BLANK;

    if (folder_id >= sfs_param->nbr_folders)
    {
        /** Wrong folder ID. This folder doesn't exist */
        return SFS_STATUS_WRONG_FOLDER;
    }

    addr = sfs_param->sfs_folder_info[folder_id].start_address;
    end_addr = addr + sfs_param->sfs_folder_info[folder_id].folder_len;
    last_written_address = sfs_param->sfs_folder_info[folder_id].last_written_address;
    page_len = sfs_param->sfs_folder_info[folder_id].page_len;
    /** Set address to zero before the search */
    file_info->address = 0;
    start_addr = addr;

    /** Update search address from last written file while finding new space */
    if ((search == SFS_SEARCH_FREE_SPACE) && (last_written_address != 0))
    {
        addr = last_written_address;
    }

    for (; (addr < end_addr) && (status == SFS_STATUS_BLANK);)
    {
        status = sfs_search_page(addr, start_addr, search, file_info, page_len);
        /** Assign start address of a page */
        addr = PAGE_START_ADDR(addr, start_addr, page_len);
        addr += page_len;
    }

    if (status == SFS_STATUS_BLANK)
    {
        if (file_info->address == 0)
        {
            if (search == SFS_SEARCH_FREE_SPACE)
            {
                /** Run garbage collection when unable to find free space */
                status = sfs_perform_gc(file_info, &nbr_of_fresh_pages_after_gc);
                if (status == SFS_STATUS_SUCCESS)
                {
                    if (nbr_of_fresh_pages_after_gc > 0)
                    {
                        /** Find next available address to write */
                        status = sfs_search(search, file_info);
                    }
                    else
                    {
                        /** No pages available for write after GC */
                        status = SFS_STATUS_NO_SPACE;
                    }
                }
            }
            else if (search == SFS_SEARCH_FILE_ID || search == SFS_SEARCH_PARTIAL_FILE_ID)
            {
                /** Assign file not found after going through all the pages */
                status = SFS_STATUS_FILE_NOT_FOUND;
            }
        }
        if (search == SFS_SEARCH_LAST_FILE_ADDR)
        {
            /** Store last written address into the folder info */
            status = SFS_STATUS_SUCCESS;
        }
    }

    return status;
}

sfs_status_t sfs_write_file(uint32_t file_id, uint8_t *data, uint32_t data_len)
{
    sfs_status_t status;
    sfs_file_info_t new_file_info;
    sfs_file_info_t old_file_info;
    uint16_t folder_id = FOLDER(file_id) - 1;

    new_file_info.file_header.file_id = file_id;
    new_file_info.file_header.data_len = data_len;
    old_file_info.file_header.file_id = file_id;
    old_file_info.address = 0;
    /** Search for new space and perform GC if needed*/
    status = sfs_search(SFS_SEARCH_FREE_SPACE, &new_file_info);
    /** Search for an existing file */
    sfs_search(SFS_SEARCH_FILE_ID, &old_file_info);
    if (status == SFS_STATUS_SUCCESS)
    {
        new_file_info.file_header.status = ACTIVE_FILE;
        new_file_info.file_header.data_len = data_len;
        new_file_info.file_header.crc16 = crc16_compute(data, data_len, NULL);
        sfs_param->sfs_folder_info[folder_id].last_written_address = new_file_info.address;
        NRF_LOG_INFO("Data written at %x, len: %d", new_file_info.address, data_len);
        /** Write the header */
        if (sfs_param->mem_write(new_file_info.address, (uint8_t*) &new_file_info.file_header, sizeof(sfs_file_header_t)) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }
        /** Write the data */
        new_file_info.address += sizeof(sfs_file_header_t);
        if (sfs_param->mem_write(new_file_info.address, data, data_len) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }
    }

    /** Inactivate the old file */
    if (old_file_info.address)
    {
        old_file_info.file_header.status = OLD_FILE;
        NRF_LOG_INFO("Data invalidated at %x, len: %d", old_file_info.address, old_file_info.file_header.data_len);
        /** Write the header */
        if (sfs_param->mem_write(old_file_info.address, (uint8_t*) &old_file_info.file_header.status, sizeof(old_file_info.file_header.status)) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }
    }
    NRF_LOG_FLUSH();
    return status;
}

sfs_status_t sfs_read_file_info(sfs_file_info_t *file_info)
{
    return sfs_search(SFS_SEARCH_FILE_ID, file_info);;
}

sfs_status_t sfs_read_file_data(sfs_file_info_t *file_info, uint8_t *data, uint32_t data_len)
{
    NRF_LOG_INFO("Read from %x, len: %d", file_info->address, data_len);
    NRF_LOG_FLUSH();

    file_info->address += sizeof(sfs_file_header_t);

    if (data_len != file_info->file_header.data_len)
    {
        return SFS_STATUS_FILE_LEN_MISMATCH;
    }

    if (sfs_param->mem_read(file_info->address, data, data_len) != 0)
    {
        return SFS_STATUS_DRIVER_ERROR;
    }

    /** Ignore CRC error when it does not contain valid data */
    if ((crc16_compute(data, data_len, NULL) != file_info->file_header.crc16) && (file_info->file_header.crc16 != 0xFFFF))
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
sfs_status_t sfs_write_file_in_parts(uint32_t file_id, uint32_t rem_len, uint8_t *data, uint32_t data_len)
{
    sfs_status_t status;
    sfs_file_info_t new_file_info;
    sfs_file_info_t old_file_info;
    uint16_t folder_id = FOLDER(file_id) - 1;
    uint32_t file_address;

    new_file_info.file_header.file_id = file_id;

    /** Search for a partially written file */
    status = sfs_search(SFS_SEARCH_PARTIAL_FILE_ID, &new_file_info);

    if (status == SFS_STATUS_FILE_NOT_FOUND)
    {
        NRF_LOG_INFO("No Partial File Found %x", file_id);
        /** No parts written before, search for a new space */
        new_file_info.file_header.data_len = rem_len;
        status = sfs_search(SFS_SEARCH_FREE_SPACE, &new_file_info);
    }

    if ((status == SFS_STATUS_SUCCESS) && (new_file_info.file_header.status == NEW_FILE || new_file_info.file_header.status == PARTIAL_FILE))
    {
        file_address = new_file_info.address;
        /** Check if it contains a valid status */
        if (new_file_info.file_header.status == NEW_FILE)
        {
            NRF_LOG_INFO("Write a new file %x at %x", file_id, file_address);
            /** Length is uninitialized, assign a correct length */
            new_file_info.file_header.data_len = rem_len;
            /** Ignore CRC16 for files written in parts */
            new_file_info.file_header.crc16 = MAX_VALUE_OF_TYPE(new_file_info.file_header.crc16);
            new_file_info.file_header.file_id = file_id;
            new_file_info.file_header.status = PARTIAL_FILE;
            /** Write the header */
            if (sfs_param->mem_write(file_address, (uint8_t*) &new_file_info.file_header, sizeof(sfs_file_header_t)) != 0)
            {
                return SFS_STATUS_DRIVER_ERROR;
            }
        }
        /** Write the data */
        file_address += (sizeof(sfs_file_header_t) + new_file_info.file_header.data_len - rem_len);
        /** Write the data */
        if (sfs_param->mem_write(file_address, data, data_len) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }
        NRF_LOG_INFO("Write file part %x at %x for %d", file_id, file_address, data_len);

        if (data_len == rem_len)
        {
            /** Read the old file and update its status */
            old_file_info.file_header.file_id = file_id;
            old_file_info.address = 0;
            status = sfs_search(SFS_SEARCH_FILE_ID, &old_file_info);
            old_file_info.file_header.status = OLD_FILE;
            if (status == SFS_STATUS_SUCCESS && old_file_info.address)
            {
                NRF_LOG_INFO("Invalidate old file %x at %x", file_id, old_file_info.address);
                /** Update the status of the old file */
                if (sfs_param->mem_write(old_file_info.address, (uint8_t*) &old_file_info.file_header.status,
                                         sizeof(old_file_info.file_header.status)) != 0)
                {
                    return SFS_STATUS_DRIVER_ERROR;
                }
            }
            /** Point address to the beginning of the file header */
            file_address += (data_len - new_file_info.file_header.data_len - sizeof(new_file_info.file_header));
            /** Update the status as active file and invalidate the old file while updating the last part */
            new_file_info.file_header.status = ACTIVE_FILE;
            if (sfs_param->mem_write(file_address, (uint8_t*) &new_file_info.file_header.status, sizeof(new_file_info.file_header.status)) != 0)
            {
                return SFS_STATUS_DRIVER_ERROR;
            }
            NRF_LOG_INFO("Activate the partial file %x at %x", file_id, file_address);
            sfs_param->sfs_folder_info[folder_id].last_written_address = file_address;
        }
    }
    NRF_LOG_FLUSH();
    return status;
}

sfs_status_t sfs_read_file_in_parts(uint32_t file_id, uint32_t rem_len, uint8_t *data, uint32_t data_len)
{
    sfs_status_t status;
    sfs_file_info_t file_info;
    uint32_t address;

    file_info.file_header.file_id = file_id;

    /** Search for the file */
    status = sfs_search(SFS_SEARCH_FILE_ID, &file_info);

    /** Read data in parts */
    if (status == SFS_STATUS_SUCCESS)
    {
        address = file_info.address + (file_info.file_header.data_len - rem_len) + sizeof(sfs_file_header_t);
        if (sfs_param->mem_read(address, data, data_len) != 0)
        {
            return SFS_STATUS_DRIVER_ERROR;
        }
        NRF_LOG_INFO("Read the partial file %x at %x, len %d", file_id, address, data_len);
    }
    NRF_LOG_FLUSH();
    return status;
}

sfs_status_t sfs_init(sfs_parameters_t *sfs_parameters)
{
    uint8_t i;
    sfs_status_t status = SFS_STATUS_SUCCESS;
    sfs_file_info_t file_info;

    sfs_param = sfs_parameters;

    /** Initialize last written_address */
    for (i = 0; (i < sfs_param->nbr_folders) && (status == SFS_STATUS_SUCCESS); i++)
    {
        file_info.file_header.file_id = FILE_ID((i + 1), 0);
        status = sfs_search(SFS_SEARCH_LAST_FILE_ADDR, &file_info);
        sfs_param->sfs_folder_info[i].last_written_address = file_info.address;

        if (sfs_param->sfs_folder_info[i].start_address % ADDRESS_ALIGNMENT != 0)
        {
            NRF_LOG_INFO("Address 0x%x not aligned with %x", sfs_param->sfs_folder_info[i].start_address, ADDRESS_ALIGNMENT);
            return SFS_STATUS_ADDRESS_ALIGNMENT_ERROR;
        }
    }
    /* TODO: perform address check for data and garbage collection address */
    return status;
}

sfs_status_t sfs_uninit(void)
{
    uint8_t i;

    /** Initialize last written_address to zero */
    for (i = 0; (i < sfs_param->nbr_folders); i++)
    {
        sfs_param->sfs_folder_info[i].last_written_address = 0;
    }
    return SFS_STATUS_SUCCESS;
}

/** Only for debugging */
sfs_status_t sfs_last_written_info(uint32_t file_id, uint32_t *address, sfs_file_header_t *header)
{
    uint16_t folder_id = FOLDER(file_id) - 1;

    *address = sfs_param->sfs_folder_info[folder_id].last_written_address;
    if (sfs_param->mem_read(*address, (uint8_t*)header, sizeof(sfs_file_header_t)) != 0)
    {
        return SFS_STATUS_DRIVER_ERROR;
    }

    return SFS_STATUS_SUCCESS;
}
