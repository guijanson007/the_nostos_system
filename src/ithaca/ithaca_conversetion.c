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
 * @Name : sendReadyError
 * @Def : Refuses a file transfer with an error READY frame.
 * @Arg : In: nFdClient = socket of the sender
 *        In: psReason = reason of the refusal
 * @Ret : None.
 *
 ************************************************/
static void sendReadyError(int nFdClient, const char *psReason) {
    Message tMsg;

    memset(&tMsg, 0, sizeof(Message));
    tMsg.type = MSG_FILE_READY;
    buildMessage(&tMsg, FLAG_RESPONSE | FLAG_ERROR, psReason);
    sendMessage(&tMsg, nFdClient);
}

/***********************************************
 *
 * @Name : fileBaseName
 * @Def : Gives the file name of a path (the part after the last '/').
 * @Arg : In: psFilePath = path
 * @Ret : Pointer inside psFilePath to its file name.
 *
 ************************************************/
static char *fileBaseName(char *psFilePath) {
    char *psBaseName = psFilePath;
    int i = 0;

    for (i = 0; '\0' != psFilePath[i]; i++) {
        if (PATH_SEPARATOR == psFilePath[i]) {
            psBaseName = &psFilePath[i + 1];
        }
    }
    return psBaseName;
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

/***********************************************
 *
 * @Name : releaseVoyage
 * @Def : Makes a voyage available again and forgets its owner.
 * @Arg : In/Out: ptVoyage = voyage to release
 * @Ret : None.
 *
 ************************************************/
static void releaseVoyage(Voyage *ptVoyage) {
    free(ptVoyage->psOwner);
    ptVoyage->psOwner = NULL;
    ptVoyage->eState = VOYAGE_AVAILABLE;
}

static Voyage *findVoyage(Ithaca *ptIthaca, int nId);
static Voyage *findActiveVoyage(Ithaca *ptIthaca, const char *psName);
static void replyError(int nFdClient, Message *ptMsg, char *psReason);

/***********************************************
 *
 * @Name : buildObjectPath
 * @Def : Builds the path of a voyage's object: the mission folder (line 2
 *        of the ithaca config) plus the file name from voyages.dat.
 * @Arg : In: ptIthaca = ithaca (for the folder)
 *        In: ptVoyage = voyage whose object is wanted
 * @Ret : The path (free it), or NULL if memory ran out.
 *
 ************************************************/
static char *buildObjectPath(Ithaca *ptIthaca, Voyage *ptVoyage) {
    char *psPath = NULL;

    if (-1 == asprintf(&psPath, OBJECT_PATH_FORMAT, ptIthaca->psMission,
                       fileBaseName(ptVoyage->psFile))) {
        return NULL;
    }
    return psPath;
}

/***********************************************
 *
 * @Name : startObjectJob
 * @Def : Gives the socket to a worker that sends the voyage's object. The
 *        caller checked that the pool has room before answering ACCEPT OK,
 *        so this only fails if the pool breaks. Then the voyage is released
 *        and the client dropped, because it already received the OK.
 * @Arg : In/Out: ptIthaca = ithaca with the clients, voyages and pool
 *        In: nClient = index of the client
 *        In/Out: ptVoyage = voyage just assigned
 *        In: ptJob = job that sends the object (owned by the client after
 *            this call, or freed)
 * @Ret : None.
 *
 ************************************************/
static void startObjectJob(Ithaca *ptIthaca, int nClient, Voyage *ptVoyage,
                           PoolJob *ptJob) {
    Client *ptClient = &ptIthaca->atClients[nClient];
    char *psText = NULL;

    if (-1 != asprintf(&psText, MSG_SENDING_OBJECT, ptVoyage->psObject,
                       ptClient->psName)) {
        println(psText);
        free(psText);
    }

    ptClient->ptJob = ptJob;
    FD_CLR(ptClient->nFd, &ptIthaca->tFdsClients);
    if (POOL_OK == poolSubmit(&ptIthaca->tPool, ptJob)) {
        return;
    }
    FD_SET(ptClient->nFd, &ptIthaca->tFdsClients);
    ptClient->ptJob = NULL;
    freeJob(ptJob);

    releaseVoyage(ptVoyage);
    consoleLog(ptClient->psName, LOG_OBJECT_FAILED);
    removeClient(ptIthaca, ptClient->nFd);
}


/***********************************************
 *
 * @Name : finishObject
 * @Def : Handles the end of an OBJECT transfer. If it failed, the voyage
 *        is released so another odysseus can take it.
 * @Arg : In/Out: ptIthaca = ithaca with the clients and voyages
 *        In: nClient = index of the client
 *        In: ptJob = finished job
 * @Ret : None.
 *
 ************************************************/
static void finishObject(Ithaca *ptIthaca, int nClient, PoolJob *ptJob) {
    char *psName = ptIthaca->atClients[nClient].psName;
    Voyage *ptVoyage = findActiveVoyage(ptIthaca, psName);

    if (TRANSFER_OK == ptJob->nResult) {
        println(MSG_OBJECT_SENT);
        return;
    }

    if (NULL != ptVoyage) {
        releaseVoyage(ptVoyage);
    }
    consoleLog(psName, LOG_OBJECT_FAILED);
}

/***********************************************
 *
 * @Name : finishClaim
 * @Def : Handles the end of a CLAIM verification: on success the voyage is
 *        completed and the reward sent, otherwise the reason is sent.
 * @Arg : In/Out: ptIthaca = ithaca with the clients and voyages
 *        In: nClient = index of the client
 *        In: ptJob = finished job
 * @Ret : None.
 *
 ************************************************/
static void finishClaim(Ithaca *ptIthaca, int nClient, PoolJob *ptJob) {
    Message tMsg;
    Voyage *ptVoyage = NULL;
    char *psText = NULL;
    char *psName = ptIthaca->atClients[nClient].psName;

    memset(&tMsg, 0, sizeof(Message));
    tMsg.type = MSG_CLAIM;

    ptVoyage = findActiveVoyage(ptIthaca, psName);
    if (NULL == ptVoyage) {
        replyError(ptJob->nFd, &tMsg, ERROR_CLAIM_NO_VOYAGE);
        return;
    }

    switch (ptJob->nResult) {
        case TRANSFER_OK:
            // The reply is built first: if memory runs out the voyage is
            // still assigned and can be claimed again
            if (-1 == asprintf(&psText, CLAIM_OK_FORMAT, ptVoyage->nReward)) {
                replyError(ptJob->nFd, &tMsg, ERROR_MEMORY_ALLOCATION);
                return;
            }
            ptVoyage->eState = VOYAGE_COMPLETED;
            buildMessage(&tMsg, FLAG_RESPONSE, psText);
            free(psText);
            psText = NULL;
            sendMessage(&tMsg, ptJob->nFd);

            println(MSG_OBJECT_RECEIVED);
            if (-1 != asprintf(&psText, MSG_GLYPH_VERIFIED,
                               ptVoyage->psDestination)) {
                println(psText);
                free(psText);
            }
            if (-1 != asprintf(&psText, MSG_VOYAGE_COMPLETED, ptVoyage->nId,
                               psName)) {
                println(psText);
                free(psText);
            }
            if (-1 != asprintf(&psText, MSG_REWARD_GRANTED,
                               ptVoyage->nReward)) {
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

/***********************************************
 *
 * @Name : finishJob
 * @Def : Applies the result of a finished transfer job: gives the socket
 *        back to the select() loop (or drops the client if it left) and
 *        frees the job.
 * @Arg : In/Out: ptIthaca = ithaca with the clients and voyages
 *        In: ptJob = job returned by poolCollect
 * @Ret : None.
 *
 ************************************************/
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
        
        switch (ptJob->eType) {
            case JOB_VERIFY_CLAIM:
                finishClaim(ptIthaca, nClient, ptJob);
                break;
            case JOB_SEND_FILE:
                finishObject(ptIthaca, nClient, ptJob);
                break;
            default:
                break;
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
    ptIthaca->atClients[ptIthaca->tNumClients].psClaimError = NULL;
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
            // The client is waiting for nTotal frames: end the list
            replyError(nFdClient, ptMsg, ERROR_MEMORY_ALLOCATION);
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
 * @Ret : 0 if the voyage was assigned, -1 if memory ran out (the voyage is
 *        left untouched and the error is sent).
 *
 ************************************************/
static int assignVoyage(Voyage *ptVoyage, const char *psName, Message *ptMsg,
                         int nFdClient) {
    char *psVoyageInfo = NULL;
    char *psOwner = NULL;

    // Everything that can fail is done before touching the voyage
    if (-1 == asprintf(&psVoyageInfo, VOYAGE_ACCEPTED_FORMAT, ptVoyage->nId,
                       ptVoyage->psObject, ptVoyage->psDestination,
                       ptVoyage->nReward)) {
        replyError(nFdClient, ptMsg, ERROR_MEMORY_ALLOCATION);
        return -1;
    }
    psOwner = strdup(psName);
    if (NULL == psOwner) {
        free(psVoyageInfo);
        replyError(nFdClient, ptMsg, ERROR_MEMORY_ALLOCATION);
        return -1;
    }

    free(ptVoyage->psOwner);
    ptVoyage->psOwner = psOwner;
    ptVoyage->eState = VOYAGE_ASSIGNED;

    buildMessage(ptMsg, FLAG_RESPONSE, psVoyageInfo);
    free(psVoyageInfo);
    psVoyageInfo = NULL;
    sendMessage(ptMsg, nFdClient);

    if (-1 !=
        asprintf(&psVoyageInfo, MSG_VOYAGE_ASSIGNED, ptVoyage->nId, psName)) {
        println(psVoyageInfo);
        free(psVoyageInfo);
    }

    return 0;
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
    PoolJob *ptJob = NULL;
    char *psObjectPath = NULL;
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
    if (0 == isPositiveInteger(tMsg.data)) {
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

    if (VOYAGE_AVAILABLE != ptVoyage->eState) {
        replyError(nFdClient, &tMsg, ERROR_VOYAGE_NOT_AVAILABLE);
        return;
    }

    psObjectPath = buildObjectPath(ptIthaca, ptVoyage);
    if (NULL == psObjectPath || 0 != access(psObjectPath, R_OK)) {
        free(psObjectPath);
        replyError(nFdClient, &tMsg, ERROR_OBJECT_MISSING);
        return;
    }

    // The transfer job must be ready before answering OK: after the OK the
    // client waits for the object
    ptJob = createJob(JOB_SEND_FILE, nFdClient, psObjectPath,
                      fileBaseName(ptVoyage->psFile), NULL, NULL);
    free(psObjectPath);
    if (NULL == ptJob) {
        replyError(nFdClient, &tMsg, ERROR_MEMORY_ALLOCATION);
        return;
    }
    if (0 == poolHasRoom(&ptIthaca->tPool)) {
        freeJob(ptJob);
        replyError(nFdClient, &tMsg, ERROR_ITHACA_BUSY);
        return;
    }

    if (0 != assignVoyage(ptVoyage, psName, &tMsg, nFdClient)) {
        freeJob(ptJob);
        return;
    }
    startObjectJob(ptIthaca, nClient, ptVoyage, ptJob);
}

/***********************************************
 *
 * @Name : claimHeader
 * @Def : Handles the HEADER of the CLAIM transfer, sent right after the
 *        CLAIM request. A refused claim is answered here, once, with an
 *        error READY (the sender waits for READY before anything else).
 *        Otherwise a worker receives and verifies the image.
 * @Arg : In: nFdClient = socket of the client
 *        In/Out: ptIthaca = ithaca with the clients, voyages and pool
 *        In: tMsg = HEADER received
 * @Ret : None.
 *
 ************************************************/
static void claimHeader(int nFdClient, Ithaca *ptIthaca, Message tMsg) {
    FileInfo tInfo;
    Voyage *ptVoyage = NULL;
    PoolJob *ptJob = NULL;
    char *psRecordPath = NULL;
    const char *psClaimError = NULL;
    int nClient = findClientIndex(ptIthaca, nFdClient);

    memset(&tInfo, 0, sizeof(FileInfo));

    if (-1 == nClient) {
        sendReadyError(nFdClient, ERROR_CLAIM_NO_VOYAGE);
        return;
    }
    if (0 == ptIthaca->atClients[nClient].nClaimPending) {
        sendReadyError(nFdClient, ERROR_CLAIM_NO_REQUEST);
        return;
    }
    psClaimError = ptIthaca->atClients[nClient].psClaimError;
    ptIthaca->atClients[nClient].nClaimPending = 0;
    ptIthaca->atClients[nClient].psClaimError = NULL;
    if (NULL != psClaimError) {
        sendReadyError(nFdClient, psClaimError);
        return;
    }

    ptVoyage = findActiveVoyage(ptIthaca, ptIthaca->atClients[nClient].psName);
    if (NULL == ptVoyage) {
        sendReadyError(nFdClient, ERROR_CLAIM_NO_VOYAGE);
        return;
    }

    if (TRANSFER_OK != parseHeader(&tMsg, TRANSFER_CLAIM, &tInfo)) {
        sendReadyError(nFdClient, ERROR_CLAIM_HEADER);
        return;
    }

    if (0 != strcmp(tInfo.psFileName, fileBaseName(ptVoyage->psFile))) {
        freeFileInfo(&tInfo);
        sendReadyError(nFdClient, ERROR_CLAIM_WRONG_FILE);
        return;
    }

    // A valid claim image is kept in the mission folder as its proof
    psRecordPath = buildRecordPath(ptIthaca->psMission,
                                   ptIthaca->atClients[nClient].psName,
                                   ptVoyage->nId, tInfo.psFileName);
    if (NULL != psRecordPath) {
        ptJob = createJob(JOB_VERIFY_CLAIM, nFdClient, psRecordPath,
                          fileBaseName(ptVoyage->psFile),
                          ptVoyage->psDestination, &tInfo);
        free(psRecordPath);
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

/***********************************************
 *
 * @Name : claimRequest
 * @Def : Handles a CLAIM request (<VoyageID>). Nothing is answered here:
 *        the client sends the CLAIM HEADER right away, so a refusal is
 *        stored and answered on that HEADER (one error per claim).
 * @Arg : In: nFdClient = socket of the client
 *        In/Out: ptIthaca = ithaca with the clients and voyages
 *        In: tMsg = request received
 * @Ret : None.
 *
 ************************************************/
static void claimRequest(int nFdClient, Ithaca *ptIthaca, Message tMsg) {
    int nClient = findClientIndex(ptIthaca, nFdClient);
    Client *ptClient = NULL;
    Voyage *ptVoyage = NULL;
    char *psText = NULL;

    // Not connected: its HEADER is refused anyway
    if (-1 == nClient) {
        return;
    }
    ptClient = &ptIthaca->atClients[nClient];
    ptClient->nClaimPending = 1;
    ptClient->psClaimError = NULL;

    if (FLAG_NONE != tMsg.flag) {
        ptClient->psClaimError = ERROR_MESSAGE_FORMAT;
        return;
    }
    ptVoyage = findActiveVoyage(ptIthaca, ptClient->psName);
    if (NULL == ptVoyage) {
        ptClient->psClaimError = ERROR_CLAIM_NO_VOYAGE;
        return;
    }
    if (0 == isPositiveInteger(tMsg.data) || ptVoyage->nId != atoi(tMsg.data)) {
        ptClient->psClaimError = ERROR_CLAIM_WRONG_VOYAGE;
        return;
    }

    if (-1 != asprintf(&psText, MSG_CLAIMS_VOYAGE, ptClient->psName,
                       ptVoyage->nId)) {
        println(psText);
        free(psText);
    }
    if (-1 != asprintf(&psText, MSG_RECEIVING_STAMPED, ptVoyage->psObject)) {
        println(psText);
        free(psText);
    }
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