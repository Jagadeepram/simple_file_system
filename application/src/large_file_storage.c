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

#define SEARCH_SPACE    0x01
#define SEARCH_FILE     0x02

#define NEW_META_DATA_SPACE             (0xFF)
#define ACTIVE_META_DATA_SPACE          (0x01)
#define INACTIVE_META_DATA_SPACE        (0x00)

/** Meta data structure */
typedef struct __attribute__((packed))
{
    uint8_t status;
    uint32_t last_written_meas_address;
} meta_data_t;

static meas_store_status_t set_latest_written_file_address(uint32_t address);
static uint32_t get_latest_written_file_address(void);
static uint32_t get_meta_address(uint8_t action);
static uint32_t current_file_address;

static uint32_t get_meta_address(uint8_t action)
{
    uint32_t address = META_DATA_START_ADDRESS;
    uint8_t status;

    if (current_file_address == 0)
    {
        /** No current file info is stored. Start searching */
        while (address < META_DATA_END_ADDRESS)
        {
            memory_read(address, &status, sizeof(status));
            if (status == NEW_META_DATA_SPACE)
            {
                if (action == SEARCH_FILE)
                {
                    /** No file exists */
                    address = 0;
                }
                return address;
            }
            else if (status == ACTIVE_META_DATA_SPACE)
            {
                current_file_address = address;
                break;
            }
            address += sizeof(meta_data_t);
        }

        if (address >= META_DATA_END_ADDRESS)
        {
            if (action == SEARCH_FILE)
            {   /** No file exists */
                address = 0;
            }
        }
    }
    else
    {
        address = current_file_address;
    }

    if (action == SEARCH_SPACE)
    {
        if ((address + 2*(sizeof(meta_data_t))) < META_DATA_END_ADDRESS)
        {
            address += sizeof(meta_data_t);
        }
        else
        {
            address = META_DATA_START_ADDRESS;
            memory_erase(address, META_DATA_END_ADDRESS - META_DATA_START_ADDRESS);
        }
    }

    return address;
}

meas_store_status_t set_latest_written_file_address(uint32_t data_address)
{
    uint32_t address = get_meta_address(SEARCH_SPACE);
    uint32_t previous_address = current_file_address;
    meta_data_t meta_data;

    /** Update new data */
    meta_data.status = ACTIVE_META_DATA_SPACE;
    meta_data.last_written_meas_address = data_address;

    memory_write(address, (uint8_t *) &meta_data, sizeof(meta_data));

    /** Invalidate old data */
    if (previous_address != 0)
    {
        meta_data.status = INACTIVE_META_DATA_SPACE;
        memory_write(previous_address, (uint8_t *) &meta_data.status, sizeof(meta_data.status));
    }
//NRF_LOG_INFO("meta write address 0x%x meta last_wt_addr 0x%x", address, data_address);
    current_file_address = address;
    return MEAS_STORE_STATUS_SUCCESS;
}

uint32_t get_latest_written_file_address(void)
{
    uint32_t address = get_meta_address(SEARCH_FILE);
    uint32_t data_address = 0;
    meta_data_t meta_data;

    if (address != 0)
    {
        memory_read(address, (uint8_t *)&meta_data, sizeof(meta_data));
        data_address = meta_data.last_written_meas_address;
    }
//NRF_LOG_INFO("meta read address 0x%x meta last_wt_addr 0x%x", address, data_address);
    return data_address;
}

meas_store_status_t write_measurement_in_parts(uint32_t rem_len, uint8_t *data, uint32_t data_len)
{
    /** Last written address */
    uint32_t address = get_latest_written_file_address();
    /** Total number of files can be stored */
    uint32_t count = ((DATA_END_ADDRESS - DATA_START_ADDRESS)/FILE_ALLOC_SIZE) + 1;
    meas_store_status_t status = MEAS_STORE_STATUS_SUCCESS;
    file_header_t file_header;
    uint32_t erase_address;

    while (count > 0)
    {
        if ((address < DATA_START_ADDRESS) || ((address + FILE_ALLOC_SIZE) >= DATA_END_ADDRESS))
        {
            address = DATA_START_ADDRESS;
        }

        /** Check if there is a partial or new file */
        if (memory_read(address, (uint8_t *)&file_header, sizeof(file_header)) != 0)
        {
            return MEAS_STORE_STATUS_IO_ERROR;
        }

        if ((file_header.status == SPACE_FOR_NEW_FILE) || (file_header.status == PARTIAL_FILE))
        {
            /** End searching file */
            break;
        }
        count--;
        address += FILE_ALLOC_SIZE;
    }

    if (count == 0)
    {
        /** All allocations are occupied, erase the first allocation */
        address = DATA_START_ADDRESS;
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
        file_header.file_id = (address - DATA_START_ADDRESS)/FILE_ALLOC_SIZE;
        set_latest_written_file_address(address);
        file_header.file_len = rem_len;

        if (memory_write(address, (uint8_t *) &file_header, sizeof(file_header)) != 0)
        {
            return MEAS_STORE_STATUS_IO_ERROR;
        }

//NRF_LOG_INFO("New File %d len %d address 0x%x", file_header.file_id, rem_len, address);
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
        erase_address = ((address + (2*FILE_ALLOC_SIZE)) >= DATA_END_ADDRESS) ? DATA_START_ADDRESS : (address + FILE_ALLOC_SIZE);
        if (memory_erase(erase_address, FILE_ALLOC_SIZE) != 0)
        {
            return MEAS_STORE_STATUS_IO_ERROR;
        }

//NRF_LOG_INFO("End Of File %d len %d address 0x%x", file_header.file_id, file_header.file_len, address);
//NRF_LOG_INFO("Erase address 0x%x", erase_address);
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

    if (memory_read(file_address, (uint8_t *) &file_header, sizeof(file_header)) != 0)
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
//NRF_LOG_INFO("Mark Read File %d len %d address 0x%x", file_header.file_id, file_header.file_len, file_address);
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
    uint32_t address = get_latest_written_file_address();
    uint32_t count = ((DATA_END_ADDRESS - DATA_START_ADDRESS)/FILE_ALLOC_SIZE) + 1;
    uint32_t i;
    uint8_t status;

    for (i= 0; i < count; i++)
    {
        if ((address < DATA_START_ADDRESS) || ((address + FILE_ALLOC_SIZE) >= DATA_END_ADDRESS))
        {
            address = DATA_START_ADDRESS;
        }
        else
        {
            address += FILE_ALLOC_SIZE;
        }

        if (memory_read(address,  &status, sizeof(status)) != 0)
        {
            return MEAS_STORE_STATUS_IO_ERROR;
        }

        /** Check for unread file */
        if (status == UNREAD_FILE)
        {
            break;
        }
    }

    if (i == count)
    {
        /** No written files found, set the address to zero */
        address = 0;
    }
//NRF_LOG_INFO("Read File at address 0x%x", address);
    return address;
}
