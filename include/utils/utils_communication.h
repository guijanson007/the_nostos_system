#ifndef _UTILS_COMMUNICATION_H
#define _UTILS_COMMUNICATION_H

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

#define MESSAGE_SIZE 256

// Maps are not in stock.db: fixed product, quantity and price
#define MAP_PRODUCT "MAP"
#define MAP_AMOUNT 1
#define MAP_PRICE 50

#define FLAG_NONE 0x00
#define FLAG_RESPONSE 0x80
#define FLAG_ERROR 0x40
#define FLAG_BINARY 0x20

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
} MessageType;

typedef struct {
    unsigned char type;
    unsigned char flag;
    unsigned char dataLength;
    char data[254];
} Message;

void buildMessage(Message *msg, const char flags, const char *data);
int sendMessage(const Message *msg, int fd);
int receiveMessage(Message *msg, int fd);

#endif
