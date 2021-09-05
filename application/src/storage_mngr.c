#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "app_util_platform.h"
#include "app_error.h"
#include "nrf_gpio.h"
#include "ext_mem_driver.h"
#include "simple_fs.h"
#include "storage_mngr.h"

static sfs_parameters_t sfs_parameters;
static sfs_folder_info_t sfs_folder_info[TOTAL_NBR_FOLDER];

sfs_status_t init_storage (void)
{
    ext_mem_init();

    /** Function to write external memory */
    sfs_parameters.mem_write = memory_write;
    /** Function to read external memory */
    sfs_parameters.mem_read = memory_read;
    /** Function to page erase external memory */
    sfs_parameters.mem_erase = memory_erase;
    /** Total size of the memory allocation */
    sfs_parameters.mem_len = MEMORY_SIZE;
    /** Address for the garbage collection */
    sfs_parameters.gc_address = MEM_START_ADDRESS;
    /** Length of the garbage collection */
    sfs_parameters.gc_len = MEM_SECTOR_SIZE;
    /** Number of folders within the total allocation (sfs_properties.mem_len) */
    sfs_parameters.nbr_folders = 3;

    sfs_folder_info[CONFIG_FOLDER].page_len = MEM_SECTOR_SIZE;
    sfs_folder_info[CONFIG_FOLDER].folder_len = sfs_folder_info[CONFIG_FOLDER].page_len * 2;
    sfs_folder_info[CONFIG_FOLDER].start_address = MEM_START_ADDRESS + MEM_SECTOR_SIZE + MEM_PAGE_SIZE * 5;

    sfs_folder_info[LOG_FOLDER].page_len = MEM_PAGE_SIZE;
    sfs_folder_info[LOG_FOLDER].folder_len = sfs_folder_info[LOG_FOLDER].page_len * 2;
    sfs_folder_info[LOG_FOLDER].start_address = (sfs_folder_info[CONFIG_FOLDER].start_address + sfs_folder_info[CONFIG_FOLDER].folder_len);

    sfs_folder_info[DATA_FOLDER].page_len = MEM_SECTOR_SIZE;
    sfs_folder_info[DATA_FOLDER].folder_len = sfs_folder_info[DATA_FOLDER].page_len;
    sfs_folder_info[DATA_FOLDER].start_address = (sfs_folder_info[LOG_FOLDER].start_address + sfs_folder_info[LOG_FOLDER].folder_len);

    sfs_parameters.sfs_folder_info = sfs_folder_info;

    return sfs_init(&sfs_parameters);
}

sfs_status_t uninit_storage (void)
{
    free(sfs_folder_info);
    return SFS_STATUS_SUCCESS;
}
