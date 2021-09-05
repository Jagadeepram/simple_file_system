/**
 * @brief BLE Running Speed and Cadence Collector application main file.
 *
 * This file contains the source code for a sample Running Speed and Cadence collector application.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_drv_uart.h"
#include "nrf_delay.h"

#include "app_timer.h"
#include "boards.h"
#include "app_uart.h"
#include "crc16.h"

#include "uart_command.h"
#include "ext_mem_driver.h"
#include "simple_fs.h"

/** When UART is used for communication with the host do not use flow control.*/
#define UART_HWFC NRF_UART_HWFC_DISABLED

/** UART Special characters used in packet framing */
#define STX 0x02
#define ETX 0x03
#define DLE 0x04

/** Optimal to have half of the maximum packet length (4096 bytes) */
#define UART_RX_FIFO_SIZE (1024 * 2)
#define UART_TX_FIFO_SIZE (1024 * 2)

/** Should be more than the packet length (4096 bytes) */
#define UART_RX_BUFFER_SIZE (1024 * 5)
#define UART_TX_BUFFER_SIZE (1024 * 5)

/**
 * @brief UART data receive states.
 */
typedef enum
{
    UART_RX_STATE_NONE = 0, UART_RX_STATE_BEGIN, UART_RX_STATE_PROGRESS
} uart_rx_state_t;

typedef void (*cmd_function_t)(uart_cmd_t *p_uart_cmd);
static cmd_function_t get_cmd_function(uart_cmd_t *p_uart_cmd);
static void parse_cmd(uint16_t rx_data_len);
static void clear_cmd(void);
static uart_cmd_t m_uart_cmd;
static uint8_t m_rx_buffer[UART_RX_BUFFER_SIZE];
static uint8_t m_tx_buffer[UART_TX_BUFFER_SIZE];
static uart_rx_state_t m_uart_rx_state = UART_RX_STATE_NONE;
static bool m_data_ready_to_send = false;
static bool m_crc_match_status = true;

/********** Command Functions ********************/
/**@brief Function to test UART communication */
void cmd_uart_test(uart_cmd_t *p_uart_cmd);
/**@brief Function to read data from a specific address */
void cmd_ext_mem_read(uart_cmd_t *p_uart_cmd);
/**@brief Function to write from a specific address */
void cmd_ext_mem_write(uart_cmd_t *p_uart_cmd);
/**@brief Function to erase a page */
void cmd_ext_mem_page_erase(uart_cmd_t *p_uart_cmd);
/**@brief Function to erase entire chip */
void cmd_ext_mem_chip_erase(uart_cmd_t *p_uart_cmd);
/**@brief Function to read sfs */
void cmd_sfs_read(uart_cmd_t *p_uart_cmd);
/**@brief Function to write sfs */
void cmd_sfs_write(uart_cmd_t *p_uart_cmd);
/**@brief Function to write a file in parts */
void cmd_sfs_write_in_parts(uart_cmd_t *p_uart_cmd);
/**@brief Function to read a file in parts */
void cmd_sfs_read_in_parts(uart_cmd_t *p_uart_cmd);
/**@brief Function to read last written file info */
void cmd_sfs_last_written(uart_cmd_t *p_uart_cmd);
/**
 * @brief Command structure for command handling.
 */
typedef struct
{
    uint16_t cmd;
    cmd_function_t cmd_function;
} cmd_struct_t;

cmd_struct_t cmd_struct[] = {   { COMMAND_UART_TEST, cmd_uart_test },
                                { COMMAND_EXT_MEM_READ, cmd_ext_mem_read },
                                { COMMAND_EXT_MEM_WRITE, cmd_ext_mem_write },
                                { COMMAND_EXT_MEM_PAGE_ERASE, cmd_ext_mem_page_erase },
                                { COMMAND_EXT_MEM_CHIP_ERASE, cmd_ext_mem_chip_erase },
                                { COMMAND_SFS_READ, cmd_sfs_read },
                                { COMMAND_SFS_WRITE, cmd_sfs_write },
                                { COMMAND_SFS_WRITE_IN_PARTS, cmd_sfs_write_in_parts },
                                { COMMAND_SFS_READ_IN_PARTS, cmd_sfs_read_in_parts },
                                { COMMAND_SFS_LAST_WRITTEN, cmd_sfs_last_written}};

/**@brief Function for handling UART errors.
 *
 * @param[in]   p_event   UART event.
 */
void uart_error_handle(app_uart_evt_t *p_event)
{
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        /** Wait for 5 seconds to avoid continues restart before WP is ready during bootup */
        nrf_delay_ms(5000);
        NRF_LOG_INFO("UART error");
        //APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}

/**@brief Function for handling UART initialization. */
void uart_init(void)
{
    uint32_t err_code;

    const app_uart_comm_params_t comm_params = {
    RX_PIN_NUMBER,
    TX_PIN_NUMBER,
    RTS_PIN_NUMBER,
    CTS_PIN_NUMBER,
    UART_HWFC,
    false, NRF_UART_BAUDRATE_115200 };

    APP_UART_FIFO_INIT(&comm_params, UART_RX_FIFO_SIZE, UART_TX_FIFO_SIZE, uart_error_handle, _PRIO_APP_HIGH, err_code);

    APP_ERROR_CHECK(err_code);

    /** Set UART data receive state to the BEGIN to receive data */
    m_uart_rx_state = UART_RX_STATE_BEGIN;

    NRF_LOG_INFO("Initialize UART FIFO");
}

static void uart_put(char byte)
{
    while (app_uart_put(byte) != NRF_SUCCESS);
}

/**@brief Function for escaping bytes in the UART stream.
 *
 * @param[in]   byte   Byte to escape and write.
 */
static void uart_write_escaped(uint8_t byte)
{
    if (byte == STX || byte == ETX || byte == DLE)
    {
#ifdef DEBUG
        NRF_LOG_INFO("Escaping byte 0x%02x as 0x%02x%02x", byte, DLE, (uint8_t) ~byte);
#endif
        uart_put(DLE);
        uart_put(~byte);
    }
    else
    {
        uart_put(byte);
    }
}

/**@brief Function for writing a buffer to UART.
 *
 * @param[in]   buf   Buffer to write.
 * @param[in]   len   Length of buffer.
 */
static void uart_write_buffer(void *buf, int len)
{
#ifdef DEBUG
    NRF_LOG_INFO("Writing to UART with length %d", len);
    ASSERT(buf);
#endif

    uint8_t *ptr = (uint8_t*) buf;
    uint32_t i;

    for (i = 0; i < len; i++)
    {
#ifdef NO_UART_ESCAPING
        uart_put(*ptr++);
#else
        uart_write_escaped(*ptr++);
#endif
    }
}

cmd_function_t get_cmd_function(uart_cmd_t *p_uart_cmd)
{
    cmd_function_t cmd_function = NULL;
    uint16_t i;
    uint16_t nbr_of_cmds = sizeof(cmd_struct) / sizeof(cmd_struct_t);

    if (p_uart_cmd == NULL)
    {
        return NULL;
    }

    for (i = 0; i < nbr_of_cmds; i++)
    {
        if (p_uart_cmd->cmd_resp == cmd_struct[i].cmd)
        {
            return cmd_struct[i].cmd_function;
        }
    }

    return cmd_function;
}

static void parse_cmd(uint16_t rx_data_len)
{
    uint16_t index = 0;
    uint16_t crc16 = 0;

    memcpy(&m_uart_cmd.crc, m_rx_buffer + index, sizeof(m_uart_cmd.crc));
    index += sizeof(m_uart_cmd.crc);

    /** Check crc16 */
    crc16 = crc16_compute(m_rx_buffer + index, rx_data_len - index, NULL);
    if (crc16 != m_uart_cmd.crc)
    {
        /** Set CRC mismatch status */
        m_crc_match_status = false;
        m_uart_cmd.nbr_arg = ZERO;
        return;
    }
    else
    {
        m_crc_match_status = true;
    }

    memcpy(&m_uart_cmd.msg_id, m_rx_buffer + index, sizeof(m_uart_cmd.msg_id));
    index += sizeof(m_uart_cmd.msg_id);

    memcpy(&m_uart_cmd.cmd_resp, m_rx_buffer + index, sizeof(m_uart_cmd.cmd_resp));
    index += sizeof(m_uart_cmd.cmd_resp);

    memcpy(&m_uart_cmd.nbr_arg, m_rx_buffer + index, sizeof(m_uart_cmd.nbr_arg));
    index += sizeof(m_uart_cmd.nbr_arg);

    for (uint16_t i = 0; i < m_uart_cmd.nbr_arg; i++)
    {
        memcpy(&m_uart_cmd.arg[i], m_rx_buffer + index, sizeof(m_uart_cmd.arg[0]));
        index += sizeof(m_uart_cmd.arg[0]);
    }

    m_uart_cmd.paylen = rx_data_len - index;
    memcpy(m_uart_cmd.payload, m_rx_buffer + index, m_uart_cmd.paylen);
}

static void send_cmd_data(uart_cmd_t *p_uart_cmd)
{
    uint16_t tx_data_len;
    uint16_t crc16;
    uint16_t msg_data_len;

    if (m_crc_match_status == false)
    {
        /** UART Packet CRC Error */
        p_uart_cmd->cmd_resp = UART_RESP_PKT_CRC_ERROR;
        p_uart_cmd->nbr_arg = 0;
        p_uart_cmd->paylen = 0;
    }

    tx_data_len = sizeof(p_uart_cmd->crc);
    msg_data_len = sizeof(p_uart_cmd->msg_id);
    msg_data_len += sizeof(p_uart_cmd->cmd_resp);
    msg_data_len += sizeof(p_uart_cmd->nbr_arg);
    msg_data_len += p_uart_cmd->nbr_arg * sizeof(p_uart_cmd->arg[0]);
    msg_data_len += p_uart_cmd->paylen;

    memcpy(m_tx_buffer + tx_data_len, &p_uart_cmd->msg_id, sizeof(p_uart_cmd->msg_id));
    tx_data_len += sizeof(p_uart_cmd->msg_id);

    memcpy(m_tx_buffer + tx_data_len, &p_uart_cmd->cmd_resp, sizeof(p_uart_cmd->cmd_resp));
    tx_data_len += sizeof(p_uart_cmd->cmd_resp);

    memcpy(m_tx_buffer + tx_data_len, &p_uart_cmd->nbr_arg, sizeof(p_uart_cmd->nbr_arg));
    tx_data_len += sizeof(p_uart_cmd->nbr_arg);

    for (uint16_t i = 0; i < p_uart_cmd->nbr_arg; i++)
    {
        memcpy(m_tx_buffer + tx_data_len, &p_uart_cmd->arg[i], sizeof(p_uart_cmd->arg[0]));
        tx_data_len += sizeof(p_uart_cmd->arg[0]);
    }

    memcpy(m_tx_buffer + tx_data_len, p_uart_cmd->payload, p_uart_cmd->paylen);
    tx_data_len += p_uart_cmd->paylen;

    crc16 = crc16_compute(m_tx_buffer + sizeof(p_uart_cmd->crc), msg_data_len, NULL);
    /** Update calculated CRC */
    memcpy(m_tx_buffer, &crc16, sizeof(p_uart_cmd->crc));

    uart_put(STX);
    uart_write_buffer(m_tx_buffer, tx_data_len);
    uart_put(ETX);
}

static void execute_send_cmd(void)
{
    cmd_function_t cmd_function = get_cmd_function(&m_uart_cmd);

    if (cmd_function != NULL)
    {
        /** Execute the command when CRC matched */
        if (m_crc_match_status == true)
        {
            cmd_function(&m_uart_cmd);
        }
    }
    else
    {
        clear_cmd();
        /** Send the error COMMAND NOT SUPPORTED */
        m_uart_cmd.cmd_resp = UART_RESP_CMD_NOT_SUPPORTED;
    }

    /** Send the result after executing command */
    send_cmd_data(&m_uart_cmd);
}

static void clear_cmd(void)
{
    m_crc_match_status = true;
    m_uart_cmd.cmd_resp = UART_RESP_NO_ERROR;
    m_uart_cmd.crc = ZERO;
    m_uart_cmd.nbr_arg = ZERO;
    m_uart_cmd.paylen = ZERO;
}

void uart_data_handle(void)
{
    uint8_t cr;
    uint32_t err_code;
    static uint16_t rx_data_len;
    static uint8_t negate_byte;

    if ((m_data_ready_to_send == true) && (m_uart_rx_state == UART_RX_STATE_BEGIN))
    {
        send_cmd_data(&m_uart_cmd);
        m_data_ready_to_send = false;
    }

    err_code = app_uart_get(&cr);

    if (err_code == NRF_SUCCESS)
    {
        switch (m_uart_rx_state)
        {
            case UART_RX_STATE_NONE:
            case UART_RX_STATE_BEGIN:
                if (cr == STX)
                {
                    rx_data_len = 0;
                    negate_byte = 0;
                    m_uart_rx_state = UART_RX_STATE_PROGRESS;
                }
                break;
            case UART_RX_STATE_PROGRESS:
                if (cr == STX && negate_byte == 0)
                {
                    rx_data_len = 0;
                    m_uart_rx_state = UART_RX_STATE_PROGRESS;
                }
                else if (cr == ETX && negate_byte == 0)
                {
                    if (rx_data_len == 0)
                    {
                        uart_put(STX);
                        uart_put(ETX);
                    }
                    else
                    {
                        app_uart_flush();
                        parse_cmd(rx_data_len);
                        execute_send_cmd();
                        clear_cmd();
                    }
                    m_uart_rx_state = UART_RX_STATE_BEGIN;
                }
                else if (cr == DLE && negate_byte == 0)
                {
                    negate_byte = 1;
                }
                else
                {
                    if (negate_byte == 1)
                    {
                        m_rx_buffer[rx_data_len] = ~cr;
                        negate_byte = 0;
                    }
                    else
                    {
                        m_rx_buffer[rx_data_len] = cr;
                    }
                    rx_data_len++;
                }
                break;
            default:
                break;
        }
    }
}

void cmd_uart_test(uart_cmd_t *p_uart_cmd)
{
    /** Do nothing, the goal of this function is to transfer all the incoming data.
     But set the response to zero */
    p_uart_cmd->cmd_resp = UART_RESP_NO_ERROR;
}

void cmd_ext_mem_write(uart_cmd_t *p_uart_cmd)
{
    p_uart_cmd->cmd_resp = memory_write(p_uart_cmd->arg[0], p_uart_cmd->payload, p_uart_cmd->paylen);
}

void cmd_ext_mem_read(uart_cmd_t *p_uart_cmd)
{
    p_uart_cmd->cmd_resp = memory_read(p_uart_cmd->arg[0], p_uart_cmd->payload, p_uart_cmd->arg[1]);
    p_uart_cmd->paylen = p_uart_cmd->arg[1];
}

void cmd_ext_mem_page_erase(uart_cmd_t *p_uart_cmd)
{
    p_uart_cmd->cmd_resp = memory_erase_page(p_uart_cmd->arg[0]);
}

void cmd_ext_mem_chip_erase(uart_cmd_t *p_uart_cmd)
{
    memory_erase_chip();
    sfs_uninit();
    p_uart_cmd->cmd_resp = UART_RESP_NO_ERROR;
}

void cmd_sfs_read(uart_cmd_t *p_uart_cmd)
{
    sfs_file_info_t file_info;
    /** Assign file name */
    file_info.file_header.file_id = p_uart_cmd->arg[0];
    p_uart_cmd->cmd_resp = sfs_read_file_info(&file_info);

    p_uart_cmd->paylen = file_info.file_header.data_len;
    /** Send the  file info */
    p_uart_cmd->arg[1] = file_info.address;
    p_uart_cmd->arg[2] = file_info.file_header.file_id;
    p_uart_cmd->arg[3] = file_info.file_header.data_len;
    p_uart_cmd->arg[4] = file_info.file_header.status;
    p_uart_cmd->arg[5] = file_info.address + sizeof(sfs_file_header_t) + file_info.file_header.data_len;
    p_uart_cmd->nbr_arg = 6;

    if (p_uart_cmd->cmd_resp == 0)
    {
        p_uart_cmd->cmd_resp = sfs_read_file_data(&file_info, p_uart_cmd->payload, file_info.file_header.data_len);
    }
}

void cmd_sfs_write(uart_cmd_t *p_uart_cmd)
{
    uint32_t address;
    sfs_file_header_t header;

    p_uart_cmd->cmd_resp = sfs_write_file(p_uart_cmd->arg[0], p_uart_cmd->payload, p_uart_cmd->paylen);
    if (sfs_last_written_info(p_uart_cmd->arg[0], &address, &header) == SFS_STATUS_SUCCESS)
    {
        /** Send the last written file info */
        p_uart_cmd->arg[1] = address;
        p_uart_cmd->arg[2] = header.file_id;
        p_uart_cmd->arg[3] = header.data_len;
        p_uart_cmd->arg[4] = header.status;
        p_uart_cmd->arg[5] = address + sizeof(header) + header.data_len;
        p_uart_cmd->nbr_arg = 6;
    }
}

void cmd_sfs_write_in_parts(uart_cmd_t *p_uart_cmd)
{
    p_uart_cmd->cmd_resp = sfs_write_file_in_parts(p_uart_cmd->arg[0], p_uart_cmd->arg[1], p_uart_cmd->payload, p_uart_cmd->paylen);
}

void cmd_sfs_read_in_parts(uart_cmd_t *p_uart_cmd)
{
    p_uart_cmd->cmd_resp = sfs_read_file_in_parts(p_uart_cmd->arg[0], p_uart_cmd->arg[1], p_uart_cmd->payload, p_uart_cmd->arg[2]);
    p_uart_cmd->paylen = p_uart_cmd->arg[2];
}

void cmd_sfs_last_written(uart_cmd_t *p_uart_cmd)
{
    uint32_t address;
    sfs_file_header_t header;

    p_uart_cmd->cmd_resp = sfs_last_written_info(p_uart_cmd->arg[0], &address, &header);
    /** Send the last written file info */
    p_uart_cmd->arg[1] = address;
    p_uart_cmd->arg[2] = header.file_id;
    p_uart_cmd->arg[3] = header.data_len;
    p_uart_cmd->arg[4] = header.status;
    p_uart_cmd->arg[5] = address + sizeof(header) + header.data_len;
    p_uart_cmd->nbr_arg = 6;
}
