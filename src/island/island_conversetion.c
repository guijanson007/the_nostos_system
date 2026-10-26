
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
        buildMessage(&msg, FLAG_RESPONSE | FLAG_ERROR, ERROR_MESSAGE_FORMAT);
        sendMessage(&msg, fd_client);
        return;
    }

    if (findClientIndex(island, fd_client) != -1) {
        buildMessage(&msg, FLAG_RESPONSE | FLAG_ERROR, ERROR_ODYSSEUS_ALREDY_EXIST);
        sendMessage(&msg, fd_client);
        return;
    }

    psName = strdup(msg.data);
    if (NULL == psName) {
        buildMessage(&msg, FLAG_RESPONSE | FLAG_ERROR, ERROR_MEMORY);
        sendMessage(&msg, fd_client);
        return;
    }

    atTmp = realloc(island->client, sizeof(Client) * (island->numClients + 1));
    if (NULL == atTmp) {
        free(psName);
        buildMessage(&msg, FLAG_RESPONSE | FLAG_ERROR, ERROR_MEMORY);
        sendMessage(&msg, fd_client);
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

static void listMarket(Stock *atStock, size_t tNumStock, Message tMsg,
                       int nFd_client) {
    tMsg.type = MSG_LIST_MARKET;
    char *buff;

    if (tMsg.flag != FLAG_NONE || tMsg.dataLength != 0) {
        buildMessage(&tMsg, FLAG_RESPONSE | FLAG_ERROR, ERROR_MESSAGE_FORMAT);
        sendMessage(&tMsg, nFd_client);
        return;
    }

    for (size_t i = 0; i < tNumStock; i++) {
        buff = NULL;
        if (-1 == asprintf(&buff, "%zu&%zu&%s&%d&%d", i + 1, tNumStock + 1,
                           atStock[i].sName, atStock[i].nQuantity,
                           atStock[i].nPrice)) {
            return;
        }

        buildMessage(&tMsg, FLAG_RESPONSE, buff);
        free(buff);

        if (0 != sendMessage(&tMsg, nFd_client)) {
            return;
        }
    }

    buff = NULL;
    if (-1 !=
        asprintf(&buff, "%zu&%zu&MAP&1&50", tNumStock + 1, tNumStock + 1)) {
        buildMessage(&tMsg, FLAG_RESPONSE, buff);
        free(buff);
        sendMessage(&tMsg, nFd_client);
    }
}

void choseOption(Island *island, Message msg, int fd_client) {
    switch (msg.type) {
        case MSG_ARRIVAL_ISLAND:
            // ARRIVAL AT AN ISLAND (RESPONSE)
            arravalIsland(island, msg, fd_client);
            break;
        case MSG_LIST_MARKET:
            // LIST MARKET (RESPONSE)
            listMarket(island->atStock, island->tNumStock, msg, fd_client);
            break;
        case MSG_BUY:
            // BUY (RESPONSE)
            break;
        case MSG_SELL:
            // SELL (RESPONSE)
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