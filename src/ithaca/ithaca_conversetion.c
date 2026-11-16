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

/***********************************************
 *
 * @Name : findClientIndex
 * @Def : Looks for the client that uses a socket.
 * @Arg : In: ptIthaca = ithaca with the client list
 *        In: nFd = socket of the client
 * @Ret : Index of the client in the list, or -1 if it isn't there.
 *
 ************************************************/
static int findClientIndex(Ithaca *ptIthaca, int nFd) {
    size_t i = 0;

    for (i = 0; i < ptIthaca->tNumClients; i++) {
        if (nFd == ptIthaca->atClients[i].nFd) {
            return (int)i;
        }
    }
    return -1;
}

/***********************************************
 *
 * @Name : disconectIthaca
 * @Def : Handles a DISCONNECT request: answers the client and, if it was
 *        connected, removes it.
 * @Arg : In: nFdClient = socket of the client
 *        In/Out: ptIthaca = ithaca with the client list
 *        In: tMsg = request received
 * @Ret : None.
 *
 ************************************************/
static void disconectIthaca(int nFdClient, Ithaca *ptIthaca, Message tMsg) {
    int nIndex = findClientIndex(ptIthaca, nFdClient);

    tMsg.type = MSG_DISCONNECT_ITHACA;

    if (FLAG_NONE != tMsg.flag || 0 != tMsg.dataLength) {
        buildMessage(&tMsg, FLAG_RESPONSE | FLAG_ERROR, ERROR_MESSAGE_FORMAT);
        sendMessage(&tMsg, nFdClient);
    } else if (-1 == findClientIndex(ptIthaca, nFdClient)) {
        buildMessage(&tMsg, FLAG_RESPONSE | FLAG_ERROR, ERROR_DISCONECTING);
        sendMessage(&tMsg, nFdClient);
    } else {
        buildMessage(&tMsg, FLAG_RESPONSE, OK_MESSAGE);
        sendMessage(&tMsg, nFdClient);
        consoleLog(ptIthaca->atClients[nIndex].psName, LOG_LEFT_ITHACA);
        removeClient(ptIthaca, nFdClient);
    }
}

static Voyage *findVoyage(Ithaca *ptIthaca, int nId);
static Voyage *findActiveVoyage(Ithaca *ptIthaca, const char *psName);
static void replyError(int nFdClient, Message *ptMsg, char *psReason);

static void finishClaim(Ithaca *ptIthaca, int nClient, PoolJob *ptJob) {
    Message tMsg;
    Voyage *ptVoyage = NULL;
    char *psText = NULL;
    char *psName = ptIthaca->atClients[nClient].psName;

    tMsg.type = MSG_CLAIM;

    ptVoyage = findActiveVoyage(ptIthaca, psName);
    if (NULL == ptVoyage) {
        replyError(ptJob->nFd, &tMsg, ERROR_CLAIM_NO_VOYAGE);
        return;
    }

    switch (ptJob->nResult) {
        case TRANSFER_OK:
            ptVoyage->eState = VOYAGE_COMPLETED;

            if (-1 == asprintf(&psText, CLAIM_OK_FORMAT, ptVoyage->nReward)) {
                replyError(ptJob->nFd, &tMsg, ERROR_MEMORY_ALLOCATION);
                return;
            }
            buildMessage(&tMsg, FLAG_RESPONSE, psText);
            free(psText);
            psText = NULL;
            sendMessage(&tMsg, ptJob->nFd);

            if (-1 != asprintf(&psText, MSG_VOYAGE_COMPLETED, ptVoyage->nId,
                               psName, ptVoyage->nReward)) {
                println(psText);
                free(psText);
            }
            break;
        case TRANSFER_PEER_GONE:
            replyError(ptJob->nFd, &tMsg, ERROR_CLAIM_PEER_GONE);
            break;
        case TRANSFER_WRONG_FILE:
            replyError(ptJob->nFd, &tMsg, ERROR_CLAIM_WRONG_FILE);
            break;
        case TRANSFER_SIZE_ERROR:
            replyError(ptJob->nFd, &tMsg, ERROR_CLAIM_SIZE_ERROR);
            break;
        case TRANSFER_MD5_ERROR:
            replyError(ptJob->nFd, &tMsg, ERROR_CLAIM_MD5_ERROR);
            break;
        default:
            replyError(ptJob->nFd, &tMsg, ERROR_CLAIM_FAILED);
            break;
    }    
}

void finishJob(Ithaca *ptIthaca, PoolJob *ptJob) {
    int nClient = findClientIndex(ptIthaca, ptJob->nFd);

    if (-1 != nClient) {
        ptIthaca->atClients[nClient].ptJob = NULL;
    }

    if (TRANSFER_PEER_GONE == ptJob->nResult || -1 == nClient) {
        if (-1 != nClient) {
            consoleLog(ptIthaca->atClients[nClient].psName, LOG_LEFT_ITHACA);
            removeClient(ptIthaca, ptJob->nFd);
        }
    } else {
        FD_SET(ptJob->nFd, &ptIthaca->tFdsClients);
        if (JOB_VERIFY_CLAIM == ptJob->eType) {
            finishClaim(ptIthaca, nClient, ptJob);
        }
    }
    freeJob(ptJob);
}

/***********************************************
 *
 * @Name : replyError
 * @Def : Builds an error response with the given reason and sends it.
 * @Arg : In: nFdClient = socket of the client
 *        In/Out: ptMsg = message used to build the response
 *        In: psReason = reason of the error
 * @Ret : None.
 *
 ************************************************/
static void replyError(int nFdClient, Message *ptMsg, char *psReason) {
    buildMessage(ptMsg, FLAG_RESPONSE | FLAG_ERROR, psReason);
    sendMessage(ptMsg, nFdClient);
}

/***********************************************
 *
 * @Name : connectIthaca
 * @Def : Handles a CONNECT request: checks that the client isn't already
 *        connected (by socket or by name) and adds it to the list.
 * @Arg : In: nFdClient = socket of the client
 *        In/Out: ptIthaca = ithaca with the client list
 *        In: tMsg = request received
 * @Ret : None.
 *
 ************************************************/
static void connectIthaca(int nFdClient, Ithaca *ptIthaca, Message tMsg) {
    Client *atTempClients = NULL;
    char *psName = NULL;
    size_t i = 0;

    tMsg.type = MSG_CONNECT_ITHACA;

    if (FLAG_NONE != tMsg.flag || 0 == tMsg.dataLength) {
        replyError(nFdClient, &tMsg, ERROR_MESSAGE_FORMAT);
        return;
    }

    if (-1 != findClientIndex(ptIthaca, nFdClient)) {
        replyError(nFdClient, &tMsg, ERROR_CLIENT_ALREADY_CONNECTED);
        return;
    }
    for (i = 0; i < ptIthaca->tNumClients; i++) {
        if (0 == strcmp(ptIthaca->atClients[i].psName, tMsg.data)) {
            replyError(nFdClient, &tMsg, ERROR_CLIENT_ALREADY_CONNECTED);
            return;
        }
    }

    psName = strdup(tMsg.data);
    atTempClients = realloc(ptIthaca->atClients,
                            (ptIthaca->tNumClients + 1) * sizeof(Client));
    if (NULL == psName || NULL == atTempClients) {
        free(psName);
        // A failed realloc leaves the old array valid, so only keep it
        if (NULL != atTempClients) {
            ptIthaca->atClients = atTempClients;
        }
        replyError(nFdClient, &tMsg, ERROR_MEMORY_ALLOCATION);
        return;
    }

    ptIthaca->atClients = atTempClients;
    ptIthaca->atClients[ptIthaca->tNumClients].nFd = nFdClient;
    ptIthaca->atClients[ptIthaca->tNumClients].psName = psName;
    ptIthaca->atClients[ptIthaca->tNumClients].ptJob = NULL;
    ptIthaca->atClients[ptIthaca->tNumClients].nClaimPending = 0;
    ptIthaca->tNumClients++;

    consoleLog(psName, LOG_CONNECTED_ITHACA);
    buildMessage(&tMsg, FLAG_RESPONSE, OK_MESSAGE);
    sendMessage(&tMsg, nFdClient);
}

/***********************************************
 *
 * @Name : findVoyage
 * @Def : Looks for the voyage with the given id.
 * @Arg : In: ptIthaca = ithaca with the voyages
 *        In: nId = id of the voyage
 * @Ret : Pointer to the voyage, or NULL if it doesn't exist.
 *
 ************************************************/
static Voyage *findVoyage(Ithaca *ptIthaca, int nId) {
    size_t i = 0;

    for (i = 0; i < ptIthaca->tNumVoyages; i++) {
        if (nId == ptIthaca->atVoyages[i].nId) {
            return &ptIthaca->atVoyages[i];
        }
    }
    return NULL;
}

/***********************************************
 *
 * @Name : findActiveVoyage
 * @Def : Looks for the voyage currently assigned to an odysseus.
 * @Arg : In: ptIthaca = ithaca with the voyages
 *        In: psName = name of the odysseus
 * @Ret : Pointer to the voyage, or NULL if it has no active voyage.
 *
 ************************************************/
static Voyage *findActiveVoyage(Ithaca *ptIthaca, const char *psName) {
    Voyage *ptVoyage = NULL;
    size_t i = 0;

    for (i = 0; i < ptIthaca->tNumVoyages; i++) {
        ptVoyage = &ptIthaca->atVoyages[i];
        if (VOYAGE_ASSIGNED == ptVoyage->eState &&
            0 == strcmp(ptVoyage->psOwner, psName)) {
            return ptVoyage;
        }
    }
    return NULL;
}

/***********************************************
 *
 * @Name : countAvailableVoyages
 * @Def : Counts the voyages that are available.
 * @Arg : In: ptIthaca = ithaca with the voyages
 * @Ret : Number of available voyages.
 *
 ************************************************/
static int countAvailableVoyages(Ithaca *ptIthaca) {
    int nTotal = 0;
    size_t i = 0;

    for (i = 0; i < ptIthaca->tNumVoyages; i++) {
        if (VOYAGE_AVAILABLE == ptIthaca->atVoyages[i].eState) {
            nTotal++;
        }
    }
    return nTotal;
}

/***********************************************
 *
 * @Name : sendAvailableVoyages
 * @Def : Sends the available voyages to a client, one frame per voyage.
 * @Arg : In: ptIthaca = ithaca with the voyages
 *        In/Out: ptMsg = message used to build the frames
 *        In: nFdClient = socket of the client
 *        In: nTotal = number of available voyages
 * @Ret : None.
 *
 ************************************************/
static void sendAvailableVoyages(Ithaca *ptIthaca, Message *ptMsg,
                                 int nFdClient, int nTotal) {
    char *psVoyageInfo = NULL;
    Voyage *ptVoyage = NULL;
    int nIndex = 0;
    size_t i = 0;

    for (i = 0; i < ptIthaca->tNumVoyages; i++) {
        ptVoyage = &ptIthaca->atVoyages[i];
        if (VOYAGE_AVAILABLE != ptVoyage->eState) {
            continue;
        }
        nIndex++;
        if (-1 == asprintf(&psVoyageInfo, VOYAGE_LINE_FORMAT, nIndex, nTotal,
                           ptVoyage->nId, ptVoyage->psObject,
                           ptVoyage->psDestination, ptVoyage->nReward)) {
            return;
        }
        buildMessage(ptMsg, FLAG_RESPONSE, psVoyageInfo);
        free(psVoyageInfo);
        psVoyageInfo = NULL;
        if (0 != sendMessage(ptMsg, nFdClient)) {
            return;
        }
    }
}

/***********************************************
 *
 * @Name : listVoyages
 * @Def : Handles a LIST VOYAGES request. The previous active voyage of the
 *        client is marked as failed and the available voyages are sent.
 * @Arg : In: nFdClient = socket of the client
 *        In/Out: ptIthaca = ithaca with the clients and the voyages
 *        In: tMsg = request received
 * @Ret : None.
 *
 ************************************************/
static void listVoyages(int nFdClient, Ithaca *ptIthaca, Message tMsg) {
    const char *psName = NULL;
    Voyage *ptVoyage = NULL;
    int nTotal = 0;
    int nClient = findClientIndex(ptIthaca, nFdClient);

    tMsg.type = MSG_LIST_VOYAGES;

    if (-1 == nClient) {
        replyError(nFdClient, &tMsg, ERROR_NOT_CONNECTED);
        return;
    }
    if (FLAG_NONE != tMsg.flag || 0 != tMsg.dataLength) {
        replyError(nFdClient, &tMsg, ERROR_MESSAGE_FORMAT);
        return;
    }
    psName = ptIthaca->atClients[nClient].psName;
    consoleLog(psName, LOG_REQUESTED_VOYAGES);

    // Listing again means the previous mission was lost: it is failed for good
    while (NULL != (ptVoyage = findActiveVoyage(ptIthaca, psName))) {
        ptVoyage->eState = VOYAGE_FAILED;
    }

    nTotal = countAvailableVoyages(ptIthaca);
    if (0 == nTotal) {
        buildMessage(&tMsg, FLAG_RESPONSE, NO_VOYAGES_MESSAGE);
        sendMessage(&tMsg, nFdClient);
        return;
    }

    sendAvailableVoyages(ptIthaca, &tMsg, nFdClient, nTotal);
}

/***********************************************
 *
 * @Name : assignVoyage
 * @Def : Assigns a voyage to an odysseus, answers OK with the voyage data
 *        and prints the assignment.
 * @Arg : In/Out: ptVoyage = voyage to assign
 *        In: psName = name of the odysseus
 *        In/Out: ptMsg = message used to build the response
 *        In: nFdClient = socket of the client
 * @Ret : None.
 *
 ************************************************/
static void assignVoyage(Voyage *ptVoyage, const char *psName, Message *ptMsg,
                         int nFdClient) {
    char *psVoyageInfo = NULL;
    char *psOwner = NULL;

    psOwner = strdup(psName);
    if (NULL == psOwner) {
        replyError(nFdClient, ptMsg, ERROR_MEMORY_ALLOCATION);
        return;
    }

    free(ptVoyage->psOwner);
    ptVoyage->psOwner = psOwner;
    ptVoyage->eState = VOYAGE_ASSIGNED;

    if (-1 == asprintf(&psVoyageInfo, VOYAGE_ACCEPTED_FORMAT, ptVoyage->nId,
                       ptVoyage->psObject, ptVoyage->psDestination,
                       ptVoyage->nReward)) {
        replyError(nFdClient, ptMsg, ERROR_MEMORY_ALLOCATION);
        return;
    }
    buildMessage(ptMsg, FLAG_RESPONSE, psVoyageInfo);
    free(psVoyageInfo);
    psVoyageInfo = NULL;
    sendMessage(ptMsg, nFdClient);

    if (-1 !=
        asprintf(&psVoyageInfo, MSG_VOYAGE_ASSIGNED, ptVoyage->nId, psName)) {
        println(psVoyageInfo);
        free(psVoyageInfo);
    }
}

/***********************************************
 *
 * @Name : acceptVoyage
 * @Def : Handles an ACCEPT request: validates the voyage id and assigns
 *        the voyage if the client has no active voyage and it is free.
 * @Arg : In: nFdClient = socket of the client
 *        In/Out: ptIthaca = ithaca with the clients and the voyages
 *        In: tMsg = request received
 * @Ret : None.
 *
 ************************************************/
static void acceptVoyage(int nFdClient, Ithaca *ptIthaca, Message tMsg) {
    const char *psName = NULL;
    Voyage *ptVoyage = NULL;
    int nClient = findClientIndex(ptIthaca, nFdClient);

    tMsg.type = MSG_ACCEPT;

    if (-1 == nClient) {
        replyError(nFdClient, &tMsg, ERROR_NOT_CONNECTED);
        return;
    }
    if (FLAG_NONE != tMsg.flag || 0 == tMsg.dataLength) {
        replyError(nFdClient, &tMsg, ERROR_MESSAGE_FORMAT);
        return;
    }
    // At most 9 digits so atoi can't overflow
    if (MAX_VOYAGE_ID_DIGITS < tMsg.dataLength ||
        0 == isPositiveInteger(tMsg.data)) {
        replyError(nFdClient, &tMsg, ERROR_INVALID_VOYAGE_ID);
        return;
    }
    psName = ptIthaca->atClients[nClient].psName;
    ptVoyage = findVoyage(ptIthaca, atoi(tMsg.data));
    if (NULL == ptVoyage) {
        replyError(nFdClient, &tMsg, ERROR_INVALID_VOYAGE_ID);
        return;
    }
    if (NULL != findActiveVoyage(ptIthaca, psName)) {
        replyError(nFdClient, &tMsg, ERROR_ACTIVE_VOYAGE);
        return;
    }
    // Ithaca handles one request at a time, so this check-and-set is atomic
    if (VOYAGE_AVAILABLE != ptVoyage->eState) {
        replyError(nFdClient, &tMsg, ERROR_VOYAGE_NOT_AVAILABLE);
        return;
    }

    assignVoyage(ptVoyage, psName, &tMsg, nFdClient);
}

static void sendReadyError(int nFdClient, const char *psReason) {
    Message tMsg;

    tMsg.type = MSG_FILE_READY;
    buildMessage(&tMsg, FLAG_RESPONSE | FLAG_ERROR, psReason);
    sendMessage(&tMsg, nFdClient);
}

static char *fileBaseName(char *psFilePath) {
    char *psBaseName = psFilePath;
    int i = 0;

    for (i = 0; '\0' != psFilePath[i]; i++) {
        if ('/' == psFilePath[i]) {
            psBaseName = &psFilePath[i + 1];
        }
    }
    return psBaseName;
}
static void claimHeader(int nFdClient, Ithaca *ptIthaca, Message tMsg) {
    FileInfo tInfo;
    Voyage *ptVoyage = NULL;
    PoolJob *ptJob = NULL;
    char *psTempPath = NULL;
    int nClient = findClientIndex(ptIthaca, nFdClient);

    memset(&tInfo, 0, sizeof(FileInfo));

    if (-1 != nClient) {
        ptVoyage = findActiveVoyage(ptIthaca, ptIthaca->atClients[nClient].psName);
    }
    if (NULL == ptVoyage) {
        sendReadyError(nFdClient, ERROR_CLAIM_NO_VOYAGE);
        return;
    }
    if (0 == ptIthaca->atClients[nClient].nClaimPending) {
        sendReadyError(nFdClient, ERROR_CLAIM_NO_REQUEST);
        return;
    }
    ptIthaca->atClients[nClient].nClaimPending = 0;

    if (TRANSFER_OK != parseHeader(&tMsg, TRANSFER_CLAIM, &tInfo)) {
        sendReadyError(nFdClient, ERROR_CLAIM_HEADER);
        return;
    }

    if (0 != strcmp(tInfo.psFileName, fileBaseName(ptVoyage->psFile))) {
        freeFileInfo(&tInfo);
        sendReadyError(nFdClient, ERROR_CLAIM_WRONG_FILE);
        return;
    }


    if (-1 == asprintf(&psTempPath, CLAIM_TEMP_FORMAT, ptIthaca->psMission,
                       ptVoyage->nId)) {
        psTempPath = NULL;
    }
    if (NULL != psTempPath) {
        ptJob = createJob(JOB_VERIFY_CLAIM, nFdClient, psTempPath,
                          fileBaseName(ptVoyage->psFile),
                          ptVoyage->psDestination, &tInfo);
        free(psTempPath);
    }
    if (NULL == ptJob) {
        freeFileInfo(&tInfo);
        sendReadyError(nFdClient, ERROR_MEMORY_ALLOCATION);
        return;
    }

    ptIthaca->atClients[nClient].ptJob = ptJob;
    FD_CLR(nFdClient, &ptIthaca->tFdsClients);

    if (POOL_OK != poolSubmit(&ptIthaca->tPool, ptJob)) {
       FD_SET(nFdClient, &ptIthaca->tFdsClients);
       ptIthaca->atClients[nClient].ptJob = NULL;
       freeJob(ptJob);
       sendReadyError(nFdClient, ERROR_CLAIM_BUSY);
    }
}

static void claimRequest(int nFdClient, Ithaca *ptIthaca, Message tMsg) {
    int nClient = findClientIndex(ptIthaca, nFdClient);

    tMsg.type = MSG_CLAIM;

    if (FLAG_NONE != tMsg.flag) {
        replyError(nFdClient, &tMsg, ERROR_MESSAGE_FORMAT);
        return;
    }
    if (-1 == nClient ||
        NULL == findActiveVoyage(ptIthaca, ptIthaca->atClients[nClient].psName)) {
        replyError(nFdClient, &tMsg, ERROR_CLAIM_NO_VOYAGE);
        return;
    }

    ptIthaca->atClients[nClient].nClaimPending = 1;
}

/***********************************************
 *
 * @Name : consoleLog
 * @Def : Prints ">>> <name><postfix>" in the console.
 * @Arg : In: psOdysseusName = name of the odysseus
 *        In: psPostFix = text printed after the name
 * @Ret : None.
 *
 ************************************************/
void consoleLog(const char *psOdysseusName, const char *psPostFix) {
    print(LOG_PREFIX);
    print(psOdysseusName);
    println(psPostFix);
}

/***********************************************
 *
 * @Name : removeClient
 * @Def : Closes a client's socket, removes it from the select set, frees
 *        its name and removes it from the list. The last client is moved
 *        into the freed slot.
 * @Arg : In/Out: ptIthaca = ithaca with the client list
 *        In: nFd = socket of the client to remove
 * @Ret : 0 if the client was removed, -1 if it wasn't in the list.
 *
 ************************************************/
int removeClient(Ithaca *ptIthaca, int nFd) {
    int nIndex = findClientIndex(ptIthaca, nFd);

    if (-1 == nIndex)
        return -1;

    close(nFd);
    FD_CLR(nFd, &ptIthaca->tFdsClients);
    free(ptIthaca->atClients[nIndex].psName);

    ptIthaca->atClients[nIndex] =
        ptIthaca->atClients[ptIthaca->tNumClients - 1];
    ptIthaca->tNumClients--;

    return 0;
}

/***********************************************
 *
 * @Name : choseOption
 * @Def : Calls the handler that matches the type of the message.
 * @Arg : In/Out: ptIthaca = ithaca that handles the message
 *        In: tMsg = message received
 *        In: nFdClient = socket of the client
 * @Ret : None.
 *
 ************************************************/
void choseOption(Ithaca *ptIthaca, Message tMsg, int nFdClient) {
    switch (tMsg.type) {
        case MSG_CONNECT_ITHACA:
            // CONNECT TO ITHACA (REQUEST)
            connectIthaca(nFdClient, ptIthaca, tMsg);
            break;
        case MSG_LIST_VOYAGES:
            // LIST VOYAGES (REQUEST)
            listVoyages(nFdClient, ptIthaca, tMsg);
            break;
        case MSG_ACCEPT:
            // ACCEPT (REQUEST)
            acceptVoyage(nFdClient, ptIthaca, tMsg);
            break;
        case MSG_CLAIM:
            // CLAIM (REQUEST)
            claimRequest(nFdClient, ptIthaca, tMsg);
            break;
        case MSG_FILE_HEADER:
            // FILE HEADER (REQUEST)
            claimHeader(nFdClient, ptIthaca, tMsg);
            break;
        case MSG_DISCONNECT_ITHACA:
            // DISCONNECT FROM ITHACA (REQUEST)
            disconectIthaca(nFdClient, ptIthaca, tMsg);
            break;
        default:
            sendNack(nFdClient, NACK_INVALID_TYPE);
            break;
    }
}