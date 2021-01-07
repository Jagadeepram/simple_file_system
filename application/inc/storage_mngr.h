#ifndef STORAGE_MNGR_H
#define ESTORAGE_MNGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "simple_fs.h"

enum {
    CONFIG_FOLDER = 0,
    LOG_FOLDER,
    DATA_FOLDER
};

sfs_status_t init_storage(void);
sfs_status_t uninit_storage(void);


#ifdef __cplusplus
}
#endif

#endif //STORAGE_MNGR_H
