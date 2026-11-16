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

// Format of the HEADER payload: <KIND>&<FileName>&<FileSize>&<MD5SUM>
#define HEADER_FORMAT "%s&%s&%ld&%s"

// Seconds to wait for each frame of a transfer before giving the peer up
#define TRANSFER_TIMEOUT_SECONDS 10
// Biggest size the HEADER can announce (9 digits, see isPositiveInteger)
#define TRANSFER_MAX_SIZE 999999999L
// Permissions of the files created by a receiver
#define TRANSFER_FILE_MODE 0600

// Payloads of READY and FINAL ACK (flags 0x80 on success, 0xC0 on error)
#define REPLY_READY "READY"
#define REPLY_OK "OK"
#define REPLY_SIZE_ERROR "SIZE_ERROR"
#define REPLY_MD5_ERROR "MD5_ERROR"
#define REPLY_BAD_HEADER "INVALID_HEADER"
#define REPLY_FILE_ERROR "FILE_ERROR"

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

/*
 * Transfer API. Every function returns one of the TRANSFER_* codes. READY and
 * FINAL ACK are always sent by the receiver (0x80 ok / 0xC0 error); the
 * HEADER and DATA flags depend on the kind (spec 8.4). All of them are
 * reentrant, so a pool worker can call them.
 *
 * sendFile: HEADER -> wait READY -> N x DATA -> wait FINAL ACK. The file is
 *   streamed in MESSAGE_DATA_MAX chunks. psFileName goes in the HEADER.
 * receiveFile: waits for the HEADER, parses it, then does receiveBody.
 *   Out ptInfo holds the header (free with freeFileInfo).
 * receiveBody: for servers that already read the HEADER in their select()
 *   loop and parsed it with parseHeader. Sends READY, receives the DATA into
 *   psDestPath, checks size and MD5 and sends the FINAL ACK. On any failure
 *   the partial file is deleted.
 * rejectTransfer: tells the sender it cannot start (READY with 0xC0).
 */
int sendFile(int nFd, TransferType eType, const char *psPath, const char *psFileName);
int receiveFile(int nFd, TransferType eType, const char *psDestPath, FileInfo *ptInfo);
int receiveBody(int nFd, TransferType eType, const char *psDestPath, const FileInfo *ptInfo);
int rejectTransfer(int nFd, const char *psReason);

#endif