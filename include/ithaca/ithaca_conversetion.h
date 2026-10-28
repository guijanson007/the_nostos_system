#ifndef _ITHACA_CONVERSETION_H_
#define _ITHACA_CONVERSETION_H_

#include "ithaca.h"

#define ERROR_MESSAGE_FORMAT                                                   \
    "ERROR: The message that you send has the wrong format"
#define ERROR_DISCONECTING "ERROR: you couldn't be diconected"
#define ERROR_CLIENT_ALREADY_CONNECTED "ERROR: The client is already connected"
#define ERROR_MEMORY_ALLOCATION "ERROR: Memory allocation failed"
#define ERROR_NOT_CONNECTED "ERROR: You must connect to Ithaca first"
#define ERROR_NO_VOYAGES "ERROR: No voyages available"
#define ERROR_INVALID_VOYAGE_ID "ERROR: Invalid voyage id"
#define ERROR_VOYAGE_NOT_AVAILABLE "ERROR: The voyage is not available"
#define ERROR_ACTIVE_VOYAGE "ERROR: You already have an active voyage"
#define OK_MESSAGE "OK"

void consoleLog(const char *sOdysseusName, const char *sPostFix);
void choseOption(Ithaca *ptIthaca, Message msg, int fd_client);
int removeClient(Ithaca *ptIthaca, int fd_client);

#endif
