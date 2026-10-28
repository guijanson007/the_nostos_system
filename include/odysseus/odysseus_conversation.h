#ifndef _ODYSSEUS_CONVERSETION_H
#define _ODYSSEUS_CONVERSETION_H

#include "odysseus/odysseus.h"

#define ARRIVAL_NONE 0
#define ARRIVAL_WAIT 1
#define ARRIVAL_DOCKED 2
#define ARRIVAL_LOST_AT_SEA 3

OdysseusStatus connectToIthaca(Odysseus *tOdysseus);
OdysseusStatus requestListOfVoyages(Odysseus *tOdysseus);
OdysseusStatus requestAcceptVoyage(Odysseus *tOdysseus, int nVoyageId);
OdysseusStatus disconnectFromIthaca(Odysseus *tOdysseus);
OdysseusStatus arriveAtIsland(Odysseus *tOdysseus, int *pnArrival);
OdysseusStatus notifyConnection(Odysseus *tOdysseus);
OdysseusStatus getListMarket(Odysseus *tOdysseus);
OdysseusStatus buyProducts(Odysseus *tOdysseus, const char *psProduct,
                           const char *psAmount);
OdysseusStatus waitingAtPort(Odysseus *ptOdysseus, int *pnArrival);

#endif
