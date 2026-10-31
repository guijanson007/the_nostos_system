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

// Constant declarations

#define ERROR_CREATE_SOCKET "ERROR: Could not create socket: "
#define ERROR_CONNECT_SERVER "ERROR: Could not connect to the server: "
 
#define ERRNO_ERROR_FORMAT "%s%s"
#define REMOTE_ERROR_FORMAT "%.*s"
#define STATUS_OK_MESSAGE ""
#define STATUS_ERROR_UNKNOWN "ERROR: Unknown error"
#define STATUS_ERROR_IP "ERROR: Could not convert IP address"
#define STATUS_ERROR_IO "ERROR: Could not communicate with the server"
#define STATUS_ERROR_PROTOCOL "ERROR: Unexpected response from Ithaca"
#define STATUS_ERROR_ISLAND_PROTOCOL "ERROR: Unexpected response from the island"
#define STATUS_ERROR_NOT_AT_ISLAND "ERROR: You are not docked at an island"
#define STATUS_ERROR_NOT_ENOUGH_GOLD "ERROR: You don't have enough gold"
#define STATUS_ERROR_MAP_AMOUNT "ERROR: Only one map can be bought at a time"
#define STATUS_ERROR_ALREADY_CONNECTED "ERROR: Already connected to Ithaca"
#define STATUS_ERROR_INVALID_COMMAND "ERROR: Invalid request from your current position"
#define STATUS_ERROR_NOT_CONNECTED "ERROR: You are not connected to Ithaca"
#define STATUS_ERROR_ACTIVE_VOYAGE "ERROR: You already have an active voyage"
#define STATUS_ERROR_NOT_LISTED "ERROR: That voyage is not in the last listing, use LIST VOYAGES"
#define STATUS_ERROR_NOT_ENOUGH_CARGO "ERROR: Not enough cargo in the odysseus"
#define STATUS_ERROR_NOT_IN_CARGO "ERROR: The odysseus dosn't have the product"


// Enumaration
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
    ODY_ERR_NOT_AT_ISLAND,
    ODY_LOST_AT_SEA,
    ODY_ERR_NOT_ENOUGH_GOLD,
    ODY_ERR_MAP_AMOUNT,
    ODY_ERR_NOT_IN_CARGO,
    ODY_ERR_NOT_ENOUGH_CARGO
} OdysseusStatus;

struct Odysseus;


// Declared in odysseus.h, which includes this file before defining it
struct Odysseus;
 
// Prototypes
void odysseusSetErrnoError(struct Odysseus *ptOdysseus, const char *psPrefix, int nErrno);
void odysseusSetRemoteError(struct Odysseus *ptOdysseus, const char *psData, size_t tDataLength);
const char *odysseusStatusMessage(const struct Odysseus *ptOdysseus, OdysseusStatus eStatus);
 

#endif // !#ifndef __ODYSSEUS_STATUS_H__
