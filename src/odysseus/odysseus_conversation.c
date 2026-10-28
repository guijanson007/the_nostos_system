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

// own headers
#include "odysseus/odysseus_parser.h"
#include "utils/utils_communication.h"

#define WAIT "WAIT"
#define DOCKED "DOCKED"

OdysseusStatus connectToIthaca(Odysseus *tOdysseus) {
    struct sockaddr_in sAddr;
    int nfdSocket = -1;

    nfdSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nfdSocket < 0) {
        odysseusSetErrnoError(tOdysseus, ERROR_CREATE_SOCKET, errno);
        return ODY_ERR_SOCKET;
    }

    memset(&sAddr, 0, sizeof(sAddr));
    sAddr.sin_family = AF_INET;
    sAddr.sin_port = htons(atoi(tOdysseus->port));
    if (inet_pton(AF_INET, tOdysseus->ip, &sAddr.sin_addr) <= 0) {
        close(nfdSocket);
        return ODY_ERR_IP;
    }

    if (connect(nfdSocket, (struct sockaddr *)&sAddr, sizeof(sAddr)) < 0) {
        odysseusSetErrnoError(tOdysseus, ERROR_CONNECT_SERVER, errno);
        close(nfdSocket);
        return ODY_ERR_CONNECT;
    }
    tOdysseus->ithaca_socket = nfdSocket;
    return ODY_OK;
}

/*************************************************
 *
 * @Name : splitFields
 * @Def : Splits a '&' separated payload in place into its fields.
 * @Arg : In/Out: psData = payload, '&' are replaced by '\0'
 *        Out: apsFields = pointers to the fields found
 *        In: nMaxFields = size of apsFields
 * @Ret : Number of fields found.
 *
 **************************************************/
static int splitFields(char *psData, char **apsFields, int nMaxFields) {
    int nFields = 0;

    apsFields[nFields++] = psData;
    for (; '\0' != *psData && nFields < nMaxFields; psData++) {
        if ('&' == *psData) {
            *psData = '\0';
            apsFields[nFields++] = psData + 1;
        }
    }
    return nFields;
}

/*************************************************
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
 **************************************************/
static int fillVoyage(OdysseusVoyage *ptVoyage, int nId, const char *psObject,
                      const char *psDestination, const char *psReward) {
    if (!isPositiveInteger(psReward) && 0 != strcmp(psReward, "0")) {
        return -1;
    }
    ptVoyage->id = nId;
    ptVoyage->object = strdup(psObject);
    ptVoyage->destination = strdup(psDestination);
    ptVoyage->reward = atoi(psReward);
    if (NULL == ptVoyage->object || NULL == ptVoyage->destination) {
        freeOdysseusVoyage(ptVoyage);
        return -1;
    }
    return 0;
}

/*************************************************
 *
 * @Name : requestListOfVoyages
 * @Def : Asks Ithaca for the available voyages and stores them in the
 * Odysseus listing. Every frame of the answer is read, even after a bad one,
 * so the socket stays in sync with Ithaca.
 * @Arg : In/Out: tOdysseus = Odysseus instance
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 **************************************************/
OdysseusStatus requestListOfVoyages(Odysseus *tOdysseus) {
    Message tMessage = {0};
    char *apsFields[6] = {NULL};
    OdysseusVoyage *atVoyages = NULL;
    OdysseusStatus eStatus = ODY_OK;
    int nTotal = 0;
    int nIndex = 0;
    int nId = 0;
    int i = 0;

    tMessage.type = MSG_LIST_VOYAGES;
    tMessage.flag = FLAG_NONE;

    if (sendMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }

    // The previous listing is stale from now on
    freeListedVoyages(tOdysseus);

    for (i = 1; i <= (0 == nTotal ? 1 : nTotal); i++) {
        if (receiveMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
            eStatus = ODY_ERR_IO;
            break;
        }
        if (tMessage.type != MSG_LIST_VOYAGES ||
            !(tMessage.flag & FLAG_RESPONSE)) {
            eStatus = ODY_ERR_PROTOCOL;
            break;
        }
        if (tMessage.flag & FLAG_ERROR) {
            odysseusSetRemoteError(tOdysseus, tMessage.data,
                                   tMessage.dataLength);
            eStatus = ODY_ERR_REMOTE;
            break;
        }

        // <Index>&<Total>&<VoyageID>&<Object>&<Destination>&<Reward>
        if (6 != splitFields(tMessage.data, apsFields, 6) ||
            !isPositiveInteger(apsFields[0]) ||
            !isPositiveInteger(apsFields[1]) ||
            !isPositiveInteger(apsFields[2])) {
            eStatus = ODY_ERR_PROTOCOL;
            if (0 == nTotal) {
                break; // Without a total we can't know how many to drain
            }
            continue;
        }
        nIndex = atoi(apsFields[0]);
        nId = atoi(apsFields[2]);
        if (0 == nTotal) {
            nTotal = atoi(apsFields[1]);
            atVoyages = calloc((size_t)nTotal, sizeof(OdysseusVoyage));
        }
        if (NULL == atVoyages) {
            eStatus = ODY_ERR_PROTOCOL; // Keep draining the remaining frames
            continue;
        }
        if (ODY_OK == eStatus &&
            (nIndex != i || nTotal != atoi(apsFields[1]) ||
             0 != fillVoyage(&atVoyages[i - 1], nId, apsFields[3], apsFields[4],
                             apsFields[5]))) {
            eStatus = ODY_ERR_PROTOCOL;
        }
    }

    if (ODY_OK != eStatus) {
        for (i = 0; NULL != atVoyages && i < nTotal; i++) {
            freeOdysseusVoyage(&atVoyages[i]);
        }
        free(atVoyages);
        return eStatus;
    }

    tOdysseus->listedVoyages = atVoyages;
    tOdysseus->numListedVoyages = nTotal;
    return ODY_OK;
}

/*************************************************
 *
 * @Name : requestAcceptVoyage
 * @Def : Asks Ithaca to assign a voyage and stores it as the active voyage.
 * @Arg : In/Out: tOdysseus = Odysseus instance
 *        In: nVoyageId = id of the voyage to accept
 * @Ret : ODY_OK on success, otherwise the status code of the failure.
 *
 **************************************************/
OdysseusStatus requestAcceptVoyage(Odysseus *tOdysseus, int nVoyageId) {
    Message tMessage = {0};
    char *apsFields[5] = {NULL};
    OdysseusVoyage tVoyage = {0};
    char *psId = NULL;

    tMessage.type = MSG_ACCEPT;
    tMessage.flag = FLAG_NONE;
    if (-1 == asprintf(&psId, "%d", nVoyageId)) {
        return ODY_ERR_IO;
    }
    buildMessage(&tMessage, FLAG_NONE, psId);
    free(psId);

    if (sendMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }
    if (receiveMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }
    if (tMessage.type != MSG_ACCEPT || !(tMessage.flag & FLAG_RESPONSE)) {
        return ODY_ERR_PROTOCOL;
    }
    if (tMessage.flag & FLAG_ERROR) {
        odysseusSetRemoteError(tOdysseus, tMessage.data,
                               tMessage.dataLength);
        return ODY_ERR_REMOTE;
    }

    // OK&<VoyageID>&<Object>&<Destination>&<Reward>
    if (5 != splitFields(tMessage.data, apsFields, 5) ||
        0 != strcmp(apsFields[0], "OK") || atoi(apsFields[1]) != nVoyageId ||
        0 != fillVoyage(&tVoyage, nVoyageId, apsFields[2], apsFields[3],
                        apsFields[4])) {
        return ODY_ERR_PROTOCOL;
    }

    freeOdysseusVoyage(&tOdysseus->activeVoyage);
    tOdysseus->activeVoyage = tVoyage;
    // The ids of the old listing no longer mean anything
    freeListedVoyages(tOdysseus);
    return ODY_OK;
}

OdysseusStatus disconnectFromIthaca(Odysseus *tOdysseus) {
    Message tMessage = {0};
    tMessage.type = MSG_DISCONNECT_ITHACA;
    tMessage.flag = FLAG_NONE;
    tMessage.dataLength = 0;
    tMessage.data[0] = '\0';

    if (sendMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }

    // Wait for the response from Ithaca
    if (receiveMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }

    if (tMessage.type != MSG_DISCONNECT_ITHACA) {
        return ODY_ERR_PROTOCOL;
    }

    if (tMessage.flag & FLAG_ERROR) {
        odysseusSetRemoteError(tOdysseus, tMessage.data, tMessage.dataLength);
        return ODY_ERR_REMOTE;
    } else if (!(tMessage.flag & FLAG_RESPONSE)) {
        return ODY_ERR_PROTOCOL;
    }

    close(tOdysseus->ithaca_socket);
    tOdysseus->ithaca_socket = -1;
    return ODY_OK;
}

OdysseusStatus arriveAtIsland(Odysseus *tOdysseus, int *pnArrival) {
    Message tMessage = {0};
    tMessage.type = MSG_ARRIVAL_ISLAND;
    tMessage.flag = FLAG_NONE;
    tMessage.dataLength = strlen(tOdysseus->name);
    strcpy(tMessage.data, tOdysseus->name);

    if (sendMessage(&tMessage, tOdysseus->island_socket) != 0) {
        return ODY_ERR_IO;
    }

    // Wait for the response from the island
    if (receiveMessage(&tMessage, tOdysseus->island_socket) != 0) {
        return ODY_ERR_IO;
    }

    if (tMessage.type != MSG_ARRIVAL_ISLAND) {
        return ODY_ERR_ISLAND_PROTOCOL;
    }

    if (tMessage.flag & FLAG_ERROR) {
        odysseusSetRemoteError(tOdysseus, tMessage.data, tMessage.dataLength);
        return ODY_ERR_REMOTE;
    } else if (tMessage.flag & FLAG_RESPONSE) {
        if (strcmp(tMessage.data, WAIT) == 0) {
            *pnArrival = ARRIVAL_WAIT;
            return ODY_OK;
        } else if (strcmp(tMessage.data, DOCKED) == 0) {
            *pnArrival = ARRIVAL_DOCKED;
            return ODY_OK;
        }
    }
    return ODY_ERR_ISLAND_PROTOCOL;
}

OdysseusStatus notifyConnection(Odysseus *tOdysseus) {
    Message tMessage = {0};

    tMessage.type = MSG_CONNECT_ITHACA;
    tMessage.flag = FLAG_NONE;
    tMessage.dataLength = strlen(tOdysseus->name);
    strcpy(tMessage.data, tOdysseus->name);

    if (sendMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }

    // Wait for the response from Ithaca
    if (receiveMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return ODY_ERR_IO;
    }

    if (tMessage.type != MSG_CONNECT_ITHACA) {
        return ODY_ERR_PROTOCOL;
    }

    if (tMessage.flag & FLAG_ERROR) {
        odysseusSetRemoteError(tOdysseus, tMessage.data,
                               tMessage.dataLength);
        return ODY_ERR_REMOTE;
    } else if (tMessage.flag & FLAG_RESPONSE) {
        return ODY_OK;
    }
    return ODY_ERR_PROTOCOL;
}

OdysseusStatus waitingAtPort(Odysseus *ptOdysseus, int *pnArrival) {
    fd_set tReadFds;
    struct timeval tTimeout;
    Message tMessage;
    char *psBuffer = NULL;
    int nReady = 0;

    tTimeout.tv_sec = 5;
    tTimeout.tv_usec = 0;

    while (1) {
        FD_ZERO(&tReadFds);
        FD_SET(ptOdysseus->island_socket, &tReadFds);
        nReady = select(ptOdysseus->island_socket + 1, &tReadFds, NULL, NULL,
                        &tTimeout);

        if (-1 == nReady) {
            return ODY_ERR_IO;
        }

        if (0 == nReady) {
            consumeFood(&ptOdysseus->cargo);
            if (-1 != asprintf(&psBuffer,
                               "\n>>> The crew consumes another ration.\n"
                               "Provisions remaining: %d kg",
                               ptOdysseus->cargo.totalAmount)) {
                println(psBuffer);
                free(psBuffer);
            }
            if (0 >= ptOdysseus->cargo.totalAmount) {
                *pnArrival = ARRIVAL_LOST_AT_SEA;
                return ODY_OK;
            }
            tTimeout.tv_sec = 5;
            tTimeout.tv_usec = 0;
            continue;
        }

        if (0 != receiveMessage(&tMessage, ptOdysseus->island_socket)) {
            return ODY_ERR_IO;
        }

        if (MSG_ARRIVAL_ISLAND == tMessage.type &&
            FLAG_RESPONSE == tMessage.flag &&
            0 == strcmp(tMessage.data, DOCKED)) {
            *pnArrival = ARRIVAL_DOCKED;
            return ODY_OK;
        }
    }
}
