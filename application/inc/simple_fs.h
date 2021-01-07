
#ifndef SIMPLE_FS_H
#define SIMPLE_FS_H

#include "stdint.h"


/*** Simple File System Status ***/
typedef enum {
    SFS_STATUS_NONE = 0,
    SFS_STATUS_DRIVER_ERROR,
    SFS_STATUS_FILE_LEN_MISMATCH,
    SFS_STATUS_WRONG_FOLDER,
    SFS_STATUS_FILE_NOT_FOUND,
    SFS_STATUS_WARE_OUT,
    SFS_STATUS_NO_SPACE,
    SFS_STATUS_CRC_ERROR,
    SFS_STATUS_READ_ERROR
} sfs_status_t;

typedef struct __attribute__((packed)){
    uint8_t status;
    uint32_t file_id;
    uint16_t data_len;
    uint16_t crc16;
} sfs_file_header_t;

typedef struct {
    sfs_file_header_t file_header;
    uint32_t address;
} sfs_file_info_t;

typedef struct {
    uint32_t start_address;
    uint32_t folder_len;
    uint32_t last_written_address;
} sfs_folder_info_t;

typedef uint32_t (*mem_write_t)(uint32_t, uint8_t *,uint32_t);
typedef uint32_t (*mem_read_t)(uint32_t, uint8_t *,uint32_t);
typedef uint32_t (*mem_erase_page_t)(uint32_t);

typedef struct {
    mem_write_t mem_write;
    mem_read_t mem_read;
    mem_erase_page_t mem_erase_page;
    uint32_t mem_len;
    uint32_t gc_len;
    uint32_t gc_address;
    uint8_t nbr_folders;
    sfs_folder_info_t *sfs_folder_info;
} sfs_parameters_t;

sfs_status_t sfs_write_file(uint32_t file_id, uint8_t *data, uint32_t data_len);
sfs_status_t sfs_read_file(uint32_t file_id, uint8_t *data, uint32_t data_len);
sfs_status_t sfs_read_file_info(sfs_file_info_t *file_info);
sfs_status_t sfs_read_file_data(sfs_file_info_t *file_info, uint8_t *data, uint32_t data_len);
sfs_status_t sfs_init(sfs_parameters_t *sfs_parameters);
sfs_status_t sfs_uninit(void);

#endif // SIMPLE_FS_H
