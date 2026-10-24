/***********************************************
 *
 * @File : odysseus_conversetion.c
 * @Purpose : Manages the conversation between odysseus and ithaca or an island.
 * @Author : Axel Boleda Callizo and Guilherme Rodrigues
 * @Date : 22/10/2026
 *
 ************************************************/

#define _GNU_SOURCE

#include "odysseus/odysseus_conversetion.h"

#define WAIT "WAIT"
#define DOCKED "DOCKED"

int disconnectFromIthaca(Odysseus *tOdysseus) {
    Message tMessage = {0};
    tMessage.type = MSG_DISCONNECT_ITHACA;
    tMessage.flag = FLAG_NONE;
    tMessage.dataLength = 0;
    tMessage.data[0] = '\0';

    if (sendMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return -1;
    }

    //Wait for the response from Ithaca
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
    }
    else if (tMessage.flag & FLAG_RESPONSE) {
        println(tMessage.data);
    }
    else {
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

    if (sendMessage(&tMessage, tOdysseus->island_socket) != 0) return -1;

    //Wait for the response from the island
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
    }
    else if (tMessage.flag & FLAG_RESPONSE) {
        if (strcmp(tMessage.data, WAIT) == 0) {
            return ARRIVAL_WAIT;
        }
        else if (strcmp(tMessage.data, DOCKED) == 0) {
            return ARRIVAL_DOCKED;
        }
        else {
            printErr(ERROR_ISLAND_RESPONSE);
            return -1;
        }
    }
    printErr(ERROR_ISLAND_RESPONSE);
    return -1;
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
        nReady = select(ptOdysseus->island_socket + 1, &tReadFds, NULL, NULL, &tTimeout);

        if (-1 == nReady) {
            return ARRIVAL_ERROR;
        }

        if (0 == nReady) {
            consumeFood(&ptOdysseus->cargo);
            if (-1 != asprintf(&psBuffer, "\n>>> The crew consumes another ration.\n"
                               "Provisions remaining: %d kg", ptOdysseus->cargo.totalAmount)) {
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

        if (MSG_ARRIVAL_ISLAND == tMessage.type && FLAG_RESPONSE == tMessage.flag &&
            0 == strcmp(tMessage.data, DOCKED)) {
            return ARRIVAL_DOCKED;
        }
    }
}