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

int connectToIthaca(Odysseus *tOdysseus) {
    struct sockaddr_in sAddr;
    int nfdSocket = -1;

    nfdSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nfdSocket < 0) {
        printErr(ERROR_CREATE_SOCKET);
        return -1;
    }

    memset(&sAddr, 0, sizeof(sAddr));
    sAddr.sin_family = AF_INET;
    sAddr.sin_port = htons(atoi(tOdysseus->port));
    if (inet_pton(AF_INET, tOdysseus->ip, &sAddr.sin_addr) < 0) {
        printErr(ERROR_IP_CONVERSION);
        close(nfdSocket);
        return -1;
    }

    if (connect(nfdSocket, (struct sockaddr *)&sAddr, sizeof(sAddr)) < 0) {
        printErr(ERROR_CONNECT_SOCKET);
        close(nfdSocket);
        return -1;
    }
    tOdysseus->ithaca_socket = nfdSocket;
    return 1;
}

int disconnectFromIthaca(Odysseus *tOdysseus) {
    Message tMessage = {0};
    tMessage.type = DISCONNECT_ITHACA;
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

    if (tMessage.type != DISCONNECT_ITHACA) {
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
    tMessage.type = ARRIVAL_ISLAND;
    tMessage.flag = FLAG_NONE;
    tMessage.dataLength = strlen(tOdysseus->name);
    strcpy(tMessage.data, tOdysseus->name);

    if (sendMessage(&tMessage, tOdysseus->island_socket) != 0) {
        return -1;
    }

    // Wait for the response from the island
    if (receiveMessage(&tMessage, tOdysseus->island_socket) != 0) {
        return -1;
    }

    if (tMessage.type != ARRIVAL_ISLAND) {
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

int notifyConnection(Odysseus *tOdysseus) {
    Message tMessage = {0};

    tMessage.type = CONNECT_ITHACA;
    tMessage.flag = FLAG_NONE;
    tMessage.dataLength = strlen(tOdysseus->name);
    strcpy(tMessage.data, tOdysseus->name);

    if (sendMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return -1;
    }

    // Wait for the response from Ithaca
    if (receiveMessage(&tMessage, tOdysseus->ithaca_socket) != 0) {
        return -1;
    }

    if (tMessage.type != CONNECT_ITHACA) {
        printErr(ERROR_ITHACA_RESPONSE);
        return -1;
    }

    if (tMessage.flag & FLAG_ERROR) {
        printErr(tMessage.data);
        return -1;
    } else if (tMessage.flag & FLAG_RESPONSE) {
        println(tMessage.data);
        return 0;
    } else {
        printErr(ERROR_ITHACA_RESPONSE);
        return -1;
    }
}
