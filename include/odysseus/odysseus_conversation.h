#ifndef _ODYSSEUS_CONVERSETION_H
#define _ODYSSEUS_CONVERSETION_H

/***********************************************
 *
 * @File : odysseus_conversation.h
 * @Purpose : declarations for the conversation between odysseus and ithaca
 *            or an island
 * @Author : Axel Boleda Callizo and Guilherme Rodrigues
 * @Date : 31/10/2026
 *
 ************************************************/

#include "odysseus/odysseus.h"


// Constant declarations
#define ARRIVAL_NONE 0
#define ARRIVAL_WAIT 1
#define ARRIVAL_DOCKED 2
#define ARRIVAL_LOST_AT_SEA 3
#define MAX_LISTED_VOYAGES 1000
 
#define WAIT "WAIT"
#define DOCKED "DOCKED"
#define OK_RESPONSE "OK"
#define ZERO_STRING "0"
#define INT_FORMAT "%d"
 
#define VOYAGE_LIST_FIELDS 6
#define VOYAGE_ACCEPT_FIELDS 5
#define MAP_FIELDS_EMPTY 3
#define MAP_FIELDS_ROUTE 7
#define MAX_MAP_ROUTES 20
#define MAX_PORT 65535
#define MAX_DATA_LENGTH 253
#define RATION_SECONDS 5
#define NO_PRICE -1
#define NO_TOTAL -1
 
#define TRADE_BUY_FORMAT "%s&%s"
#define TRADE_SELL_FORMAT "%s&%d"
#define MSG_RATION "\n>>> The crew consumes another ration.\nProvisions remaining: %d kg"
#define MSG_MARKET_HEADER "--- %s MARKET ---"
#define MSG_MARKET_LINE "%s\t%d kg\t %d gold / kg"
#define MSG_MARKET_MAP "%s\t\t\t%d gold"
#define MAP_LABEL "Map"
#define MSG_BOUGHT "Bought %d kg of %s for %d gold."
#define MSG_SOLD "Sold %d kg of %s for %d gold."
#define MSG_BOUGHT_MAP "Bought the map of %s for %d gold."
#define MSG_NEW_ROUTES "New routes have been charted."
#define MSG_NO_ROUTES "There were no routes to chart."
#define ERROR_NOT_DOCKED_YET "ERROR: You are not docked yet. Only STATUS and MAP are available"

// Functions
OdysseusStatus connectToIthaca(Odysseus *ptOdysseus);
OdysseusStatus requestListOfVoyages(Odysseus *ptOdysseus);
OdysseusStatus requestAcceptVoyage(Odysseus *ptOdysseus, int nVoyageId);
OdysseusStatus disconnectFromIthaca(Odysseus *ptOdysseus);
OdysseusStatus arriveAtIsland(Odysseus *ptOdysseus, int *pnArrival);
OdysseusStatus notifyConnection(Odysseus *ptOdysseus);
OdysseusStatus getListMarket(Odysseus *ptOdysseus);
OdysseusStatus buyProducts(Odysseus *ptOdysseus, const char *psProduct, const char *psAmount);
OdysseusStatus sellProduct(Odysseus *ptOdysseus, const char *psProduct, int nAmount);
OdysseusStatus buyMap(Odysseus *ptOdysseus);
OdysseusStatus departFromIsland(Odysseus *ptOdysseus);
OdysseusStatus waitingAtPort(Odysseus *ptOdysseus, int *pnArrival);
OdysseusStatus leaveIsland(Odysseus *ptOdysseus);
 

#endif
