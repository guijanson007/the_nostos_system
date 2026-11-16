#define _GNU_SOURCE

#include "utils/utils_communication.h"

static size_t readFull(int nFd, unsigned char *pbyBuffer, size_t tSize) {
    size_t tRead = 0;
    int nBytes = 0;

    while (tRead < tSize) {
        nBytes = read(nFd, pbyBuffer + tRead, tSize - tRead);
        if (0 >= nBytes) {
            break;
        }
        tRead += (size_t)nBytes;
    }
    return tRead;
}

static size_t writeFull(int nFd, const unsigned char *pbyBuffer, size_t tSize) {
    size_t tWritten = 0;
    int nBytes = 0;

    while (tWritten < tSize) {
        nBytes = write(nFd, pbyBuffer + tWritten, tSize - tWritten);
        if (0 >= nBytes) {
            break;
        }
        tWritten += (size_t)nBytes;
    }
    return tWritten;
}

/***********************************************
 *
 * @Name : buildBinaryMessage
 * @Def : Fills the flags, length and data of a message with raw bytes.
 *        Unlike buildMessage, the data can contain '\0' bytes.
 * @Arg : Out: ptMsg = message to fill
 *        In: cFlags = flags of the message
 *        In: pData = bytes to copy
 *        In: nLength = number of bytes in pData
 * @Ret : -
 *
 ************************************************/
void buildBinaryMessage(Message *ptMsg, unsigned char cFlags, const char *pData, int nLength) {
    if (0 > nLength) {
        nLength = 0;
    }
    else if (MESSAGE_DATA_MAX < nLength) {
        nLength = MESSAGE_DATA_MAX;
    }

    ptMsg->flag = cFlags;
    ptMsg->dataLength = (unsigned char)nLength;
    memcpy(ptMsg->data, pData, (size_t)nLength);
    ptMsg->data[nLength] = '\0';
}

/***********************************************
 *
 * @Name : buildMessage
 * @Def : Fills the flags and the data of a message. The data is cut to
 *        MESSAGE_DATA_MAX characters.
 * @Arg : In/Out: ptMsg = message to fill (its type is not changed)
 *        In: cFlags = flags of the message
 *        In: psData = text of the message
 * @Ret : None.
 *
 ************************************************/
void buildMessage(Message *ptMsg, const char cFlags, const char *psData) {
    size_t tLength = strlen(psData);

    buildBinaryMessage(ptMsg, cFlags, psData, (int)tLength);
}

/***********************************************
 *
 * @Name : sendMessage
 * @Def : Sends a message through a socket as a frame of MESSAGE_SIZE
 *        bytes: type, flag, data length and data.
 * @Arg : In: ptMsg = message to send
 *        In: nFd = socket to send it through
 * @Ret : 0 if the whole frame was sent, -1 otherwise.
 *
 ************************************************/
int sendMessage(const Message *ptMsg, int nFd) {
    unsigned char *pbyBuffer = NULL;

    if (NULL == ptMsg || 0 > nFd || MESSAGE_DATA_MAX < ptMsg->dataLength) {
        return -1;
    }

    pbyBuffer = calloc(MESSAGE_SIZE, sizeof(unsigned char));
    if (NULL == pbyBuffer) {
        return -1;
    }

    pbyBuffer[FRAME_TYPE_POS] = ptMsg->type;
    pbyBuffer[FRAME_FLAG_POS] = ptMsg->flag;
    pbyBuffer[FRAME_LENGTH_POS] = ptMsg->dataLength;
    memcpy(pbyBuffer + FRAME_DATA_POS, ptMsg->data, ptMsg->dataLength);

    if (MESSAGE_SIZE != writeFull(nFd, pbyBuffer, MESSAGE_SIZE)) {
        free(pbyBuffer);
        return -1;
    }

    free(pbyBuffer);
    return 0;
}

/***********************************************
 *
 * @Name : receiveMessage
 * @Def : Reads one frame of MESSAGE_SIZE bytes from a socket (looping until
 *        the whole frame has arrived) and checks
 *        its data length and flags.
 * @Arg : Out: ptMsg = message received
 *        In: nFd = socket to read from
 * @Ret : 0 if a valid message was received, -1 on read error, -2 if the
 *        data length is too big, -3 if reserved flags are set.
 *
 ************************************************/
int receiveMessage(Message *ptMsg, int nFd) {
    unsigned char *pbyBuffer = NULL;    

    if (0 > nFd || NULL == ptMsg) {
        return -1;
    }

    pbyBuffer = calloc(MESSAGE_SIZE, sizeof(unsigned char));
    if (NULL == pbyBuffer) {
        return -1;
    }

    if (MESSAGE_SIZE != readFull(nFd, pbyBuffer, MESSAGE_SIZE)) {
        free(pbyBuffer);
        return -1;
    }

    if (MESSAGE_DATA_MAX < pbyBuffer[FRAME_LENGTH_POS]) {
        free(pbyBuffer);
        return -2;
    }
    if (0 != (pbyBuffer[FRAME_FLAG_POS] & FLAG_RESERVED_MASK)) {
        free(pbyBuffer);
        return -3;
    }

    ptMsg->type = pbyBuffer[FRAME_TYPE_POS];
    ptMsg->flag = pbyBuffer[FRAME_FLAG_POS];
    ptMsg->dataLength = pbyBuffer[FRAME_LENGTH_POS];
    memcpy(ptMsg->data, pbyBuffer + FRAME_DATA_POS, ptMsg->dataLength);
    ptMsg->data[ptMsg->dataLength] = '\0';

    free(pbyBuffer);
    return 0;
}

/***********************************************
 *
 * @Name : receiveMessageTimeout
 * @Def : Waits at most nSeconds for a frame on the socket and reads it.
 * @Arg : Out: ptMsg = message received
 *        In: nFd = socket to read from
 *        In: nSeconds = maximum time to wait
 * @Ret : 0 if a valid message was received, -1 on read error, closed
 *        socket or signal, -2 if the data length is too big, -3 if
 *        reserved flags are set, RECEIVE_TIMEOUT if nothing arrived.
 *
 ************************************************/
int receiveMessageTimeout(Message *ptMsg, int nFd, int nSeconds) {
    fd_set tReadFds;
    struct timeval stTimeout;
    int nReady = 0;

    if (0 > nFd || NULL == ptMsg || 0 > nSeconds) {
        return -1;
    }

    FD_ZERO(&tReadFds);
    FD_SET(nFd, &tReadFds);
    stTimeout.tv_sec = nSeconds;
    stTimeout.tv_usec = 0;

    nReady = select(nFd + 1, &tReadFds, NULL, NULL, &stTimeout);
    if (-1 == nReady) {
        return -1;
    }
    if (0 == nReady) {
        return RECEIVE_TIMEOUT;
    }

    return receiveMessage(ptMsg, nFd);
}

/***********************************************
 *
 * @Name : sendNack
 * @Def : Sends a NACK message with the reason of the rejection.
 * @Arg : In: nFd = socket to send it through
 *        In: psReason = reason of the NACK
 * @Ret : None.
 *
 ************************************************/
void sendNack(int nFd, const char *psReason) {
    Message tMsg = {0};

    tMsg.type = MSG_NACK;
    buildMessage(&tMsg, FLAG_RESPONSE | FLAG_ERROR, psReason);
    sendMessage(&tMsg, nFd);
}

/***********************************************
 *
 * @Name : openListenConn
 * @Def : Opens a TCP socket that listens on the given ip and port.
 * @Arg : In: psIp = ip (or host name) to listen on
 *        In: nPort = port to listen on
 * @Ret : File descriptor of the socket, -1 if the socket couldn't be
 *        created or couldn't listen, -2 if the host couldn't be resolved,
 *        -3 if the bind failed.
 *
 ************************************************/
int openListenConn(char *psIp, int nPort) {
    struct sockaddr_in stAddr = {0};
    struct hostent *pstHost = NULL;
    int nSocketFd = -1;
    int nReuse = 1;

    nSocketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (0 > nSocketFd) {
        return -1;
    }

    setsockopt(nSocketFd, SOL_SOCKET, SO_REUSEADDR, &nReuse, sizeof(nReuse));

    stAddr.sin_family = AF_INET;
    stAddr.sin_port = htons(nPort);

    pstHost = gethostbyname(psIp);
    if (NULL == pstHost) {
        close(nSocketFd);
        return -2;
    }
    memcpy(&stAddr.sin_addr, pstHost->h_addr_list[0], pstHost->h_length);

    if (0 > bind(nSocketFd, (void *)&stAddr, sizeof(stAddr))) {
        close(nSocketFd);
        return -3;
    }

    if (0 > listen(nSocketFd, MAX_CONN)) {
        close(nSocketFd);
        return -1;
    }

    return nSocketFd;
}

/***********************************************
 *
 * @Name : openClientConn
 * @Def : Opens a TCP connection to the given ip and port.
 * @Arg : In: psIp = ip to connect to
 *        In: nPort = port to connect to
 * @Ret : File descriptor of the socket, or -1 if something failed (the
 *        error is printed).
 *
 ************************************************/
int openClientConn(char *psIp, int nPort) {
    struct sockaddr_in stAddr = {0};
    int nSocketFd = -1;

    nSocketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (0 > nSocketFd) {
        printErr(ERROR_SOCKET);
        return -1;
    }

    stAddr.sin_family = AF_INET;
    stAddr.sin_port = htons(nPort);
    if (1 != inet_pton(AF_INET, psIp, &stAddr.sin_addr)) {
        printErr(ERROR_CONVERT_IP);
        close(nSocketFd);
        return -1;
    }

    if (0 > connect(nSocketFd, (struct sockaddr *)&stAddr, sizeof(stAddr))) {
        printErr(ERR_CONNECT);
        close(nSocketFd);
        return -1;
    }

    return nSocketFd;
}
