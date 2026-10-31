/***********************************************
 *
 * @File : odysseus_handler.h
 * @Purpose : handler for the odysseus process
 * @Author : Guilherme Rodrigues, Axel Boleda
 * @Date : 31/10/2026
 *
 ************************************************/
#define _GNU_SOURCE

// header
#include "odysseus/odysseus_handler.h"

// own headers
#include "odysseus/odysseus_conversation.h"
#include "odysseus/odysseus_handler.h"
#include "odysseus/odysseus_map.h"
#include "odysseus/odysseus_parser.h"
#include "utils/utils_communication.h"

/***********************************************
 *
 * @Name : printWithName
 * @Def : Prints a message that contains one name (%s).
 * @Arg : In: psFormat = message with one %s
 *        In: psName = name to put in the message
 * @Ret : None.
 *
 ************************************************/
static void printWithName(const char *psFormat, const char *psName) {
    char *psBuffer = NULL;

    if (-1 != asprintf(&psBuffer, psFormat, psName)) {
        println(psBuffer);
        free(psBuffer);
    }
}

/***********************************************
 *
 * @Name : odysseusConnect
 * @Def : Connects odysseus to Ithaca. If odysseus is at an island, it
 *        departs from it first.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus odysseusConnect(Odysseus *ptOdysseus) {
    OdysseusStatus eStatus = ODY_OK;

    if (NO_SOCKET != ptOdysseus->nIthacaSocket) {
        return ODY_ERR_ALREADY_CONNECTED;
    }

    if (AT_ITHACA != ptOdysseus->nWhereAmI) {
        printWithName(
            MSG_LEAVING,
            ptOdysseus->tMap.atConnections[ptOdysseus->nWhereAmI].psName);
        departFromIsland(ptOdysseus);
        ptOdysseus->nWhereAmI = AT_ITHACA;
        println(MSG_RETURNING_ITHACA);
    }

    eStatus = connectToIthaca(ptOdysseus);
    if (ODY_OK != eStatus) {
        return eStatus;
    }

    eStatus = notifyConnection(ptOdysseus);
    if (ODY_OK != eStatus) {
        close(ptOdysseus->nIthacaSocket);
        ptOdysseus->nIthacaSocket = NO_SOCKET;
    }

    if (ODY_OK == eStatus) {
        println(MSG_ITHACA_CONNECTED);
    }

    return eStatus;
}

/***********************************************
 *
 * @Name : odysseusMap
 * @Def : Shows the map known by odysseus.
 * @Arg : In: ptOdysseus = Odysseus instance
 * @Ret : None.
 *
 ************************************************/
void odysseusMap(Odysseus *ptOdysseus) { displayMap(ptOdysseus); }

/***********************************************
 *
 * @Name : odysseusStatus
 * @Def : Shows the gold, the cargo, the current voyage and the location of
 *        odysseus.
 * @Arg : In: ptOdysseus = Odysseus instance
 * @Ret : None.
 *
 ************************************************/
void odysseusStatus(Odysseus *ptOdysseus) {
    char *psBuffer = NULL;
    const char *psLocation = NULL;
    int i = 0;

    if (-1 != asprintf(&psBuffer, MSG_STATUS_HEADER, ptOdysseus->psName,
                       ptOdysseus->tCargo.nGold)) {
        println(psBuffer);
        free(psBuffer);
    }

    for (i = 0; i < ptOdysseus->tCargo.nNumSupplies; i++) {
        if (NULL != ptOdysseus->tCargo.atSupplies[i].psName &&
            -1 != asprintf(&psBuffer, MSG_SUPPLY_LINE,
                           ptOdysseus->tCargo.atSupplies[i].psName,
                           ptOdysseus->tCargo.atSupplies[i].nAmount)) {
            println(psBuffer);
            free(psBuffer);
        }
    }

    if (-1 != asprintf(&psBuffer, MSG_TOTAL_PROVISIONS, ptOdysseus->tCargo.nTotalAmount)) {
        println(psBuffer);
        free(psBuffer);
    }

    if (0 == ptOdysseus->tActiveVoyage.nId) {
        println(MSG_NO_VOYAGE);
    } else if (-1 != asprintf(&psBuffer, MSG_CURRENT_VOYAGE,
                              ptOdysseus->tActiveVoyage.psObject,
                              ptOdysseus->tActiveVoyage.psDestination,
                              ptOdysseus->tActiveVoyage.nReward)) {
        println(psBuffer);
        free(psBuffer);
    }

    if (AT_ITHACA == ptOdysseus->nWhereAmI) {
        psLocation = ITHACA_NAME;
    } else {
        psLocation =
            ptOdysseus->tMap.atConnections[ptOdysseus->nWhereAmI].psName;
    }
    printWithName(MSG_LOCATION, psLocation);
}

/***********************************************
 *
 * @Name : odysseusDeliver
 * @Def : Delivers the cargo of the current voyage. (Not yet implemented)
 * @Arg : In: ptOdysseus = Odysseus instance
 * @Ret : None.
 *
 ************************************************/
void odysseusDeliver(Odysseus *ptOdysseus) {
    (void)ptOdysseus;
}
 
/***********************************************
 *
 * @Name : odysseusClaim
 * @Def : Claims the reward of the current voyage. (Not yet implemented)
 * @Arg : In: ptOdysseus = Odysseus instance
 * @Ret : None.
 *
 ************************************************/
void odysseusClaim(Odysseus *ptOdysseus) {
    (void)ptOdysseus;
}


/***********************************************
 *
 * @Name : odysseusListVoyages
 * @Def : Asks Ithaca for the available voyages and prints them.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus odysseusListVoyages(Odysseus *ptOdysseus) {
    OdysseusStatus eStatus = ODY_OK;
    const OdysseusVoyage *ptVoyage = NULL;
    char *psBuffer = NULL;
    int i = 0;
 
    if (NO_SOCKET == ptOdysseus->nIthacaSocket) {
        return ODY_ERR_NOT_CONNECTED;
    }
    if (0 != ptOdysseus->tActiveVoyage.nId) {
        return ODY_ERR_ACTIVE_VOYAGE;
    }
 
    eStatus = requestListOfVoyages(ptOdysseus);
    if (ODY_OK != eStatus) {
        return eStatus;
    }
 
    println(MSG_VOYAGES_HEADER);
    for (i = 0; i < ptOdysseus->nNumListedVoyages; i++) {
        ptVoyage = &ptOdysseus->atListedVoyages[i];
        if (-1 != asprintf(&psBuffer, MSG_VOYAGE_LINE, ptVoyage->nId,
                           ptVoyage->psObject, ptVoyage->psDestination,
                           ptVoyage->nReward)) {
            println(psBuffer);
            free(psBuffer);
        }
    }
    return ODY_OK;
}


/***********************************************
 *
 * @Name : odysseusListMarket
 * @Def : Asks the island for its market and prints it.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus odysseusListMarket(Odysseus *ptOdysseus) {
    OdysseusStatus eStatus = ODY_OK;
 
    if (NO_SOCKET == ptOdysseus->nIslandSocket ||
        AT_ITHACA == ptOdysseus->nWhereAmI) {
        return ODY_ERR_NOT_AT_ISLAND;
    }
 
    eStatus = getListMarket(ptOdysseus);
    if (ODY_OK != eStatus && ODY_ERR_REMOTE != eStatus) {
        // The island conversation is out of sync, drop it
        close(ptOdysseus->nIslandSocket);
        ptOdysseus->nIslandSocket = NO_SOCKET;
    }
 
    return eStatus;
}


/***********************************************
 *
 * @Name : odysseusAcceptVoyage
 * @Def : Accepts one of the voyages of the last listing.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 *        In: psVoyageId = id of the voyage to accept
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus odysseusAcceptVoyage(Odysseus *ptOdysseus,
                                    const char *psVoyageId) {
    OdysseusStatus eStatus = ODY_OK;
    char *psBuffer = NULL;
    int nVoyageId = atoi(psVoyageId);
    int nListed = 0;
    int i = 0;
 
    if (NO_SOCKET == ptOdysseus->nIthacaSocket) {
        return ODY_ERR_NOT_CONNECTED;
    }
    if (0 != ptOdysseus->tActiveVoyage.nId) {
        return ODY_ERR_ACTIVE_VOYAGE;
    }
    for (i = 0; i < ptOdysseus->nNumListedVoyages; i++) {
        if (nVoyageId == ptOdysseus->atListedVoyages[i].nId) {
            nListed = 1;
        }
    }
    if (0 == nListed) {
        return ODY_ERR_NOT_LISTED;
    }
 
    eStatus = requestAcceptVoyage(ptOdysseus, nVoyageId);
    if (ODY_OK != eStatus) {
        return eStatus;
    }
 
    if (-1 != asprintf(&psBuffer, MSG_VOYAGE_ACCEPTED,
                       ptOdysseus->tActiveVoyage.nId,
                       ptOdysseus->tActiveVoyage.psDestination)) {
        println(psBuffer);
        free(psBuffer);
    }
    return ODY_OK;
}


/***********************************************
 *
 * @Name : leaveCurrentPlace
 * @Def : Leaves the place where odysseus is: disconnects from Ithaca or
 *        departs from the current island.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : None.
 *
 ************************************************/
static void leaveCurrentPlace(Odysseus *ptOdysseus) {
    if (AT_ITHACA == ptOdysseus->nWhereAmI) {
        println(MSG_LEAVING_ITHACA);
        disconnectFromIthaca(ptOdysseus);
    } else {
        printWithName(MSG_LEAVING,
                      ptOdysseus->tMap.atConnections[ptOdysseus->nWhereAmI]
                          .psName);
        departFromIsland(ptOdysseus);
    }
}


/***********************************************
 *
 * @Name : dockAtIsland
 * @Def : Announces the arrival to the island and, if the harbor is full,
 *        waits offshore. If it fails or is interrupted, the socket of the
 *        island is closed.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 *        Out: pnArrival = result of the arrival
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
static OdysseusStatus dockAtIsland(Odysseus *ptOdysseus, int *pnArrival) {
    OdysseusStatus eStatus = ODY_OK;
 
    eStatus = arriveAtIsland(ptOdysseus, pnArrival);
    if (ODY_OK == eStatus && ARRIVAL_WAIT == *pnArrival) {
        println(MSG_HARBOR_FULL);
        eStatus = waitingAtPort(ptOdysseus, pnArrival);
    }
    if (ODY_OK != eStatus || ARRIVAL_NONE == *pnArrival) {
        close(ptOdysseus->nIslandSocket);
        ptOdysseus->nIslandSocket = NO_SOCKET;
    }
    return eStatus;
}


/***********************************************
 *
 * @Name : lostAtSea
 * @Def : Prints that the crew ran out of provisions and closes the socket
 *        of the island.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : ODY_LOST_AT_SEA.
 *
 ************************************************/
static OdysseusStatus lostAtSea(Odysseus *ptOdysseus) {
    printWithName(MSG_LOST_AT_SEA, ptOdysseus->psName);
    close(ptOdysseus->nIslandSocket);
    ptOdysseus->nIslandSocket = NO_SOCKET;
    return ODY_LOST_AT_SEA;
}

/***********************************************
 *
 * @Name : printArrival
 * @Def : Prints that the island granted access and, if it is the
 *        destination of the active voyage, says so.
 * @Arg : In: ptOdysseus = Odysseus instance
 *        In: ptDestination = island where odysseus has docked
 * @Ret : None.
 *
 ************************************************/
static void printArrival(Odysseus *ptOdysseus, KnownIsland *ptDestination) {
    char *psBuffer = NULL;
 
    printWithName(MSG_GRANTED_ACCESS, ptDestination->psName);
 
    if (0 != ptOdysseus->tActiveVoyage.nId &&
        0 == strcasecmp(ptOdysseus->tActiveVoyage.psDestination,
                        ptDestination->psName)) {
        if (-1 != asprintf(&psBuffer, MSG_VOYAGE_DESTINATION,
                           ptDestination->psName,
                           ptOdysseus->tActiveVoyage.psObject)) {
            println(psBuffer);
            free(psBuffer);
        }
    }
}


/***********************************************
 *
 * @Name : odysseusSail
 * @Def : Sails from the current place to a neighbor island and docks
 *        there.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 *        In: psDestination = name of the island to sail to
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus odysseusSail(Odysseus *ptOdysseus, const char *psDestination) {
    OdysseusStatus eStatus = ODY_OK;
    KnownIsland *ptDestination = NULL;
    int nDestinationIndex = -1;
    int nArrival = ARRIVAL_NONE;
 
    nDestinationIndex = findDestinationFromIsland(ptOdysseus, psDestination);
 
    if (-1 == nDestinationIndex) {
        printWithName(MSG_CANT_SAIL, psDestination);
        return ODY_OK;
    }
    ptDestination = &ptOdysseus->tMap.atConnections[nDestinationIndex];
 
    leaveCurrentPlace(ptOdysseus);
    printWithName(MSG_SAILING_TO, ptDestination->psName);
 
    ptOdysseus->nIslandSocket = openClientConn(ptDestination->psIp,
                                               ptDestination->nPort);
    if (0 > ptOdysseus->nIslandSocket) {
        ptOdysseus->nIslandSocket = NO_SOCKET;
        return ODY_OK;
    }
    printWithName(MSG_CONNECTED_TO, ptDestination->psName);
 
    eStatus = dockAtIsland(ptOdysseus, &nArrival);
    if (ODY_OK != eStatus || ARRIVAL_NONE == nArrival) {
        return eStatus;
    }
 
    if (ARRIVAL_LOST_AT_SEA == nArrival) {
        return lostAtSea(ptOdysseus);
    }
 
    ptOdysseus->nWhereAmI = nDestinationIndex;
    printArrival(ptOdysseus, ptDestination);
 
    return ODY_OK;
}


/***********************************************
 *
 * @Name : odysseusBuy
 * @Def : Buys a product or the map at the island where odysseus is.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 *        In: psProduct = product to buy (or MAP)
 *        In: psAmount = amount to buy
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus odysseusBuy(Odysseus *ptOdysseus, const char *psProduct,
                           const char *psAmount) {
    OdysseusStatus eStatus = ODY_OK;
 
    if (NO_SOCKET == ptOdysseus->nIslandSocket ||
        AT_ITHACA == ptOdysseus->nWhereAmI) {
        return ODY_ERR_NOT_AT_ISLAND;
    }
 
    if (0 == strcasecmp(psProduct, MAP_PRODUCT)) {
        if (MAP_AMOUNT != atoi(psAmount)) {
            return ODY_ERR_MAP_AMOUNT;
        }
        if (ptOdysseus->tCargo.nGold < MAP_PRICE) {
            return ODY_ERR_NOT_ENOUGH_GOLD;
        }
        eStatus = buyMap(ptOdysseus);
    } else {
        eStatus = buyProducts(ptOdysseus, psProduct, psAmount);
    }
 
    if (ODY_OK != eStatus && ODY_ERR_REMOTE != eStatus &&
        ODY_ERR_NOT_ENOUGH_GOLD != eStatus) {
        // The island conversation is out of sync, drop it
        close(ptOdysseus->nIslandSocket);
        ptOdysseus->nIslandSocket = NO_SOCKET;
    }
 
    return eStatus;
}


/***********************************************
 *
 * @Name : odysseusSell
 * @Def : Sells a product of the cargo at the island where odysseus is.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 *        In: psProduct = product to sell
 *        In: psAmount = amount to sell
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus odysseusSell(Odysseus *ptOdysseus, const char *psProduct,
                            const char *psAmount) {
    int nAmount = atoi(psAmount);
    int i = 0;
 
    if (NO_SOCKET == ptOdysseus->nIslandSocket ||
        AT_ITHACA == ptOdysseus->nWhereAmI) {
        return ODY_ERR_NOT_AT_ISLAND;
    }
 
    for (i = 0; i < ptOdysseus->tCargo.nNumSupplies; i++) {
        if (0 == strcasecmp(ptOdysseus->tCargo.atSupplies[i].psName,
                            psProduct)) {
            if (nAmount <= ptOdysseus->tCargo.atSupplies[i].nAmount) {
                return sellProduct(ptOdysseus, psProduct, nAmount);
            }
            return ODY_ERR_NOT_ENOUGH_CARGO;
        }
    }
 
    return ODY_ERR_NOT_IN_CARGO;
}
