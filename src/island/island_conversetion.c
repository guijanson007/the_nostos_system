
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

void decideState(Island *island, Message *msg, int fd) {
    if (island->nCapacity > island->numDocked) {
        island->client[island->numClients].state = CLIENT_DOCKED;
        island->numDocked++;
        buildMessage(msg, FLAG_RESPONSE, DOCKED);
    }
    else {
        island->client[island->numClients].state = CLIENT_WAITING;
        island->client[island->numClients].nQueued = island->nArrivalCounter++;
        buildMessage(msg, FLAG_RESPONSE, WAIT);
    }
    sendMessage(msg, fd);
}

void arravalIsland(Island *island, Message msg, int fd_client) {
    Client *atTmp = NULL;
    char *psName = NULL;

    msg.type = MSG_ARRIVAL_ISLAND;
    if (msg.flag != FLAG_NONE || msg.dataLength == 0) {
        buildMessage(&msg, FLAG_RESPONSE | FLAG_ERROR, ERROR_MESSAGE_FORMAT);
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
    decideState(island, &msg, fd_client);

    island->numClients++;
}

void choseOption(Island *island, Message msg, int fd_client) {
    switch (msg.type)
    {
    case MSG_ARRIVAL_ISLAND:
        //ARRIVAL AT AN ISLAND (RESPONSE)
        arravalIsland(island, msg, fd_client);
        break;
    case MSG_LIST_MARKET:
        //LIST MARKET (RESPONSE)
        break;
    case MSG_BUY:
        //BUY (RESPONSE)
        break;
    case MSG_SELL:
        //SELL (RESPONSE)
        break;
    case MSG_BUY_MAP:
        //BUY MAP (RESPONSE)
        break;
    case MSG_DELIVER:
        //DELIVER (RESPONSE)
        break;
    case MSG_DEPARTURE_ISLAND:
        //DEPARTURE FROM AN ISLAND (RESPONSE)
        break;
    default:
        break;
    }
}