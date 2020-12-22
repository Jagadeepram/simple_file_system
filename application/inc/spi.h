
#ifndef SPI_H
#define SPI_H

#include "stdint.h"
#include "stdbool.h"
#include "sdk_errors.h"

/**
 * @brief SPI data transfer function
 *
 * @param tx_buff[in] Buffer containing data to be transmitted
 * @param tx_len[in] Number of bytes in tx_buff
 * @param rx_buff[in] Buffer to receive data
 * @param rx_len[in] Number of bytes to be received
 *
 * @return :: ret_code_t
 */
ret_code_t spi_txrx(const uint8_t *tx_buff, size_t tx_len, uint8_t *rx_buff, size_t rx_len);

/**
 * @brief Initialize SPI driver.
 *
 * @return ret_code_t
 */
void spi_init(void);

/**
 * @brief Un-Initialize SPI driver
 *
 * @return None
 */
void spi_uninit(void);

#endif // SPI_H
