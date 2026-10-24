#ifndef _ITHACA_CONVERSETION_H_
#define _ITHACA_CONVERSETION_H_

#include "ithaca.h"

#define ERROR_MESSAGE_FORMAT                                                   \
    "ERROR: The message that you send has the wrong format"
#define ERROR_DISCONECTING "ERROR: you couldn't be diconected"
#define ERROR_CLIENT_ALREADY_CONNECTED "ERROR: The client is already connected"
#define ERROR_MEMORY_ALLOCATION "ERROR: Memory allocation failed"
#define OK_MESSAGE "OK"

void choseOption(Ithaca *ithaca, Message msg, int fd_client);
int removeClient(Ithaca *ptIthaca, int fd_client);

#endif
