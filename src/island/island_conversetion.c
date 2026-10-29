
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

    for (i = 0; i < ptIsland->numClients; i++) {
        if (nFd == ptIsland->client[i].nFd) {
            return (int)i;
        }
    }

    return -1;
}


static int isClientDocked(const Island *ptIsland, int nFd) {
    int nIndex = findClientIndex(ptIsland, nFd);

    return (-1 != nIndex && CLIENT_DOCKED == ptIsland->client[nIndex].state);
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
    FD_CLR(nFd, &ptIsland->fds_clients);

    if (-1 == nIndex) {
        return;
    }

    free(ptIsland->client[nIndex].psName);
    ptIsland->client[nIndex] = ptIsland->client[ptIsland->numClients - 1];
    ptIsland->numClients--;
}

static int dockClient(Island *ptIsland, size_t tIndex) {
    Message tMsg = {0};

    tMsg.type = MSG_ARRIVAL_ISLAND;
    buildMessage(&tMsg, FLAG_RESPONSE, DOCKED);
    if (0 != sendMessage(&tMsg, ptIsland->client[tIndex].nFd)) {
        return -1;
    }
    ptIsland->client[tIndex].state = CLIENT_DOCKED;
    ptIsland->numDocked++;
    return 0;
}

static int queueClient(Island *ptIsland, size_t tIndex) {
    Message tMsg = {0};

    ptIsland->client[tIndex].state = CLIENT_WAITING;
    ptIsland->client[tIndex].nQueued = ptIsland->nArrivalCounter++;

    tMsg.type = MSG_ARRIVAL_ISLAND;
    buildMessage(&tMsg, FLAG_RESPONSE, WAIT);
    return sendMessage(&tMsg, ptIsland->client[tIndex].nFd);
}

static void decideState(Island *ptIsland, size_t tIndex) {
    char *psBuffer = NULL;

    if (ptIsland->numDocked < ptIsland->nCapacity) {
        if (0 == dockClient(ptIsland, tIndex)) {
            if (-1 != asprintf(&psBuffer, ">>> %s entered the port.",
                               ptIsland->client[tIndex].psName)) {
                println(psBuffer);
                free(psBuffer);
            }
        }
    } else {
        if (0 == queueClient(ptIsland, tIndex)) {
            if (-1 !=
                asprintf(
                    &psBuffer,
                    ">>> Port full. %s waiting offshore in the FIFO queue.",
                    ptIsland->client[tIndex].psName)) {
                println(psBuffer);
                free(psBuffer);
            }
        }
    }
}

static void arravalIsland(Island *island, Message msg, int fd_client) {
    Client *atTmp = NULL;
    char *psName = NULL;
    char *buffer;

    msg.type = MSG_ARRIVAL_ISLAND;
    if (msg.flag != FLAG_NONE || msg.dataLength == 0) {
        sendError(&msg, fd_client, ERROR_MESSAGE_FORMAT);
        return;
    }

    if (findClientIndex(island, fd_client) != -1) {
        sendError(&msg, fd_client, ERROR_ODYSSEUS_ALREDY_EXIST);
        return;
    }

    psName = strdup(msg.data);
    if (NULL == psName) {
        sendError(&msg, fd_client, ERROR_MEMORY);
        return;
    }

    atTmp = realloc(island->client, sizeof(Client) * (island->numClients + 1));
    if (NULL == atTmp) {
        free(psName);
        sendError(&msg, fd_client, ERROR_MEMORY);
        return;
    }
    island->client = atTmp;

    island->client[island->numClients].nFd = fd_client;
    island->client[island->numClients].psName = psName;
    island->client[island->numClients].state = CLIENT_NEW;
    island->client[island->numClients].nQueued = 0;

    asprintf(&buffer, ">>> %s approaching %s.", psName, island->psName);
    println(buffer);
    free(buffer);
    island->numClients++;
    decideState(island, island->numClients - 1);
}

static void admitNextShips(Island *ptIsland) {
    char *psBuffer = NULL;
    int nBest = -1;

    while (ptIsland->numDocked < ptIsland->nCapacity) {
        nBest = -1;
        for (size_t i = 0; i < ptIsland->numClients; i++) {
            if (CLIENT_WAITING == ptIsland->client[i].state && (-1 == nBest || ptIsland->client[i].nQueued < ptIsland->client[nBest].nQueued)) {
                nBest = (int)i;
            }
        }
        if (-1 == nBest) return;

        if (0 == dockClient(ptIsland, (size_t)nBest)) {
            if (-1 != asprintf(&psBuffer, ">>> DOCKED sent to %s; he may enter the port.", ptIsland->client[nBest].psName)) {
                println(psBuffer);
                free(psBuffer);
            }
        } else {
            removeClient(ptIsland, ptIsland->client[nBest].nFd);
        }
    }
}

static void listMarket(Island *ptIsland, Message tMsg, int nFd) {
    char *buff = NULL;
    tMsg.type = MSG_LIST_MARKET;

    if (!isClientDocked(ptIsland, nFd)) {
        sendError(&tMsg, nFd, "NOT_DOCKED");
        return;
    }
    if (FLAG_NONE != tMsg.flag || 0 != tMsg.dataLength) {
        sendError(&tMsg, nFd, ERROR_MESSAGE_FORMAT);
        return;
    }

    for (size_t i = 0; i < ptIsland->tNumStock; i++) {
        buff = NULL;
        if (-1 == asprintf(&buff, "%zu&%zu&%s&%d&%d", i + 1, ptIsland->tNumStock + 1,ptIsland->atStock[i].sName, ptIsland->atStock[i].nQuantity, ptIsland->atStock[i].nPrice)) {
            return;
        }

        buildMessage(&tMsg, FLAG_RESPONSE, buff);
        free(buff);

        if (0 != sendMessage(&tMsg, nFd)) {
            return;
        }
    }

    buff = NULL;
    if (-1 !=
        asprintf(&buff, "%zu&%zu&MAP&1&50", ptIsland->tNumStock + 1, ptIsland->tNumStock + 1)) {
        buildMessage(&tMsg, FLAG_RESPONSE, buff);
        free(buff);
        sendMessage(&tMsg, nFd);
    }
}

static int parseTradeRequest(char *psData, int nLength, char **psProduct,
                             int *pnAmount) {
    char *psAmount = NULL;

    if (NULL == psData || nLength <= 0 || nLength > 253) {
        return -1;
    }
    psData[nLength] = '\0';

    *psProduct = strtok(psData, "&");
    psAmount = strtok(NULL, "&");

 
    if (NULL == *psProduct || NULL == psAmount || NULL != strtok(NULL, "&")) {
        return -1;
    }
    if (!isPositiveInteger(psAmount)) {
        return -1;
    }

    *pnAmount = atoi(psAmount);

    return 0;
}

static int saveStockEntry(int nFdStock, size_t tIndex, Stock *ptItem) {
    int nWritten = 0;

    if (-1 == lseek(nFdStock, tIndex * sizeof(Stock), SEEK_SET)) {
        return -1;
    }

    nWritten = write(nFdStock, ptItem, sizeof(Stock));
    if (-1 == nWritten) {
        return -1;
    }
    if (nWritten != sizeof(Stock)) {
        return -1;
    }

    return 0;
}

static int prepareTrade(Island *ptIsland, Message *ptMsg, int nFd, char nType, int *pnAmount, size_t *ptIndex) {
    char *psProduct = NULL;
    size_t i = 0;

    ptMsg->type = nType;
    if (!isClientDocked(ptIsland, nFd)) {
        sendError(ptMsg, nFd, "NOT_DOCKED");
        return -1;
    }

    if (FLAG_NONE != ptMsg->flag ||
        0 != parseTradeRequest(ptMsg->data, ptMsg->dataLength, &psProduct, pnAmount)) {
        sendError(ptMsg, nFd, ERROR_MESSAGE_FORMAT);
        return -1;
    }

    for (i = 0; i < ptIsland->tNumStock; i++) {
        if (0 == strcasecmp(psProduct, ptIsland->atStock[i].sName)) {
            break;
        }
    }

    if (i == ptIsland->tNumStock) {
        sendError(ptMsg, nFd, "UNKNOWN_PRODUCT");
        return -1;
    }

    *ptIndex = i;
    return 0;
}

static void completeTrade(Island *ptIsland, Message *ptMsg, int nFd, size_t tIndex, int nAmount, char *psVerb) {
    Stock *ptItem = &ptIsland->atStock[tIndex];
    char *psBuff = NULL;
    int nIndex = 0;

    if (0 != saveStockEntry(ptIsland->nFdStock, tIndex, ptItem)) {
        printErr("ERROR: could not update the stock file");
    }

    if (-1 == asprintf(&psBuff, "OK&%s&%d&%d", ptItem->sName, nAmount,
                       nAmount * ptItem->nPrice)) {
        return;
    }
    buildMessage(ptMsg, FLAG_RESPONSE, psBuff);
    free(psBuff);
    sendMessage(ptMsg, nFd);

    nIndex = findClientIndex(ptIsland, nFd);
    if (-1 != nIndex &&
        -1 != asprintf(&psBuff, ">>> %s %s %d kg of %s.",
                       ptIsland->client[nIndex].psName, psVerb, nAmount,
                       ptItem->sName)) {
        println(psBuff);
        free(psBuff);
    }
}

static void buyProduct(Island *ptIsland, Message tMsg, int nFd) {
    int nAmount = 0;
    size_t i = 0;

    if (0 != prepareTrade(ptIsland, &tMsg, nFd, MSG_BUY, &nAmount, &i)) {
        return;
    }
    if (ptIsland->atStock[i].nQuantity < nAmount) {
        sendError(&tMsg, nFd, "NOT_ENOUGH_STOCK");
        return;
    }

    ptIsland->atStock[i].nQuantity -= nAmount;
    completeTrade(ptIsland, &tMsg, nFd, i, nAmount, "bought");
}

static void sellProduct(Island *ptIsland, Message tMsg, int nFd) {
    int nAmount = 0;
    size_t i = 0;

    if (0 != prepareTrade(ptIsland, &tMsg, nFd, MSG_SELL, &nAmount, &i)) {
        return;
    }

    ptIsland->atStock[i].nQuantity += nAmount;
    completeTrade(ptIsland, &tMsg, nFd, i, nAmount, "sold");
}

void choseOption(Island *island, Message msg, int fd_client) {
    switch (msg.type) {
        case MSG_ARRIVAL_ISLAND:
            // ARRIVAL AT AN ISLAND (RESPONSE)
            arravalIsland(island, msg, fd_client);
            break;
        case MSG_LIST_MARKET:
            // LIST MARKET (RESPONSE)
            listMarket(island, msg, fd_client);
            break;
        case MSG_BUY:
            // BUY PRODUCT (RESPONSE)
            buyProduct(island, msg, fd_client);
            break;
        case MSG_SELL:
            // SELL (RESPONSE)
            sellProduct(island, msg, fd_client);
            break;
        case MSG_BUY_MAP:
            // BUY MAP (RESPONSE)
            break;
        case MSG_DELIVER:
            // DELIVER (RESPONSE)
            break;
        case MSG_DEPARTURE_ISLAND:
            // DEPARTURE FROM AN ISLAND (RESPONSE)
            break;
        default:
            break;
    }
}

void clientLeaves(Island *ptIsland, int nFd) {
    int nIndex = findClientIndex(ptIsland, nFd);
    char *psBuffer = NULL;

    if (-1 != nIndex && CLIENT_DOCKED == ptIsland->client[nIndex].state) {
        ptIsland->numDocked--;
        if (-1 != asprintf(&psBuffer, ">>> %s has left.",
                           ptIsland->client[nIndex].psName)) {
            println(psBuffer);
            free(psBuffer);
        }
    }

    removeClient(ptIsland, nFd);
    admitNextShips(ptIsland);
}