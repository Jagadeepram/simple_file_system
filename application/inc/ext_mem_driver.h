#ifndef EXTERNAL_MEMORY_H
#define EXTERNAL_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nrf_delay.h"
#include "app_util_platform.h"
#include "app_error.h"
#include "boards.h"
#include "sdk_config.h"


/**@brief Initialize External Memory
 *
 */
void ext_mem_init(void);

/**@brief Erase page
 *
 * @param[in]  uint32_t Address belongs to the page to be deleted
 */
void ext_mem_erase_page(uint32_t start_address);

/**@brief Erase sector
 *
 * @param[in]  uint32_t Address belongs to the sector to be deleted
 */
void ext_mem_erase_sector(uint32_t start_address);

/**@brief Erase entire chip
 */
void ext_mem_erase_chip(void);

/**@brief Write data to the external memory
 *
 * @param[in]  uint8_t* Data buffer to write
 * @param[in]  uint32_t Length of the data to write
 * @param[in]  uint32_t Address of the data
 *
 * @return ret_code_t
 */
uint32_t ext_mem_write_data(uint8_t *data, uint32_t len, uint32_t address);

/**@brief Read data from the external memory
 *
 * @param[in]  uint8_t* Data buffer to read
 * @param[in]  uint32_t Length of the data to read
 * @param[in]  uint32_t Address of the data
 *
 * @return ret_code_t
 */
uint32_t ext_mem_read_data(uint8_t *data, uint32_t len, uint32_t address);

/**@brief Perform external memory test for read and write
 */
void ext_mem_test(void);
#ifdef __cplusplus
}
#endif

#endif //EXTERNAL_MEMORY_H
