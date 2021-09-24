
#ifndef LARGE_FILE_STORAGE_H_
#define LARGE_FILE_STORAGE_H_


/** Start address of the storage. Do not start at 0x0 location */
#define START_ADDRESS           0x80000
/** End address of the storage */
#define END_ADDRESS             0x100000
/** Allocated size for one file in multiples of 4K */
#define FILE_ALLOC_SIZE         (73728) // 4096 * 18

/** A space to store a new file */
#define SPACE_FOR_NEW_FILE      (0xFF)
/** Partially written file */
#define PARTIAL_FILE            (0x7F)
/** A file written, but unread */
#define UNREAD_FILE             (0x3F)
/** A file is written and read */
#define READ_FILE               (0x0F)
/** Obsolete or deleted file */
#define OBSOLETE_FILE           (0x00)

/*** Meas Storage Status ***/
typedef enum
{
    MEAS_STORE_STATUS_SUCCESS = 0,
    MEAS_STORE_STATUS_INTERNAL_ERROR,
    MEAS_STORE_STATUS_INVALID_FILE,
    MEAS_STORE_STATUS_IO_ERROR
} meas_store_status_t;

typedef struct __attribute__((packed))
{
    uint8_t status;
    uint32_t file_id;
    uint32_t file_len;
} file_header_t;

meas_store_status_t write_measurement_in_parts(uint32_t rem_len, uint8_t *data, uint32_t data_len);
meas_store_status_t read_measurement_in_parts(uint32_t file_address, uint32_t rem_len, uint8_t *data, uint32_t data_len);
uint32_t first_written_file_address(void);


#endif /* LARGE_FILE_STORAGE_H_ */
