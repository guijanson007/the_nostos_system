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
 * @Def : Frees every string of an array of strings and then the array
 *        itself.
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
 * @Arg : In: psMessage = message to write
 * @Ret : None.
 *
 ************************************************/
void println(const char *psMessage) {
    write(STDOUT_FILENO, psMessage, strlen(psMessage));
    write(STDOUT_FILENO, NEW_LINE, strlen(NEW_LINE));
}
 
/***********************************************
 *
 * @Name : print
 * @Def : Writes a message to the standard output.
 * @Arg : In: psMessage = message to write
 * @Ret : None.
 *
 ************************************************/
void print(const char *psMessage) {
    write(STDOUT_FILENO, psMessage, strlen(psMessage));
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
    write(STDERR_FILENO, NEW_LINE, strlen(NEW_LINE));
}
 
/***********************************************
 *
 * @Name : strToUpper
 * @Def : Converts a string to uppercase in place.
 * @Arg : In/Out: psString = string to convert
 * @Ret : None.
 *
 ************************************************/
void strToUpper(char *psString) {
    int i = 0;
 
    if (NULL == psString) {
        return;
    }
    for (i = 0; '\0' != psString[i]; i++) {
        if ('a' <= psString[i] && psString[i] <= 'z') {
            // Convert to uppercase
            psString[i] -= UPPER_CASE_OFFSET;
        }
    }
}
 
/***********************************************
 *
 * @Name : readUntil
 * @Def : Reads characters from a file descriptor until the end character.
 *        The end character is not stored.
 * @Arg : In: nFd = file descriptor to be read
 *        In: cEnd = character that ends the reading
 * @Ret : Dynamically allocated string with the characters read, or NULL if
 *        the file ended or there was a read error before cEnd.
 *
 ************************************************/
char *readUntil(int nFd, char cEnd) {
    int i = 0;
    int nCharsRead = 0;
    char cChar = ' ';
    char *psBuffer = NULL;
 
    while (cChar != cEnd) {
        nCharsRead = (int)read(nFd, &cChar, sizeof(char));
        if (0 >= nCharsRead) {
            free(psBuffer);
            return NULL;
        }
        if (NULL == psBuffer) {
            psBuffer = (char *)malloc(sizeof(char));
        }
        if (0 < nCharsRead) {
            if (cChar != cEnd) {
                psBuffer[i] = cChar;
                psBuffer = (char *)realloc(psBuffer, (sizeof(char)) * (i + 2));
            }
            i++;
        }
    }
 
    psBuffer[i - 1] = '\0';
    return psBuffer;
}
 
/***********************************************
 *
 * @Name : growBuffer
 * @Def : Changes the size of a dynamic buffer. If the memory can't be
 *        allocated, the old buffer is freed.
 * @Arg : In: pvBuffer = buffer to resize
 *        In: tNewCapacity = number of elements of the new buffer
 *        In: tElementSize = size in bytes of each element
 * @Ret : The resized buffer, or NULL if the memory couldn't be allocated
 *        (the old buffer is already freed).
 *
 ************************************************/
static void *growBuffer(void *pvBuffer, size_t tNewCapacity,
                        size_t tElementSize) {
    void *pvTemp = NULL;
 
    pvTemp = realloc(pvBuffer, tNewCapacity * tElementSize);
    if (NULL == pvTemp) {
        free(pvBuffer);
    }
    return pvTemp;
}
 
/***********************************************
 *
 * @Name : trimBuffer
 * @Def : Reduces a dynamic buffer to the exact number of elements used. If
 *        it can't be reduced, the old buffer is kept.
 * @Arg : In: pvBuffer = buffer to reduce
 *        In: tCount = number of elements used
 *        In: tElementSize = size in bytes of each element
 * @Ret : The reduced buffer, or the old one if realloc failed.
 *
 ************************************************/
static void *trimBuffer(void *pvBuffer, size_t tCount, size_t tElementSize) {
    void *pvExact = NULL;
 
    pvExact = realloc(pvBuffer, tCount * tElementSize);
    if (NULL != pvExact) {
        return pvExact;
    }
    return pvBuffer;
}
 
/***********************************************
 *
 * @Name : readElement
 * @Def : Reads one element from a file descriptor. If the element arrives
 *        in several parts (pipes), it keeps reading until it is complete.
 * @Arg : In: nFd = file descriptor to be read
 *        In: pcBuffer = buffer where the element is stored
 *        In: tOffset = position of the element in the buffer, in bytes
 *        In: tElementSize = size in bytes of the element
 *        Out: ptBytesRead = result of the last read
 * @Ret : 1 if a whole element was read, 0 on EOF or read error.
 *
 ************************************************/
static int readElement(int nFd, char *pcBuffer, size_t tOffset,
                       size_t tElementSize, ssize_t *ptBytesRead) {
    size_t tTotalRead = 0;
 
    *ptBytesRead = read(nFd, pcBuffer + tOffset, tElementSize);
    if (0 >= *ptBytesRead) {
        return 0;
    }
 
    // Handle partial reads (for pipes)
    tTotalRead = *ptBytesRead;
    while (tTotalRead < tElementSize) {
        *ptBytesRead = read(nFd, pcBuffer + tOffset + tTotalRead,
                            tElementSize - tTotalRead);
        if (0 >= *ptBytesRead) {
            break;
        }
        tTotalRead += *ptBytesRead;
    }
 
    // EOF or read error
    if (tTotalRead < tElementSize) {
        return 0;
    }
    return 1;
}
 
/***********************************************
 *
 * @Name : readUntilEmpty
 * @Def : Reads elements from a file descriptor (can be a pipe) until it is
 *        empty. Works similarly to malloc.
 * @Arg : In: nFd = file descriptor to be read
 *        In: tElementSize = size of the element to be read (use 1 byte for
 *            reading byte by byte)
 *        Out: ptOutCount = amount of elements read
 * @Ret : Dynamically allocated array with the elements read, or NULL if
 *        nothing was read or an error occurred.
 *
 ************************************************/
void *readUntilEmpty(int nFd, size_t tElementSize, size_t *ptOutCount) {
    int nCapacity = INITIAL_ELEMENT_CAPACITY;
    int nCount = 0;
    void *pvBuffer = NULL;
    ssize_t tBytesRead = 0;
 
    if (0 == tElementSize || NULL == ptOutCount) {
        return NULL;
    }
 
    pvBuffer = malloc(tElementSize * nCapacity);
    if (NULL == pvBuffer) {
        printErr(ERROR_MALLOC);
        return NULL;
    }
 
    while (1 == readElement(nFd, pvBuffer, nCount * tElementSize,
                            tElementSize, &tBytesRead)) {
        nCount++;
        // Resize check before next read
        if (nCount == nCapacity) {
            nCapacity *= 2;
            pvBuffer = growBuffer(pvBuffer, nCapacity, tElementSize);
            if (NULL == pvBuffer) {
                printErr(ERROR_REALLOC);
                return NULL;
            }
        }
    }
 
    // Check if it's a true error and not just an interruption
    if (0 > tBytesRead && EINTR != errno) {
        printErr(ERROR_READ);
        free(pvBuffer);
        return NULL;
    }
 
    if (0 == nCount) {
        free(pvBuffer);
        *ptOutCount = 0;
        return NULL;
    }
 
    if (nCount < nCapacity) {
        pvBuffer = trimBuffer(pvBuffer, nCount, tElementSize);
    }
    *ptOutCount = nCount;
    return pvBuffer;
}
 
/***********************************************
 *
 * @Name : readLine
 * @Def : Reads one line from a file descriptor, removing the trailing '\n'
 *        (and '\r' if present).
 * @Arg : In: nFd = file descriptor to be read
 *        Out: ppsBuffer = allocated string where the line is stored
 * @Ret : Returns 1 if a line was read, 0 on EOF with nothing read, -1 on a
 *        read or allocation error.
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
            psBuffer = growBuffer(psBuffer, tBufferSize + 1, sizeof(char));
            if (NULL == psBuffer) {
                return -1;
            }
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
 * @Ret : Dynamically allocated array of strings, one per line read, or
 *        NULL if an error occurred.
 *
 ************************************************/
char **readLines(int nFd, size_t *ptOutCount) {
    size_t tLinesCapacity = INITIAL_LINES_CAPACITY;
    size_t tLinesCount = 0;
    size_t tNewCapacity = 0;
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
                freeLines(apsLines, tLinesCount);
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
        freeLines(apsLines, tLinesCount);
        return NULL;
    }
 
    *ptOutCount = tLinesCount;
    return apsLines;
}
 
/***********************************************
 *
 * @Name : readBinaryElement
 * @Def : Reads the next fixed-size element of a binary file. The buffer is
 *        made bigger first if it is full.
 * @Arg : In: nFd = file descriptor to be read
 *        In/Out: ppcBuffer = buffer where the elements are stored
 *        In/Out: ptCapacity = number of elements that fit in the buffer
 *        In: tCount = number of elements already read
 *        In: tElementSize = size in bytes of each element
 * @Ret : 1 if an element was read, 0 on EOF, -1 on error (the buffer is
 *        freed).
 *
 ************************************************/
static int readBinaryElement(int nFd, char **ppcBuffer, size_t *ptCapacity,
                             size_t tCount, size_t tElementSize) {
    char *pcDestination = NULL;
    int nBytesRead = 0;
 
    if (tCount >= *ptCapacity) {
        *ptCapacity *= 2;
        *ppcBuffer = growBuffer(*ppcBuffer, *ptCapacity, tElementSize);
        if (NULL == *ppcBuffer) {
            return -1;
        }
    }
 
    pcDestination = *ppcBuffer + tCount * tElementSize;
    nBytesRead = read(nFd, pcDestination, tElementSize);
    if (0 > nBytesRead) {
        free(*ppcBuffer);
        return -1;
    }
    if (0 == nBytesRead) {
        return 0;
    }
    if ((size_t)nBytesRead != tElementSize) {
        free(*ppcBuffer);
        return -1;
    }
    return 1;
}
 
/***********************************************
 *
 * @Name : readBinary
 * @Def : Reads fixed-size binary elements from a file descriptor until
 *        EOF.
 * @Arg : In: nFd = file descriptor to be read
 *        In: tElementSize = size in bytes of each element
 *        Out: ptOutCount = number of elements read
 * @Ret : Dynamically allocated array with the elements read, or NULL if
 *        nothing was read, a partial element was found, or an error
 *        occurred.
 *
 ************************************************/
void *readBinary(int nFd, size_t tElementSize, size_t *ptOutCount) {
    size_t tCapacity = INITIAL_BINARY_CAPACITY;
    size_t tCount = 0;
    char *pcBuffer = NULL;
    int nDone = 0;
    int nResult = 0;
 
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
        nResult = readBinaryElement(nFd, &pcBuffer, &tCapacity, tCount,
                                    tElementSize);
        if (-1 == nResult) {
            return NULL;
        } else if (0 == nResult) {
            nDone = 1;
        } else {
            tCount++;
        }
    }
 
    if (0 == tCount) {
        free(pcBuffer);
        *ptOutCount = 0;
        return NULL;
    }
 
    pcBuffer = trimBuffer(pcBuffer, tCount, tElementSize);
 
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
 
    // Bounded length so atoi never overflows
    return MAX_INT_DIGITS >= strlen(psString) && 0 < atoi(psString);
}
 