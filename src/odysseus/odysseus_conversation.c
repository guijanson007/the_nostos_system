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
#include "utils/utils_communication.h"

#define WAIT "WAIT"
#define DOCKED "DOCKED"

OdysseusStatus connectToIthaca(Odysseus *tOdysseus) {
    struct sockaddr_in sAddr;
    int nfdSocket = -1;

    nfdSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nfdSocket < 0) {
        odysseusSetError(tOdysseus, "ERROR: Could not create socket: %s",
                         strerror(errno));
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
        odysseusSetError(tOdysseus,
                         "ERROR: Could not connect to the server: %s",
                         strerror(errno));
        close(nfdSocket);
        return ODY_ERR_CONNECT;
    }
    tOdysseus->ithaca_socket = nfdSocket;
    return ODY_OK;
}

int disconnectFromIthaca(Odysseus *tOdysseus) {
    Message tMessage = {0};
    tMessage.type = MSG_DISCONNECT_ITHACA;
    tMessage.flag = FLAG_NONE;
    tMessage.dataLength = 0;
    tMessage.data[0] = '\0';

    if (sendMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return -1;
    }

    // Wait for the response from Ithaca
    if (receiveMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return -1;
    }

    if (tMessage.type != MSG_DISCONNECT_ITHACA) {
        printErr(ERROR_ITHACA_RESPONSE);
        return -1;
    }

    if (tMessage.flag & FLAG_ERROR) {
        printErr(tMessage.data);
        return -1;
    } else if (tMessage.flag & FLAG_RESPONSE) {
        println(tMessage.data);
    } else {
        printErr(ERROR_ITHACA_RESPONSE);
        return -1;
    }

    close(tOdysseus->ithaca_socket);
    tOdysseus->ithaca_socket = -1;
    return 0;
}

int arriveAtIsland(Odysseus *tOdysseus) {
    Message tMessage = {0};
    tMessage.type = MSG_ARRIVAL_ISLAND;
    tMessage.flag = FLAG_NONE;
    tMessage.dataLength = strlen(tOdysseus->name);
    strcpy(tMessage.data, tOdysseus->name);

    if (sendMessage(&tMessage, tOdysseus->island_socket) != 0)
        return -1;

    // Wait for the response from the island
    if (receiveMessage(&tMessage, tOdysseus->island_socket) != 0) {
        return -1;
    }

    if (tMessage.type != MSG_ARRIVAL_ISLAND) {
        printErr(ERROR_ISLAND_RESPONSE);
        return -1;
    }

    if (tMessage.flag & FLAG_ERROR) {
        printErr(tMessage.data);
        return -1;
    } else if (tMessage.flag & FLAG_RESPONSE) {
        if (strcmp(tMessage.data, WAIT) == 0) {
            return ARRIVAL_WAIT;
        } else if (strcmp(tMessage.data, DOCKED) == 0) {
            return ARRIVAL_DOCKED;
        } else {
            printErr(ERROR_ISLAND_RESPONSE);
            return -1;
        }
    }
    printErr(ERROR_ISLAND_RESPONSE);
    return -1;
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
        odysseusSetError(tOdysseus, "%.*s", (int)tMessage.dataLength,
                         tMessage.data);
        return ODY_ERR_REMOTE;
    } else if (tMessage.flag & FLAG_RESPONSE) {
        return ODY_OK;
    }
    return ODY_ERR_PROTOCOL;
}

int waitingAtPort(Odysseus *ptOdysseus) {
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
            return ARRIVAL_ERROR;
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
                return ARRIVAL_LOST_AT_SEA;
            }
            tTimeout.tv_sec = 5;
            tTimeout.tv_usec = 0;
            continue;
        }

        if (0 != receiveMessage(&tMessage, ptOdysseus->island_socket)) {
            printErr(ERROR_ISLAND_RESPONSE);
            return ARRIVAL_ERROR;
        }

        if (MSG_ARRIVAL_ISLAND == tMessage.type &&
            FLAG_RESPONSE == tMessage.flag &&
            0 == strcmp(tMessage.data, DOCKED)) {
            return ARRIVAL_DOCKED;
        }
    }
}

static int parseMessageMarket(char *psData, int nLength, int *pnIndex,
                              int *pnTotal, char **ppsProduct, int *pnAmount,
                              int *pnPrice) {
    char *psIndex = NULL;
    char *psTotal = NULL;
    char *psAmount = NULL;
    char *psPrice = NULL;

    if (NULL == psData || nLength <= 0 || nLength > 253) {
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
    if (!isAllDigits(psIndex) || !isAllDigits(psTotal) ||
        !isAllDigits(psAmount) || !isAllDigits(psPrice)) {
        return -1;
    }

    *pnIndex = atoi(psIndex);
    *pnTotal = atoi(psTotal);
    *pnAmount = atoi(psAmount);
    *pnPrice = atoi(psPrice);

    if (*pnIndex < 1 || *pnIndex > *pnTotal) {
        return -1;
    }
    return 0;
}

OdysseusStatus getListMarket(Odysseus *tOdysseus) {
    int index = 0;
    int total = 0;
    char *product;
    int amount = 0;
    int price = 0;

    char *buff = NULL;

    Message msg;
    msg.type = MSG_LIST_MARKET;
    msg.flag = FLAG_NONE;
    msg.dataLength = 0;
    msg.data[0] = '\0';

    if (sendMessage(&msg, tOdysseus->island_socket) != 0) {
        return ODY_ERR_REMOTE;
    }

    asprintf(&buff, "--- %s MARKET ---",
             tOdysseus->map.connections[tOdysseus->whereAmI].name);
    println(buff);
    free(buff);
    while (1) {
        if (receiveMessage(&msg, tOdysseus->island_socket) != 0)
            return ODY_ERR_REMOTE;

        if (msg.type != MSG_LIST_MARKET || msg.dataLength == 0)
            return ODY_ERR_INVALID_COMMAND;

        if (msg.flag & FLAG_ERROR) {
            odysseusSetError(tOdysseus, "%.*s", (int)msg.dataLength, msg.data);
            return ODY_ERR_REMOTE;
        }

        if (0 != parseMessageMarket(msg.data, msg.dataLength, &index, &total,
                                    &product, &amount, &price)) {
            return ODY_ERR_REMOTE;
        }

        if (index == total) { // MAP
            println("...");
            asprintf(&buff, "%s\t\t\t%d gold", product, price);
            println(buff);
            free(buff);
            break;
        }

        asprintf(&buff, "%s\t%d kg\t %d gold / kg", product, amount, price);
        println(buff);
        free(buff);
    }

    return ODY_OK;
}