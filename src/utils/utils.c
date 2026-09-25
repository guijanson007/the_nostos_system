#include "../include/utils/utils.h"

// System headers
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define READ_CHUNK_SIZE 4096

void freeLines(char **lines, size_t count) {
    if (!lines)
        return;
    for (size_t i = 0; i < count; i++) {
        free(lines[i]);
    }
    free(lines);
}

void writeToFileAndStdOut(int fd, const char *message) {
    write(fd, message, strlen(message));
    write(STDOUT_FILENO, message, strlen(message));
}

void println(const char *message) {
    write(STDOUT_FILENO, message, strlen(message));
    write(STDOUT_FILENO, "\n", strlen("\n"));
}

void printErr(const char *message) {
    write(STDERR_FILENO, message, strlen(message));
    write(STDERR_FILENO, "\n", strlen("\n"));
}

// This function works similarly to malloc
// fd = file descriptor to be read (can be a pipe)
// elementSize = size of the element to be read (use 1 byte for reading byte by
// byte)
// outCount = Output parameter containing the amount of elements read
// return: dynamically allocated array
void *readUntilEmpty(int fd, size_t elementSize, size_t *outCount) {
    if (elementSize == 0 || outCount == NULL) {
        return NULL;
    }
    int cap = 100;
    void *buff = malloc(elementSize * cap);

    if (buff == NULL) {
        printErr("Malloc error");
        return NULL;
    }

    int i = 0;
    ssize_t bytesRead = 0;
    while ((bytesRead = read(fd, buff + (i * elementSize), elementSize)) > 0) {
        // Handle partial reads (for pipes)
        size_t totalRead = bytesRead;
        while (totalRead < elementSize) {
            bytesRead = read(fd, buff + (i * elementSize) + totalRead,
                             elementSize - totalRead);
            if (bytesRead <= 0)
                break;
            totalRead += bytesRead;
        }

        if (totalRead < elementSize) {
            // EOF or read error
            break;
        }

        i++;

        // Resize check before next read
        if (i == cap) {
            cap *= 2;
            char *temp = realloc(buff, cap * elementSize);

            if (temp == NULL) {
                printErr("Realloc error");
                free(buff);
                return NULL;
            }
            buff = temp;
        }
    }

    // check if its a true error and not just an interruption
    if (bytesRead < 0 && errno != EINTR) {
        printErr("Read error");
        free(buff);
        return NULL;
    }

    if (0 == i) {
        free(buff);
        *outCount = 0;
        return NULL;
    }

    if (i < cap) {
        char *temp = realloc(buff, i * elementSize);
        if (temp != NULL)
            buff = temp;
    }

    *outCount = i;
    return buff;
}

// Return a malloced string that must be free by the caller
// Check if the malloc worked
char *readLine(int fd) {
    size_t cap = 150;
    size_t len = 0;
    char *buff = malloc(cap);
    char c;
    ssize_t n;

    if (buff == NULL)
        return NULL;

    while ((n = read(fd, &c, 1)) > 0) {
        if (c == '\n')
            break;
        if (len + 1 >= cap) {
            char *tmp = realloc(buff, cap * 2);
            if (!tmp) {
                free(buff);
                return NULL;
            }
            buff = tmp;
            cap *= 2;
        }
        buff[len++] = c;
    }

    if (n <= 0 && len == 0) {
        free(buff);
        return NULL;
    }

    if (len > 0 && buff[len - 1] == '\r')
        len--;
    buff[len] = '\0';

    char *exact = realloc(buff, len + 1);
    if (exact)
        buff = exact;

    return buff;
}

// returns an array of strings, each string is a line read from the file
// descriptor
char **readLines(int fd, size_t *outCount) {
    if (!outCount)
        return NULL;
    *outCount = 0;

    size_t linesCount = 0;
    size_t linesCap = 16;
    char **lines = (char **)malloc(linesCap * sizeof(char *));
    if (!lines)
        return NULL;

    char *line;
    while ((line = readLine(fd)) != NULL) {
        if (linesCount >= linesCap) {
            linesCap *= 2;
            char **tmp = realloc(lines, linesCap * sizeof(char *));
            if (!tmp) {
                free(line);
                freeLines(lines, linesCount);
                return NULL;
            }
            lines = tmp;
        }
        lines[linesCount++] = line;
    }

    *outCount = linesCount;
    return lines;
}

int isAllDigits(const char *str) {
    if (!str || *str == '\0') {
        return 0;
    }
    for (const char *p = str; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
    }
    return 1;
}

int isPositiveInteger(const char *str) {
    if (!str || *str == '\0') {
        return 0;
    }
    if (!isAllDigits(str))
        return 0;

    return (atoi(str) > 0);
}
