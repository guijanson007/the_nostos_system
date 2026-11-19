/***********************************************
 *
 * @File : odysseus_handler.h
 * @Purpose : handler for the odysseus process
 * @Author : Guilherme Rodrigues, Axel Boleda
 * @Date : 31/10/2026
 *
 ************************************************/
#define _GNU_SOURCE

#include <strings.h>

// header
#include "odysseus/odysseus_handler.h"

// own headers
#include "odysseus/odysseus_conversation.h"
#include "odysseus/odysseus_handler.h"
#include "odysseus/odysseus_map.h"
#include "odysseus/odysseus_parser.h"
#include "odysseus/odysseus_persistence.h"
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
 * @Name : returnToIthaca
 * @Def : Leaves the current island and returns directly to Ithaca (7.1),
 *        without connecting to it. Does nothing if already at Ithaca.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : None.
 *
 ************************************************/
static void returnToIthaca(Odysseus *ptOdysseus) {
    if (AT_ITHACA == ptOdysseus->nWhereAmI) {
        return;
    }
    printWithName(MSG_LEAVING,
                  ptOdysseus->tMap.atConnections[ptOdysseus->nWhereAmI].psName);
    departFromIsland(ptOdysseus);
    ptOdysseus->nWhereAmI = AT_ITHACA;
    println(MSG_RETURNING_ITHACA);
}

/***********************************************
 *
 * @Name : odysseusConnect
 * @Def : Connects odysseus to Ithaca. If odysseus is at an island, it
 *        returns directly to Ithaca first.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus odysseusConnect(Odysseus *ptOdysseus) {
    OdysseusStatus eStatus = ODY_OK;

    if (NO_SOCKET != ptOdysseus->nIthacaSocket) {
        return ODY_ERR_ALREADY_CONNECTED;
    }

    returnToIthaca(ptOdysseus);

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
    char *psUpperName = NULL;
    const char *psLocation = NULL;
    int i = 0;

    psUpperName = strdup(ptOdysseus->psName);
    if (NULL != psUpperName) {
        strToUpper(psUpperName);
        if (-1 != asprintf(&psBuffer, MSG_STATUS_HEADER, psUpperName,
                           ptOdysseus->tCargo.nGold)) {
            println(psBuffer);
            free(psBuffer);
        }
        free(psUpperName);
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

    if (-1 != asprintf(&psBuffer, MSG_TOTAL_PROVISIONS,
                       ptOdysseus->tCargo.nTotalAmount)) {
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
        if (NULL != ptOdysseus->tActiveVoyage.psObjectPath) {
            println(MSG_OBJECT_STORED);
        }
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
 * @Name : dropIslandOnError
 * @Def : After an I/O or protocol error the island conversation is out of
 *        sync (a late frame would be read as the answer of the next
 *        command), so the connection is closed. Errors the island answered
 *        in order (its own error replies, failed transfers that were
 *        resynchronised) keep it.
 * @Arg : In/Out: ptOdysseus = odysseus at the island
 *        In: eStatus = result of the island command
 * @Ret : None.
 *
 ************************************************/
static void dropIslandOnError(Odysseus *ptOdysseus, OdysseusStatus eStatus) {
    if ((ODY_ERR_IO == eStatus || ODY_ERR_ISLAND_PROTOCOL == eStatus ||
         ODY_ERR_PROTOCOL == eStatus) &&
        NO_SOCKET != ptOdysseus->nIslandSocket) {
        close(ptOdysseus->nIslandSocket);
        ptOdysseus->nIslandSocket = NO_SOCKET;
    }
}

/***********************************************
 *
 * @Name : odysseusDeliver
 * @Def : Delivers the object of the active voyage to the island where
 *        odysseus is docked: sends it, waits for the island to seal it and
 *        stores the sealed object. Only allowed at the destination island,
 *        once per voyage.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus odysseusDeliver(Odysseus *ptOdysseus) {
    OdysseusStatus eStatus = ODY_OK;
    OdysseusVoyage *ptVoyage = &ptOdysseus->tActiveVoyage;
    const char *psIsland = NULL;
    char *psBuffer = NULL;

    if (NO_SOCKET == ptOdysseus->nIslandSocket ||
        AT_ITHACA == ptOdysseus->nWhereAmI) {
        return ODY_ERR_NOT_AT_ISLAND;
    }
    if (0 == ptVoyage->nId || NULL == ptVoyage->psObjectPath) {
        return ODY_ERR_NO_VOYAGE;
    }
    psIsland = ptOdysseus->tMap.atConnections[ptOdysseus->nWhereAmI].psName;
    if (0 != strcasecmp(ptVoyage->psDestination, psIsland)) {
        return ODY_ERR_NOT_DESTINATION;
    }
    if (0 != ptVoyage->nDelivered) {
        return ODY_ERR_ALREADY_DELIVERED;
    }

    if (-1 != asprintf(&psBuffer, MSG_DELIVERING, ptVoyage->psObject,
                       psIsland)) {
        println(psBuffer);
        free(psBuffer);
    }
    eStatus = sendDelivery(ptOdysseus);
    if (ODY_OK != eStatus) {
        dropIslandOnError(ptOdysseus, eStatus);
        return eStatus;
    }
    println(MSG_OBJECT_TRANSFERRED);
    printWithName(MSG_STAMPING, psIsland);

    eStatus = receiveSigned(ptOdysseus);
    if (ODY_OK != eStatus) {
        dropIslandOnError(ptOdysseus, eStatus);
        return eStatus;
    }
    println(MSG_STAMPED_RECEIVED);
    printWithName(MSG_DELIVERY_COMPLETED, psIsland);
    return ODY_OK;
}

/***********************************************
 *
 * @Name : odysseusClaim
 * @Def : Claims the reward of the active voyage from Ithaca by presenting
 *        the object sealed by the island. Only allowed connected to Ithaca
 *        after a delivery. On success the reward is added to the gold, the
 *        voyage (and its files) is cleared and the state is saved.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus odysseusClaim(Odysseus *ptOdysseus) {
    OdysseusStatus eStatus = ODY_OK;
    OdysseusVoyage *ptVoyage = &ptOdysseus->tActiveVoyage;
    char *psDestination = NULL;
    char *psBuffer = NULL;
    int nVoyageId = 0;
    int nReward = 0;

    if (NO_SOCKET == ptOdysseus->nIthacaSocket) {
        return ODY_ERR_NOT_CONNECTED;
    }
    if (0 == ptVoyage->nId) {
        return ODY_ERR_NO_VOYAGE;
    }
    if (0 == ptVoyage->nDelivered || NULL == ptVoyage->psSignedPath) {
        return ODY_ERR_NOT_DELIVERED;
    }

    if (-1 != asprintf(&psBuffer, MSG_CLAIM_PRESENTING, ptVoyage->psObject)) {
        println(psBuffer);
        free(psBuffer);
    }
    eStatus = requestClaim(ptOdysseus, &nReward);
    if (ODY_OK != eStatus) {
        if (ODY_ERR_REMOTE != eStatus) {
            // The Ithaca conversation is out of sync, drop it
            close(ptOdysseus->nIthacaSocket);
            ptOdysseus->nIthacaSocket = NO_SOCKET;
        }
        return eStatus;
    }

    // The voyage is cleared below, keep what the messages need
    nVoyageId = ptVoyage->nId;
    psDestination = strdup(ptVoyage->psDestination);
    if (NULL != psDestination) {
        printWithName(MSG_GLYPH_VERIFIED, psDestination);
        free(psDestination);
    }
    if (-1 != asprintf(&psBuffer, MSG_VOYAGE_COMPLETED, nVoyageId)) {
        println(psBuffer);
        free(psBuffer);
    }
    if (-1 != asprintf(&psBuffer, MSG_REWARD_RECEIVED, nReward)) {
        println(psBuffer);
        free(psBuffer);
    }

    ptOdysseus->tCargo.nGold += nReward;
    discardOdysseusVoyage(ptVoyage);
    odysseusSaveState(ptOdysseus);
    return ODY_OK;
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
        if (ODY_ERR_REMOTE != eStatus) {
            // The Ithaca conversation is out of sync, drop it
            close(ptOdysseus->nIthacaSocket);
            ptOdysseus->nIthacaSocket = NO_SOCKET;
        }
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
        // The island conversation is off
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
        if (ODY_ERR_REMOTE != eStatus) {
            // The Ithaca conversation is out of sync, drop it
            close(ptOdysseus->nIthacaSocket);
            ptOdysseus->nIthacaSocket = NO_SOCKET;
        }
        return eStatus;
    }

    if (-1 != asprintf(&psBuffer, MSG_VOYAGE_ACCEPTED,
                       ptOdysseus->tActiveVoyage.nId,
                       ptOdysseus->tActiveVoyage.psDestination)) {
        println(psBuffer);
        free(psBuffer);
    }
    println(MSG_OBJECT_RECEIVED);
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
        printWithName(
            MSG_LEAVING,
            ptOdysseus->tMap.atConnections[ptOdysseus->nWhereAmI].psName);
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
 * @Def : Prints that the crew ran out of provisions, forgets the saved
 *        state (6.8: restart with the initial resources) and closes the
 *        socket of the island.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : ODY_LOST_AT_SEA.
 *
 ************************************************/
static OdysseusStatus lostAtSea(Odysseus *ptOdysseus) {
    printWithName(MSG_LOST_AT_SEA, ptOdysseus->psName);
    // The ship and its treasure are lost: forget the saved state (restart
    // uses the initial resources) and empty the memory copy so the final
    // save on exit does not bring it back
    ptOdysseus->tCargo.nGold = 0;
    odysseusResetState(ptOdysseus);
    // Being lost at sea also loses the object (7.1)
    discardOdysseusVoyage(&ptOdysseus->tActiveVoyage);
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

    // From any island the ship can return directly to Ithaca (7.1); it
    // connects to Ithaca later with CONNECT ITHACA
    if (0 == strcasecmp(psDestination, ITHACA_NAME)) {
        if (AT_ITHACA == ptOdysseus->nWhereAmI) {
            printWithName(MSG_CANT_SAIL, ITHACA_NAME);
        } else {
            returnToIthaca(ptOdysseus);
        }
        return ODY_OK;
    }

    nDestinationIndex = findDestinationFromIsland(ptOdysseus, psDestination);

    if (-1 == nDestinationIndex) {
        printWithName(MSG_CANT_SAIL, psDestination);
        return ODY_OK;
    }
    ptDestination = &ptOdysseus->tMap.atConnections[nDestinationIndex];

    leaveCurrentPlace(ptOdysseus);
    printWithName(MSG_SAILING_TO, ptDestination->psName);

    ptOdysseus->nIslandSocket =
        openClientConn(ptDestination->psIp, ptDestination->nPort);
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
    OdysseusStatus eStatus = ODY_OK;
    int nAmount = atoi(psAmount);
    int i = 0;

    if (NO_SOCKET == ptOdysseus->nIslandSocket ||
        AT_ITHACA == ptOdysseus->nWhereAmI) {
        return ODY_ERR_NOT_AT_ISLAND;
    }

    for (i = 0; i < ptOdysseus->tCargo.nNumSupplies; i++) {
        if (0 ==
            strcasecmp(ptOdysseus->tCargo.atSupplies[i].psName, psProduct)) {
            if (nAmount > ptOdysseus->tCargo.atSupplies[i].nAmount) {
                return ODY_ERR_NOT_ENOUGH_CARGO;
            }
            eStatus = sellProduct(ptOdysseus, psProduct, nAmount);
            dropIslandOnError(ptOdysseus, eStatus);
            return eStatus;
        }
    }

    return ODY_ERR_NOT_IN_CARGO;
}
