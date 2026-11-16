#ifndef _UTILS_TRANSFER_H
#define _UTILS_TRANSFER_H

#include <stdlib.h>
#include <string.h>
#include "utils/utils_communication.h"
#include "utils/utils_md5.h"

#define HEADER_SEPARATOR "&"
#define HEADER_FIELDS 4
#define HEADER_KIND 0
#define HEADER_NAME 1
#define HEADER_SIZE 2
#define HEADER_MD5 3
#define DECIMAL_BASE 10

#define TRANSFER_OK 0
#define TRANSFER_ERROR -1
#define TRANSFER_PEER_GONE -2
#define TRANSFER_SIZE_ERROR -3
#define TRANSFER_MD5_ERROR -4
#define TRANSFER_REJECTED -5
#define TRANSFER_WRONG_FILE -6
#define TRANSFER_GLYPH_ERROR -7

#define KIND_OBJECT "OBJECT"
#define KIND_DELIVERY "DELIVERY"
#define KIND_SIGNED "SIGNED"
#define KIND_CLAIM "CLAIM"

typedef enum {
    TRANSFER_OBJECT,
    TRANSFER_DELIVERY,
    TRANSFER_SIGNED,
    TRANSFER_CLAIM
} TransferType;

typedef struct {
    char *psFileName;
    long nSize;
    char *psMd5;
} FileInfo;

int parseHeader(const Message *ptMsg, TransferType eType, FileInfo *ptInfo);
void freeFileInfo(FileInfo *ptInfo);

#endif