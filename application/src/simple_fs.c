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


static sfs_parameters_t * sfs_param;

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
    for (uint8_t i = 0; i < sfs_param->nbr_folders; i++)
    {
        free(sfs_param->sfs_folder_info);
    }
    return SFS_STATUS_NONE;
}

sfs_status_t sfs_write_file(uint16_t file_id, uint8_t *data, uint16_t data_len)
{
    return SFS_STATUS_NONE;
}
