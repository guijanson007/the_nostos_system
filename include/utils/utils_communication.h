#ifndef _UTILS_COMMUNICATION_H
#define _UTILS_COMMUNICATION_H

/***********************************************
 *
 * @File : utils_communication.h
 * @Purpose : messages of the communication protocol between the processes
 *            and the functions to open connections and send / receive
 *            messages
 * @Author : Axel Boleda Callizo
 * @Date : 31/10/2026
 *
 ************************************************/

// System includes
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Project includes
#include "utils/utils.h"

// Constant declarations
#define MESSAGE_SIZE 256
#define MESSAGE_DATA_MAX 253

// Position of each field inside the frame sent through the socket
#define FRAME_TYPE_POS 0
#define FRAME_FLAG_POS 1
#define FRAME_LENGTH_POS 2
#define FRAME_DATA_POS 3

// Maps are not in stock.db: fixed product, quantity and price
#define MAP_PRODUCT "MAP"
#define MAP_AMOUNT 1
#define MAP_PRICE 50

#define FLAG_NONE 0x00
#define FLAG_RESPONSE 0x80
#define FLAG_ERROR 0x40
#define FLAG_BINARY 0x20
#define FLAG_RESERVED_MASK 0x1F

#define NACK_INVALID_TYPE "INVALID_TYPE"
#define NACK_INVALID_LENGTH "INVALID_LENGTH"
#define NACK_INVALID_FLAGS "INVALID_FLAGS"

#define MAX_CONN 10
#define ERROR_SOCKET "ERROR: the socket could not be created\n"
#define ERR_CONNECT "ERROR: Could not connect to the server\n"
#define ERROR_CONVERT_IP "ERROR: Could not convert IP address\n"

// Custom type definitions
typedef enum {
    MSG_CONNECT_ITHACA = 0x01,
    MSG_LIST_VOYAGES = 0x02,
    MSG_ACCEPT = 0x03,
    MSG_CLAIM = 0x04,
    MSG_DISCONNECT_ITHACA = 0x05,
    MSG_ARRIVAL_ISLAND = 0x10,
    MSG_LIST_MARKET = 0x11,
    MSG_BUY = 0x12,
    MSG_SELL = 0x13,
    MSG_BUY_MAP = 0x14,
    MSG_DELIVER = 0x15,
    MSG_DEPARTURE_ISLAND = 0x16,
    MSG_NACK = 0x7F
} MessageType;

typedef struct {
    unsigned char type;
    unsigned char flag;
    unsigned char dataLength;
    char data[MESSAGE_DATA_MAX + 1];
} Message;

//Functions
void buildMessage(Message *ptMsg, const char cFlags, const char *psData);
int sendMessage(const Message *ptMsg, int nFd);
int receiveMessage(Message *ptMsg, int nFd);
void sendNack(int nFd, const char *psReason);
int openListenConn(char *psIp, int nPort);
int openClientConn(char *psIp, int nPort);

#endif
