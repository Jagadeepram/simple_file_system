#include "boards.h"
#include "app_timer.h"
#include "sdk_config.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_error.h"
#include "app_error.h"
#include "large_file_storage.h"
#include "ext_mem_driver.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

static uint32_t current_file_address;

meas_store_status_t write_measurement_in_parts(uint32_t rem_len, uint8_t *data, uint32_t data_len)
{
    meas_store_status_t status = MEAS_STORE_STATUS_SUCCESS;
    file_header_t file_header;
    uint32_t address = START_ADDRESS;
    uint32_t erase_address;

    while ((address + FILE_ALLOC_SIZE) < END_ADDRESS)
    {
        /** Check if there is a partial or new file */
        if (memory_read(address, (uint8_t *) &file_header.status, sizeof(file_header.status)) != 0)
        {
            return MEAS_STORE_STATUS_IO_ERROR;
        }

        if ((file_header.status == SPACE_FOR_NEW_FILE) || (file_header.status == PARTIAL_FILE))
        {
            /** End searching file */
            break;
        }
        address += FILE_ALLOC_SIZE;
    }

    if ((address + FILE_ALLOC_SIZE) >= END_ADDRESS)
    {
        /** All allocations are occupied, erase the first allocation */
        address = START_ADDRESS;
        file_header.status = SPACE_FOR_NEW_FILE;
        if (memory_erase(address, FILE_ALLOC_SIZE) != 0)
        {
            return MEAS_STORE_STATUS_IO_ERROR;
        }
    }

    if (file_header.status == SPACE_FOR_NEW_FILE)
    {
        /** Create a new file */
        file_header.status = PARTIAL_FILE;
        file_header.file_id = (address - START_ADDRESS)/FILE_ALLOC_SIZE;
        current_file_address = address;
        file_header.file_len = rem_len;
        if (memory_write(address, (uint8_t *) &file_header, sizeof(file_header)) != 0)
        {
            return MEAS_STORE_STATUS_IO_ERROR;
        }
    }

    if (rem_len == data_len)
    {
        /** This is the last part of the file, mark the file as completed */
        file_header.status = UNREAD_FILE;
        if (memory_write(address, (uint8_t *) &file_header.status, sizeof(file_header.status)) != 0)
        {
            return MEAS_STORE_STATUS_IO_ERROR;
        }
        /** Erase memory for next file */
        erase_address = ((address + (2*FILE_ALLOC_SIZE)) >= END_ADDRESS) ? START_ADDRESS : (address + FILE_ALLOC_SIZE);
        if (memory_erase(erase_address, FILE_ALLOC_SIZE) != 0)
        {
            return MEAS_STORE_STATUS_IO_ERROR;
        }
    }

    address += ((file_header.file_len - rem_len) + sizeof(file_header));
    if (memory_write(address, data, data_len) != 0)
    {
        return MEAS_STORE_STATUS_IO_ERROR;
    }
    return status;
}

meas_store_status_t read_measurement_in_parts(uint32_t file_address, uint32_t rem_len, uint8_t *data, uint32_t data_len)
{
    meas_store_status_t status = MEAS_STORE_STATUS_SUCCESS;
    file_header_t file_header;

    if (memory_read(file_address, (uint8_t *) &file_header.status, sizeof(file_header.status)) != 0)
    {
        return MEAS_STORE_STATUS_IO_ERROR;
    }

    if ((file_header.status == SPACE_FOR_NEW_FILE) || (file_header.status == OBSOLETE_FILE))
    {
        return MEAS_STORE_STATUS_INVALID_FILE;
    }

    if (rem_len == data_len)
    {
        /** This is the last part of the file, mark the file as read */
        file_header.status = READ_FILE;
        if (memory_write(file_address, (uint8_t *) &file_header.status, sizeof(file_header.status)) != 0)
        {
            return MEAS_STORE_STATUS_IO_ERROR;
        }
    }

    file_address += ((file_header.file_len - rem_len) + sizeof(file_header));
    if (memory_read(file_address, data, data_len) != 0)
    {
        return MEAS_STORE_STATUS_IO_ERROR;
    }
    return status;
}

uint32_t first_written_file_address(void)
{
    uint32_t address = current_file_address + FILE_ALLOC_SIZE;
    uint32_t count = (END_ADDRESS - START_ADDRESS)/FILE_ALLOC_SIZE;
    uint32_t i;
    uint8_t status;

    for (i= 0; i < count; i++)
    {
        if ((address < START_ADDRESS) || ((address + FILE_ALLOC_SIZE) >= END_ADDRESS))
        {
            address = START_ADDRESS;
        }

        /** Check if there is a partial or new file */
        if (memory_read(address,  &status, sizeof(status)) != 0)
        {
            return MEAS_STORE_STATUS_IO_ERROR;
        }

        if (status == UNREAD_FILE)
        {
            break;
        }
        address += FILE_ALLOC_SIZE;
    }

    if (i == count)
    {
        /** No written files found, set the address to zero */
        address = 0;
    }

    return address;
}
