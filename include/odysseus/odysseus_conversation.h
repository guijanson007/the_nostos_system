#ifndef _ODYSSEUS_CONVERSETION_H
#define _ODYSSEUS_CONVERSETION_H

#include "odysseus/odysseus.h"

#define ERROR_ITHACA_RESPONSE "ERROR: Unexpected response from Ithaca\n"
#define ERROR_ISLAND_RESPONSE "ERROR: Unexpected response from the island\n"

#define ARRIVAL_ERROR -1
#define ARRIVAL_WAIT 1
#define ARRIVAL_DOCKED 2
#define ARRIVAL_LOST_AT_SEA 3

OdysseusStatus connectToIthaca(Odysseus *tOdysseus);
int disconnectFromIthaca(Odysseus *tOdysseus);
int arriveAtIsland(Odysseus *tOdysseus);
OdysseusStatus notifyConnection(Odysseus *tOdysseus);
int waitingAtPort(Odysseus *ptOdysseus);
OdysseusStatus getListMarket(Odysseus *tOdysseus);

#endif
