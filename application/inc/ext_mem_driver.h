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

/* Memory size 1 Mbyte */
#define MEMORY_SIZE         (0x100000)
#define MEM_START_ADDRESS   (0x000000)


#define MEM_PAGE_SIZE         (0x1000)
#define MEM_SECTOR_SIZE      (0x10000)

/** Take the last sector () for Garbage collection **/
#define GC_ADDRESS (MEMORY_SIZE - MEM_SECTOR_SIZE)

/**@brief Initialize External Memory
 *
 */
void ext_mem_init(void);

/**@brief Erase
 *
 * @param[in]  uint32_t Address belongs to the page to be deleted
 * @param[in]  size Number of bytes to delete. It should be multiple of 4096 (or 4K).
 */

ret_code_t memory_erase (uint32_t address, uint32_t size);

/**@brief Erase page
 *
 * @param[in]  uint32_t Address belongs to the page to be deleted
 */
uint32_t memory_erase_page(uint32_t start_address);

/**@brief Erase sector
 *
 * @param[in]  uint32_t Address belongs to the sector to be deleted
 */
uint32_t memory_erase_sector(uint32_t start_address);

/**@brief Erase entire chip
 */
void memory_erase_chip(void);

/**@brief Write data to the external memory
 *
 * @param[in]  uint8_t* Data buffer to write
 * @param[in]  uint32_t Length of the data to write
 * @param[in]  uint32_t Address of the data
 *
 * @return ret_code_t
 */
ret_code_t memory_write(uint32_t address, uint8_t *data, uint32_t len);

/**@brief Read data from the external memory
 *
 * @param[in]  uint8_t* Data buffer to read
 * @param[in]  uint32_t Length of the data to read
 * @param[in]  uint32_t Address of the data
 *
 * @return ret_code_t
 */
ret_code_t memory_read(uint32_t address, uint8_t *data, uint32_t len);

/**@brief Perform external memory test for read and write
 */
void ext_mem_test(void);
#ifdef __cplusplus
}
#endif

#endif //EXTERNAL_MEMORY_H
