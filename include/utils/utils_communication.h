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

#define FLAG_NONE 0x00
#define FLAG_RESPONSE 0x80
#define FLAG_ERROR 0x40
#define FLAG_BINARY 0x20

typedef enum {
    CONNECT_ITHACA = 0x01,
    LIST_VOYAGES = 0x02,
    ACCEPT = 0x03,
    CLAIM = 0x04,
    DISCONNECT_ITHACA = 0x05,
    ARRIVAL_ISLAND = 0x10,
    LIST_MARKET = 0x11,
    BUY = 0x12,
    SELL = 0x13,
    BUY_MAP = 0x14,
    DELIVER = 0x15,
    DEPARTURE_ISLAND = 0x16,
} MessageType;

typedef struct {
    unsigned char type;
    unsigned char flag;
    unsigned char dataLength;
    char data[254];
} Message;

int sendMessage(const Message *msg, int fd);
int receiveMessage(Message *msg, int fd);

#endif
