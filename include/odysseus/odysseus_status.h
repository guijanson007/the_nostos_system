#ifndef __ODYSSEUS_STATUS_H__
#define __ODYSSEUS_STATUS_H__

/***********************************************
 *
 * @File : odysseus_status.h
 * @Purpose : status codes returned by the odysseus layers (command, handler,
 * conversation) and the text the menu prints for each of them. Lower layers
 * never print errors, they return one of these codes.
 * @Author : Guilherme Rodrigues
 * @Date : 22/10/2026
 *
 ************************************************/

// System includes
#include <stddef.h>

// Enumerations
typedef enum {
    ODY_OK = 0,
    ODY_ERR_SOCKET,
    ODY_ERR_CONNECT,
    ODY_ERR_IP,
    ODY_ERR_IO,
    ODY_ERR_PROTOCOL,
    ODY_ERR_REMOTE,
    ODY_ERR_ALREADY_CONNECTED,
    ODY_ERR_INVALID_COMMAND,
    ODY_ERR_NOT_CONNECTED,
    ODY_ERR_ACTIVE_VOYAGE,
    ODY_ERR_NOT_LISTED,
    ODY_ERR_ISLAND_PROTOCOL,
    ODY_ERR_NOT_AT_ISLAND
} OdysseusStatus;

struct Odysseus;

// Prefixes of the messages that carry a detail
#define ERROR_CREATE_SOCKET "ERROR: Could not create socket: "
#define ERROR_CONNECT_SERVER "ERROR: Could not connect to the server: "

// Functions
void odysseusSetErrnoError(struct Odysseus *ptOdysseus, const char *psPrefix,
                           int nErrno);
void odysseusSetRemoteError(struct Odysseus *ptOdysseus, const char *psData,
                            size_t tDataLength);
const char *odysseusStatusMessage(const struct Odysseus *ptOdysseus,
                                  OdysseusStatus eStatus);

#endif // !#ifndef __ODYSSEUS_STATUS_H__
