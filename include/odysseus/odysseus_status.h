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
    ODY_ERR_NOT_CONNECTED,
    ODY_ERR_ACTIVE_VOYAGE,
    ODY_ERR_NOT_LISTED
} OdysseusStatus;

struct Odysseus;

// Functions
void odysseusSetError(struct Odysseus *ptOdysseus, const char *psFormat, ...);
const char *odysseusStatusMessage(const struct Odysseus *ptOdysseus,
                                  OdysseusStatus eStatus);

#endif // !#ifndef __ODYSSEUS_STATUS_H__
