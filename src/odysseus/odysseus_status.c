/***********************************************
 *
 * @File : odysseus_status.c
 * @Purpose : maps odysseus status codes to the text shown in the menu.
 * @Author : Guilherme Rodrigues
 * @Date : 22/10/2026
 *
 ************************************************/

#define _GNU_SOURCE

// header
#include "odysseus/odysseus_status.h"

// own headers
#include "odysseus/odysseus.h"

// standard library headers
#include <stdlib.h>
#include <string.h>

/***********************************************
 *
 * @Name : odysseusSetErrnoError
 * @Def : Stores "<prefix><strerror(errno)>" as the detail message of the
 *        Odysseus instance, to be read later by odysseusStatusMessage.
 *        Replaces (and frees) any previous message. The message is freed
 *        by the next call or by freeOdysseus.
 * @Arg : In/Out: ptOdysseus = Odysseus instance that keeps the message
 *        In: psPrefix = text that goes before the error description
 *        In: nErrno = errno value to describe
 * @Ret : None. If the allocation fails the message is left NULL.
 *
 ************************************************/
void odysseusSetErrnoError(Odysseus *ptOdysseus, const char *psPrefix,
                           int nErrno) {
    free(ptOdysseus->psLastError);
    ptOdysseus->psLastError = NULL;
 
    if (-1 == asprintf(&ptOdysseus->psLastError, ERRNO_ERROR_FORMAT, psPrefix,
                       strerror(nErrno))) {
        ptOdysseus->psLastError = NULL;
    }
}

/***********************************************
 *
 * @Name : odysseusSetRemoteError
 * @Def : Stores the error text sent by the remote side as the detail
 *        message of the Odysseus instance. The text is copied with its
 *        length, so it does not need to be NUL terminated. Replaces (and
 *        frees) any previous message.
 * @Arg : In/Out: ptOdysseus = Odysseus instance that keeps the message
 *        In: psData = text received in the message
 *        In: tDataLength = number of bytes of psData to copy
 * @Ret : None. If the allocation fails the message is left NULL.
 *
 ************************************************/
void odysseusSetRemoteError(Odysseus *ptOdysseus, const char *psData,
                            size_t tDataLength) {
    free(ptOdysseus->psLastError);
    ptOdysseus->psLastError = NULL;
 
    if (-1 == asprintf(&ptOdysseus->psLastError, REMOTE_ERROR_FORMAT,
                       (int)tDataLength, psData)) {
        ptOdysseus->psLastError = NULL;
    }
}

/***********************************************
 *
 * @Name : odysseusStatusMessage
 * @Def : Returns the text to print for a status code. Codes that carry a
 *        detail (socket, connect, remote) return the message stored in the
 *        Odysseus instance.
 * @Arg : In: ptOdysseus = Odysseus instance holding the detail message
 *        In: eStatus = status code to describe
 * @Ret : Message for the status code, never NULL.
 *
 ************************************************/
const char *odysseusStatusMessage(const Odysseus *ptOdysseus,
                                  OdysseusStatus eStatus) {
    switch (eStatus) {
        case ODY_OK:
            return STATUS_OK_MESSAGE;
        case ODY_ERR_SOCKET:
        case ODY_ERR_CONNECT:
        case ODY_ERR_REMOTE:
            // The detail may be missing if its allocation failed
            if (NULL != ptOdysseus->psLastError) {
                return ptOdysseus->psLastError;
            }
            return STATUS_ERROR_UNKNOWN;
        case ODY_ERR_IP:
            return STATUS_ERROR_IP;
        case ODY_ERR_IO:
            return STATUS_ERROR_IO;
        case ODY_ERR_PROTOCOL:
            return STATUS_ERROR_PROTOCOL;
        case ODY_ERR_ISLAND_PROTOCOL:
            return STATUS_ERROR_ISLAND_PROTOCOL;
        case ODY_ERR_NOT_AT_ISLAND:
            return STATUS_ERROR_NOT_AT_ISLAND;
        case ODY_ERR_NOT_ENOUGH_GOLD:
            return STATUS_ERROR_NOT_ENOUGH_GOLD;
        case ODY_ERR_MAP_AMOUNT:
            return STATUS_ERROR_MAP_AMOUNT;
        case ODY_ERR_ALREADY_CONNECTED:
            return STATUS_ERROR_ALREADY_CONNECTED;
        case ODY_ERR_INVALID_COMMAND:
            return STATUS_ERROR_INVALID_COMMAND;
        case ODY_ERR_NOT_CONNECTED:
            return STATUS_ERROR_NOT_CONNECTED;
        case ODY_ERR_ACTIVE_VOYAGE:
            return STATUS_ERROR_ACTIVE_VOYAGE;
        case ODY_ERR_NOT_LISTED:
            return STATUS_ERROR_NOT_LISTED;
        case ODY_ERR_NOT_ENOUGH_CARGO:
            return STATUS_ERROR_NOT_ENOUGH_CARGO;
        case ODY_ERR_NOT_IN_CARGO:
            return STATUS_ERROR_NOT_IN_CARGO;
        default:
            return STATUS_ERROR_UNKNOWN;
    }
}