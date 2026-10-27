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
#include <stdarg.h>

/*************************************************
 *
 * @Name : odysseusSetError
 * @Def : Stores a formatted detail message in the Odysseus instance, to be
 * read later by odysseusStatusMessage. Replaces (and frees) any previous
 * message. The message is freed by the next call or by freeOdysseus.
 * @Arg : In/Out: ptOdysseus = Odysseus instance that keeps the message
 *        In: psFormat = printf-style format string
 *        In: ... = format arguments
 * @Ret : None. If the allocation fails the message is left NULL.
 *
 **************************************************/
void odysseusSetError(Odysseus *ptOdysseus, const char *psFormat, ...) {
    va_list tArgs;

    free(ptOdysseus->lastError);
    ptOdysseus->lastError = NULL;

    va_start(tArgs, psFormat);
    if (-1 == vasprintf(&ptOdysseus->lastError, psFormat, tArgs)) {
        ptOdysseus->lastError = NULL;
    }
    va_end(tArgs);
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
        case ODY_ERR_ALREADY_CONNECTED:
            return "ERROR: Already connected to Ithaca";
        case ODY_ERR_NOT_CONNECTED:
            return "ERROR: You are not connected to Ithaca";
        case ODY_ERR_ACTIVE_VOYAGE:
            return "ERROR: You already have an active voyage";
        case ODY_ERR_NOT_LISTED:
            return "ERROR: That voyage is not in the last listing, use LIST "
                   "VOYAGES";
        default:
            return "ERROR: Unknown error";
    }
}
