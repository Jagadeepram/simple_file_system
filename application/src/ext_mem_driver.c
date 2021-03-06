#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_delay.h"

#include "app_util_platform.h"
#include "app_error.h"
#include "spi.h"
#include "ext_mem_driver.h"

/* A key that identify formated memory */
#define MEM_INIT_KEY        (0xFEEDBABECAFEBEEF)

/* Length of a sector, a least amount of memory that can be erased */
#define SECTOR_LEN             (4*1024)

/* External Flash Commands */
/* External Flash Commands */
#define WRITE_STATUS_CMD        0x01
#define PAGE_PROGRAM_CMD        0x02
#define READ_DATA_CMD           0x03
#define READ_STATUS_CMD         0x05
#define WRITE_ENABLE_CMD        0x06
#define ERASE_4K_CMD            0x20
#define REST_ENABLE_CMD         0x66
#define REST_CMD                0x99
#define READ_RDIR_CMD           0x9F
#define ERASE_64K_CMD           0xD8
#define ERASE_FULL_CMD          0xC7
#define DEEP_POWER_DOWN         0xB9

#define MAX_MEMORY_ADDRESS      MEM_END_ADDRESS
#define MAX_PROGRAM_LEN         256
#define MAX_PROG_LEN_MASK       0xFF

/* Erase Full chip (1 MEGA Byte) */
#define ERASE_SECTOR_SIZE_FULL 1000

/* Erase 64K sector size */
#define ERASE_SECTOR_SIZE_64K  64

/* Erase 32K sector size */
#define ERASE_SECTOR_SIZE_32K  32

/* Erase 4K sector size */
#define ERASE_SECTOR_SIZE_4K   4

/* Starting address of the external memory */
#define EXT_MEM_START_ADDRESS 0x0

static ret_code_t spi_transfer(uint8_t *tx_buff, size_t tx_len, uint8_t *rx_buff, size_t  rx_len)
{
    ret_code_t err_code;

    nrf_gpio_pin_clear(SPI_nCS_PIN);

    err_code = spi_txrx(tx_buff, tx_len, rx_buff, rx_len);

    /* Enable write protect and disable chip select pin */
    nrf_gpio_pin_set(SPI_nCS_PIN);

    return err_code;
}

//static ret_code_t write_status_reg(uint8_t value)
//{
//    ret_code_t err_code;
//    uint8_t cmd[2] = { WRITE_STATUS_CMD, value };
//    uint8_t temp[2];
//
//    err_code = spi_transfer(cmd, 2, temp, 2);
//
//    return err_code;
//}

static ret_code_t wait_write_complete(void)
{
    ret_code_t err_code;
    uint8_t cmd[3] = { READ_STATUS_CMD, 0, 0 };
    uint8_t temp[3] = { 0 };

    do
    {
        err_code = spi_transfer(cmd, 3, temp, 3);
    } while (((temp[1] & 0x1) != 0) && (err_code == NRF_SUCCESS));

    return err_code;
}

static ret_code_t write_enable(void)
{
    ret_code_t err_code;
    uint8_t cmd = WRITE_ENABLE_CMD;
    uint8_t temp;

    err_code = spi_transfer(&cmd, 1, &temp, 1);

    return err_code;
}

static ret_code_t ext_mem_soft_reset(void)
{
    ret_code_t err_code;
    uint8_t cmd = REST_ENABLE_CMD;
    uint8_t temp;

    err_code = spi_transfer(&cmd, 1, &temp, 1);
    nrf_delay_ms(10);
    if (err_code == NRF_SUCCESS)
    {
        cmd = REST_CMD;
        err_code = spi_transfer(&cmd, 1, &temp, 1);
        nrf_delay_ms(10);
    }

    return err_code;
}

ret_code_t enable_ext_mem_deep_power_down(void)
{
    ret_code_t err_code;
    uint8_t cmd = DEEP_POWER_DOWN;
    uint8_t temp;

    /* Recommended to have a delay before the command */
    nrf_delay_ms(10);

    err_code = spi_transfer(&cmd, 1, &temp, 1);

    /* Must wait for at least couple of ms */
    nrf_delay_ms(10);
    return err_code;
}

ret_code_t release_ext_mem_deep_power_down(void)
{
    /* Toggle the CS pin to release from the deep power down */
    nrf_delay_ms(10);
    nrf_gpio_pin_clear(SPI_nCS_PIN);
    nrf_delay_ms(10);
    nrf_gpio_pin_set(SPI_nCS_PIN);
    nrf_delay_ms(10);
    return NRF_SUCCESS;
}

ret_code_t memory_erase(uint32_t address, uint32_t size)
{
    ret_code_t err_code;
    /* Find the starting address of the sector */
    uint8_t cmd[4] = {0};
    uint8_t temp[4];
    uint8_t count;

    memset(temp, 0 ,4);
    memset(cmd, 0, 4);

    if (size == ERASE_SECTOR_SIZE_4K)
    {
        cmd[0] = ERASE_4K_CMD;
        count = 1;
    }
    else if (size == ERASE_SECTOR_SIZE_64K)
    {
        cmd[0] = ERASE_64K_CMD;
        count = 1;
    }
    else if (size == ERASE_SECTOR_SIZE_FULL)
    {
        cmd[0] = ERASE_64K_CMD;
        /* 8M divided by 64K */
        count = 128;
//        /* 1M divided by 64K */
//        count = 16;
    }
    else
    {
        return NRF_ERROR_NOT_SUPPORTED;
    }

    do
    {
        /* Write enable */
        err_code = write_enable();

        if (err_code != NRF_SUCCESS)
        {
            break;
        }

        cmd[1] = (address >> 16) & 0xFF;
        cmd[2] = (address >> 8) & 0xFF;
        cmd[3] = (address & 0xFF);

        err_code = spi_transfer(cmd, 4, temp, 4);
        if (err_code != 0)
        {
            break;
        }

        err_code = wait_write_complete();
        if (err_code != 0)
        {
            break;
        }

        count--;
        if (count == 0)
        {
            break;
        }

        if (cmd[0] == ERASE_64K_CMD)
        {
            address += (64 * 1024);
        }

    } while (err_code == 0);

    return err_code;
}

ret_code_t memory_write(uint32_t address, uint8_t *data, uint32_t len)
{

    ret_code_t err_code = NRF_SUCCESS;
   /* Allocate extra 4 bytes for command and address */
   uint8_t data_bytes[MAX_PROGRAM_LEN + 4] = { 0 };
   /* Allocate extra 4 bytes for command and address */
   uint8_t dummy[MAX_PROGRAM_LEN + 4] = { 0 };
   size_t data_len, total_len, remaining_len;

   if (!data)
   {
       return NRF_ERROR_INVALID_PARAM;
   }

   if ((address + len) > MEM_END_ADDRESS)
   {
       /* Data size exceeds limit */
       return NRF_ERROR_DATA_SIZE;
   }

   total_len = 0;

   /* Write data as a set of MAX_PROGRAM_LEN number of bytes in a single write command */
   while ((total_len < len) && (err_code == NRF_SUCCESS))
   {
       /* Copy data only till the end of the current page */
       data_len = MAX_PROGRAM_LEN - (address & MAX_PROG_LEN_MASK);
       remaining_len = len - total_len;

       if (remaining_len < data_len)
       {
           /* Data length is less than the available write length */
           data_len = remaining_len;
       }

       data_bytes[0] = PAGE_PROGRAM_CMD;
       data_bytes[1] = (address >> 16) & 0xFF;
       data_bytes[2] = (address >> 8) & 0xFF;
       data_bytes[3] = (address & 0xFF);

       memcpy(&data_bytes[4], data + total_len, data_len);

       /* Always enable write before write/erase operation */
       err_code = write_enable();

       if (err_code == NRF_SUCCESS)
       {
           err_code = spi_transfer(data_bytes, data_len + 4, dummy, data_len + 4);
       }

       /* Wait for write completion (LSB of status register to '0') */
       if (err_code == NRF_SUCCESS)
       {
           err_code = wait_write_complete();
       }

       if (err_code == NRF_SUCCESS)
       {
           total_len += data_len;
           address += data_len;
       }
   }

    return err_code;
}

ret_code_t memory_read(uint32_t address, uint8_t *data, uint32_t len)
{

    ret_code_t err_code = NRF_SUCCESS;
    size_t data_len, total_len, remaining_len;
    /* Allocate extra 4 bytes for command and address */
    uint8_t data_bytes[MAX_PROGRAM_LEN + 4] = { 0 };
    /* Allocate extra 4 bytes for command and address */
    uint8_t dummy[MAX_PROGRAM_LEN + 4] = { 0 };

    if (!data)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    if ((address + len) > MEM_END_ADDRESS)
    {
        /* Data size exceeds limit */
        return NRF_ERROR_DATA_SIZE;
    }

    total_len = 0;

    /* Read data as a set of MAX_PROGRAM_LEN number of bytes in a single read command */
    while ((total_len < len) && (err_code == NRF_SUCCESS))
    {
        /* Copy data only till the end of the current page */
        data_len = MAX_PROGRAM_LEN - (address & MAX_PROG_LEN_MASK);
        remaining_len = len - total_len;

        if (remaining_len < data_len)
        {
            /* Data length is less than the available read length */
            data_len = remaining_len;
        }

        data_bytes[0] = READ_DATA_CMD;
        data_bytes[1] = (address >> 16) & 0xFF;
        data_bytes[2] = (address >> 8) & 0xFF;
        data_bytes[3] = (address & 0xFF);

        memset(&data_bytes[4], 0, data_len);

        err_code = spi_transfer(data_bytes, data_len + 4, dummy, data_len + 4);

        /* Copy the read data to the output buffer */
        memcpy(data + total_len, dummy + 4, data_len);

        if (err_code == NRF_SUCCESS)
        {
            total_len += data_len;
            address += data_len;
        }
    }

    return err_code;
}

void memory_erase_chip(void)
{
    uint64_t mem_key = MEM_INIT_KEY;

    memory_erase(0x0, ERASE_SECTOR_SIZE_FULL);
     /* Write memory key after formatting */
    memory_write(0x0, (uint8_t *)&mem_key, sizeof(mem_key));
}

uint32_t memory_erase_page(uint32_t start_address)
{
    return memory_erase(start_address, ERASE_SECTOR_SIZE_4K);
}

uint32_t memory_erase_sector(uint32_t start_address)
{
    return memory_erase(start_address, ERASE_SECTOR_SIZE_64K);
}

void ext_mem_init(void)
{
    uint64_t mem_key;

    spi_init();
    nrf_gpio_cfg_output(SPI_nCS_PIN);
    nrf_gpio_cfg_output(SPI_nWP_PIN);
    nrf_gpio_cfg_output(SPI_nHOLD_PIN);

    nrf_gpio_pin_set(SPI_nCS_PIN);
    nrf_gpio_pin_set(SPI_nWP_PIN);
    nrf_gpio_pin_set(SPI_nHOLD_PIN);

    /* Release from deep power down mode */
    release_ext_mem_deep_power_down();
    /* Perform software reset */
    ext_mem_soft_reset();

    /* Read first 8 bytes of memory */
    memory_read(0x0,(uint8_t *)&mem_key, sizeof(mem_key));
    if (mem_key == 0xFFFFFFFFFFFFFFFF)
    {
        /* Chip is formatted, update memory init key */
        mem_key = MEM_INIT_KEY;
        /* Write memory key after formatting */
        memory_write(0x0, (uint8_t *)&mem_key, sizeof(mem_key));
        NRF_LOG_INFO("Updated init key in external memory chip");
    }
    else if (mem_key != MEM_INIT_KEY)
    {
        NRF_LOG_INFO("Formatting external memory chip");
        NRF_LOG_FLUSH();
        /* Format memory */
        memory_erase_chip();
    }

    NRF_LOG_INFO("External memory Initiated");
}

/* Call this test function from main file to check if the external memory is
   working as expected */
#define TEST_ADDRESS (0x1000)
#define TEST_ARRAY_SIZE 4000
void ext_mem_test(void)
{
    uint8_t write_data[TEST_ARRAY_SIZE];
    uint8_t read_data[TEST_ARRAY_SIZE];
    uint16_t i;
    uint16_t j;
    uint32_t address = TEST_ADDRESS;
    uint32_t old_page;
    old_page = address & (MEM_PAGE_SIZE -1);

    NRF_LOG_INFO("Program Len %d", TEST_ARRAY_SIZE);
    for (j = 0 ; j < 1; j++)
    {

        if (old_page >= (address & (MEM_PAGE_SIZE -1)))
        {
            memory_erase(address, ERASE_SECTOR_SIZE_4K);
        }

        for (i = 0; i < TEST_ARRAY_SIZE; i++)
        {
            write_data[i] = i*j + 4;
        }

        memory_write( address, write_data, sizeof(write_data));
        memory_read( address, read_data, sizeof(read_data));

        if (memcmp(write_data, read_data, sizeof(write_data)) == 0)
        {
            NRF_LOG_INFO("Test %d success", j);
        }
        else
        {
            NRF_LOG_INFO("Test %d fail", j);
        }
        NRF_LOG_FLUSH();
        nrf_delay_ms(1000);
        old_page = address & (MEM_PAGE_SIZE -1);
        address += TEST_ARRAY_SIZE;
    }
}
