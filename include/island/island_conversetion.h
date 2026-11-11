#ifndef _ISLAND_CONVERSETION_H_
#define _ISLAND_CONVERSETION_H_


/***********************************************
 *
 * @File : island_conversetion.c
 * @Purpose : manage the logic between Island and odysseus
 * @Author : Axel Boleda i Callizo
 * @Date : 31/10/2026
 *
 ************************************************/

//project Includes
#include "island.h"


//Constants
#define MAX_TRADE_DATA 253
#define TRADE_DELIMITER "&"
 
#define ERROR_MESSAGE_FORMAT "ERROR: The message that you send has the wrong format"
#define ERROR_DISCONECTING "ERROR: you couldn't be diconected"
#define ERROR_MEMORY "ERROR: memory allocation has faild"
#define ERROR_ODYSSEUS_ALREDY_EXIST "ERROR: The odysseus is alredy conected to the Island"
#define ERROR_UPDATE_STOCK "ERROR: could not update the stock file"
#define ERROR_NOT_DOCKED "NOT_DOCKED"
#define ERROR_UNKNOWN_PRODUCT "UNKNOWN_PRODUCT"
#define ERROR_NOT_ENOUGH_STOCK "NOT_ENOUGH_STOCK"
 
#define OK_MESSAGE "OK"
#define WAIT "WAIT"
#define DOCKED "DOCKED"
#define VERB_BOUGHT "bought"
#define VERB_SOLD "sold"
 
#define MARKET_LINE_FORMAT "%zu&%zu&%s&%d&%d"
#define TRADE_OK_FORMAT "OK&%s&%d&%d"
#define MAP_EMPTY_FORMAT "%s&%d&0"
#define MAP_ROUTE_FORMAT "%s&%d&%zu&%zu&%s&%s&%d"
 
#define MSG_APPROACHING ">>> %s approaching %s."
#define MSG_ENTERED_PORT ">>> %s entered the port."
#define MSG_PORT_FULL ">>> Port full. %s waiting offshore in the FIFO queue."
#define MSG_DOCKED_SENT ">>> DOCKED sent to %s; he may enter the port."
#define MSG_TRADE_DONE ">>> %s %s %d kg of %s."
#define MSG_BOUGHT_MAP ">>> %s bought the local map."
#define MSG_HAS_LEFT ">>> %s has left."

//Functions
void removeClient(Island *ptIsland, int nFd);
void choseOption(Island *ptIsland, Message tMsg, int nFdClient);
void clientLeaves(Island *ptIsland, int nFd);

#endif