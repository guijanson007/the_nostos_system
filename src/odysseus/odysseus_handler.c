/***********************************************
 *
 * @File : odysseus_handler.c
 * @Purpose : handlers for the odysseus commands. Not yet implemented, will
 *            be used starting from phase 2 of the project.
 * @Author : Guilherme Rodrigues
 * @Date : 04/10/2026
 *
 ************************************************/
#define _GNU_SOURCE

// header
#include "odysseus/odysseus_handler.h"

int findDestination(Odysseus ptOdysseus, const char *psDestination) {
    const KnownIsland *ptHere = NULL;
    int nNeighbor = 0;
    int i = 0;

    if (-1 == ptOdysseus.whereAmI) {
        if (0 == strcasecmp(ptOdysseus.map.connections[0].name, psDestination)) {
            return 0;
        }
        return -1;
    }

    ptHere = &ptOdysseus.map.connections[ptOdysseus.whereAmI];
    for (i = 0; i < ptHere->num_neighbors; i++) {
        nNeighbor = ptHere->neighbors[i];
        if (0 == strcasecmp(ptOdysseus.map.connections[nNeighbor].name, psDestination)) {
            return nNeighbor;
        }
    }
    return -1;
}

void odysseusConnect(Odysseus *tOdysseus, const char *psDestination) {
    (void)tOdysseus;
    (void)psDestination;
}

void odysseusMap(Odysseus *tOdysseus) { (void)tOdysseus; }

void odysseusStatus(Odysseus *tOdysseus) { (void)tOdysseus; }

void odysseusDeliver(Odysseus *tOdysseus) { (void)tOdysseus; }

void odysseusClaim(Odysseus *tOdysseus) { (void)tOdysseus; }

void odysseusListVoyages(Odysseus *tOdysseus) { (void)tOdysseus; }

void odysseusListMarket(Odysseus *tOdysseus) { (void)tOdysseus; }

void odysseusAcceptVoyage(Odysseus *tOdysseus, const char *psVoyageId) {
    (void)tOdysseus;
    (void)psVoyageId;
}

void odysseusSail(Odysseus *tOdysseus, const char *psDestination) {
    char *buff = NULL;
    int nDestinationIndex = findDestination(*tOdysseus, psDestination);
    if (nDestinationIndex == -1) {
        asprintf(&buff, "We can't sail to %s from here", psDestination);
        println(buff);
        free(buff);
        return;
    }

    if (tOdysseus->whereAmI == -1 && nDestinationIndex == 0) {
        if (-1 == disconnectFromIthaca(tOdysseus)) {
            return;
        }  
         if (-1 == asprintf(&buff, "Leaving Ithaca...")) return;
    }
    else {
        if (-1 == asprintf(&buff, "Leaving %s...", tOdysseus->map.connections[tOdysseus->whereAmI].name)) return;
        close(tOdysseus->island_socket);
    }
    println(buff);
    free(buff);

    if (-1 == asprintf(&buff, "Sailing to %s...", tOdysseus->map.connections[nDestinationIndex].name)) return;
    println(buff);
    free(buff);  

    tOdysseus->island_socket = openClientConn(tOdysseus->map.connections[nDestinationIndex].ip,
                                             tOdysseus->map.connections[nDestinationIndex].port);
    if (tOdysseus->island_socket < 0) {
        return;
    }
    tOdysseus->whereAmI = nDestinationIndex;
    
    int status = arriveAtIsland(tOdysseus);
    switch (status) {
        case ARRIVAL_WAIT:
            // Waiting to be docked
            break;
        case ARRIVAL_DOCKED:
            if (-1 == asprintf(&buff, ">>> %s has granted access to the port.", tOdysseus->map.connections[tOdysseus->whereAmI].name)) return;
            println(buff);
            free(buff);
            break;
        default:
            return;
            close(tOdysseus->island_socket);
            tOdysseus->island_socket = -1;
            break;
    }
}

void odysseusBuy(Odysseus *tOdysseus, const char *psProduct,
                 const char *psAmount) {
    (void)tOdysseus;
    (void)psProduct;
    (void)psAmount;
}

void odysseusSell(Odysseus *tOdysseus, const char *psProduct,
                  const char *psAmount) {
    (void)tOdysseus;
    (void)psProduct;
    (void)psAmount;
}
