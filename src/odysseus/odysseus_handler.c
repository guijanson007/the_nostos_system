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

int findDestination(Odysseus tOdysseus, const char *psDestination) {
    if (tOdysseus.whereAmI == -1) {
        return 0; //position of Aeaea in the array
    }
    else {
        for (int i = 0; i < tOdysseus.map.numConnections; i++) {
            if (strcmp(tOdysseus.map.connections[i].sphragis.name, psDestination) == 0) {
                for (int j = 0; j < tOdysseus.map.connections[tOdysseus.whereAmI].sphragis.known_island_count; j++) {
                    if (0 == strcmp(tOdysseus.map.connections[tOdysseus.whereAmI].sphragis.known_islands[j], psDestination)) {
                        return i;
                    }
                }
                return -1;
            }
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
        if (-1 == asprintf(&buff, "Leaving %s...", tOdysseus->map.connections[tOdysseus->whereAmI].sphragis.name)) return;
        close(tOdysseus->island_socket);
    }
    println(buff);
    free(buff);

    if (-1 == asprintf(&buff, "Sailing to %s...", tOdysseus->map.connections[nDestinationIndex].sphragis.name)) return;
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
            if (-1 == asprintf(&buff, ">>> %s has granted access to the port.", tOdysseus->map.connections[tOdysseus->whereAmI].sphragis.name)) return;
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
