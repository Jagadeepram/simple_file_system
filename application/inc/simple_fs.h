
#ifndef SIMPLE_FS_H
#define SIMPLE_FS_H

#include "stdint.h"


/*** Simple File System Status ***/
typedef enum {
    SFS_STATUS_NONE = 0,
    SFS_STATUS_FILE_NOT_FOUND,
    SFS_STATUS_WARE_OUT,
    SFS_STATUS_NO_SPACE,
} sfs_status_t;

typedef struct __attribute__((packed)){
    uint8_t status;
    uint16_t file_id;
    uint16_t data_len;
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

typedef uint32_t (*mem_write_t)(uint8_t *,uint32_t, uint32_t);
typedef uint32_t (*mem_read_t)(uint8_t *,uint32_t, uint32_t);
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

sfs_status_t sfs_write_file(uint16_t file_id, uint8_t *data, uint16_t data_len);
sfs_status_t sfs_read_file(uint16_t file_id, uint8_t *data, uint16_t data_len);
sfs_file_info_t sfs_read_file_info(uint16_t file_id);
sfs_status_t sfs_init(sfs_parameters_t *sfs_parameters);
sfs_status_t sfs_uninit(void);

#endif // SIMPLE_FS_H
