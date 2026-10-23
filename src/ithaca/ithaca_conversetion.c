/***********************************************
 *
 * @File : ithaca_conversetion.c
 * @Purpose : manage the logic between Ithaca and odysseus
 * @Author : Axel Boleda i Callizo
 * @Date : 23/10/2026
 *
 ************************************************/

#define _GNU_SOURCE
#include "ithaca/ithaca_conversetion.h"

static int findClientIndex(Ithaca *ptIthaca, int nFd) {
    size_t i = 0;

    for (i = 0; i < ptIthaca->numClients; i++) {
        if (nFd == ptIthaca->client[i].nFd) {
            return (int)i;
        }
    }
    return -1;
}

static int removeClient (Ithaca *ptIthaca, int nFd) {
    int nIndex = findClientIndex(ptIthaca, nFd);
    if (nIndex == -1) return -1;

    close(nFd);
    FD_CLR(nFd, &ptIthaca->fds_clients);
    free(ptIthaca->client[nIndex].psName);

    ptIthaca->client[nIndex] = ptIthaca->client[ptIthaca->numClients - 1];
    ptIthaca->numClients--;

    return 0;
}

static void disconectIthaca (int fd_client, Ithaca *ithaca, Message msg) {
    msg.type = MSG_DISCONNECT_ITHACA;
    
    if (msg.flag != FLAG_NONE || msg.dataLength != 0) {
        buildMessage(&msg, FLAG_RESPONSE | FLAG_ERROR, ERROR_MESSAGE_FORMAT);
    }
    else {
        buildMessage(&msg, FLAG_RESPONSE, OK_MESSAGE);
        if (0 != removeClient(ithaca, fd_client)) {
            buildMessage(&msg, FLAG_RESPONSE | FLAG_ERROR, ERROR_DISCONECTING);
        }
    }
    sendMessage(&msg, fd_client);
}

void choseOption(Ithaca *ithaca, Message msg, int fd_client) {
    switch (msg.type) {
        case MSG_CONNECT_ITHACA:
            //Conct Ithaca (Request)
            break;
        case MSG_LIST_VOYAGES:
            // LIST VOYAGES (REQUEST)
            break;
        case MSG_ACCEPT:
            //ACCEPT (REQUEST)
            break;
        case MSG_CLAIM:
            //CLAIM (REQUEST)
            break;
        case MSG_DISCONNECT_ITHACA:
            //DISCONNECT FROM ITHACA (REQUEST)
            disconectIthaca(fd_client, ithaca, msg);
            break;
        default:
            msg.flag = FLAG_ERROR;
            sendMessage(&msg, fd_client);
            break;
    }
}