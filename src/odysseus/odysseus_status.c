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

/*************************************************
 *
 * @Name : odysseusSetErrnoError
 * @Def : Stores "<prefix><strerror(errno)>" as the detail message of the
 * Odysseus instance, to be read later by odysseusStatusMessage. Replaces (and
 * frees) any previous message. The message is freed by the next call or by
 * freeOdysseus.
 * @Arg : In/Out: ptOdysseus = Odysseus instance that keeps the message
 *        In: psPrefix = text that goes before the error description
 *        In: nErrno = errno value to describe
 * @Ret : None. If the allocation fails the message is left NULL.
 *
 **************************************************/
void odysseusSetErrnoError(Odysseus *ptOdysseus, const char *psPrefix,
                           int nErrno) {
    free(ptOdysseus->lastError);
    ptOdysseus->lastError = NULL;

    if (-1 ==
        asprintf(&ptOdysseus->lastError, "%s%s", psPrefix, strerror(nErrno))) {
        ptOdysseus->lastError = NULL;
    }
}

/*************************************************
 *
 * @Name : odysseusSetRemoteError
 * @Def : Stores the error text sent by the remote side as the detail message
 * of the Odysseus instance. The text is copied with its length, so it does not
 * need to be NUL terminated. Replaces (and frees) any previous message.
 * @Arg : In/Out: ptOdysseus = Odysseus instance that keeps the message
 *        In: psData = text received in the message
 *        In: tDataLength = number of bytes of psData to copy
 * @Ret : None. If the allocation fails the message is left NULL.
 *
 **************************************************/
void odysseusSetRemoteError(Odysseus *ptOdysseus, const char *psData,
                            size_t tDataLength) {
    free(ptOdysseus->lastError);
    ptOdysseus->lastError = NULL;

    if (-1 ==
        asprintf(&ptOdysseus->lastError, "%.*s", (int)tDataLength, psData)) {
        ptOdysseus->lastError = NULL;
    }
}

/*************************************************
 *
 * @Name : odysseusStatusMessage
 * @Def : Returns the text to print for a status code. Codes that carry a
 * detail (socket, connect, remote) return the message stored in the Odysseus
 * instance.
 * @Arg : In: ptOdysseus = Odysseus instance holding the detail message
 *        In: eStatus = status code to describe
 * @Ret : Message for the status code, never NULL.
 *
 **************************************************/
const char *odysseusStatusMessage(const Odysseus *ptOdysseus,
                                  OdysseusStatus eStatus) {
    switch (eStatus) {
        case ODY_OK:
            return "";
        case ODY_ERR_SOCKET:
        case ODY_ERR_CONNECT:
        case ODY_ERR_REMOTE:
            // The detail may be missing if its allocation failed
            return NULL != ptOdysseus->lastError ? ptOdysseus->lastError
                                                 : "ERROR: Unknown error";
        case ODY_ERR_IP:
            return "ERROR: Could not convert IP address";
        case ODY_ERR_IO:
            return "ERROR: Could not communicate with the server";
        case ODY_ERR_PROTOCOL:
            return "ERROR: Unexpected response from Ithaca";
        case ODY_ERR_ISLAND_PROTOCOL:
            return "ERROR: Unexpected response from the island";
        case ODY_ERR_NOT_AT_ISLAND:
            return "ERROR: You are not docked at an island";
        case ODY_ERR_NOT_ENOUGH_GOLD:
            return "ERROR: You don't have enough gold";
        case ODY_ERR_MAP_AMOUNT:
            return "ERROR: Only one map can be bought at a time";
        case ODY_ERR_ALREADY_CONNECTED:
            return "ERROR: Already connected to Ithaca";
        case ODY_ERR_INVALID_COMMAND:
            return "ERROR: Invalid request from your current position";
        case ODY_ERR_NOT_CONNECTED:
            return "ERROR: You are not connected to Ithaca";
        case ODY_ERR_ACTIVE_VOYAGE:
            return "ERROR: You already have an active voyage";
        case ODY_ERR_NOT_LISTED:
            return "ERROR: That voyage is not in the last listing, use LIST "
                   "VOYAGES";
        case ODY_ERR_NOT_ENOUGH_CARGO:
            return "ERROR: Not enough cargo in the odysseus";
        case ODY_ERR_NOT_IN_CARGO:
            return "ERROR: The odysseus dosn't have the product"
        default:
            return "ERROR: Unknown error";
    }
}
