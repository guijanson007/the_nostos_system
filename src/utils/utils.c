/***********************************************
 *
 * @File : utils.c
 * @Purpose : implementation of utility functions for file and console
 * operations
 * @Author : Guilherme Rodrigues and Axel Boleda Callizo
 * @Date : 20/09/2026
 *
 ************************************************/

#define _GNU_SOURCE 1

// header
#include "../include/utils/utils.h"

/***********************************************
 *
 * @Name : freeLines
 * @Def : Frees every string of an array of strings and then the array itself.
 * @Arg : In: apsLines = array of dynamically allocated strings
 *        In: tCount = number of strings in the array
 * @Ret : None.
 *
 ************************************************/

void freeLines(char **apsLines, size_t tCount) {
    size_t i = 0;

    if (NULL == apsLines) {
        return;
    }
    for (i = 0; i < tCount; i++) {
        free(apsLines[i]);
    }
    free(apsLines);
}

/***********************************************
 *
 * @Name : writeToFileAndStdOut
 * @Def : Writes a message to a file descriptor and to the standard output.
 * @Arg : In: nFd = file descriptor to write to
 *        In: psMessage = message to write
 * @Ret : None.
 *
 ************************************************/

void writeToFileAndStdOut(int nFd, const char *psMessage) {
    write(nFd, psMessage, strlen(psMessage));
    write(STDOUT_FILENO, psMessage, strlen(psMessage));
}

/***********************************************
 *
 * @Name : println
 * @Def : Writes a message followed by a newline to the standard output.
 * @Arg : In: psMessage is the message to write
 * @Ret : None.
 *
 ************************************************/

void println(const char *psMessage) {
    write(STDOUT_FILENO, psMessage, strlen(psMessage));
    write(STDOUT_FILENO, "\n", strlen("\n"));
}

/***********************************************
 *
 * @Name : printErr
 * @Def : Writes a message followed by a newline to the standard error.
 * @Arg : In: psMessage = message to write
 * @Ret : None.
 *
 ************************************************/

void printErr(const char *psMessage) {
    write(STDERR_FILENO, psMessage, strlen(psMessage));
    write(STDERR_FILENO, "\n", strlen("\n"));
}

/***********************************************
 *
 * @Name : strToUpper
 * @Def : Converts a string to uppercase in place.
 * @Arg : In: psString = string to convert
 * @Ret : None.
 *
 ***********************************************/

void strToUpper(char *psString) {
    if (NULL == psString) {
        return;
    }
    for (; *psString; ++psString) {
        if ('a' <= *psString && *psString <= 'z') {
            *psString -= 32; // Convert to uppercase
        }
    }
}

char *readUntil(int fd, char cEnd) {
    int i = 0;
    int chars_read = 0;
    char c = ' ';
    char *buffer = NULL;

    while (c != cEnd) {
        chars_read = (int)read(fd, &c, sizeof(char));
        if (chars_read <= 0) {
            free(buffer);
            return NULL;
        }
        if (buffer == NULL) {
            buffer = (char *)malloc(sizeof(char));
        }
        if (chars_read > 0) {
            if (c != cEnd) {
                buffer[i] = c;
                buffer = (char *)realloc(buffer, (sizeof(char)) * (i + 2));
            }
            i++;
        }
    }

    buffer[i - 1] = '\0';
    return buffer;
}

/***********************************************
 *
 * @Name : readUntilEmpty
 * @Def : Reads elements from a file descriptor (can be a pipe) until it is
 * empty. Works similarly to malloc.
 * @Arg : In: nFd = file descriptor to be read
 *        In: tElementSize = size of the element to be read (use 1 byte for
 * reading byte by byte) Out: ptOutCount = amount of elements read
 * @Ret : Dynamically allocated array with the elements read, or NULL if nothing
 * was read or an error occurred.
 *
 ************************************************/
void *readUntilEmpty(int nFd, size_t tElementSize, size_t *ptOutCount) {
    int nCapacity = INITIAL_ELEMENT_CAPACITY;
    int nCount = 0;
    void *pvBuffer = NULL;
    char *pcTemp = NULL;
    ssize_t lBytesRead = 0;
    size_t tTotalRead = 0;

    if (0 == tElementSize || NULL == ptOutCount) {
        return NULL;
    }

    pvBuffer = malloc(tElementSize * nCapacity);
    if (NULL == pvBuffer) {
        printErr("Malloc error");
        return NULL;
    }

    while (0 < (lBytesRead = read(nFd, pvBuffer + (nCount * tElementSize),
                                  tElementSize))) {
        // Handle partial reads (for pipes)
        tTotalRead = lBytesRead;
        while (tTotalRead < tElementSize) {
            lBytesRead =
                read(nFd, pvBuffer + (nCount * tElementSize) + tTotalRead,
                     tElementSize - tTotalRead);
            if (0 >= lBytesRead) {
                break;
            }
            tTotalRead += lBytesRead;
        }

        // EOF or read error
        if (tTotalRead < tElementSize) {
            break;
        }

        nCount++;

        // Resize check before next read
        if (nCount == nCapacity) {
            nCapacity *= 2;
            pcTemp = realloc(pvBuffer, nCapacity * tElementSize);

            if (NULL == pcTemp) {
                printErr("Realloc error");
                free(pvBuffer);
                return NULL;
            }
            pvBuffer = pcTemp;
        }
    }

    // Check if it's a true error and not just an interruption
    if (0 > lBytesRead && EINTR != errno) {
        printErr("Read error");
        free(pvBuffer);
        return NULL;
    }

    if (0 == nCount) {
        free(pvBuffer);
        *ptOutCount = 0;
        return NULL;
    }

    if (nCount < nCapacity) {
        pcTemp = realloc(pvBuffer, nCount * tElementSize);
        if (NULL != pcTemp) {
            pvBuffer = pcTemp;
        }
    }

    *ptOutCount = nCount;
    return pvBuffer;
}

/***********************************************
 *
 * @Name : readLine
 * @Def : Reads one line from a file descriptor, removing the trailing '\n' (and
 * '\r' if present).
 * @Arg : In: nFd = file descriptor to be read
 *        Out: ppsBuffer = allocated string where the line is stored
 * @Ret : Returns 1 if a line was read, 0 on EOF with nothing read, -1 on a
 * read or allocation error.
 *
 ************************************************/
int readLine(int nFd, char **ppsBuffer) {
    char cChar = '\0';
    ssize_t tBytesRead = 0;
    size_t tLength = 0;
    char *psBuffer = NULL;
    size_t tBufferSize = INITIAL_LINES_CAPACITY;

    if (NULL == ppsBuffer) {
        return -1;
    }

    psBuffer = malloc(tBufferSize);
    if (NULL == psBuffer) {
        return -1;
    }

    while (0 < (tBytesRead = read(nFd, &cChar, 1))) {
        if ('\n' == cChar) {
            break;
        }

        if (tLength + 1 >= tBufferSize) {
            char *psTemp = realloc(psBuffer, tBufferSize + 1);
            if (NULL == psTemp) {
                free(psBuffer);
                return -1;
            }
            psBuffer = psTemp;
            tBufferSize++;
        }
        psBuffer[tLength++] = cChar;
    }

    if (-1 == tBytesRead) {
        free(psBuffer);
        return -1;
    }

    if (0 == tBytesRead && 0 == tLength) {
        free(psBuffer);
        return 0;
    }

    if (0 < tLength && '\r' == psBuffer[tLength - 1]) {
        tLength--;
    }
    psBuffer[tLength] = '\0';
    *ppsBuffer = psBuffer;
    return 1;
}

/***********************************************
 *
 * @Name : readLines
 * @Def : Reads every line from a file descriptor.
 * @Arg : In: nFd = file descriptor to be read
 *        Out: ptOutCount = number of lines read
 * @Ret : Dynamically allocated array of strings, one per line read, or NULL if
 * an error occurred.
 *
 ************************************************/

char **readLines(int nFd, size_t *ptOutCount) {
    size_t tLinesCapacity = INITIAL_LINES_CAPACITY;
    size_t tLinesCount = 0;
    size_t tNewCapacity = 0;
    size_t i = 0;
    char **apsLines = NULL;
    char **apsTemp = NULL;
    char *psLine = NULL;
    int nReadResult = 0;

    if (NULL == ptOutCount) {
        return NULL;
    }
    *ptOutCount = 0;

    apsLines = malloc(tLinesCapacity * sizeof(char *));
    if (NULL == apsLines) {
        return NULL;
    }

    while (1 == (nReadResult = readLine(nFd, &psLine))) {
        if (tLinesCount >= tLinesCapacity) {
            tNewCapacity = tLinesCapacity * 2;
            apsTemp = realloc(apsLines, tNewCapacity * sizeof(char *));
            if (NULL == apsTemp) {
                free(psLine);
                for (i = 0; i < tLinesCount; i++) {
                    free(apsLines[i]);
                }
                free(apsLines);
                return NULL;
            }
            apsLines = apsTemp;
            tLinesCapacity = tNewCapacity;
        }

        apsLines[tLinesCount] = psLine;
        psLine = NULL;
        tLinesCount++;
    }

    if (-1 == nReadResult) {
        for (i = 0; i < tLinesCount; i++) {
            free(apsLines[i]);
        }
        free(apsLines);
        return NULL;
    }

    *ptOutCount = tLinesCount;
    return apsLines;
}

/***********************************************
 *
 * @Name : readBinary
 * @Def : Reads fixed-size binary elements from a file descriptor until EOF.
 * @Arg : In: nFd = file descriptor to be read
 *        In: tElementSize = size in bytes of each element
 *        Out: ptOutCount = number of elements read
 * @Ret : Dynamically allocated array with the elements read, or NULL if nothing
 * was read, a partial element was found, or an error occurred.
 *
 ************************************************/
void *readBinary(int nFd, size_t tElementSize, size_t *ptOutCount) {
    size_t tCapacity = INITIAL_BINARY_CAPACITY;
    size_t tCount = 0;
    char *pcBuffer = NULL;
    char *pcTemp = NULL;
    char *pcDestination = NULL;
    char *pcExact = NULL;
    int nDone = 0;
    int nBytesRead = 0;

    if (NULL == ptOutCount) {
        return NULL;
    }
    *ptOutCount = 0;
    if (0 == tElementSize) {
        return NULL;
    }

    pcBuffer = malloc(tCapacity * tElementSize);
    if (NULL == pcBuffer) {
        return NULL;
    }

    while (0 == nDone) {
        if (tCount >= tCapacity) {
            tCapacity *= 2;
            pcTemp = realloc(pcBuffer, tCapacity * tElementSize);
            if (NULL == pcTemp) {
                free(pcBuffer);
                return NULL;
            }
            pcBuffer = pcTemp;
        }

        pcDestination = pcBuffer + tCount * tElementSize;
        nBytesRead = read(nFd, pcDestination, tElementSize);
        if (0 > nBytesRead) {
            free(pcBuffer);
            return NULL;
        } else if (0 == nBytesRead) {
            nDone = 1;
        } else if ((size_t)nBytesRead != tElementSize) {
            free(pcBuffer);
            return NULL;
        } else {
            tCount++;
        }
    }

    if (0 == tCount) {
        free(pcBuffer);
        *ptOutCount = 0;
        return NULL;
    }

    pcExact = realloc(pcBuffer, tCount * tElementSize);
    if (NULL != pcExact) {
        pcBuffer = pcExact;
    }

    *ptOutCount = tCount;
    return pcBuffer;
}

/***********************************************
 *
 * @Name : isAllDigits
 * @Def : Checks whether a string is non-empty and contains only digits.
 * @Arg : In: psString = string to check
 * @Ret : Returns 1 if every character is a digit, 0 otherwise.
 *
 ************************************************/

int isAllDigits(const char *psString) {
    const char *pcCurrent = NULL;

    if (NULL == psString || '\0' == *psString) {
        return 0;
    }
    for (pcCurrent = psString; '\0' != *pcCurrent; pcCurrent++) {
        if ('0' > *pcCurrent || '9' < *pcCurrent) {
            return 0;
        }
    }
    return 1;
}

/***********************************************
 *
 * @Name : isPositiveInteger
 * @Def : Checks whether a string represents an integer greater than 0.
 * @Arg : In: psString = string to check
 * @Ret : Returns 1 if the string is a positive integer, 0 otherwise.
 *
 ************************************************/

int isPositiveInteger(const char *psString) {
    if (NULL == psString || '\0' == *psString) {
        return 0;
    }

    if (0 == isAllDigits(psString)) {
        return 0;
    }

    return (0 < atoi(psString));
}

int openListenConn(char *ip, int port) {
    struct sockaddr_in s_addr;
    int fd_socket = -1;

    fd_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (fd_socket < 0) {
        return -1;
    }

    int nReuse = 1;
    setsockopt(fd_socket, SOL_SOCKET, SO_REUSEADDR, &nReuse, sizeof(nReuse));

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);

    struct hostent *host = gethostbyname(ip);
    if (host == NULL) {
        close(fd_socket);
        return -2;
    }
    memcpy(&s_addr.sin_addr, host->h_addr_list[0], host->h_length);

    if (bind(fd_socket, (void *)&s_addr, sizeof(s_addr)) < 0) {
        close(fd_socket);
        return -3;
    }

    if (listen(fd_socket, MAX_CONN) < 0) {
        close(fd_socket);
        return -1;
    }

    return fd_socket;
}

int openClientConn(char *ip, int port) {
    struct sockaddr_in s_addr;
    int fd_socket = -1;

    fd_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd_socket < 0) {
        printErr(ERROR_SOCKET);
        return -1;
    }

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &s_addr.sin_addr) != 1) {
        printErr("ERROR: Could not convert IP address\n");
        close(fd_socket);
        return -1;
    }

    if (connect(fd_socket, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0) {
        printErr(ERR_CONNECT);
        close(fd_socket);
        return -1;
    }

    return fd_socket;
}
