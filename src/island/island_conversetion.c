
/***********************************************
 *
 * @File : island_conversetion.c
 * @Purpose : manage the logic between Island and odysseus
 * @Author : Axel Boleda i Callizo
 * @Date : 23/10/2026
 *
 ************************************************/

#define _GNU_SOURCE

#include "island/island_conversetion.h"


/***********************************************
 *
 * @Name : sendError
 * @Def : Builds an error response with the given reason and sends it.
 * @Arg : In/Out: ptMsg = message used to build the response
 *        In: nFd = socket of the client
 *        In: psReason = reason of the error
 * @Ret : None.
 *
 ************************************************/
static void sendError(Message *ptMsg, int nFd, const char *psReason) {
    buildMessage(ptMsg, FLAG_RESPONSE | FLAG_ERROR, psReason);
    sendMessage(ptMsg, nFd);
}
 
/***********************************************
 *
 * @Name : findClientIndex
 * @Def : Looks for the client that uses a socket.
 * @Arg : In: ptIsland = island with the client list
 *        In: nFd = socket of the client
 * @Ret : Index of the client in the list, or -1 if it isn't there.
 *
 ************************************************/
static int findClientIndex(const Island *ptIsland, int nFd) {
    size_t i = 0;
 
    for (i = 0; i < ptIsland->tNumClients; i++) {
        if (nFd == ptIsland->atClients[i].nFd) {
            return (int)i;
        }
    }
 
    return -1;
}
 
/***********************************************
 *
 * @Name : isClientDocked
 * @Def : Checks if the client that uses a socket is docked.
 * @Arg : In: ptIsland = island with the client list
 *        In: nFd = socket of the client
 * @Ret : 1 if the client is docked, 0 otherwise.
 *
 ************************************************/
static int isClientDocked(const Island *ptIsland, int nFd) {
    int nIndex = findClientIndex(ptIsland, nFd);
 
    return (-1 != nIndex && CLIENT_DOCKED == ptIsland->atClients[nIndex].eState);
}
 
/***********************************************
 *
 * @Name : removeClient
 * @Def : Closes a client's socket, removes it from the select set and,
 *        if it is in the list, frees its name and removes it. The last
 *        client is moved into the freed slot (order doesn't matter).
 * @Arg : In/Out: ptIsland = island with the client list
 *        In: nFd = socket of the client to remove
 * @Ret : None.
 *
 ************************************************/
void removeClient(Island *ptIsland, int nFd) {
    int nIndex = findClientIndex(ptIsland, nFd);
 
    close(nFd);
    FD_CLR(nFd, &ptIsland->tFdsClients);
 
    if (-1 == nIndex) {
        return;
    }
 
    free(ptIsland->atClients[nIndex].psName);
    ptIsland->atClients[nIndex] = ptIsland->atClients[ptIsland->tNumClients - 1];
    ptIsland->tNumClients--;
}
 
/***********************************************
 *
 * @Name : dockClient
 * @Def : Sends DOCKED to a client and marks it as docked.
 * @Arg : In/Out: ptIsland = island with the client list
 *        In: tIndex = index of the client in the list
 * @Ret : 0 if the client was docked, -1 if the message couldn't be sent.
 *
 ************************************************/
static int dockClient(Island *ptIsland, size_t tIndex) {
    Message tMsg = {0};
 
    tMsg.type = MSG_ARRIVAL_ISLAND;
    buildMessage(&tMsg, FLAG_RESPONSE, DOCKED);
    if (0 != sendMessage(&tMsg, ptIsland->atClients[tIndex].nFd)) {
        return -1;
    }
    ptIsland->atClients[tIndex].eState = CLIENT_DOCKED;
    ptIsland->nNumDocked++;
    return 0;
}
 
/***********************************************
 *
 * @Name : queueClient
 * @Def : Puts a client in the waiting queue and sends it WAIT.
 * @Arg : In/Out: ptIsland = island with the client list
 *        In: tIndex = index of the client in the list
 * @Ret : Result of sendMessage (0 if the message was sent).
 *
 ************************************************/
static int queueClient(Island *ptIsland, size_t tIndex) {
    Message tMsg = {0};
 
    ptIsland->atClients[tIndex].eState = CLIENT_WAITING;
    ptIsland->atClients[tIndex].tQueued = ptIsland->tArrivalCounter++;
 
    tMsg.type = MSG_ARRIVAL_ISLAND;
    buildMessage(&tMsg, FLAG_RESPONSE, WAIT);
    return sendMessage(&tMsg, ptIsland->atClients[tIndex].nFd);
}
 
/***********************************************
 *
 * @Name : decideState
 * @Def : Docks a new client if the port has room, otherwise puts it in
 *        the waiting queue.
 * @Arg : In/Out: ptIsland = island with the client list
 *        In: tIndex = index of the client in the list
 * @Ret : None.
 *
 ************************************************/
static void decideState(Island *ptIsland, size_t tIndex) {
    char *psBuffer = NULL;
 
    if (ptIsland->nNumDocked < ptIsland->nCapacity) {
        if (0 == dockClient(ptIsland, tIndex)) {
            if (-1 != asprintf(&psBuffer, MSG_ENTERED_PORT,
                               ptIsland->atClients[tIndex].psName)) {
                println(psBuffer);
                free(psBuffer);
            }
        }
    } else {
        if (0 == queueClient(ptIsland, tIndex)) {
            if (-1 != asprintf(&psBuffer, MSG_PORT_FULL,
                               ptIsland->atClients[tIndex].psName)) {
                println(psBuffer);
                free(psBuffer);
            }
        }
    }
}
 
/***********************************************
 *
 * @Name : arravalIsland
 * @Def : Handles the arrival of an odysseus: validates the message, adds
 *        the client to the list and decides if it docks or waits.
 * @Arg : In/Out: ptIsland = island with the client list
 *        In: tMsg = request received
 *        In: nFdClient = socket of the client
 * @Ret : None.
 *
 ************************************************/
static void arravalIsland(Island *ptIsland, Message tMsg, int nFdClient) {
    Client *atTmp = NULL;
    char *psName = NULL;
    char *psBuffer = NULL;
 
    tMsg.type = MSG_ARRIVAL_ISLAND;
    if (FLAG_NONE != tMsg.flag || 0 == tMsg.dataLength) {
        sendError(&tMsg, nFdClient, ERROR_MESSAGE_FORMAT);
        return;
    }
 
    if (-1 != findClientIndex(ptIsland, nFdClient)) {
        sendError(&tMsg, nFdClient, ERROR_ODYSSEUS_ALREDY_EXIST);
        return;
    }
 
    psName = strdup(tMsg.data);
    if (NULL == psName) {
        sendError(&tMsg, nFdClient, ERROR_MEMORY);
        return;
    }
 
    atTmp = realloc(ptIsland->atClients,
                    sizeof(Client) * (ptIsland->tNumClients + 1));
    if (NULL == atTmp) {
        free(psName);
        sendError(&tMsg, nFdClient, ERROR_MEMORY);
        return;
    }
    ptIsland->atClients = atTmp;
 
    ptIsland->atClients[ptIsland->tNumClients].nFd = nFdClient;
    ptIsland->atClients[ptIsland->tNumClients].psName = psName;
    ptIsland->atClients[ptIsland->tNumClients].eState = CLIENT_NEW;
    ptIsland->atClients[ptIsland->tNumClients].tQueued = 0;
 
    if (-1 != asprintf(&psBuffer, MSG_APPROACHING, psName, ptIsland->psName)) {
        println(psBuffer);
        free(psBuffer);
    }
    ptIsland->tNumClients++;
    decideState(ptIsland, ptIsland->tNumClients - 1);
}
 
/***********************************************
 *
 * @Name : admitNextShips
 * @Def : While the port has room, docks the waiting client that arrived
 *        first. A client that can't be docked is removed.
 * @Arg : In/Out: ptIsland = island with the client list
 * @Ret : None.
 *
 ************************************************/
static void admitNextShips(Island *ptIsland) {
    char *psBuffer = NULL;
    int nBest = -1;
    size_t i = 0;
 
    while (ptIsland->nNumDocked < ptIsland->nCapacity) {
        nBest = -1;
        for (i = 0; i < ptIsland->tNumClients; i++) {
            if (CLIENT_WAITING == ptIsland->atClients[i].eState &&
                (-1 == nBest || ptIsland->atClients[i].tQueued <
                                    ptIsland->atClients[nBest].tQueued)) {
                nBest = (int)i;
            }
        }
        if (-1 == nBest) {
            return;
        }
 
        if (0 == dockClient(ptIsland, (size_t)nBest)) {
            if (-1 != asprintf(&psBuffer, MSG_DOCKED_SENT,
                               ptIsland->atClients[nBest].psName)) {
                println(psBuffer);
                free(psBuffer);
            }
        } else {
            removeClient(ptIsland, ptIsland->atClients[nBest].nFd);
        }
    }
}
 
/***********************************************
 *
 * @Name : listMarket
 * @Def : Sends the market to a docked client, one frame per product, and
 *        a last frame with the map.
 * @Arg : In: ptIsland = island with the stock and the client list
 *        In: tMsg = request received
 *        In: nFd = socket of the client
 * @Ret : None.
 *
 ************************************************/
static void listMarket(Island *ptIsland, Message tMsg, int nFd) {
    char *psBuff = NULL;
    size_t i = 0;
 
    tMsg.type = MSG_LIST_MARKET;
 
    if (0 == isClientDocked(ptIsland, nFd)) {
        sendError(&tMsg, nFd, ERROR_NOT_DOCKED);
        return;
    }
    if (FLAG_NONE != tMsg.flag || 0 != tMsg.dataLength) {
        sendError(&tMsg, nFd, ERROR_MESSAGE_FORMAT);
        return;
    }
 
    for (i = 0; i < ptIsland->tNumStock; i++) {
        psBuff = NULL;
        if (-1 == asprintf(&psBuff, MARKET_LINE_FORMAT, i + 1,
                           ptIsland->tNumStock + 1, ptIsland->atStock[i].sName,
                           ptIsland->atStock[i].nQuantity,
                           ptIsland->atStock[i].nPrice)) {
            return;
        }
 
        buildMessage(&tMsg, FLAG_RESPONSE, psBuff);
        free(psBuff);
 
        if (0 != sendMessage(&tMsg, nFd)) {
            return;
        }
    }
 
    psBuff = NULL;
    if (-1 != asprintf(&psBuff, MARKET_LINE_FORMAT, ptIsland->tNumStock + 1,
                       ptIsland->tNumStock + 1, MAP_PRODUCT, MAP_AMOUNT,
                       MAP_PRICE)) {
        buildMessage(&tMsg, FLAG_RESPONSE, psBuff);
        free(psBuff);
        sendMessage(&tMsg, nFd);
    }
}
 
/***********************************************
 *
 * @Name : parseTradeRequest
 * @Def : Splits a BUY/SELL request in "<product>&<amount>".
 * @Arg : In/Out: psData = data of the request (it is modified by strtok)
 *        In: nLength = length of the data
 *        Out: ppsProduct = name of the product
 *        Out: pnAmount = amount of the product
 * @Ret : 0 if the request is valid, -1 otherwise.
 *
 ************************************************/
static int parseTradeRequest(char *psData, int nLength, char **ppsProduct,
                             int *pnAmount) {
    char *psAmount = NULL;
 
    if (NULL == psData || 0 >= nLength || MAX_TRADE_DATA < nLength) {
        return -1;
    }
    psData[nLength] = '\0';
 
    *ppsProduct = strtok(psData, TRADE_DELIMITER);
    psAmount = strtok(NULL, TRADE_DELIMITER);
 
    if (NULL == *ppsProduct || NULL == psAmount ||
        NULL != strtok(NULL, TRADE_DELIMITER)) {
        return -1;
    }
    if (0 == isPositiveInteger(psAmount)) {
        return -1;
    }
 
    *pnAmount = atoi(psAmount);
 
    return 0;
}
 
/***********************************************
 *
 * @Name : saveStockEntry
 * @Def : Writes one stock entry in its position of the stock file.
 * @Arg : In: nFdStock = stock file descriptor
 *        In: tIndex = position of the entry in the file
 *        In: ptItem = entry to write
 * @Ret : 0 if the entry was written, -1 otherwise.
 *
 ************************************************/
static int saveStockEntry(int nFdStock, size_t tIndex, Stock *ptItem) {
    int nWritten = 0;
 
    if (-1 == lseek(nFdStock, tIndex * sizeof(Stock), SEEK_SET)) {
        return -1;
    }
 
    nWritten = write(nFdStock, ptItem, sizeof(Stock));
    if (-1 == nWritten) {
        return -1;
    }
    if (sizeof(Stock) != nWritten) {
        return -1;
    }
 
    return 0;
}
 
/***********************************************
 *
 * @Name : prepareTrade
 * @Def : Checks that the client is docked, parses the BUY/SELL request
 *        and looks for the product in the stock.
 * @Arg : In: ptIsland = island with the stock and the client list
 *        In/Out: ptMsg = request received (also used for the error)
 *        In: nFd = socket of the client
 *        In: nType = type of the message (MSG_BUY or MSG_SELL)
 *        Out: pnAmount = amount of the product
 *        Out: ptIndex = index of the product in the stock
 * @Ret : 0 if the trade can be done, -1 otherwise.
 *
 ************************************************/
static int prepareTrade(Island *ptIsland, Message *ptMsg, int nFd, char nType,
                        int *pnAmount, size_t *ptIndex) {
    char *psProduct = NULL;
    size_t i = 0;
 
    ptMsg->type = nType;
    if (0 == isClientDocked(ptIsland, nFd)) {
        sendError(ptMsg, nFd, ERROR_NOT_DOCKED);
        return -1;
    }
 
    if (FLAG_NONE != ptMsg->flag ||
        0 != parseTradeRequest(ptMsg->data, ptMsg->dataLength, &psProduct,
                               pnAmount)) {
        sendError(ptMsg, nFd, ERROR_MESSAGE_FORMAT);
        return -1;
    }
 
    for (i = 0; i < ptIsland->tNumStock; i++) {
        if (0 == strcasecmp(psProduct, ptIsland->atStock[i].sName)) {
            break;
        }
    }
 
    if (i == ptIsland->tNumStock) {
        sendError(ptMsg, nFd, ERROR_UNKNOWN_PRODUCT);
        return -1;
    }
 
    *ptIndex = i;
    return 0;
}
 
/***********************************************
 *
 * @Name : completeTrade
 * @Def : Saves the updated stock entry, answers OK to the client and
 *        prints the trade.
 * @Arg : In: ptIsland = island with the stock and the client list
 *        In/Out: ptMsg = message used to build the response
 *        In: nFd = socket of the client
 *        In: tIndex = index of the product in the stock
 *        In: nAmount = amount traded
 *        In: psVerb = "bought" or "sold"
 * @Ret : None.
 *
 ************************************************/
static void completeTrade(Island *ptIsland, Message *ptMsg, int nFd,
                          size_t tIndex, int nAmount, char *psVerb) {
    Stock *ptItem = &ptIsland->atStock[tIndex];
    char *psBuff = NULL;
    int nIndex = 0;
 
    if (0 != saveStockEntry(ptIsland->nFdStock, tIndex, ptItem)) {
        printErr(ERROR_UPDATE_STOCK);
    }
 
    if (-1 == asprintf(&psBuff, TRADE_OK_FORMAT, ptItem->sName, nAmount,
                       nAmount * ptItem->nPrice)) {
        return;
    }
    buildMessage(ptMsg, FLAG_RESPONSE, psBuff);
    free(psBuff);
    sendMessage(ptMsg, nFd);
 
    nIndex = findClientIndex(ptIsland, nFd);
    if (-1 != nIndex &&
        -1 != asprintf(&psBuff, MSG_TRADE_DONE,
                       ptIsland->atClients[nIndex].psName, psVerb, nAmount,
                       ptItem->sName)) {
        println(psBuff);
        free(psBuff);
    }
}
 
/***********************************************
 *
 * @Name : buyProduct
 * @Def : Handles a BUY request: removes the amount from the stock if
 *        there is enough.
 * @Arg : In/Out: ptIsland = island with the stock and the client list
 *        In: tMsg = request received
 *        In: nFd = socket of the client
 * @Ret : None.
 *
 ************************************************/
static void buyProduct(Island *ptIsland, Message tMsg, int nFd) {
    int nAmount = 0;
    size_t i = 0;
 
    if (0 != prepareTrade(ptIsland, &tMsg, nFd, MSG_BUY, &nAmount, &i)) {
        return;
    }
    if (ptIsland->atStock[i].nQuantity < nAmount) {
        sendError(&tMsg, nFd, ERROR_NOT_ENOUGH_STOCK);
        return;
    }
 
    ptIsland->atStock[i].nQuantity -= nAmount;
    completeTrade(ptIsland, &tMsg, nFd, i, nAmount, VERB_BOUGHT);
}
 
/***********************************************
 *
 * @Name : sellProduct
 * @Def : Handles a SELL request: adds the amount to the stock.
 * @Arg : In/Out: ptIsland = island with the stock and the client list
 *        In: tMsg = request received
 *        In: nFd = socket of the client
 * @Ret : None.
 *
 ************************************************/
static void sellProduct(Island *ptIsland, Message tMsg, int nFd) {
    int nAmount = 0;
    size_t i = 0;
 
    if (0 != prepareTrade(ptIsland, &tMsg, nFd, MSG_SELL, &nAmount, &i)) {
        return;
    }
 
    ptIsland->atStock[i].nQuantity += nAmount;
    completeTrade(ptIsland, &tMsg, nFd, i, nAmount, VERB_SOLD);
}
 
/***********************************************
 *
 * @Name : sendMapRoutes
 * @Def : Sends the routes of the island to a client, one frame per route.
 *        If the island has no routes, it sends a single empty frame.
 * @Arg : In: ptIsland = island with the routes
 *        In/Out: ptMsg = message used to build the frames
 *        In: nFd = socket of the client
 * @Ret : 0 if every frame was sent, -1 otherwise.
 *
 ************************************************/
static int sendMapRoutes(Island *ptIsland, Message *ptMsg, int nFd) {
    char *psBuff = NULL;
    size_t i = 0;
 
    if (0 == ptIsland->tNumRoutes) {
        if (-1 == asprintf(&psBuff, MAP_EMPTY_FORMAT, OK_MESSAGE, MAP_PRICE)) {
            return -1;
        }
        buildMessage(ptMsg, FLAG_RESPONSE, psBuff);
        free(psBuff);
        if (0 != sendMessage(ptMsg, nFd)) {
            return -1;
        }
    }
 
    for (i = 0; i < ptIsland->tNumRoutes; i++) {
        psBuff = NULL;
        if (-1 == asprintf(&psBuff, MAP_ROUTE_FORMAT, OK_MESSAGE, MAP_PRICE,
                           i + 1, ptIsland->tNumRoutes,
                           ptIsland->atRoutes[i].psName,
                           ptIsland->atRoutes[i].psIp,
                           ptIsland->atRoutes[i].nPort)) {
            return -1;
        }
        buildMessage(ptMsg, FLAG_RESPONSE, psBuff);
        free(psBuff);
        if (0 != sendMessage(ptMsg, nFd)) {
            return -1;
        }
    }
 
    return 0;
}
 
/***********************************************
 *
 * @Name : buyMap
 * @Def : Answers a BUY MAP request with all the (SPHRAGIS validated)
 *        routes of the island, one frame per route. The map is not in the
 *        stock, so nothing is modified.
 * @Arg : In: ptIsland = island with the routes and the client list
 *        In: tMsg = request received
 *        In: nFd = socket of the client
 * @Ret : None.
 *
 ************************************************/
static void buyMap(Island *ptIsland, Message tMsg, int nFd) {
    char *psBuff = NULL;
    int nIndex = 0;
 
    tMsg.type = MSG_BUY_MAP;
    if (0 == isClientDocked(ptIsland, nFd)) {
        sendError(&tMsg, nFd, ERROR_NOT_DOCKED);
        return;
    }
    if (FLAG_NONE != tMsg.flag || 0 == tMsg.dataLength ||
        0 == isPositiveInteger(tMsg.data) || MAP_AMOUNT != atoi(tMsg.data)) {
        sendError(&tMsg, nFd, ERROR_MESSAGE_FORMAT);
        return;
    }
 
    if (0 != sendMapRoutes(ptIsland, &tMsg, nFd)) {
        return;
    }
 
    nIndex = findClientIndex(ptIsland, nFd);
    if (-1 != nIndex &&
        -1 != asprintf(&psBuff, MSG_BOUGHT_MAP,
                       ptIsland->atClients[nIndex].psName)) {
        println(psBuff);
        free(psBuff);
    }
}
 
/***********************************************
 *
 * @Name : departure
 * @Def : Frees the berth of a docked client that is about to sail,
 *        answers OK and admits the next ship of the queue. The later close
 *        of the socket then finds the client already marked as left.
 * @Arg : In/Out: ptIsland = island with the client list
 *        In: tMsg = request received
 *        In: nFd = socket of the client
 * @Ret : None.
 *
 ************************************************/
static void departure(Island *ptIsland, Message tMsg, int nFd) {
    char *psBuff = NULL;
    int nIndex = findClientIndex(ptIsland, nFd);
 
    tMsg.type = MSG_DEPARTURE_ISLAND;
    if (0 == isClientDocked(ptIsland, nFd)) {
        sendError(&tMsg, nFd, ERROR_NOT_DOCKED);
        return;
    }
    if (FLAG_NONE != tMsg.flag || 0 != tMsg.dataLength) {
        sendError(&tMsg, nFd, ERROR_MESSAGE_FORMAT);
        return;
    }
 
    ptIsland->atClients[nIndex].eState = CLIENT_LEFT;
    ptIsland->nNumDocked--;
    buildMessage(&tMsg, FLAG_RESPONSE, OK_MESSAGE);
    sendMessage(&tMsg, nFd);
 
    if (-1 != asprintf(&psBuff, MSG_HAS_LEFT,
                       ptIsland->atClients[nIndex].psName)) {
        println(psBuff);
        free(psBuff);
    }
    admitNextShips(ptIsland);
}
 
/***********************************************
 *
 * @Name : choseOption
 * @Def : Calls the handler that matches the type of the message.
 * @Arg : In/Out: ptIsland = island that handles the message
 *        In: tMsg = message received
 *        In: nFdClient = socket of the client
 * @Ret : None.
 *
 ************************************************/
void choseOption(Island *ptIsland, Message tMsg, int nFdClient) {
    switch (tMsg.type) {
        case MSG_ARRIVAL_ISLAND:
            // ARRIVAL AT AN ISLAND (RESPONSE)
            arravalIsland(ptIsland, tMsg, nFdClient);
            break;
        case MSG_LIST_MARKET:
            // LIST MARKET (RESPONSE)
            listMarket(ptIsland, tMsg, nFdClient);
            break;
        case MSG_BUY:
            // BUY PRODUCT (RESPONSE)
            buyProduct(ptIsland, tMsg, nFdClient);
            break;
        case MSG_SELL:
            // SELL (RESPONSE)
            sellProduct(ptIsland, tMsg, nFdClient);
            break;
        case MSG_BUY_MAP:
            buyMap(ptIsland, tMsg, nFdClient);
            break;
        case MSG_DELIVER:
            // DELIVER (RESPONSE)
            sendNack(nFdClient, NACK_INVALID_TYPE);
            break;
        case MSG_DEPARTURE_ISLAND:
            departure(ptIsland, tMsg, nFdClient);
            break;
        default:
            sendNack(nFdClient, NACK_INVALID_TYPE);
            break;
    }
}
 
/***********************************************
 *
 * @Name : clientLeaves
 * @Def : Handles a client that disconnected: frees its berth if it was
 *        docked, removes it and admits the next ship of the queue.
 * @Arg : In/Out: ptIsland = island with the client list
 *        In: nFd = socket of the client
 * @Ret : None.
 *
 ************************************************/
void clientLeaves(Island *ptIsland, int nFd) {
    int nIndex = findClientIndex(ptIsland, nFd);
    char *psBuffer = NULL;
 
    if (-1 != nIndex && CLIENT_DOCKED == ptIsland->atClients[nIndex].eState) {
        ptIsland->nNumDocked--;
        if (-1 != asprintf(&psBuffer, MSG_HAS_LEFT,
                           ptIsland->atClients[nIndex].psName)) {
            println(psBuffer);
            free(psBuffer);
        }
    }
 
    removeClient(ptIsland, nFd);
    admitNextShips(ptIsland);
}
 