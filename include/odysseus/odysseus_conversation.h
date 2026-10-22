#ifndef _ODYSSEUS_CONVERSETION_H
#define _ODYSSEUS_CONVERSETION_H

#include "odysseus/odysseus.h"
#include "utils/utils_communication.h"

#define ERROR_CREATE_SOCKET "ERROR: Could not create socket\n"
#define ERROR_CONNECT_SOCKET "ERROR: Could not connect to the server\n"
#define ERROR_IP_CONVERSION "ERROR: Could not convert IP address\n"
#define ERROR_ITHACA_RESPONSE "ERROR: Unexpected response from Ithaca\n"
#define ERROR_ISLAND_RESPONSE "ERROR: Unexpected response from the island\n"

#define ARRIVAL_WAIT 1
#define ARRIVAL_DOCKED 2

int connectToIthaca(Odysseus *tOdysseus);
int disconnectFromIthaca(Odysseus *tOdysseus);
int arriveAtIsland(Odysseus *tOdysseus);
int notifyConnection(Odysseus *tOdysseus);

#endif
