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

// Own
#include "odysseus/odysseus_command.h"
#include "odysseus/odysseus_conversation.h"
#include "odysseus/odysseus_map.h"
#include "odysseus/odysseus_parser.h"
#include "utils/utils_communication.h"

/***********************************************
 *
 * @Name : receiveIthacaResponse
 * @Def : Receives a response from Ithaca and checks that it has the
 *        expected type, the response flag and no error flag.
 * @Arg : In/Out: ptOdysseus = odysseus connected to Ithaca
 *        Out: ptMessage = message received
 *        In: nType = type of the expected response
 * @Ret : ODY_OK if the response is valid, otherwise the status code of the
 *        failure.
 *
 ************************************************/
static OdysseusStatus receiveIthacaResponse(Odysseus *ptOdysseus,
                                            Message *ptMessage, int nType) {
    if (0 != receiveMessage(ptMessage, ptOdysseus->nIthacaSocket)) {
        return ODY_ERR_IO;
    }
    if (nType != ptMessage->type || 0 == (ptMessage->flag & FLAG_RESPONSE)) {
        return ODY_ERR_PROTOCOL;
    }
    if (0 != (ptMessage->flag & FLAG_ERROR)) {
        odysseusSetRemoteError(ptOdysseus, ptMessage->data,
                               ptMessage->dataLength);
        return ODY_ERR_REMOTE;
    }
    return ODY_OK;
}

/***********************************************
 *
 * @Name : receiveIslandResponse
 * @Def : Receives a response from the island and checks that it has the
 *        expected type and no error flag.
 * @Arg : In/Out: ptOdysseus = odysseus docked at the island
 *        Out: ptMessage = message received
 *        In: nType = type of the expected response
 * @Ret : ODY_OK if the response is valid, otherwise the status code of the
 *        failure.
 *
 ************************************************/
static OdysseusStatus receiveIslandResponse(Odysseus *ptOdysseus,
                                            Message *ptMessage, int nType) {
    if (0 != receiveMessage(ptMessage, ptOdysseus->nIslandSocket)) {
        return ODY_ERR_IO;
    }
    if (nType != ptMessage->type) {
        return ODY_ERR_ISLAND_PROTOCOL;
    }
    if (0 != (ptMessage->flag & FLAG_ERROR)) {
        odysseusSetRemoteError(ptOdysseus, ptMessage->data,
                               ptMessage->dataLength);
        return ODY_ERR_REMOTE;
    }
    return ODY_OK;
}

/***********************************************
 *
 * @Name : connectToIthaca
 * @Def : Opens a TCP connection with Ithaca.
 * @Arg : In/Out: ptOdysseus = odysseus that connects
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus connectToIthaca(Odysseus *ptOdysseus) {
    struct sockaddr_in stAddr = {0};
    int nSocketFd = -1;

    nSocketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (0 > nSocketFd) {
        odysseusSetErrnoError(ptOdysseus, ERROR_CREATE_SOCKET, errno);
        return ODY_ERR_SOCKET;
    }

    stAddr.sin_family = AF_INET;
    stAddr.sin_port = htons(atoi(ptOdysseus->psPort));
    if (0 >= inet_pton(AF_INET, ptOdysseus->psIp, &stAddr.sin_addr)) {
        close(nSocketFd);
        return ODY_ERR_IP;
    }

    if (0 > connect(nSocketFd, (struct sockaddr *)&stAddr, sizeof(stAddr))) {
        odysseusSetErrnoError(ptOdysseus, ERROR_CONNECT_SERVER, errno);
        close(nSocketFd);
        return ODY_ERR_CONNECT;
    }
    ptOdysseus->nIthacaSocket = nSocketFd;
    return ODY_OK;
}

/***********************************************
 *
 * @Name : splitFields
 * @Def : Splits a '&' separated payload in place into its fields.
 * @Arg : In/Out: psData = payload, '&' are replaced by '\0'
 *        Out: apsFields = pointers to the fields found
 *        In: nMaxFields = size of apsFields
 * @Ret : Number of fields found.
 *
 ************************************************/
static int splitFields(char *psData, char **apsFields, int nMaxFields) {
    int nFields = 0;
    int i = 0;

    apsFields[nFields++] = psData;
    for (i = 0; '\0' != psData[i] && nFields < nMaxFields; i++) {
        if ('&' == psData[i]) {
            psData[i] = '\0';
            apsFields[nFields++] = &psData[i + 1];
        }
    }
    return nFields;
}

/***********************************************
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
 ************************************************/
static int fillVoyage(OdysseusVoyage *ptVoyage, int nId, const char *psObject,
                      const char *psDestination, const char *psReward) {
    if (0 == isPositiveInteger(psReward) &&
        0 != strcmp(psReward, ZERO_STRING)) {
        return -1;
    }

    ptVoyage->nId = nId;
    ptVoyage->psObject = strdup(psObject);
    ptVoyage->psDestination = strdup(psDestination);
    ptVoyage->nReward = atoi(psReward);

    if (NULL == ptVoyage->psObject || NULL == ptVoyage->psDestination) {
        freeOdysseusVoyage(ptVoyage);
        return -1;
    }
    return 0;
}

/***********************************************
 *
 * @Name : voyagesToRead
 * @Def : Number of frames of the voyage list to read. Until the first
 *        frame tells the total, at least one frame is read.
 * @Arg : In: nTotal = total of voyages announced (0 if not known yet)
 * @Ret : Number of frames to read.
 *
 ************************************************/
static int voyagesToRead(int nTotal) {
    if (0 == nTotal) {
        return 1;
    }
    return nTotal;
}

/***********************************************
 *
 * @Name : freeVoyageArray
 * @Def : Frees every voyage of an array and the array itself.
 * @Arg : In/Out: atVoyages = array of voyages (can be NULL)
 *        In: nTotal = number of voyages in the array
 * @Ret : None.
 *
 ************************************************/
static void freeVoyageArray(OdysseusVoyage *atVoyages, int nTotal) {
    int i = 0;

    for (i = 0; NULL != atVoyages && i < nTotal; i++) {
        freeOdysseusVoyage(&atVoyages[i]);
    }
    free(atVoyages);
}

/***********************************************
 *
 * @Name : handleVoyageFrame
 * @Def : Processes one frame of the voyage list:
 *        <Index>&<Total>&<VoyageID>&<Object>&<Destination>&<Reward>.
 *        The first frame sets the total and allocates the voyages.
 * @Arg : In/Out: psData = payload of the frame, split in place
 *        In/Out: apsFields = memory for the pointers to the fields
 *        In: i = number of the frame (starting at 1)
 *        In/Out: pnTotal = total of voyages (0 until the first frame)
 *        In/Out: patVoyages = array of voyages
 *        In/Out: peStatus = status of the listing
 * @Ret : 1 if the listing has to stop, 0 to keep reading frames.
 *
 ************************************************/
static int handleVoyageFrame(char *psData, char **apsFields, int i,
                             int *pnTotal, OdysseusVoyage **patVoyages,
                             OdysseusStatus *peStatus) {
    int nIndex = 0;
    int nId = 0;

    if (0 == *pnTotal && 0 == strcmp(psData, ZERO_STRING)) {
        return 1;
    }
    if (VOYAGE_LIST_FIELDS !=
            splitFields(psData, apsFields, VOYAGE_LIST_FIELDS) ||
        0 == isPositiveInteger(apsFields[0]) ||
        0 == isPositiveInteger(apsFields[1]) ||
        0 == isPositiveInteger(apsFields[2])) {
        *peStatus = ODY_ERR_PROTOCOL;
        if (0 == *pnTotal) {
            return 1;
        }
        return 0;
    }

    nIndex = atoi(apsFields[0]);
    nId = atoi(apsFields[2]);

    if (0 == *pnTotal) {
        *pnTotal = atoi(apsFields[1]);
        if (MAX_LISTED_VOYAGES < *pnTotal) {
            *pnTotal = 0;
            *peStatus = ODY_ERR_PROTOCOL;
            return 1;
        }
        *patVoyages = calloc((size_t)*pnTotal, sizeof(OdysseusVoyage));
    }

    if (NULL == *patVoyages) {
        *peStatus = ODY_ERR_PROTOCOL;
        return 0;
    }

    if (ODY_OK == *peStatus &&
        (nIndex != i || *pnTotal != atoi(apsFields[1]) ||
         0 != fillVoyage(&(*patVoyages)[i - 1], nId, apsFields[3], apsFields[4],
                         apsFields[5]))) {
        *peStatus = ODY_ERR_PROTOCOL;
    }

    return 0;
}
/***********************************************
 *
 * @Name : requestListOfVoyages
 * @Def : Asks Ithaca for the available voyages and stores them in the
 *        Odysseus listing. Every frame of the answer is read, even after a
 *        bad one, so the socket stays in sync with Ithaca.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus requestListOfVoyages(Odysseus *ptOdysseus) {
    Message tMessage = {0};
    char **apsFields = NULL;
    OdysseusVoyage *atVoyages = NULL;
    OdysseusStatus eStatus = ODY_OK;
    OdysseusStatus eFrameStatus = ODY_OK;
    int nTotal = 0;
    int i = 0;

    tMessage.type = MSG_LIST_VOYAGES;
    tMessage.flag = FLAG_NONE;

    apsFields = calloc(VOYAGE_LIST_FIELDS, sizeof(char *));
    if (NULL == apsFields) {
        return ODY_ERR_IO;
    }
    if (0 != sendMessage(&tMessage, ptOdysseus->nIthacaSocket)) {
        free(apsFields);
        return ODY_ERR_IO;
    }

    freeListedVoyages(ptOdysseus);

    for (i = 1; i <= voyagesToRead(nTotal); i++) {
        eFrameStatus =
            receiveIthacaResponse(ptOdysseus, &tMessage, MSG_LIST_VOYAGES);
        if (ODY_OK != eFrameStatus) {
            eStatus = eFrameStatus;
            break;
        }
        if (0 != handleVoyageFrame(tMessage.data, apsFields, i, &nTotal,
                                   &atVoyages, &eStatus)) {
            break;
        }
    }
    free(apsFields);

    if (ODY_OK != eStatus) {
        freeVoyageArray(atVoyages, nTotal);
        return eStatus;
    }

    ptOdysseus->atListedVoyages = atVoyages;
    ptOdysseus->nNumListedVoyages = nTotal;
    return ODY_OK;
}

/***********************************************
 *
 * @Name : requestAcceptVoyage
 * @Def : Asks Ithaca to assign a voyage and stores it as the active
 *        voyage.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 *        In: nVoyageId = id of the voyage to accept
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus requestAcceptVoyage(Odysseus *ptOdysseus, int nVoyageId) {
    Message tMessage = {0};
    OdysseusVoyage tVoyage = {0};
    OdysseusStatus eStatus = ODY_OK;
    char **apsFields = NULL;
    char *psId = NULL;

    tMessage.type = MSG_ACCEPT;
    tMessage.flag = FLAG_NONE;
    if (-1 == asprintf(&psId, INT_FORMAT, nVoyageId)) {
        return ODY_ERR_IO;
    }
    buildMessage(&tMessage, FLAG_NONE, psId);
    free(psId);

    if (0 != sendMessage(&tMessage, ptOdysseus->nIthacaSocket)) {
        return ODY_ERR_IO;
    }
    eStatus = receiveIthacaResponse(ptOdysseus, &tMessage, MSG_ACCEPT);
    if (ODY_OK != eStatus) {
        return eStatus;
    }

    apsFields = calloc(VOYAGE_ACCEPT_FIELDS, sizeof(char *));
    if (NULL == apsFields) {
        return ODY_ERR_IO;
    }
    // OK&<VoyageID>&<Object>&<Destination>&<Reward>
    if (VOYAGE_ACCEPT_FIELDS !=
            splitFields(tMessage.data, apsFields, VOYAGE_ACCEPT_FIELDS) ||
        0 != strcmp(apsFields[0], OK_RESPONSE) ||
        atoi(apsFields[1]) != nVoyageId ||
        0 != fillVoyage(&tVoyage, nVoyageId, apsFields[2], apsFields[3],
                        apsFields[4])) {
        free(apsFields);
        return ODY_ERR_PROTOCOL;
    }
    free(apsFields);

    freeOdysseusVoyage(&ptOdysseus->tActiveVoyage);
    ptOdysseus->tActiveVoyage = tVoyage;

    freeListedVoyages(ptOdysseus);
    return ODY_OK;
}

/***********************************************
 *
 * @Name : disconnectFromIthaca
 * @Def : Tells Ithaca that odysseus leaves and closes the socket.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus disconnectFromIthaca(Odysseus *ptOdysseus) {
    Message tMessage = {0};
    OdysseusStatus eStatus = ODY_OK;

    if (NO_SOCKET == ptOdysseus->nIthacaSocket) {
        return ODY_OK;
    }

    tMessage.type = MSG_DISCONNECT_ITHACA;
    tMessage.flag = FLAG_NONE;

    if (0 != sendMessage(&tMessage, ptOdysseus->nIthacaSocket)) {
        close(ptOdysseus->nIthacaSocket);
        ptOdysseus->nIthacaSocket = NO_SOCKET;
        return ODY_ERR_IO;
    }

    eStatus =
        receiveIthacaResponse(ptOdysseus, &tMessage, MSG_DISCONNECT_ITHACA);

    close(ptOdysseus->nIthacaSocket);
    ptOdysseus->nIthacaSocket = NO_SOCKET;
    return eStatus;
}

/***********************************************
 *
 * @Name : arriveAtIsland
 * @Def : Announces the arrival of odysseus to the island and reads if it
 *        can dock or has to wait.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 *        Out: pnArrival = ARRIVAL_WAIT or ARRIVAL_DOCKED
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus arriveAtIsland(Odysseus *ptOdysseus, int *pnArrival) {
    Message tMessage = {0};
    OdysseusStatus eStatus = ODY_OK;

    tMessage.type = MSG_ARRIVAL_ISLAND;
    tMessage.flag = FLAG_NONE;
    buildMessage(&tMessage, FLAG_NONE, ptOdysseus->psName);

    if (0 != sendMessage(&tMessage, ptOdysseus->nIslandSocket)) {
        return ODY_ERR_IO;
    }

    // Wait for the response from the island
    eStatus = receiveIslandResponse(ptOdysseus, &tMessage, MSG_ARRIVAL_ISLAND);
    if (ODY_OK != eStatus) {
        return eStatus;
    }

    if (0 != (tMessage.flag & FLAG_RESPONSE)) {
        if (0 == strcmp(tMessage.data, WAIT)) {
            *pnArrival = ARRIVAL_WAIT;
            return ODY_OK;
        } else if (0 == strcmp(tMessage.data, DOCKED)) {
            *pnArrival = ARRIVAL_DOCKED;
            return ODY_OK;
        }
    }
    return ODY_ERR_ISLAND_PROTOCOL;
}

/***********************************************
 *
 * @Name : notifyConnection
 * @Def : Sends the name of odysseus to Ithaca to register the connection.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus notifyConnection(Odysseus *ptOdysseus) {
    Message tMessage = {0};

    tMessage.type = MSG_CONNECT_ITHACA;
    tMessage.flag = FLAG_NONE;
    buildMessage(&tMessage, FLAG_NONE, ptOdysseus->psName);

    if (0 != sendMessage(&tMessage, ptOdysseus->nIthacaSocket)) {
        return ODY_ERR_IO;
    }

    // Wait for the response from Ithaca
    if (0 != receiveMessage(&tMessage, ptOdysseus->nIthacaSocket)) {
        return ODY_ERR_IO;
    }

    if (MSG_CONNECT_ITHACA != tMessage.type) {
        return ODY_ERR_PROTOCOL;
    }

    if (0 != (tMessage.flag & FLAG_ERROR)) {
        odysseusSetRemoteError(ptOdysseus, tMessage.data, tMessage.dataLength);
        return ODY_ERR_REMOTE;
    } else if (0 != (tMessage.flag & FLAG_RESPONSE)) {
        return ODY_OK;
    }
    return ODY_ERR_PROTOCOL;
}

/***********************************************
 *
 * @Name : resetTimeout
 * @Def : Sets the timeout of select to the time of one ration.
 * @Arg : Out: pstTimeout = timeout to set
 * @Ret : None.
 *
 ************************************************/
static void resetTimeout(struct timeval *pstTimeout) {
    pstTimeout->tv_sec = RATION_SECONDS;
    pstTimeout->tv_usec = 0;
}

/***********************************************
 *
 * @Name : consumeRation
 * @Def : The crew consumes one ration and the provisions left are printed.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 * @Ret : 1 if there are no provisions left (lost at sea), 0 otherwise.
 *
 ************************************************/
static int consumeRation(Odysseus *ptOdysseus) {
    char *psBuffer = NULL;

    consumeFood(&ptOdysseus->tCargo);
    if (-1 !=
        asprintf(&psBuffer, MSG_RATION, ptOdysseus->tCargo.nTotalAmount)) {
        println(psBuffer);
        free(psBuffer);
    }
    if (0 >= ptOdysseus->tCargo.nTotalAmount) {
        return 1;
    }
    return 0;
}

/***********************************************
 *
 * @Name : readPortMessage
 * @Def : Reads a message from the island while waiting offshore and
 *        checks if it is the DOCKED response.
 * @Arg : In/Out: ptOdysseus = odysseus waiting at the port
 *        Out: pnArrival = set to ARRIVAL_DOCKED when the island sends it
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
static OdysseusStatus readPortMessage(Odysseus *ptOdysseus, int *pnArrival) {
    Message tMessage = {0};

    if (0 != receiveMessage(&tMessage, ptOdysseus->nIslandSocket)) {
        return ODY_ERR_IO;
    }

    if (0 != (tMessage.flag & FLAG_ERROR)) {
        odysseusSetRemoteError(ptOdysseus, tMessage.data, tMessage.dataLength);
        return ODY_ERR_REMOTE;
    }

    if (MSG_ARRIVAL_ISLAND == tMessage.type && FLAG_RESPONSE == tMessage.flag &&
        0 == strcmp(tMessage.data, DOCKED)) {
        *pnArrival = ARRIVAL_DOCKED;
    }
    return ODY_OK;
}

/***********************************************
 *
 * @Name : handelWaitingInputs
 * @Def : While waiting, it checks the comand stated buy the user
 * @Arg : In: ptOdysseus = Odysseus instance
 * @Ret : 0 on success, otherwise it returns a -1.
 *
 ************************************************/
static int handleWaitingInputs(Odysseus *ptOdysseus) {
    OdysseusCommand tCommand;
    const char *psErrorMessage = NULL;
    char *psInput = NULL;

    memset(&tCommand, 0, sizeof(tCommand));

    if (-1 == readLine(STDIN_FILENO, &psInput)) {
        return -1;
    }
    if (NULL == psInput) {
        return 1;
    }

    if (0 == parseOdysseusCommand(psInput, &tCommand)) {
        free(psInput);
        return 0;
    }

    if (COMMAND_VALID != validateOdysseusCommand(&tCommand, &psErrorMessage)) {
        printErr(psErrorMessage);
    } else if (COMMAND_STATUS == tCommand.type) {
        odysseusStatus(ptOdysseus);
    } else if (COMMAND_MAP == tCommand.type) {
        odysseusMap(ptOdysseus);
    } else {
        printErr(ERROR_NOT_DOCKED_YET);
    }
    println("");
    free(psInput);

    return 0;
}

/***********************************************
 *
 * @Name : waitingAtPort
 * @Def : Waits offshore until the island sends DOCKED. Every
 *        RATION_SECONDS the crew consumes a ration; if the provisions run
 *        out, odysseus is lost at sea.
 * @Arg : In/Out: ptOdysseus = Odysseus instance
 *        Out: pnArrival = ARRIVAL_DOCKED, ARRIVAL_LOST_AT_SEA or
 *             ARRIVAL_NONE (interrupted)
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus waitingAtPort(Odysseus *ptOdysseus, int *pnArrival) {
    fd_set tReadFds;
    struct timeval stTimeout = {0};
    OdysseusStatus eStatus = ODY_OK;
    int nReady = 0;
    int nResult = 0;
    int nWatchInputs = 1;

    *pnArrival = ARRIVAL_NONE;
    resetTimeout(&stTimeout);

    while (1) {
        FD_ZERO(&tReadFds);
        FD_SET(ptOdysseus->nIslandSocket, &tReadFds);
        if (0 != nWatchInputs) {
            FD_SET(STDIN_FILENO, &tReadFds);
        }
        nReady = select(ptOdysseus->nIslandSocket + 1, &tReadFds, NULL, NULL, &stTimeout);

        if (-1 == nReady) {
            if (EINTR == errno) {
                return ODY_OK;
            }
            return ODY_ERR_IO;
        }
        if (0 == nReady) {
            if (0 != consumeRation(ptOdysseus)) {
                *pnArrival = ARRIVAL_LOST_AT_SEA;
                return ODY_OK;
            }
            resetTimeout(&stTimeout);
            continue;
        }

        if (FD_ISSET(ptOdysseus->nIslandSocket, &tReadFds)) {
            eStatus = readPortMessage(ptOdysseus, pnArrival);
            if (ODY_OK != eStatus || ARRIVAL_DOCKED == *pnArrival) {
                return eStatus;
            }
        }

        if (0 != nWatchInputs && FD_ISSET(STDIN_FILENO, &tReadFds)) {
            nResult = handleWaitingInputs(ptOdysseus);
            if (-1 == nResult) {
                return (EINTR == errno) ? ODY_OK : ODY_ERR_IO;
            }
            if (1 == nResult) {
                nWatchInputs = 0;
            }
        }
    }
}

/***********************************************
 *
 * @Name : parseMessageMarket
 * @Def : Parses one frame of the market:
 *        <Index>&<Total>&<Product>&<Amount>&<Price>.
 * @Arg : In/Out: psData = payload, split in place
 *        In: nLength = length of the payload
 *        Out: pnIndex = index of the frame
 *        Out: pnTotal = total of frames
 *        Out: ppsProduct = name of the product
 *        Out: pnAmount = amount of the product
 *        Out: pnPrice = price of the product
 * @Ret : 0 if the frame is valid, -1 otherwise.
 *
 ************************************************/
static int parseMessageMarket(char *psData, int nLength, int *pnIndex,
                              int *pnTotal, char **ppsProduct, int *pnAmount,
                              int *pnPrice) {
    char *psIndex = NULL;
    char *psTotal = NULL;
    char *psAmount = NULL;
    char *psPrice = NULL;

    if (NULL == psData || 0 >= nLength || MAX_DATA_LENGTH < nLength) {
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
    if (0 == isAllDigits(psIndex) || 0 == isAllDigits(psTotal) ||
        0 == isAllDigits(psAmount) || 0 == isAllDigits(psPrice)) {
        return -1;
    }

    *pnIndex = atoi(psIndex);
    *pnTotal = atoi(psTotal);
    *pnAmount = atoi(psAmount);
    *pnPrice = atoi(psPrice);

    if (1 > *pnIndex || *pnIndex > *pnTotal) {
        return -1;
    }
    return 0;
}

/***********************************************
 *
 * @Name : printMarketMap
 * @Def : Prints the last line of the market, the map and its price.
 * @Arg : In: nPrice = price of the map
 * @Ret : None.
 *
 ************************************************/
static void printMarketMap(int nPrice) {
    char *psBuffer = NULL;

    println(MSG_MARKET_MORE);
    if (-1 != asprintf(&psBuffer, MSG_MARKET_MAP, MAP_LABEL, nPrice)) {
        println(psBuffer);
        free(psBuffer);
    }
}

/***********************************************
 *
 * @Name : printMarketHeader
 * @Def : Prints the title of the market of the current island.
 * @Arg : In: ptOdysseus = odysseus docked at the island
 * @Ret : None.
 *
 ************************************************/
static void printMarketHeader(Odysseus *ptOdysseus) {
    char *psBuffer = NULL;

    if (-1 !=
        asprintf(
            &psBuffer, MSG_MARKET_HEADER,
            ptOdysseus->tMap.atConnections[ptOdysseus->nWhereAmI].psName)) {
        println(psBuffer);
        free(psBuffer);
    }
}

/***********************************************
 *
 * @Name : useMarketLine
 * @Def : Saves the price of the wanted product and prints the line of the
 *        market if asked.
 * @Arg : In: psProduct = product of the line
 *        In: nAmount = amount of the product
 *        In: nPrice = price of the product
 *        In: nPrint = 1 to print the line, 0 to stay silent
 *        In: psWanted = product whose price is wanted, or NULL
 *        Out: pnPrice = price of psWanted when it is this product
 * @Ret : None.
 *
 ************************************************/
static void useMarketLine(const char *psProduct, int nAmount, int nPrice,
                          int nPrint, const char *psWanted, int *pnPrice) {
    char *psBuffer = NULL;

    if (NULL != psWanted && 0 == strcasecmp(psProduct, psWanted)) {
        *pnPrice = nPrice;
    }
    if (0 != nPrint && -1 != asprintf(&psBuffer, MSG_MARKET_LINE, psProduct,
                                      nAmount, nPrice)) {
        println(psBuffer);
        free(psBuffer);
    }
}

/***********************************************
 *
 * @Name : queryMarket
 * @Def : Asks the island for its market. It can print it and/or look for
 *        the price of one product.
 * @Arg : In/Out: ptOdysseus = odysseus docked at the island
 *        In: nPrint = 1 to print the market, 0 to stay silent
 *        In: psWanted = product whose price is wanted, or NULL
 *        Out: pnPrice = price of psWanted when the island sells it
 * @Ret : ODY_OK if everything went well, the error status otherwise.
 *
 ************************************************/
static OdysseusStatus queryMarket(Odysseus *ptOdysseus, int nPrint,
                                  const char *psWanted, int *pnPrice) {
    Message tMessage = {0};
    OdysseusStatus eStatus = ODY_OK;
    char *psProduct = NULL;
    int nIndex = 0;
    int nTotal = 0;
    int nExpected = 1;
    int nAmount = 0;
    int nPrice = 0;

    tMessage.type = MSG_LIST_MARKET;
    tMessage.flag = FLAG_NONE;

    if (0 != sendMessage(&tMessage, ptOdysseus->nIslandSocket)) {
        return ODY_ERR_IO;
    }

    if (0 != nPrint) {
        printMarketHeader(ptOdysseus);
    }
    while (1) {
        eStatus = receiveIslandResponse(ptOdysseus, &tMessage, MSG_LIST_MARKET);
        if (ODY_OK != eStatus) {
            return eStatus;
        }

        if (0 != parseMessageMarket(tMessage.data, tMessage.dataLength, &nIndex,
                                    &nTotal, &psProduct, &nAmount, &nPrice) ||
            nIndex != nExpected) {
            return ODY_ERR_ISLAND_PROTOCOL;
        }
        nExpected++;

        useMarketLine(psProduct, nAmount, nPrice, nPrint, psWanted, pnPrice);

        // The last frame is the MAP
        if (nIndex == nTotal) {
            if (0 != nPrint) {
                printMarketMap(nPrice);
            }
            break;
        }
    }

    return ODY_OK;
}

/***********************************************
 *
 * @Name : getListMarket
 * @Def : Asks the island for its market and prints it.
 * @Arg : In/Out: ptOdysseus = odysseus docked at the island
 * @Ret : ODY_OK if everything went well, the error status otherwise.
 *
 ************************************************/
OdysseusStatus getListMarket(Odysseus *ptOdysseus) {
    return queryMarket(ptOdysseus, 1, NULL, NULL);
}

/***********************************************
 *
 * @Name : parseMessageTrade
 * @Def : Parses the answer of a BUY/SELL: OK&<Product>&<Amount>&<Cost>.
 * @Arg : In/Out: psData = payload, split in place
 *        In: nLength = length of the payload
 *        Out: ppsProduct = name of the product
 *        Out: pnAmount = amount traded
 *        Out: pnCost = gold paid or received
 * @Ret : 0 if the answer is valid, -1 otherwise.
 *
 ************************************************/
static int parseMessageTrade(char *psData, int nLength, char **ppsProduct,
                             int *pnAmount, int *pnCost) {
    char *psOk = NULL;
    char *psAmount = NULL;
    char *psCost = NULL;

    if (NULL == psData || 0 >= nLength || MAX_DATA_LENGTH < nLength) {
        return -1;
    }

    psData[nLength] = '\0';

    psOk = strtok(psData, "&");
    *ppsProduct = strtok(NULL, "&");
    psAmount = strtok(NULL, "&");
    psCost = strtok(NULL, "&");

    if (NULL == psOk || NULL == *ppsProduct || NULL == psAmount ||
        NULL == psCost || NULL != strtok(NULL, "&")) {
        return -1;
    }

    if (0 != strcmp(psOk, OK_RESPONSE)) {
        return -1;
    }

    if (0 == isAllDigits(psAmount) || 0 == isAllDigits(psCost)) {
        return -1;
    }

    *pnAmount = atoi(psAmount);
    *pnCost = atoi(psCost);

    return 0;
}

/***********************************************
 *
 * @Name : checkEnoughGold
 * @Def : Looks for the price of a product in the market and checks that
 *        odysseus has enough gold to buy the amount. The island can't
 *        know our gold, so it is checked here before ordering.
 * @Arg : In/Out: ptOdysseus = odysseus docked at the island
 *        In: psProduct = product to buy
 *        In: psAmount = amount to buy
 * @Ret : ODY_OK if odysseus can pay, otherwise the status code.
 *
 ************************************************/
static OdysseusStatus checkEnoughGold(Odysseus *ptOdysseus,
                                      const char *psProduct,
                                      const char *psAmount) {
    OdysseusStatus eStatus = ODY_OK;
    int nPrice = NO_PRICE;

    eStatus = queryMarket(ptOdysseus, 0, psProduct, &nPrice);
    if (ODY_OK != eStatus) {
        return eStatus;
    }
    if (NO_PRICE != nPrice &&
        (long long)atoi(psAmount) * nPrice > ptOdysseus->tCargo.nGold) {
        return ODY_ERR_NOT_ENOUGH_GOLD;
    }
    return ODY_OK;
}

/***********************************************
 *
 * @Name : buyProducts
 * @Def : Buys an amount of a product at the island and adds it to the
 *        cargo.
 * @Arg : In/Out: ptOdysseus = odysseus docked at the island
 *        In: psProduct = product to buy
 *        In: psAmount = amount to buy
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus buyProducts(Odysseus *ptOdysseus, const char *psProduct,
                           const char *psAmount) {
    Message tMessage = {0};
    OdysseusStatus eStatus = ODY_OK;
    char *psBuffer = NULL;
    char *psBought = NULL;
    int nAmount = 0;
    int nCost = 0;

    eStatus = checkEnoughGold(ptOdysseus, psProduct, psAmount);
    if (ODY_OK != eStatus) {
        return eStatus;
    }

    tMessage.type = MSG_BUY;
    if (-1 == asprintf(&psBuffer, TRADE_BUY_FORMAT, psProduct, psAmount)) {
        return ODY_ERR_IO;
    }
    buildMessage(&tMessage, FLAG_NONE, psBuffer);
    free(psBuffer);
    psBuffer = NULL;
    if (-1 == sendMessage(&tMessage, ptOdysseus->nIslandSocket)) {
        return ODY_ERR_IO;
    }

    eStatus = receiveIslandResponse(ptOdysseus, &tMessage, MSG_BUY);
    if (ODY_OK != eStatus) {
        return eStatus;
    }
    if (0 != parseMessageTrade(tMessage.data, tMessage.dataLength, &psBought,
                               &nAmount, &nCost)) {
        return ODY_ERR_ISLAND_PROTOCOL;
    }

    if (0 != addSupply(&ptOdysseus->tCargo, psBought, nAmount)) {
        return ODY_ERR_IO;
    }
    ptOdysseus->tCargo.nGold -= nCost;

    if (-1 != asprintf(&psBuffer, MSG_BOUGHT, nAmount, psBought, nCost)) {
        println(psBuffer);
        free(psBuffer);
    }

    return ODY_OK;
}

/***********************************************
 *
 * @Name : parseEmptyMap
 * @Def : Checks the frame of an island without routes: OK&50&0. It has to
 *        be the first frame.
 * @Arg : In: apsFields = fields of the frame
 *        In/Out: pnTotal = routes announced, set to 0
 * @Ret : 0 if the frame is valid, -1 otherwise.
 *
 ************************************************/
static int parseEmptyMap(char **apsFields, int *pnTotal) {
    if (NO_TOTAL != *pnTotal || 0 != strcmp(apsFields[2], ZERO_STRING)) {
        return -1;
    }
    *pnTotal = 0;
    return 0;
}

/***********************************************
 *
 * @Name : parseRouteFields
 * @Def : Checks the frame of one route,
 *        OK&50&<Index>&<Total>&<Name>&<IP>&<Port>, and copies the route.
 * @Arg : In: apsFields = fields of the frame
 *        In: nFields = number of fields found
 *        In: nReceived = routes received before this frame
 *        In/Out: pnTotal = routes announced, NO_TOTAL until the first frame
 *        Out: ptRoute = route found (strings are duplicated)
 * @Ret : 0 if the frame is valid, -1 otherwise.
 *
 ************************************************/
static int parseRouteFields(char **apsFields, int nFields, int nReceived,
                            int *pnTotal, MapRoute *ptRoute) {
    if (MAP_FIELDS_ROUTE != nFields || 0 == isPositiveInteger(apsFields[2]) ||
        0 == isPositiveInteger(apsFields[3]) || '\0' == *apsFields[4] ||
        '\0' == *apsFields[5] || 0 == isPositiveInteger(apsFields[6]) ||
        MAX_PORT < atoi(apsFields[6])) {
        return -1;
    }
    if (atoi(apsFields[2]) != nReceived + 1 || 1 > atoi(apsFields[3]) ||
        MAX_MAP_ROUTES < atoi(apsFields[3]) ||
        (NO_TOTAL != *pnTotal && atoi(apsFields[3]) != *pnTotal)) {
        return -1;
    }
    *pnTotal = atoi(apsFields[3]);

    ptRoute->psName = strdup(apsFields[4]);
    ptRoute->psIp = strdup(apsFields[5]);
    ptRoute->nPort = atoi(apsFields[6]);

    if (NULL == ptRoute->psName || NULL == ptRoute->psIp) {
        free(ptRoute->psName);
        free(ptRoute->psIp);
        return -1;
    }
    return 0;
}

/***********************************************
 *
 * @Name : parseMapRoute
 * @Def : Parses one frame of a BUY MAP response and checks it is the next
 *        one expected. The first frame sets the total.
 * @Arg : In/Out: psData = payload, split in place
 *        In: nReceived = routes received before this frame
 *        In/Out: pnTotal = routes announced by the island, NO_TOTAL until
 *                the first frame
 *        Out: ptRoute = route found (strings are duplicated), if any
 *        Out: pnIsRoute = 1 if the frame carried a route, 0 for the empty
 *             map
 * @Ret : 0 if the frame is valid, -1 otherwise.
 *
 ************************************************/
static int parseMapRoute(char *psData, int nReceived, int *pnTotal,
                         MapRoute *ptRoute, int *pnIsRoute) {
    char **apsFields = NULL;
    int nFields = 0;
    int nResult = 0;

    *pnIsRoute = 0;
    apsFields = calloc(MAP_FIELDS_ROUTE, sizeof(char *));
    if (NULL == apsFields) {
        return -1;
    }
    nFields = splitFields(psData, apsFields, MAP_FIELDS_ROUTE);

    if (MAP_FIELDS_EMPTY > nFields || 0 != strcmp(apsFields[0], OK_RESPONSE) ||
        0 == isPositiveInteger(apsFields[1]) ||
        MAP_PRICE != atoi(apsFields[1])) {
        nResult = -1;
    } else if (MAP_FIELDS_EMPTY == nFields) {
        nResult = parseEmptyMap(apsFields, pnTotal);
    } else {
        nResult =
            parseRouteFields(apsFields, nFields, nReceived, pnTotal, ptRoute);
        if (0 == nResult) {
            *pnIsRoute = 1;
        }
    }

    free(apsFields);
    return nResult;
}

/***********************************************
 *
 * @Name : appendRoute
 * @Def : Adds a route at the end of the array of routes received.
 * @Arg : In/Out: patRoutes = array of routes
 *        In/Out: pnReceived = number of routes in the array
 *        In: tRoute = route to add (its strings are freed on error)
 * @Ret : ODY_OK on success, ODY_ERR_IO if the memory couldn't be
 *        allocated.
 *
 ************************************************/
static OdysseusStatus appendRoute(MapRoute **patRoutes, int *pnReceived,
                                  MapRoute tRoute) {
    MapRoute *atTmp = NULL;

    atTmp = realloc(*patRoutes, sizeof(MapRoute) * (*pnReceived + 1));
    if (NULL == atTmp) {
        free(tRoute.psName);
        free(tRoute.psIp);
        return ODY_ERR_IO;
    }
    *patRoutes = atTmp;
    (*patRoutes)[(*pnReceived)++] = tRoute;
    return ODY_OK;
}

/***********************************************
 *
 * @Name : receiveMapRoutes
 * @Def : Reads every frame of a BUY MAP response and stores the routes.
 * @Arg : In/Out: ptOdysseus = odysseus docked at the island
 *        Out: ptMessage = memory for the frames received
 *        Out: patRoutes = array of routes received
 *        Out: pnReceived = number of routes received
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
static OdysseusStatus receiveMapRoutes(Odysseus *ptOdysseus, Message *ptMessage,
                                       MapRoute **patRoutes, int *pnReceived) {
    OdysseusStatus eStatus = ODY_OK;
    MapRoute tRoute = {0};
    int nTotal = NO_TOTAL;
    int nIsRoute = 0;

    while (ODY_OK == eStatus && (NO_TOTAL == nTotal || *pnReceived < nTotal)) {
        eStatus = receiveIslandResponse(ptOdysseus, ptMessage, MSG_BUY_MAP);
        if (ODY_OK != eStatus) {
            break;
        }
        if (FLAG_RESPONSE != ptMessage->flag ||
            0 != parseMapRoute(ptMessage->data, *pnReceived, &nTotal, &tRoute,
                               &nIsRoute)) {
            eStatus = ODY_ERR_ISLAND_PROTOCOL;
        } else if (0 != nIsRoute) {
            eStatus = appendRoute(patRoutes, pnReceived, tRoute);
        }
    }
    return eStatus;
}

/***********************************************
 *
 * @Name : buyMap
 * @Def : Buys the map of the island and merges its routes into the map of
 *        odysseus.
 * @Arg : In/Out: ptOdysseus = odysseus docked at the island
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus buyMap(Odysseus *ptOdysseus) {
    Message tMessage = {0};
    OdysseusStatus eStatus = ODY_OK;
    MapRoute *atRoutes = NULL;
    char *psBuffer = NULL;
    int nReceived = 0;

    tMessage.type = MSG_BUY_MAP;
    if (-1 == asprintf(&psBuffer, INT_FORMAT, MAP_AMOUNT)) {
        return ODY_ERR_IO;
    }
    buildMessage(&tMessage, FLAG_NONE, psBuffer);
    free(psBuffer);
    psBuffer = NULL;
    if (0 != sendMessage(&tMessage, ptOdysseus->nIslandSocket)) {
        return ODY_ERR_IO;
    }

    eStatus = receiveMapRoutes(ptOdysseus, &tMessage, &atRoutes, &nReceived);
    if (ODY_OK == eStatus &&
        0 != mergeRoutes(ptOdysseus, atRoutes, nReceived)) {
        eStatus = ODY_ERR_IO;
    }

    freeMapRoutes(atRoutes, nReceived);
    if (ODY_OK != eStatus) {
        return eStatus;
    }

    ptOdysseus->tCargo.nGold -= MAP_PRICE;
    if (-1 !=
        asprintf(&psBuffer, MSG_BOUGHT_MAP,
                 ptOdysseus->tMap.atConnections[ptOdysseus->nWhereAmI].psName,
                 MAP_PRICE)) {
        println(psBuffer);
        free(psBuffer);
    }
    if (0 < nReceived) {
        println(MSG_NEW_ROUTES);
    } else {
        println(MSG_NO_ROUTES);
    }

    return ODY_OK;
}

/***********************************************
 *
 * @Name : departFromIsland
 * @Def : Tells the island that odysseus departs and closes the socket.
 * @Arg : In/Out: ptOdysseus = odysseus docked at the island
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus departFromIsland(Odysseus *ptOdysseus) {
    Message tMessage = {0};
    OdysseusStatus eStatus = ODY_OK;

    if (NO_SOCKET == ptOdysseus->nIslandSocket) {
        return ODY_OK;
    }

    tMessage.type = MSG_DEPARTURE_ISLAND;
    tMessage.flag = FLAG_NONE;

    if (0 != sendMessage(&tMessage, ptOdysseus->nIslandSocket)) {
        return ODY_ERR_IO;
    }
    eStatus =
        receiveIslandResponse(ptOdysseus, &tMessage, MSG_DEPARTURE_ISLAND);
    if (ODY_OK != eStatus) {
        return eStatus;
    }
    if (0 != strcmp(tMessage.data, OK_RESPONSE)) {
        return ODY_ERR_ISLAND_PROTOCOL;
    }

    close(ptOdysseus->nIslandSocket);
    ptOdysseus->nIslandSocket = NO_SOCKET;

    return ODY_OK;
}

/***********************************************
 *
 * @Name : sellProduct
 * @Def : Sells an amount of a product at the island and removes it from
 *        the cargo.
 * @Arg : In/Out: ptOdysseus = odysseus docked at the island
 *        In: psProduct = product to sell
 *        In: nAmount = amount to sell
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 ************************************************/
OdysseusStatus sellProduct(Odysseus *ptOdysseus, const char *psProduct,
                           int nAmount) {
    Message tMessage = {0};
    OdysseusStatus eStatus = ODY_OK;
    char *psBuffer = NULL;
    char *psSold = NULL;
    int nIncome = 0;

    tMessage.type = MSG_SELL;

    if (-1 == asprintf(&psBuffer, TRADE_SELL_FORMAT, psProduct, nAmount)) {
        return ODY_ERR_IO;
    }
    buildMessage(&tMessage, FLAG_NONE, psBuffer);
    free(psBuffer);
    psBuffer = NULL;

    if (-1 == sendMessage(&tMessage, ptOdysseus->nIslandSocket)) {
        return ODY_ERR_IO;
    }

    eStatus = receiveIslandResponse(ptOdysseus, &tMessage, MSG_SELL);
    if (ODY_OK != eStatus) {
        return eStatus;
    }
    if (0 != parseMessageTrade(tMessage.data, tMessage.dataLength, &psSold,
                               &nAmount, &nIncome)) {
        return ODY_ERR_ISLAND_PROTOCOL;
    }

    if (0 != removeSuplies(&ptOdysseus->tCargo, psSold, nAmount)) {
        return ODY_ERR_ISLAND_PROTOCOL;
    }
    ptOdysseus->tCargo.nGold += nIncome;

    if (-1 != asprintf(&psBuffer, MSG_SOLD, nAmount, psSold, nIncome)) {
        println(psBuffer);
        free(psBuffer);
    }

    return ODY_OK;
}