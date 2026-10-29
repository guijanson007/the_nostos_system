/***********************************************
 *
 * @File : odysseus_conversetion.c
 * @Purpose : Manages the conversation between odysseus and ithaca or an island.
 * @Author : Axel Boleda Callizo and Guilherme Rodrigues
 * @Date : 22/10/2026
 *
 ************************************************/
#define _GNU_SOURCE

// header
#include "odysseus/odysseus_conversation.h"

// own headers
#include "odysseus/odysseus_map.h"
#include "odysseus/odysseus_parser.h"
#include "utils/utils_communication.h"

#define WAIT "WAIT"
#define DOCKED "DOCKED"
#define OK_RESPONSE "OK"
#define MAP_FIELDS_EMPTY 3
#define MAP_FIELDS_ROUTE 7
#define MAX_MAP_ROUTES 20
#define MAX_PORT 65535
#define NO_PRICE -1
#define NO_TOTAL -1

OdysseusStatus connectToIthaca(Odysseus *tOdysseus) {
    struct sockaddr_in sAddr;
    int nfdSocket = -1;

    nfdSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nfdSocket < 0) {
        odysseusSetErrnoError(tOdysseus, ERROR_CREATE_SOCKET, errno);
        return ODY_ERR_SOCKET;
    }

    memset(&sAddr, 0, sizeof(sAddr));
    sAddr.sin_family = AF_INET;
    sAddr.sin_port = htons(atoi(tOdysseus->port));
    if (inet_pton(AF_INET, tOdysseus->ip, &sAddr.sin_addr) <= 0) {
        close(nfdSocket);
        return ODY_ERR_IP;
    }

    if (connect(nfdSocket, (struct sockaddr *)&sAddr, sizeof(sAddr)) < 0) {
        odysseusSetErrnoError(tOdysseus, ERROR_CONNECT_SERVER, errno);
        close(nfdSocket);
        return ODY_ERR_CONNECT;
    }
    tOdysseus->ithaca_socket = nfdSocket;
    return ODY_OK;
}

/*************************************************
 *
 * @Name : splitFields
 * @Def : Splits a '&' separated payload in place into its fields.
 * @Arg : In/Out: psData = payload, '&' are replaced by '\0'
 *        Out: apsFields = pointers to the fields found
 *        In: nMaxFields = size of apsFields
 * @Ret : Number of fields found.
 *
 **************************************************/
static int splitFields(char *psData, char **apsFields, int nMaxFields) {
    int nFields = 0;

    apsFields[nFields++] = psData;
    for (; '\0' != *psData && nFields < nMaxFields; psData++) {
        if ('&' == *psData) {
            *psData = '\0';
            apsFields[nFields++] = psData + 1;
        }
    }
    return nFields;
}

/*************************************************
 *
 * @Name : fillVoyage
 * @Def : Fills a voyage from its object, destination and reward fields.
 * @Arg : Out: ptVoyage = voyage to fill (strings are duplicated)
 *        In: nId = voyage id
 *        In: psObject = object name
 *        In: psDestination = destination island
 *        In: psReward = reward as text
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 **************************************************/
static int fillVoyage(OdysseusVoyage *ptVoyage, int nId, const char *psObject,
                      const char *psDestination, const char *psReward) {
    if (!isPositiveInteger(psReward) && 0 != strcmp(psReward, "0")) {
        return -1;
    }
    ptVoyage->id = nId;
    ptVoyage->object = strdup(psObject);
    ptVoyage->destination = strdup(psDestination);
    ptVoyage->reward = atoi(psReward);
    if (NULL == ptVoyage->object || NULL == ptVoyage->destination) {
        freeOdysseusVoyage(ptVoyage);
        return -1;
    }
    return 0;
}

/*************************************************
 *
 * @Name : requestListOfVoyages
 * @Def : Asks Ithaca for the available voyages and stores them in the
 * Odysseus listing. Every frame of the answer is read, even after a bad one,
 * so the socket stays in sync with Ithaca.
 * @Arg : In/Out: tOdysseus = Odysseus instance
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 **************************************************/
OdysseusStatus requestListOfVoyages(Odysseus *tOdysseus) {
    Message tMessage;
    char *apsFields[6];
    OdysseusVoyage *atVoyages = NULL;
    OdysseusStatus eStatus = ODY_OK;
    int nTotal = 0;
    int nIndex = 0;
    int nId = 0;
    int i = 0;

    memset(&tMessage, 0, sizeof(tMessage));
    memset(apsFields, 0, sizeof(apsFields));
    tMessage.type = MSG_LIST_VOYAGES;
    tMessage.flag = FLAG_NONE;

    if (sendMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }

    // The previous listing is stale from now on
    freeListedVoyages(tOdysseus);

    for (i = 1; i <= (0 == nTotal ? 1 : nTotal); i++) {
        if (receiveMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
            eStatus = ODY_ERR_IO;
            break;
        }
        if (tMessage.type != MSG_LIST_VOYAGES ||
            !(tMessage.flag & FLAG_RESPONSE)) {
            eStatus = ODY_ERR_PROTOCOL;
            break;
        }
        if (tMessage.flag & FLAG_ERROR) {
            odysseusSetRemoteError(tOdysseus, tMessage.data,
                                   tMessage.dataLength);
            eStatus = ODY_ERR_REMOTE;
            break;
        }

        // <Index>&<Total>&<VoyageID>&<Object>&<Destination>&<Reward>
        if (6 != splitFields(tMessage.data, apsFields, 6) ||
            !isPositiveInteger(apsFields[0]) ||
            !isPositiveInteger(apsFields[1]) ||
            !isPositiveInteger(apsFields[2])) {
            eStatus = ODY_ERR_PROTOCOL;
            if (0 == nTotal) {
                break; // Without a total we can't know how many to drain
            }
            continue;
        }
        nIndex = atoi(apsFields[0]);
        nId = atoi(apsFields[2]);
        if (0 == nTotal) {
            nTotal = atoi(apsFields[1]);
            if (nTotal > MAX_LISTED_VOYAGES) {
                // A frame count we won't allocate for, and can't drain
                nTotal = 0;
                eStatus = ODY_ERR_PROTOCOL;
                break;
            }
            atVoyages = calloc((size_t)nTotal, sizeof(OdysseusVoyage));
        }
        if (NULL == atVoyages) {
            eStatus = ODY_ERR_PROTOCOL; // Keep draining the remaining frames
            continue;
        }
        if (ODY_OK == eStatus &&
            (nIndex != i || nTotal != atoi(apsFields[1]) ||
             0 != fillVoyage(&atVoyages[i - 1], nId, apsFields[3], apsFields[4],
                             apsFields[5]))) {
            eStatus = ODY_ERR_PROTOCOL;
        }
    }

    if (ODY_OK != eStatus) {
        for (i = 0; NULL != atVoyages && i < nTotal; i++) {
            freeOdysseusVoyage(&atVoyages[i]);
        }
        free(atVoyages);
        return eStatus;
    }

    tOdysseus->listedVoyages = atVoyages;
    tOdysseus->numListedVoyages = nTotal;
    return ODY_OK;
}

/*************************************************
 *
 * @Name : requestAcceptVoyage
 * @Def : Asks Ithaca to assign a voyage and stores it as the active voyage.
 * @Arg : In/Out: tOdysseus = Odysseus instance
 *        In: nVoyageId = id of the voyage to accept
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 **************************************************/
OdysseusStatus requestAcceptVoyage(Odysseus *tOdysseus, int nVoyageId) {
    Message tMessage;
    char *apsFields[5];
    OdysseusVoyage tVoyage;
    char *psId = NULL;

    memset(&tMessage, 0, sizeof(tMessage));
    memset(apsFields, 0, sizeof(apsFields));
    memset(&tVoyage, 0, sizeof(tVoyage));
    tMessage.type = MSG_ACCEPT;
    tMessage.flag = FLAG_NONE;
    if (-1 == asprintf(&psId, "%d", nVoyageId)) {
        return ODY_ERR_IO;
    }
    buildMessage(&tMessage, FLAG_NONE, psId);
    free(psId);

    if (sendMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }
    if (receiveMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }
    if (tMessage.type != MSG_ACCEPT || !(tMessage.flag & FLAG_RESPONSE)) {
        return ODY_ERR_PROTOCOL;
    }
    if (tMessage.flag & FLAG_ERROR) {
        odysseusSetRemoteError(tOdysseus, tMessage.data,
                               tMessage.dataLength);
        return ODY_ERR_REMOTE;
    }

    // OK&<VoyageID>&<Object>&<Destination>&<Reward>
    if (5 != splitFields(tMessage.data, apsFields, 5) ||
        0 != strcmp(apsFields[0], "OK") || atoi(apsFields[1]) != nVoyageId ||
        0 != fillVoyage(&tVoyage, nVoyageId, apsFields[2], apsFields[3],
                        apsFields[4])) {
        return ODY_ERR_PROTOCOL;
    }

    freeOdysseusVoyage(&tOdysseus->activeVoyage);
    tOdysseus->activeVoyage = tVoyage;
    // The ids of the old listing no longer mean anything
    freeListedVoyages(tOdysseus);
    return ODY_OK;
}

OdysseusStatus disconnectFromIthaca(Odysseus *tOdysseus) {
    Message tMessage;
    memset(&tMessage, 0, sizeof(tMessage));
    tMessage.type = MSG_DISCONNECT_ITHACA;
    tMessage.flag = FLAG_NONE;
    tMessage.dataLength = 0;
    tMessage.data[0] = '\0';

    if (sendMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }

    // Wait for the response from Ithaca
    if (receiveMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }

    if (tMessage.type != MSG_DISCONNECT_ITHACA) {
        return ODY_ERR_PROTOCOL;
    }

    if (tMessage.flag & FLAG_ERROR) {
        odysseusSetRemoteError(tOdysseus, tMessage.data, tMessage.dataLength);
        return ODY_ERR_REMOTE;
    } else if (!(tMessage.flag & FLAG_RESPONSE)) {
        return ODY_ERR_PROTOCOL;
    }

    close(tOdysseus->ithaca_socket);
    tOdysseus->ithaca_socket = -1;
    return ODY_OK;
}

OdysseusStatus arriveAtIsland(Odysseus *tOdysseus, int *pnArrival) {
    Message tMessage;
    memset(&tMessage, 0, sizeof(tMessage));
    tMessage.type = MSG_ARRIVAL_ISLAND;
    tMessage.flag = FLAG_NONE;
    buildMessage(&tMessage, FLAG_NONE, tOdysseus->name);

    if (sendMessage(&tMessage, tOdysseus->island_socket) != 0) {
        return ODY_ERR_IO;
    }

    // Wait for the response from the island
    if (receiveMessage(&tMessage, tOdysseus->island_socket) != 0) {
        return ODY_ERR_IO;
    }

    if (tMessage.type != MSG_ARRIVAL_ISLAND) {
        return ODY_ERR_ISLAND_PROTOCOL;
    }

    if (tMessage.flag & FLAG_ERROR) {
        odysseusSetRemoteError(tOdysseus, tMessage.data, tMessage.dataLength);
        return ODY_ERR_REMOTE;
    } else if (tMessage.flag & FLAG_RESPONSE) {
        if (strcmp(tMessage.data, WAIT) == 0) {
            *pnArrival = ARRIVAL_WAIT;
            return ODY_OK;
        } else if (strcmp(tMessage.data, DOCKED) == 0) {
            *pnArrival = ARRIVAL_DOCKED;
            return ODY_OK;
        }
    }
    return ODY_ERR_ISLAND_PROTOCOL;
}

OdysseusStatus notifyConnection(Odysseus *tOdysseus) {
    Message tMessage;
    memset(&tMessage, 0, sizeof(tMessage));

    tMessage.type = MSG_CONNECT_ITHACA;
    tMessage.flag = FLAG_NONE;
    buildMessage(&tMessage, FLAG_NONE, tOdysseus->name);

    if (sendMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }

    // Wait for the response from Ithaca
    if (receiveMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }

    if (tMessage.type != MSG_CONNECT_ITHACA) {
        return ODY_ERR_PROTOCOL;
    }

    if (tMessage.flag & FLAG_ERROR) {
        odysseusSetRemoteError(tOdysseus, tMessage.data,
                               tMessage.dataLength);
        return ODY_ERR_REMOTE;
    } else if (tMessage.flag & FLAG_RESPONSE) {
        return ODY_OK;
    }
    return ODY_ERR_PROTOCOL;
}

OdysseusStatus waitingAtPort(Odysseus *ptOdysseus, int *pnArrival) {
    fd_set tReadFds;
    struct timeval tTimeout;
    Message tMessage;
    char *psBuffer = NULL;
    int nReady = 0;

    memset(&tMessage, 0, sizeof(tMessage));
    *pnArrival = ARRIVAL_NONE;
    tTimeout.tv_sec = 5;
    tTimeout.tv_usec = 0;

    while (1) {
        FD_ZERO(&tReadFds);
        FD_SET(ptOdysseus->island_socket, &tReadFds);
        nReady = select(ptOdysseus->island_socket + 1, &tReadFds, NULL, NULL,
                        &tTimeout);

        if (-1 == nReady) {
            // SIGINT: stop waiting quietly, the menu loop will quit
            return EINTR == errno ? ODY_OK : ODY_ERR_IO;
        }

        if (0 == nReady) {
            consumeFood(&ptOdysseus->cargo);
            if (-1 != asprintf(&psBuffer,
                               "\n>>> The crew consumes another ration.\n"
                               "Provisions remaining: %d kg",
                               ptOdysseus->cargo.totalAmount)) {
                println(psBuffer);
                free(psBuffer);
            }
            if (0 >= ptOdysseus->cargo.totalAmount) {
                *pnArrival = ARRIVAL_LOST_AT_SEA;
                return ODY_OK;
            }
            tTimeout.tv_sec = 5;
            tTimeout.tv_usec = 0;
            continue;
        }

        if (0 != receiveMessage(&tMessage, ptOdysseus->island_socket)) {
            return ODY_ERR_IO;
        }

        if (tMessage.flag & FLAG_ERROR) {
            odysseusSetRemoteError(ptOdysseus, tMessage.data,
                                   tMessage.dataLength);
            return ODY_ERR_REMOTE;
        }

        if (MSG_ARRIVAL_ISLAND == tMessage.type &&
            FLAG_RESPONSE == tMessage.flag &&
            0 == strcmp(tMessage.data, DOCKED)) {
            *pnArrival = ARRIVAL_DOCKED;
            return ODY_OK;
        }
    }
}

static int parseMessageMarket(char *psData, int nLength, int *pnIndex,
                              int *pnTotal, char **ppsProduct, int *pnAmount,
                              int *pnPrice) {
    char *psIndex = NULL;
    char *psTotal = NULL;
    char *psAmount = NULL;
    char *psPrice = NULL;

    if (NULL == psData || nLength <= 0 || nLength > 253) {
        return -1;
    }
    psData[nLength] = '\0';

    psIndex = strtok(psData, "&");
    psTotal = strtok(NULL, "&");
    *ppsProduct = strtok(NULL, "&");
    psAmount = strtok(NULL, "&");
    psPrice = strtok(NULL, "&");

    if (NULL == psIndex || NULL == psTotal || NULL == *ppsProduct ||
        NULL == psAmount || NULL == psPrice || NULL != strtok(NULL, "&")) {
        return -1;
    }
    if (!isAllDigits(psIndex) || !isAllDigits(psTotal) ||
        !isAllDigits(psAmount) || !isAllDigits(psPrice)) {
        return -1;
    }

    *pnIndex = atoi(psIndex);
    *pnTotal = atoi(psTotal);
    *pnAmount = atoi(psAmount);
    *pnPrice = atoi(psPrice);

    if (*pnIndex < 1 || *pnIndex > *pnTotal) {
        return -1;
    }
    return 0;
}

/***********************************************
 *
 * @Name : queryMarket
 * @Def : Asks the island for its market. It can print it and/or look for the
 *        price of one product.
 * @Arg : In/Out: tOdysseus = odysseus docked at the island
 *        In: nPrint = 1 to print the market, 0 to stay silent
 *        In: psWanted = product whose price is wanted, or NULL
 *        Out: pnPrice = price of psWanted when the island sells it
 * @Ret : ODY_OK if everything went well, the error status otherwise.
 *
 ************************************************/
static OdysseusStatus queryMarket(Odysseus *tOdysseus, int nPrint,
                                  const char *psWanted, int *pnPrice) {
    int nIndex = 0;
    int nTotal = 0;
    int nExpected = 1;
    char *psProduct = NULL;
    int nAmount = 0;
    int nPrice = 0;
    char *psBuffer = NULL;
    Message tMessage;

    memset(&tMessage, 0, sizeof(tMessage));
    tMessage.type = MSG_LIST_MARKET;
    tMessage.flag = FLAG_NONE;

    if (sendMessage(&tMessage, tOdysseus->island_socket) != 0) {
        return ODY_ERR_IO;
    }

    if (nPrint &&
        -1 != asprintf(&psBuffer, "--- %s MARKET ---",
                       tOdysseus->map.connections[tOdysseus->whereAmI].name)) {
        println(psBuffer);
        free(psBuffer);
    }
    while (1) {
        if (receiveMessage(&tMessage, tOdysseus->island_socket) != 0) {
            return ODY_ERR_IO;
        }

        if (tMessage.type != MSG_LIST_MARKET) {
            return ODY_ERR_ISLAND_PROTOCOL;
        }

        if (tMessage.flag & FLAG_ERROR) {
            odysseusSetRemoteError(tOdysseus, tMessage.data,
                                   tMessage.dataLength);
            return ODY_ERR_REMOTE;
        }

        if (0 != parseMessageMarket(tMessage.data, tMessage.dataLength,
                                    &nIndex, &nTotal, &psProduct, &nAmount,
                                    &nPrice) ||
            nIndex != nExpected) {
            return ODY_ERR_ISLAND_PROTOCOL;
        }
        nExpected++;

        if (nIndex == nTotal) { // MAP
            if (nPrint) {
                println("...");
                if (-1 != asprintf(&psBuffer, "%s\t\t\t%d gold", psProduct,
                                   nPrice)) {
                    println(psBuffer);
                    free(psBuffer);
                }
            }
            break;
        }

        if (NULL != psWanted && 0 == strcasecmp(psProduct, psWanted)) {
            *pnPrice = nPrice;
        }
        if (nPrint &&
            -1 != asprintf(&psBuffer, "%s\t%d kg\t %d gold / kg", psProduct,
                           nAmount, nPrice)) {
            println(psBuffer);
            free(psBuffer);
        }
    }

    return ODY_OK;
}

OdysseusStatus getListMarket(Odysseus *tOdysseus) {
    return queryMarket(tOdysseus, 1, NULL, NULL);
}

static int parseMessageTrade(char *psData, int nLength, char **ppsProduct,
                             int *pnAmount, int *pnCost) {
    char *psOk = NULL;
    char *psAmount = NULL;
    char *psCost = NULL;

    if (NULL == psData || nLength <= 0 || nLength > 253) return -1;
    
    psData[nLength] = '\0';

    psOk = strtok(psData, "&");
    *ppsProduct = strtok(NULL, "&");
    psAmount = strtok(NULL, "&");
    psCost = strtok(NULL, "&");

    if (NULL == psOk || NULL == *ppsProduct || NULL == psAmount ||
        NULL == psCost || NULL != strtok(NULL, "&")) return -1;

    if (0 != strcmp(psOk, "OK")) return -1;

    if (!isAllDigits(psAmount) || !isAllDigits(psCost)) return -1;

    *pnAmount = atoi(psAmount);
    *pnCost = atoi(psCost);

    return 0;
}

static int addSupply(Cargo *ptCargo, const char *psName, int nAmount) {
    Supply *atTmp = NULL;
    char *psCopy = NULL;
    int i = 0;

    for (i = 0; i < ptCargo->numSupplies; i++) {
        if (0 == strcasecmp(ptCargo->supplies[i].name, psName)) {
            ptCargo->supplies[i].amount += nAmount;
            ptCargo->totalAmount += nAmount;
            return 0;
        }
    }

    psCopy = strdup(psName);
    if (NULL == psCopy) {
        return -1;
    }

    atTmp =
        realloc(ptCargo->supplies, sizeof(Supply) * (ptCargo->numSupplies + 1));
    if (NULL == atTmp) {
        free(psCopy);
        return -1;
    }

    ptCargo->supplies = atTmp;
    ptCargo->supplies[ptCargo->numSupplies].name = psCopy;
    ptCargo->supplies[ptCargo->numSupplies].amount = nAmount;
    ptCargo->numSupplies++;
    ptCargo->totalAmount += nAmount;

    return 0;
}

OdysseusStatus buyProducts(Odysseus *tOdysseus, const char *psProduct,
                           const char *psAmount) {
    Message tMessage;
    OdysseusStatus eStatus = ODY_OK;
    char *psBuffer = NULL;
    char *psBought = NULL;
    int nAmount = 0;
    int nCost = 0;
    int nPrice = NO_PRICE;

    // The island can't know our gold, so check it here before ordering
    eStatus = queryMarket(tOdysseus, 0, psProduct, &nPrice);
    if (ODY_OK != eStatus) {
        return eStatus;
    }
    if (NO_PRICE != nPrice &&
        (long long)atoi(psAmount) * nPrice > tOdysseus->cargo.nGold) {
        return ODY_ERR_NOT_ENOUGH_GOLD;
    }

    memset(&tMessage, 0, sizeof(tMessage));
    tMessage.type = MSG_BUY;

    if (-1 == asprintf(&psBuffer, "%s&%s", psProduct, psAmount)) {
        return ODY_ERR_IO;
    }
    buildMessage(&tMessage, FLAG_NONE, psBuffer);
    free(psBuffer);
    psBuffer = NULL;
    if (-1 == sendMessage(&tMessage, tOdysseus->island_socket)) {
        return ODY_ERR_IO;
    }

    if (0 != receiveMessage(&tMessage, tOdysseus->island_socket)) {
        return ODY_ERR_IO;
    }

    if (MSG_BUY != tMessage.type) {
        return ODY_ERR_ISLAND_PROTOCOL;
    }
    if (tMessage.flag & FLAG_ERROR) {
        odysseusSetRemoteError(tOdysseus, tMessage.data, tMessage.dataLength);
        return ODY_ERR_REMOTE;
    }

    if (0 != parseMessageTrade(tMessage.data, tMessage.dataLength, &psBought,
                               &nAmount, &nCost)) {
        return ODY_ERR_ISLAND_PROTOCOL;
    }

    // Store the goods first: if that fails nothing has been charged locally
    if (0 != addSupply(&tOdysseus->cargo, psBought, nAmount)) {
        return ODY_ERR_IO;
    }
    tOdysseus->cargo.nGold -= nCost;

    if (-1 != asprintf(&psBuffer, "Bought %d kg of %s for %d gold.", nAmount,
                       psBought, nCost)) {
        println(psBuffer);
        free(psBuffer);
    }

    return ODY_OK;
}

/***********************************************
 *
 * @Name : parseMapRoute
 * @Def : Parses one frame of a BUY MAP response and checks it is the next one
 *        expected. The first frame sets the total.
 * @Arg : In/Out: psData = payload, split in place
 *        In: nReceived = routes received before this frame
 *        In/Out: pnTotal = routes announced by the island, NO_TOTAL until the
 *                first frame
 *        Out: ptRoute = route found (strings are duplicated), if any
 *        Out: pnIsRoute = 1 if the frame carried a route, 0 for the empty map
 * @Ret : 0 if the frame is valid, -1 otherwise.
 *
 ************************************************/
static int parseMapRoute(char *psData, int nReceived, int *pnTotal,
                         MapRoute *ptRoute, int *pnIsRoute) {
    char *apsFields[MAP_FIELDS_ROUTE];
    int nFields = splitFields(psData, apsFields, MAP_FIELDS_ROUTE);

    *pnIsRoute = 0;
    if (nFields < MAP_FIELDS_EMPTY || 0 != strcmp(apsFields[0], OK_RESPONSE) ||
        !isPositiveInteger(apsFields[1]) || MAP_PRICE != atoi(apsFields[1])) {
        return -1;
    }

    if (MAP_FIELDS_EMPTY == nFields) {
        if (NO_TOTAL != *pnTotal || 0 != strcmp(apsFields[2], "0")) {
            return -1;
        }
        *pnTotal = 0;
        return 0;
    }

    // OK&50&<Index>&<Total>&<Name>&<IP>&<Port>
    if (MAP_FIELDS_ROUTE != nFields || !isPositiveInteger(apsFields[2]) ||
        !isPositiveInteger(apsFields[3]) || '\0' == *apsFields[4] ||
        '\0' == *apsFields[5] || !isPositiveInteger(apsFields[6]) ||
        atoi(apsFields[6]) > MAX_PORT) {
        return -1;
    }
    if (atoi(apsFields[2]) != nReceived + 1 || atoi(apsFields[3]) < 1 ||
        atoi(apsFields[3]) > MAX_MAP_ROUTES ||
        (NO_TOTAL != *pnTotal && atoi(apsFields[3]) != *pnTotal)) {
        return -1;
    }
    *pnTotal = atoi(apsFields[3]);

    ptRoute->name = strdup(apsFields[4]);
    ptRoute->ip = strdup(apsFields[5]);
    ptRoute->port = atoi(apsFields[6]);
    if (NULL == ptRoute->name || NULL == ptRoute->ip) {
        free(ptRoute->name);
        free(ptRoute->ip);
        return -1;
    }
    *pnIsRoute = 1;
    return 0;
}

OdysseusStatus buyMap(Odysseus *tOdysseus) {
    Message tMessage;
    OdysseusStatus eStatus = ODY_OK;
    MapRoute *atRoutes = NULL;
    MapRoute *atTmp = NULL;
    MapRoute tRoute;
    char *psBuffer = NULL;
    int nTotal = NO_TOTAL;
    int nReceived = 0;
    int nIsRoute = 0;

    memset(&tMessage, 0, sizeof(tMessage));
    tMessage.type = MSG_BUY_MAP;
    if (-1 == asprintf(&psBuffer, "%d", MAP_AMOUNT)) {
        return ODY_ERR_IO;
    }
    buildMessage(&tMessage, FLAG_NONE, psBuffer);
    free(psBuffer);
    psBuffer = NULL;
    if (0 != sendMessage(&tMessage, tOdysseus->island_socket)) {
        return ODY_ERR_IO;
    }

    // Receive everything first: nothing is charged or merged unless the whole
    // response is valid
    while (ODY_OK == eStatus && (NO_TOTAL == nTotal || nReceived < nTotal)) {
        if (0 != receiveMessage(&tMessage, tOdysseus->island_socket)) {
            eStatus = ODY_ERR_IO;
        } else if (MSG_BUY_MAP != tMessage.type) {
            eStatus = ODY_ERR_ISLAND_PROTOCOL;
        } else if (tMessage.flag & FLAG_ERROR) {
            odysseusSetRemoteError(tOdysseus, tMessage.data,
                                   tMessage.dataLength);
            eStatus = ODY_ERR_REMOTE;
        } else if (FLAG_RESPONSE != tMessage.flag ||
                   0 != parseMapRoute(tMessage.data, nReceived, &nTotal,
                                      &tRoute, &nIsRoute)) {
            eStatus = ODY_ERR_ISLAND_PROTOCOL;
        } else if (nIsRoute) {
            atTmp = realloc(atRoutes, sizeof(MapRoute) * (nReceived + 1));
            if (NULL == atTmp) {
                free(tRoute.name);
                free(tRoute.ip);
                eStatus = ODY_ERR_IO;
            } else {
                atRoutes = atTmp;
                atRoutes[nReceived++] = tRoute;
            }
        }
    }

    if (ODY_OK == eStatus && 0 != mergeRoutes(tOdysseus, atRoutes, nReceived)) {
        eStatus = ODY_ERR_IO;
    }
    freeMapRoutes(atRoutes, nReceived);
    if (ODY_OK != eStatus) {
        return eStatus;
    }

    tOdysseus->cargo.nGold -= MAP_PRICE;
    if (-1 != asprintf(&psBuffer, "Bought the map of %s for %d gold.",
                       tOdysseus->map.connections[tOdysseus->whereAmI].name,
                       MAP_PRICE)) {
        println(psBuffer);
        free(psBuffer);
    }
    println(0 < nReceived ? "New routes have been charted."
                          : "There were no routes to chart.");

    return ODY_OK;
}

OdysseusStatus departFromIsland(Odysseus *tOdysseus) {
    Message tMessage;

    memset(&tMessage, 0, sizeof(tMessage));
    tMessage.type = MSG_DEPARTURE_ISLAND;
    tMessage.flag = FLAG_NONE;
    if (0 != sendMessage(&tMessage, tOdysseus->island_socket)) {
        return ODY_ERR_IO;
    }
    if (0 != receiveMessage(&tMessage, tOdysseus->island_socket)) {
        return ODY_ERR_IO;
    }

    if (MSG_DEPARTURE_ISLAND != tMessage.type) {
        return ODY_ERR_ISLAND_PROTOCOL;
    }
    if (tMessage.flag & FLAG_ERROR) {
        odysseusSetRemoteError(tOdysseus, tMessage.data, tMessage.dataLength);
        return ODY_ERR_REMOTE;
    }
    if (0 != strcmp(tMessage.data, OK_RESPONSE)) {
        return ODY_ERR_ISLAND_PROTOCOL;
    }
    return ODY_OK;
}
