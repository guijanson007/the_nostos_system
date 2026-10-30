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
        if (nFd == ptIthaca->clients[i].nFd) {
            return (int)i;
        }
    }
    return -1;
}

static void disconectIthaca(int fd_client, Ithaca *ithaca, Message msg) {
    int nRemove = 0;

    msg.type = MSG_DISCONNECT_ITHACA;

    if (msg.flag != FLAG_NONE || msg.dataLength != 0) {
        buildMessage(&msg, FLAG_RESPONSE | FLAG_ERROR, ERROR_MESSAGE_FORMAT);
    } else if (-1 == findClientIndex(ithaca, fd_client)) {
        buildMessage(&msg, FLAG_RESPONSE | FLAG_ERROR, ERROR_DISCONECTING);
    } else {
        buildMessage(&msg, FLAG_RESPONSE, OK_MESSAGE);
        nRemove = 1;
    }

    // Reply before removeClient, which closes the socket
    sendMessage(&msg, fd_client);
    if (nRemove) {
        removeClient(ithaca, fd_client);
    }
}

static void replyError(int fd_client, Message *msg, char *psReason) {
    buildMessage(msg, FLAG_RESPONSE | FLAG_ERROR, psReason);
    sendMessage(msg, fd_client);
}

static void connectIthaca(int fd_client, Ithaca *ithaca, Message msg) {
    Client *tempClients = NULL;
    char *psName = NULL;
    size_t i = 0;

    msg.type = MSG_CONNECT_ITHACA;

    if (msg.flag != FLAG_NONE || msg.dataLength == 0) {
        replyError(fd_client, &msg, ERROR_MESSAGE_FORMAT);
        return;
    }

    if (-1 != findClientIndex(ithaca, fd_client)) {
        replyError(fd_client, &msg, ERROR_CLIENT_ALREADY_CONNECTED);
        return;
    }
    for (i = 0; i < ithaca->numClients; i++) {
        if (strcmp(ithaca->clients[i].psName, msg.data) == 0) {
            replyError(fd_client, &msg, ERROR_CLIENT_ALREADY_CONNECTED);
            return;
        }
    }

    psName = strdup(msg.data);
    tempClients =
        realloc(ithaca->clients, (ithaca->numClients + 1) * sizeof(Client));
    if (NULL == psName || NULL == tempClients) {
        free(psName);
        // A failed realloc leaves the old array valid, so only keep it
        if (NULL != tempClients) {
            ithaca->clients = tempClients;
        }
        replyError(fd_client, &msg, ERROR_MEMORY_ALLOCATION);
        return;
    }

    ithaca->clients = tempClients;
    ithaca->clients[ithaca->numClients].nFd = fd_client;
    ithaca->clients[ithaca->numClients].psName = psName;
    ithaca->numClients++;

    consoleLog(psName, " connected to Ithaca.");
    buildMessage(&msg, FLAG_RESPONSE, OK_MESSAGE);
    sendMessage(&msg, fd_client);
}

static Voyage *findVoyage(Ithaca *ithaca, int nId) {
    size_t i = 0;

    for (i = 0; i < ithaca->tNumVoyages; i++) {
        if (nId == ithaca->atVoyages[i].nId) {
            return &ithaca->atVoyages[i];
        }
    }
    return NULL;
}

static Voyage *findActiveVoyage(Ithaca *ithaca, const char *psName) {
    size_t i = 0;

    for (i = 0; i < ithaca->tNumVoyages; i++) {
        Voyage *ptVoyage = &ithaca->atVoyages[i];
        if (VOYAGE_ASSIGNED == ptVoyage->eState &&
            0 == strcmp(ptVoyage->psOwner, psName)) {
            return ptVoyage;
        }
    }
    return NULL;
}

static void listVoyages(int fd_client, Ithaca *ithaca, Message msg) {
    char voyageInfo[MESSAGE_SIZE];
    const char *psName = NULL;
    Voyage *ptVoyage = NULL;
    size_t i = 0;
    int nTotal = 0;
    int nIndex = 0;
    int nClient = findClientIndex(ithaca, fd_client);

    msg.type = MSG_LIST_VOYAGES;

    if (-1 == nClient) {
        replyError(fd_client, &msg, ERROR_NOT_CONNECTED);
        return;
    }
    if (msg.flag != FLAG_NONE || msg.dataLength != 0) {
        replyError(fd_client, &msg, ERROR_MESSAGE_FORMAT);
        return;
    }
    psName = ithaca->clients[nClient].psName;
    consoleLog(psName, " requested available voyages.");

    // Listing again means the previous mission was lost: it is failed for good
    while (NULL != (ptVoyage = findActiveVoyage(ithaca, psName))) {
        ptVoyage->eState = VOYAGE_FAILED;
    }

    for (i = 0; i < ithaca->tNumVoyages; i++) {
        if (VOYAGE_AVAILABLE == ithaca->atVoyages[i].eState) {
            nTotal++;
        }
    }
    if (0 == nTotal) {
        replyError(fd_client, &msg, ERROR_NO_VOYAGES);
        return;
    }

    for (i = 0; i < ithaca->tNumVoyages; i++) {
        ptVoyage = &ithaca->atVoyages[i];
        if (VOYAGE_AVAILABLE != ptVoyage->eState) {
            continue;
        }
        nIndex++;
        snprintf(voyageInfo, sizeof(voyageInfo), "%d&%d&%d&%s&%s&%d", nIndex,
                 nTotal, ptVoyage->nId, ptVoyage->psObject,
                 ptVoyage->psDestination, ptVoyage->nReward);
        buildMessage(&msg, FLAG_RESPONSE, voyageInfo);
        if (0 != sendMessage(&msg, fd_client)) {
            return;
        }
    }
}

static void acceptVoyage(int fd_client, Ithaca *ithaca, Message msg) {
    char voyageInfo[MESSAGE_SIZE];
    char *psOwner = NULL;
    const char *psName = NULL;
    Voyage *ptVoyage = NULL;
    int nClient = findClientIndex(ithaca, fd_client);

    msg.type = MSG_ACCEPT;

    if (-1 == nClient) {
        replyError(fd_client, &msg, ERROR_NOT_CONNECTED);
        return;
    }
    if (msg.flag != FLAG_NONE || msg.dataLength == 0) {
        replyError(fd_client, &msg, ERROR_MESSAGE_FORMAT);
        return;
    }
    // At most 9 digits so atoi can't overflow
    if (msg.dataLength > 9 || !isPositiveInteger(msg.data)) {
        replyError(fd_client, &msg, ERROR_INVALID_VOYAGE_ID);
        return;
    }
    psName = ithaca->clients[nClient].psName;
    ptVoyage = findVoyage(ithaca, atoi(msg.data));
    if (NULL == ptVoyage) {
        replyError(fd_client, &msg, ERROR_INVALID_VOYAGE_ID);
        return;
    }
    if (NULL != findActiveVoyage(ithaca, psName)) {
        replyError(fd_client, &msg, ERROR_ACTIVE_VOYAGE);
        return;
    }
    // Ithaca handles one request at a time, so this check-and-set is atomic
    if (VOYAGE_AVAILABLE != ptVoyage->eState) {
        replyError(fd_client, &msg, ERROR_VOYAGE_NOT_AVAILABLE);
        return;
    }
    psOwner = strdup(psName);
    if (NULL == psOwner) {
        replyError(fd_client, &msg, ERROR_MEMORY_ALLOCATION);
        return;
    }

    free(ptVoyage->psOwner);
    ptVoyage->psOwner = psOwner;
    ptVoyage->eState = VOYAGE_ASSIGNED;

    snprintf(voyageInfo, sizeof(voyageInfo), "OK&%d&%s&%s&%d", ptVoyage->nId,
             ptVoyage->psObject, ptVoyage->psDestination, ptVoyage->nReward);
    buildMessage(&msg, FLAG_RESPONSE, voyageInfo);
    sendMessage(&msg, fd_client);

    snprintf(voyageInfo, sizeof(voyageInfo), ">>> Voyage %d assigned to %s.",
             ptVoyage->nId, psName);
    println(voyageInfo);
}

void consoleLog(const char *sOdysseusName, const char *sPostFix) {
    print(">>> ");
    print(sOdysseusName);
    println(sPostFix);
}

int removeClient(Ithaca *ptIthaca, int nFd) {
    int nIndex = findClientIndex(ptIthaca, nFd);
    if (nIndex == -1)
        return -1;

    close(nFd);
    FD_CLR(nFd, &ptIthaca->fds_clients);
    free(ptIthaca->clients[nIndex].psName);

    ptIthaca->clients[nIndex] = ptIthaca->clients[ptIthaca->numClients - 1];
    ptIthaca->numClients--;

    return 0;
}

void choseOption(Ithaca *ithaca, Message msg, int fd_client) {
    switch (msg.type) {
        case MSG_CONNECT_ITHACA:
            // Conct Ithaca (Request)
            connectIthaca(fd_client, ithaca, msg);
            break;
        case MSG_LIST_VOYAGES:
            // LIST VOYAGES (REQUEST)
            listVoyages(fd_client, ithaca, msg);
            break;
        case MSG_ACCEPT:
            // ACCEPT (REQUEST)
            acceptVoyage(fd_client, ithaca, msg);
            break;
        case MSG_CLAIM:
            // CLAIM (REQUEST)
            sendNack(fd_client, NACK_INVALID_TYPE)
            break;
        case MSG_DISCONNECT_ITHACA:
            // DISCONNECT FROM ITHACA (REQUEST)
            disconectIthaca(fd_client, ithaca, msg);
            break;
        default:
            sendNack(fd_client, NACK_INVALID_TYPE)
            break;
    }
}
