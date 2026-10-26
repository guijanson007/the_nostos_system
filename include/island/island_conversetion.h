#ifndef _ISLAND_CONVERSETION_H_
#define _ISLAND_CONVERSETION_H_

#include "island.h"

#define ERROR_MESSAGE_FORMAT                                                   \
    "ERROR: The message that you send has the wrong format"
#define ERROR_DISCONECTING "ERROR: you couldn't be diconected"
#define ERROR_MEMORY "ERROR: memory allocation has faild"
#define ERROR_ODYSSEUS_ALREDY_EXIST "ERROR: The odysseus is alredy conected to the Island"
#define OK_MESSAGE "OK"
#define WAIT "WAIT"
#define DOCKED "DOCKED"

void choseOption(Island *island, Message msg, int fd_client);
void clientLeaves(Island *ptIsland, int nfd);

#endif