#ifndef SIMPLE_FS_H
#define SIMPLE_FS_H

#include "stdint.h"

#define ADDRESS_BIT_MASK          (0x0FFFF)
#define FILE_ID_MASK              (0xFFFFF)
#define ADDRESS_ALIGNMENT         (0x1000)

//#define MOD_DIV(a, b) (((a)/(b))*(b))

#define ABS_DIFF(a, b) ((a) < (b)? ((b)-(a)):((a)-(b)))
/** Find starting address of a page
 *     a: address of file
 *     b: starting address of the folder
 *     c: page size of the folder
 */
#define PAGE_START_ADDR(a, b, c) (((((a)-(b))/(c))*(c))+(b))

/** Convert to a FILE ID from a given folder and file name */
#define FILE_ID(folder, file) (((folder) << 16) | ((file) & ADDRESS_BIT_MASK))

#define SFS_SMALL(a, b) ((a)<(b) ? (a):(b))

/** Return maximum value based on the type */
#define MAX_VALUE_OF_TYPE(a) ((1<<(sizeof((a))*8))-1)

/** Set the length for data buffer for internal transfer */
#define DATA_TRANSFER_SIZE 4096

/*** Simple File System Status ***/
typedef enum
{
    SFS_STATUS_SUCCESS = 0,
    SFS_STATUS_BLANK,
    SFS_STATUS_DRIVER_ERROR,
    SFS_STATUS_FILE_LEN_MISMATCH,
    SFS_STATUS_WRONG_FOLDER,
    SFS_STATUS_FILE_NOT_FOUND,
    SFS_STATUS_WARE_OUT,
    SFS_STATUS_NO_SPACE,
    SFS_STATUS_CRC_ERROR,
    SFS_STATUS_READ_ERROR,
    SFS_STATUS_GARBAGE_ERROR,
    SFS_STATUS_ADDRESS_ALIGNMENT_ERROR,
    SFS_STATUS_INTERNAL_ERROR,
    SFS_STATUS_MEM_CPY_ERROR
} sfs_status_t;

typedef struct __attribute__((packed))
{
    uint8_t status;
    uint32_t file_id;
    uint32_t data_len;
    uint16_t crc16;
} sfs_file_header_t;

typedef struct
{
    sfs_file_header_t file_header;
    uint32_t address;
} sfs_file_info_t;

typedef struct
{
    /** The start address must be 4096 aligned */
    uint32_t start_address;
    /** Folder length should always be multiple of page_len */
    uint32_t folder_len;
    uint32_t last_written_address;
    /** This value should be
     *          Greater than largest file the folder shall contain
     *          Multiple of 4096 (or 4K)
     *          Less than or equal to folder_len
     */
    uint32_t page_len;
} sfs_folder_info_t;

typedef uint32_t (*mem_write_t)(uint32_t, uint8_t*, uint32_t);
typedef uint32_t (*mem_read_t)(uint32_t, uint8_t*, uint32_t);
typedef uint32_t (*mem_erase_t)(uint32_t, uint32_t);

typedef struct
{
    mem_write_t mem_write;
    mem_read_t mem_read;
    mem_erase_t mem_erase;
    uint32_t mem_len;
    uint32_t gc_len;
    uint32_t gc_address;
    uint8_t nbr_folders;
    sfs_folder_info_t *sfs_folder_info;
} sfs_parameters_t;

sfs_status_t sfs_write_file(uint32_t file_id, uint8_t *data, uint32_t data_len);
sfs_status_t sfs_read_file(uint32_t file_id, uint8_t *data, uint32_t data_len);
sfs_status_t sfs_write_file_in_parts(uint32_t file_id, uint32_t rem_len, uint8_t *data, uint32_t data_len);
sfs_status_t sfs_read_file_in_parts(uint32_t file_id, uint32_t rem_len, uint8_t *data, uint32_t data_len);
sfs_status_t sfs_read_file_info(sfs_file_info_t *file_info);
sfs_status_t sfs_read_file_data(sfs_file_info_t *file_info, uint8_t *data, uint32_t data_len);
sfs_status_t sfs_init(sfs_parameters_t *sfs_parameters);
sfs_status_t sfs_uninit(void);

/** Only for debugging */
sfs_status_t sfs_last_written_info(uint32_t file_id, uint32_t *address, sfs_file_header_t *header);

#endif // SIMPLE_FS_H
