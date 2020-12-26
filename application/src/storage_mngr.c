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
static sfs_folder_info_t *sfs_folder_info;

sfs_status_t init_storage(void)
{
    ext_mem_init();

    /** Function to write external memory */
    sfs_parameters.mem_write = ext_mem_write_data;
    /** Function to read external memory */
    sfs_parameters.mem_read = ext_mem_read_data;
    /** Function to page erase external memory */
    sfs_parameters.mem_erase_page = ext_mem_erase_page;
    /** Total size of the memory allocation */
    sfs_parameters.mem_len = MEMORY_SIZE;
    /** Address for the garbage collection */
    sfs_parameters.gc_address = GC_ADDRESS;
    /** Length of the garbage collection */
    sfs_parameters.gc_len = MEM_SECTOR_SIZE;
    /** Number of folders within the total allocation (sfs_properties.mem_len) */
    sfs_parameters.nbr_folders = 3;
    /** Create allocation for the folders and provide folder info */
    sfs_folder_info = malloc(sfs_parameters.nbr_folders * sizeof(sfs_folder_info_t));

    sfs_folder_info[CONFIG_FOLDER].folder_len = MEM_SECTOR_SIZE;
    sfs_folder_info[CONFIG_FOLDER].start_address = MEM_START_ADDRESS;

    sfs_folder_info[LOG_FOLDER].folder_len = MEM_SECTOR_SIZE;
    sfs_folder_info[LOG_FOLDER].start_address = (sfs_folder_info[CONFIG_FOLDER].start_address
            + sfs_folder_info[CONFIG_FOLDER].folder_len);

    sfs_folder_info[DATA_FOLDER].folder_len = MEM_SECTOR_SIZE;
    sfs_folder_info[DATA_FOLDER].start_address = (sfs_folder_info[LOG_FOLDER].start_address
            + sfs_folder_info[LOG_FOLDER].folder_len);

    sfs_parameters.sfs_folder_info = sfs_folder_info;

    return sfs_init(&sfs_parameters);
}

