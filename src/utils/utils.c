#define _GNU_SOURCE 1

// header
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

// returns 1 if a line was read, 0 if error
int readLine(int fd, char *buff, size_t bufferSize) {
    char c;
    ssize_t n;
    size_t len = 0;
    if (buff == NULL)
        return 0;

    while ((n = read(fd, &c, 1)) > 0) {
        if (c == '\n')
            break;
        if (len + 1 >= bufferSize) {
            return 0;
        }
        buff[len++] = c;
    }

    if (n <= 0 && len == 0) {
        return 0;
    }

    if (len > 0 && buff[len - 1] == '\r')
        len--;
    buff[len] = '\0';
    return 1;
}

// returns an array of strings, each string is a line read from the file
// descriptor
char **readLines(int fd, size_t *outCount) {
    if (!outCount)
        return NULL;
    *outCount = 0;

    size_t linesCap = 16;
    size_t linesCount = 0;
    char **lines = malloc(linesCap * sizeof(char *));
    if (!lines)
        return NULL;

    char scratch[READ_CHUNK_SIZE];

    while (readLine(fd, scratch, sizeof(scratch)) == 1) {
        if (linesCount >= linesCap) {
            size_t newCap = linesCap * 2;
            char **tmp = realloc(lines, newCap * sizeof(char *));
            if (!tmp) {
                for (size_t i = 0; i < linesCount; i++)
                    free(lines[i]);
                free(lines);
                return NULL;
            }
            lines = tmp;
            linesCap = newCap;
        }

        lines[linesCount] = strdup(scratch);
        if (!lines[linesCount]) {
            for (size_t i = 0; i < linesCount; i++)
                free(lines[i]);
            free(lines);
            return NULL;
        }
        linesCount++;
    }

    *outCount = linesCount;
    return lines;
}

void *readBinary(int fd, size_t elementSize, size_t *outCount) {
    if (outCount == NULL)
        return NULL;
    *outCount = 0;
    if (elementSize == 0)
        return NULL;

    size_t linesCap = 16;
    size_t count = 0;
    char *buff = malloc(linesCap * elementSize);
    if (buff == NULL)
        return NULL;

    int done = 0;
    while (!done) {
        if (count >= linesCap) {
            linesCap *= 2;
            char *tmp = realloc(buff, linesCap * elementSize);
            if (!tmp) {
                free(buff);
                return NULL;
            }
            buff = tmp;
        }

        char *destination = buff + count * elementSize;
        int n = read(fd, destination, elementSize);
        if (n < 0) {
            free(buff);
            return NULL;
        } else if (n == 0) {
            done = 1;
        } else if ((size_t)n != elementSize) {
            free(buff);
            return NULL;
        } else {
            count++;
        }
    }

    if (count == 0) {
        free(buff);
        *outCount = 0;
        return NULL;
    }

    char *exact = realloc(buff, count * elementSize);
    if (exact != NULL) buff = exact;

    *outCount = count;
    return buff;
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
