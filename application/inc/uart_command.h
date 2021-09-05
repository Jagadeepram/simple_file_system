#ifndef UART_COMMAND_H
#define UART_COMMAND_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "nrf_atfifo.h"

/* Maximum number of arugments in a command */
#define MAX_NBR_ARGU 10
/* Maximum bytes in Payload of a command */
#define MAX_PAYLOAD_LEN 4096

typedef enum
{
  ZERO = 0,
  ONE = 1,
  TWO = 2,
  THREE = 3,
  FOUR = 4,
  FIVE = 5,
  SIX = 6,
  SEVEN = 7,
  EIGHT = 8,
  NINE = 9,
  MAX_ARG = MAX_NBR_ARGU
} index_numbering_t;

typedef enum
{
  RESET_REASON_NONE = 0,
  RESET_REASON_RESET_PIN = 1,
  RESET_REASON_WATCHDOG = 2,
  RESET_REASON_SOFT_RESET = 3,
  RESET_REASON_CPU_LOCK_UP = 4,
  RESET_REASON_DETECT_SIGNAL = 5,
  RESET_REASON_ANADETECT_SIGNAL = 6,
  RESET_REASON_DEBUG_INTERFACE = 7
} reset_reasons_t;

/**@brief UART Response Codes */
typedef enum
{
  /** No Error */
  UART_RESP_NO_ERROR = 0,
  /** Firmware does not exist */
  UART_RESP_FW_NOT_EXIST = 1,
  /** Firmware already exists */
  UART_RESP_FW_EXIST = 2,
  /** Already connected to TR */
  UART_RESP_NOT_READY_FOR_FOTA  = 3,
  /** UART packet CRC error */
  UART_RESP_PKT_CRC_ERROR = 4,
  /** UART firmware CRC error */
  UART_RESP_FW_CRC_ERROR = 5,
  /** UART DATA not as per protocol */
  UART_RESP_CMD_DATA_ERROR = 6,
  /** COMMAND NOT SUPPORTED */
  UART_RESP_CMD_NOT_SUPPORTED = 7
} uart_response_codes_t;


/**@brief Command struct of a UART packet */
typedef struct
{
  uint16_t crc;
  uint16_t msg_id;
  uint16_t cmd_resp;
  uint16_t nbr_arg;
  uint32_t arg[MAX_NBR_ARGU];
  uint32_t paylen;
  uint8_t payload[MAX_PAYLOAD_LEN];
} uart_cmd_t;

/** Command to test UART protocol */
#define COMMAND_UART_TEST               0x0001
/** Command to write raw data into ext mem */
#define COMMAND_EXT_MEM_WRITE           0x0010
/** Command to read raw data into ext mem */
#define COMMAND_EXT_MEM_READ            0x0011
/** Command to perform page erase */
#define COMMAND_EXT_MEM_PAGE_ERASE      0x0012
/** Command to perform chip erase */
#define COMMAND_EXT_MEM_CHIP_ERASE      0x0013

/** Simple File system commands */
#define COMMAND_SFS_READ             0x0100
#define COMMAND_SFS_WRITE            0x0101
/** Command to write file in parts */
#define COMMAND_SFS_READ_IN_PARTS    0x0102
/** Command to read file in parts */
#define COMMAND_SFS_WRITE_IN_PARTS   0x0103
/** Command to read last written file */
#define COMMAND_SFS_LAST_WRITTEN   0x0104

/**@brief Function for writing the atomic FIFO to UART. */
void atfifo_to_uart(void);

/**@brief Function for handling UART initialization. */
void uart_init(void);
/**@brief Initialize all FIFOs. */
void fifo_init(void);
/**@brief Function for handling UART RX. */
void uart_data_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_COMMAND_H  */
