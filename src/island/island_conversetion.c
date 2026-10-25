
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

static void decideState(Island *island, Message *msg, int fd) {
    char *buffer;

    if (island->nCapacity > island->numDocked) {
        island->client[island->numClients].state = CLIENT_DOCKED;
        island->numDocked++;
        buildMessage(msg, FLAG_RESPONSE, DOCKED);
        asprintf(&buffer, ">>> %s entered the port.",
                 island->client[island->numClients].psName);
    } else {
        island->client[island->numClients].state = CLIENT_WAITING;
        island->client[island->numClients].nQueued = island->nArrivalCounter++;
        buildMessage(msg, FLAG_RESPONSE, WAIT);
        asprintf(&buffer, "Port full. %s waiting offshore in the FIFO queue.",
                 island->client[island->numClients].psName);
    }
    sendMessage(msg, fd);
    println(buffer);
    free(buffer);
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
    decideState(island, &msg, fd_client);

    island->numClients++;
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
    if (-1 != asprintf(&buff, "%zu&%zu&MAP&1&50", tNumStock + 1, tNumStock + 1)) {
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