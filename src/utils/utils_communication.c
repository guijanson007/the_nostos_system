#define _GNU_SOURCE

#include "utils/utils_communication.h"

void buildMessage(Message *msg, const char flags, const char *data) {
    size_t tLength = strlen(data);

    // Truncate so a too long payload never leaves stale data in the message
    if (tLength > 253) {
        tLength = 253;
    }

    msg->flag = flags;
    msg->dataLength = (unsigned char)tLength;
    memcpy(msg->data, data, tLength);
    msg->data[tLength] = '\0';
}

int sendMessage(const Message *msg, int fd) {
    unsigned char buffer[MESSAGE_SIZE] = {0};
    if (msg == NULL || fd < 0 || msg->dataLength > 253) {
        return -1; 
    }

    buffer[0] = msg->type;
    buffer[1] = msg->flag;
    buffer[2] = msg->dataLength;
    memcpy(buffer + 3, msg->data, msg->dataLength);

    if (MESSAGE_SIZE != write(fd, buffer, MESSAGE_SIZE)) {
        return -1;
    }

    return 0;
}


int receiveMessage(Message *msg, int fd) {
    unsigned char buffer[MESSAGE_SIZE] = {0};
    if (fd < 0 || NULL == msg) {
        return -1; 
    }

    if (read(fd, buffer, MESSAGE_SIZE) != MESSAGE_SIZE) {
        return -1;
    }

    if (buffer[2] > 253) {
        return -1;
    }
    
    msg->type = buffer[0];
    msg->flag = buffer[1];
    msg->dataLength = buffer[2];
    memcpy(msg->data, buffer + 3, msg->dataLength);
    msg->data[msg->dataLength] = '\0';
    
    return 0;
}