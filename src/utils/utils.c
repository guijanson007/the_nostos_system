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

/*
size_t lineLen = 0;
size_t lineCap = 128;
char *line = (char *)malloc(lineCap * sizeof(char));
if (!line) {
    free(lines);
    return NULL;
}

char buf[READ_CHUNK_SIZE];
ssize_t bytesRead;
int errorOccurred = 0;

while (!errorOccurred && (bytesRead = read(fd, buf, sizeof(buf))) != 0) {
    if (bytesRead < 0) {
        if (errno == EINTR) // read was interrupted, try again
            continue;
        errorOccurred = 1;
        break;
    }

    for (ssize_t i = 0; i < bytesRead; i++) {
        char c = buf[i];

        if (c == '\n') {
            line[lineLen] = '\0';

            // Shrink line buffer to exact size to save memory across text
            // files
            char *shrunk = realloc(line, lineLen + 1);
            if (shrunk) {
                line = shrunk;
            }
            // resize lines array if needed
            if (linesCount >= linesCap) {
                size_t newCap = linesCap * 2;
                char **tmp = realloc(lines, newCap * sizeof(char *));
                if (!tmp) {
                    errorOccurred = 1;
                    break;
                }
                lines = tmp;
                linesCap = newCap;
            }

            lines[linesCount++] = line;

            lineLen = 0;
            lineCap = 128;
            line = malloc(lineCap);
            if (!line) {
                errorOccurred = 1;
                break;
            }
        } else {
            // resize line buffer if needed
            if (lineLen + 1 >= lineCap) {
                size_t newCap = lineCap * 2;
                char *tmp = realloc(line, newCap);
                if (!tmp) {
                    errorOccurred = 1;
                    break;
                }
                line = tmp;
                lineCap = newCap;
            }
            line[lineLen++] = c;
        }
    }
}

if (errorOccurred) {
    free(line);
    freeLines(lines, linesCount);
    return NULL;
}

// Trailing line if the .txt file doesn't end with a \n
if (lineLen > 0) {
    line[lineLen] = '\0';

    // shrink line buffer to exact size to save memory across text files
    char *shrunk = realloc(line, lineLen + 1);
    if (shrunk) {
        line = shrunk;
    }
    // resize lines array if needed
    if (linesCount >= linesCap) {
        char **tmp = realloc(lines, (linesCount + 1) * sizeof(char *));
        if (!tmp) {
            free(line);
            freeLines(lines, linesCount);
            return NULL;
        }
        lines = tmp;
    }
    // give ownership of the line to the lines array to not leak memory
    lines[linesCount++] = line;
} else {
    free(line);
}
    */
