#ifndef _UTILS_TRANSFER_H
#define _UTILS_TRANSFER_H

#include "utils/utils_communication.h"
#include "utils/utils_md5.h"

#define HEADER_SEPARATOR "&"
#define HEADER_FIELDS 4
#define HEADER_KIND 0
#define HEADER_NAME 1
#define HEADER_SIZE 2
#define HEADER_MD5 3
#define DECIMAL_BASE 10

#define KIND_OBJECT "OBJECT"
#define KIND_DELIVERY "DELIVERY"
#define KIND_SIGNED "SIGNED"
#define KIND_CLAIM "CLAIM"

typedef struct {
    char *psFileName;
    long nSize;
    char *acMd5;
} FileInfo;

int parseHeader(const Message *ptMsg, TransferType eType, FileInfo *ptInfo);
void freeFileInfo(FileInfo *ptInfo);

#endif