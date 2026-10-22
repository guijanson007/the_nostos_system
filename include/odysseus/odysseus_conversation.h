#ifndef _ODYSSEUS_CONVERSETION_H
#define _ODYSSEUS_CONVERSETION_H

#include "odysseus/odysseus.h"
#include "utils/utils_communication.h"

#define ERROR_ITHACA_RESPONSE "ERROR: Unexpected response from Ithaca\n"
#define ERROR_ISLAND_RESPONSE "ERROR: Unexpected response from the island\n"

#define ARRIVAL_WAIT 1
#define ARRIVAL_DOCKED 2

OdysseusStatus connectToIthaca(Odysseus *tOdysseus);
int disconnectFromIthaca(Odysseus *tOdysseus);
int arriveAtIsland(Odysseus *tOdysseus);
OdysseusStatus notifyConnection(Odysseus *tOdysseus);

#endif
