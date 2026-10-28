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

// own headers
#include "odysseus/odysseus_conversation.h"
#include "odysseus/odysseus_parser.h"

int findDestination(Odysseus ptOdysseus, const char *psDestination) {
    const KnownIsland *ptHere = NULL;
    int nNeighbor = 0;
    int i = 0;

    if (-1 == ptOdysseus.whereAmI) {
        if (0 ==
            strcasecmp(ptOdysseus.map.connections[0].name, psDestination)) {
            return 0;
        }
        return -1;
    }

    ptHere = &ptOdysseus.map.connections[ptOdysseus.whereAmI];
    for (i = 0; i < ptHere->num_neighbors; i++) {
        nNeighbor = ptHere->neighbors[i];
        if (0 == strcasecmp(ptOdysseus.map.connections[nNeighbor].name,
                            psDestination)) {
            return nNeighbor;
        }
    }
    return -1;
}

OdysseusStatus odysseusConnect(Odysseus *tOdysseus) {
    OdysseusStatus eStatus = ODY_OK;

    if (-1 != tOdysseus->ithaca_socket) {
        return ODY_ERR_ALREADY_CONNECTED;
    }

    eStatus = connectToIthaca(tOdysseus);
    if (ODY_OK != eStatus) {
        return eStatus;
    }

    eStatus = notifyConnection(tOdysseus);
    if (ODY_OK != eStatus) {
        // Roll back so a retry starts from a clean state
        close(tOdysseus->ithaca_socket);
        tOdysseus->ithaca_socket = -1;
    }

    if (ODY_OK == eStatus) {
        println("Connected to Ithaca.");
    }

    return eStatus;
}

void odysseusMap(Odysseus *tOdysseus) { (void)tOdysseus; }

void odysseusStatus(Odysseus *ptOdysseus) {
    char *buff;

    asprintf(&buff, "--- %s ---\nGold: %.d\n\nCargo:", ptOdysseus->name,
             ptOdysseus->cargo.nGold);
    println(buff);
    free(buff);

    for (int i = 0; i < ptOdysseus->cargo.numSupplies; i++) {
        asprintf(&buff, "%s: %d kg", ptOdysseus->cargo.supplies[i].name,
                 ptOdysseus->cargo.supplies[i].amount);
        println(buff);
        free(buff);
    }

    asprintf(&buff, "\nTotal provisions: %d kg\n",
             ptOdysseus->cargo.totalAmount);
    println(buff);
    free(buff);

    // TO IMPLEMENT WHEN THE VOYAGE IS DONE (Current voyage: None)

    char *psLocation =
        (-1 == ptOdysseus->whereAmI)
            ? "Ithaca"
            : ptOdysseus->map.connections[ptOdysseus->whereAmI].name;
    asprintf(&buff, "Current location: %s", psLocation);
    println(buff);
    free(buff);
}

void odysseusDeliver(Odysseus *tOdysseus) { (void)tOdysseus; }

void odysseusClaim(Odysseus *tOdysseus) { (void)tOdysseus; }

OdysseusStatus odysseusListVoyages(Odysseus *tOdysseus) {
    OdysseusStatus eStatus = ODY_OK;
    char *psBuffer = NULL;
    int i = 0;

    if (-1 == tOdysseus->ithaca_socket) {
        return ODY_ERR_NOT_CONNECTED;
    }
    if (0 != tOdysseus->activeVoyage.id) {
        return ODY_ERR_ACTIVE_VOYAGE;
    }

    eStatus = requestListOfVoyages(tOdysseus);
    if (ODY_OK != eStatus) {
        return eStatus;
    }

    println("--- AVAILABLE VOYAGES ---");
    for (i = 0; i < tOdysseus->numListedVoyages; i++) {
        const OdysseusVoyage *ptVoyage = &tOdysseus->listedVoyages[i];
        if (-1 != asprintf(&psBuffer, "%d. %-20s -> %-10s [%d]", ptVoyage->id,
                           ptVoyage->object, ptVoyage->destination,
                           ptVoyage->reward)) {
            println(psBuffer);
            free(psBuffer);
        }
    }
    return ODY_OK;
}

OdysseusStatus odysseusListMarket(Odysseus *tOdysseus) {
    OdysseusStatus eStatus = ODY_OK;

    if (tOdysseus->island_socket == -1 || tOdysseus->whereAmI == -1)
        return ODY_ERR_ALREADY_CONNECTED;

    eStatus = getListMarket(tOdysseus);

    if (ODY_OK != eStatus) {
        close(tOdysseus->ithaca_socket);
        tOdysseus->ithaca_socket = -1;
    }

    return ODY_OK;
}

OdysseusStatus odysseusAcceptVoyage(Odysseus *tOdysseus,
                                    const char *psVoyageId) {
    OdysseusStatus eStatus = ODY_OK;
    char *psBuffer = NULL;
    int nVoyageId = atoi(psVoyageId);
    int nListed = 0;
    int i = 0;

    if (-1 == tOdysseus->ithaca_socket) {
        return ODY_ERR_NOT_CONNECTED;
    }
    if (0 != tOdysseus->activeVoyage.id) {
        return ODY_ERR_ACTIVE_VOYAGE;
    }
    for (i = 0; i < tOdysseus->numListedVoyages; i++) {
        if (nVoyageId == tOdysseus->listedVoyages[i].id) {
            nListed = 1;
        }
    }
    if (!nListed) {
        return ODY_ERR_NOT_LISTED;
    }

    eStatus = requestAcceptVoyage(tOdysseus, nVoyageId);
    if (ODY_OK != eStatus) {
        return eStatus;
    }

    if (-1 != asprintf(&psBuffer, "Voyage %d accepted.\nDestination: %s.",
                       tOdysseus->activeVoyage.id,
                       tOdysseus->activeVoyage.destination)) {
        println(psBuffer);
        free(psBuffer);
    }
    return ODY_OK;
}

OdysseusStatus odysseusSail(Odysseus *tOdysseus, const char *psDestination) {
    OdysseusStatus eStatus = ODY_OK;
    char *psBuffer = NULL;
    int nDestinationIndex = findDestination(*tOdysseus, psDestination);
    int nArrival = ARRIVAL_NONE;

    if (nDestinationIndex == -1) {
        if (-1 != asprintf(&psBuffer, "We can't sail to %s from here",
                           psDestination)) {
            println(psBuffer);
            free(psBuffer);
        }
        return ODY_OK;
    }

    if (tOdysseus->whereAmI == -1 && nDestinationIndex == 0) {
        eStatus = disconnectFromIthaca(tOdysseus);
        if (ODY_OK != eStatus) {
            return eStatus;
        }
        println("Leaving Ithaca...");
    } else {
        if (-1 != asprintf(&psBuffer, "Leaving %s...",
                           tOdysseus->map.connections[tOdysseus->whereAmI].name)) {
            println(psBuffer);
            free(psBuffer);
        }
        close(tOdysseus->island_socket);
    }

    if (-1 != asprintf(&psBuffer, "Sailing to %s...",
                       tOdysseus->map.connections[nDestinationIndex].name)) {
        println(psBuffer);
        free(psBuffer);
    }

    tOdysseus->island_socket =
        openClientConn(tOdysseus->map.connections[nDestinationIndex].ip,
                       tOdysseus->map.connections[nDestinationIndex].port);
    if (tOdysseus->island_socket < 0) {
        // openClientConn already reported the failure
        return ODY_OK;
    }
    tOdysseus->whereAmI = nDestinationIndex;

    eStatus = arriveAtIsland(tOdysseus, &nArrival);
    if (ODY_OK == eStatus && ARRIVAL_WAIT == nArrival) {
        println("The harbor is full. We drop anchor and wait.");
        eStatus = waitingAtPort(tOdysseus, &nArrival);
    }
    if (ODY_OK != eStatus) {
        close(tOdysseus->island_socket);
        tOdysseus->island_socket = -1;
        return eStatus;
    }

    switch (nArrival) {
        case ARRIVAL_DOCKED:
            if (-1 != asprintf(
                          &psBuffer, ">>> %s has granted access to the port.",
                          tOdysseus->map.connections[nDestinationIndex].name)) {
                println(psBuffer);
                free(psBuffer);
            }
            break;
        case ARRIVAL_LOST_AT_SEA:
            if (-1 != asprintf(&psBuffer,
                               "The crew has run out of provisions.\nOdysseus "
                               "%s has been lost at sea.",
                               tOdysseus->name)) {
                println(psBuffer);
                free(psBuffer);
            }
            break;
        default:
            break;
    }
    return ODY_OK;
}

OdysseusStatus odysseusBuy(Odysseus *tOdysseus, const char *psProduct, const char *psAmount) {
    OdysseusStatus eStatus;

    if (-1 != tOdysseus->ithaca_socket) {
        return ODY_ERR_ALREADY_CONNECTED;
    }

    eStatus = buyProducts(tOdysseus, psProduct, psAmount);
    if (ODY_OK != eStatus) {
        return eStatus;
    }

    return eStatus;
}

void odysseusSell(Odysseus *tOdysseus, const char *psProduct,
                  const char *psAmount) {
    (void)tOdysseus;
    (void)psProduct;
    (void)psAmount;
}
