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
#include "odysseus/odysseus_map.h"
#include "odysseus/odysseus_parser.h"
#include "utils/utils_communication.h"

OdysseusStatus odysseusConnect(Odysseus *tOdysseus) {
    OdysseusStatus eStatus = ODY_OK;
    char *psBuffer;

    if (-1 != tOdysseus->ithaca_socket) {
        return ODY_ERR_ALREADY_CONNECTED;
    }

    if (-1 != tOdysseus->whereAmI) {
        if (-1 != asprintf(&psBuffer, "Leaving %s...", tOdysseus->map.connections[tOdysseus->whereAmI].name)) {
            println(psBuffer);
            free(psBuffer);
        }
        departFromIsland(tOdysseus);
        tOdysseus->whereAmI = -1;
        println("Returning directly to Ithaca...");
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

void odysseusMap(Odysseus *tOdysseus) { displayMap(tOdysseus); }

void odysseusStatus(Odysseus *ptOdysseus) {
    char *psBuffer = NULL;
    const char *psLocation = NULL;
    int i = 0;

    if (-1 != asprintf(&psBuffer, "--- %s ---\nGold: %d\n\nCargo:",
                       ptOdysseus->name, ptOdysseus->cargo.nGold)) {
        println(psBuffer);
        free(psBuffer);
    }

    for (i = 0; i < ptOdysseus->cargo.numSupplies; i++) {
        if (NULL != ptOdysseus->cargo.supplies[i].name &&
            -1 != asprintf(&psBuffer, "%s: %d kg",
                           ptOdysseus->cargo.supplies[i].name,
                           ptOdysseus->cargo.supplies[i].amount)) {
            println(psBuffer);
            free(psBuffer);
        }
    }

    if (-1 != asprintf(&psBuffer, "\nTotal provisions: %d kg\n",
                       ptOdysseus->cargo.totalAmount)) {
        println(psBuffer);
        free(psBuffer);
    }

    if (0 == ptOdysseus->activeVoyage.id) {
        println("Current voyage: None");
    } else if (-1 != asprintf(&psBuffer, "Current voyage: %s -> %s\nReward: %d",ptOdysseus->activeVoyage.object, ptOdysseus->activeVoyage.destination,   ptOdysseus->activeVoyage.reward)) {
        println(psBuffer);
        free(psBuffer);
    }

    psLocation = (-1 == ptOdysseus->whereAmI)  ? "Ithaca" : ptOdysseus->map.connections[ptOdysseus->whereAmI].name;
    if (-1 != asprintf(&psBuffer, "Current location: %s", psLocation)) {
        println(psBuffer);
        free(psBuffer);
    }
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

    if (-1 == tOdysseus->island_socket || -1 == tOdysseus->whereAmI) {
        return ODY_ERR_NOT_AT_ISLAND;
    }

    eStatus = getListMarket(tOdysseus);
    if (ODY_OK != eStatus && ODY_ERR_REMOTE != eStatus) {
        // The island conversation is out of sync, drop it
        close(tOdysseus->island_socket);
        tOdysseus->island_socket = -1;
    }

    return eStatus;
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
    KnownIsland *ptDestination = NULL;
    char *psBuffer = NULL;
    int nDestinationIndex;
    int nArrival = ARRIVAL_NONE;

    nDestinationIndex = findDestinationFromIsland(tOdysseus, psDestination);

    if (nDestinationIndex == -1) {
        if (-1 != asprintf(&psBuffer, "We can't sail to %s from here...", psDestination)) {
            println(psBuffer);
            free(psBuffer);
        }
        return ODY_OK;
    }
    ptDestination = &tOdysseus->map.connections[nDestinationIndex];

    if (-1 == tOdysseus->whereAmI) {
        println("Leaving Ithaca...");
        disconnectFromIthaca(tOdysseus);
    } else{
        if (-1 != asprintf(&psBuffer, "Leaving %s...", tOdysseus->map.connections[tOdysseus->whereAmI].name)) {
            println(psBuffer);
            free(psBuffer);
        }
        departFromIsland(tOdysseus);
    }

    if (-1 != asprintf(&psBuffer, "Sailing to %s...", ptDestination->name)) {
        println(psBuffer);
        free(psBuffer);
    }

    tOdysseus->island_socket = openClientConn(ptDestination->ip, ptDestination->port);
    if (tOdysseus->island_socket < 0) {
        tOdysseus->island_socket = -1;
        return ODY_OK;
    }
    if (-1 != asprintf(&psBuffer, "\nConnected to %s.", ptDestination->name)) {
        println(psBuffer);
        free(psBuffer);
    }

    eStatus = arriveAtIsland(tOdysseus, &nArrival);
    if (ODY_OK == eStatus && ARRIVAL_WAIT == nArrival) {
        println("The harbor is full. We drop anchor and wait.");
        eStatus = waitingAtPort(tOdysseus, &nArrival);
    }
    if (ODY_OK != eStatus || ARRIVAL_NONE == nArrival) {
        close(tOdysseus->island_socket);
        tOdysseus->island_socket = -1;
        return eStatus;
    }

    if (ARRIVAL_LOST_AT_SEA == nArrival) {
        if (-1 != asprintf(&psBuffer,"\nThe crew has run out of provisions.\nOdysseus %s has been lost at sea.", tOdysseus->name)) {
            println(psBuffer);
            free(psBuffer);
        }
        close(tOdysseus->island_socket);
        tOdysseus->island_socket = -1;
        return ODY_LOST_AT_SEA;
    }

    tOdysseus->whereAmI = nDestinationIndex;
    if (-1 != asprintf(&psBuffer, "%s has granted access to the port.", ptDestination->name)) {
        println(psBuffer);
        free(psBuffer);
    }

    if (0 != tOdysseus->activeVoyage.id && 0 == strcasecmp(tOdysseus->activeVoyage.destination, ptDestination->name)) {
        if (-1 != asprintf(&psBuffer, "%s is the destination of your voyage (%s).", ptDestination->name, tOdysseus->activeVoyage.object)) {
            println(psBuffer);
            free(psBuffer);
        }
    }

    return ODY_OK;
}

OdysseusStatus odysseusBuy(Odysseus *tOdysseus, const char *psProduct,
                           const char *psAmount) {
    OdysseusStatus eStatus = ODY_OK;

    if (-1 == tOdysseus->island_socket || -1 == tOdysseus->whereAmI) {
        return ODY_ERR_NOT_AT_ISLAND;
    }

    if (0 == strcasecmp(psProduct, MAP_PRODUCT)) {
        if (MAP_AMOUNT != atoi(psAmount)) {
            return ODY_ERR_MAP_AMOUNT;
        }
        if (tOdysseus->cargo.nGold < MAP_PRICE) {
            return ODY_ERR_NOT_ENOUGH_GOLD;
        }
        eStatus = buyMap(tOdysseus);
    } else {
        eStatus = buyProducts(tOdysseus, psProduct, psAmount);
    }

    if (ODY_OK != eStatus && ODY_ERR_REMOTE != eStatus &&
        ODY_ERR_NOT_ENOUGH_GOLD != eStatus) {
        // The island conversation is out of sync, drop it
        close(tOdysseus->island_socket);
        tOdysseus->island_socket = -1;
    }

    return eStatus;
}

OdysseusStatus odysseusSell(Odysseus *tOdysseus, const char *psProduct,
                            const char *psAmount) {
    int nAmount = atoi(psAmount);

    if (-1 == tOdysseus->island_socket || -1 == tOdysseus->whereAmI) {
        return ODY_ERR_NOT_AT_ISLAND;
    }

    for (int i = 0; i < tOdysseus->cargo.numSupplies; i++) {
        if (0 == strcasecmp(tOdysseus->cargo.supplies[i].name, psProduct)) {
            if (nAmount <= tOdysseus->cargo.supplies[i].amount) {
                return sellProduct(tOdysseus, psProduct, nAmount);
            }
            return ODY_ERR_NOT_ENOUGH_CARGO;
        }
    }

    return ODY_ERR_NOT_IN_CARGO;
}
