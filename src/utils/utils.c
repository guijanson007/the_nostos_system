#include "utils/utils.h"

// System headers
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

//Return a malloced string that must be free by the caller
//Check if the malloc worked
char *readLine(int fd) {
    int cap = 150;
    char *buff = malloc(cap);
    char c;
    int size = 0, n = 1;

    if (buff == NULL) return NULL;

    while((n = read(fd, &c, 1)) > 0) {
        if(c == '\n') break;
        buff[size++] = c;
    }
    
    if (n <= 0 && size == 0) {
        free(buff);
        return NULL;
    }

    if (size > 0 && buff[size - 1] == '\r') size--;
    buff[size] = '\0';
    
    char *exact = realloc(buff, size + 1);
    if (exact) buff = exact;

    return buff;
}

// This function works similarly to malloc
// fd = file descriptor to be read (can be a pipe)
// element_size = size of the element to be read (use 1 byte for reading byte by
// byte)
// out_count = Output parameter containing the amount of elements read
// return: dynamically allocated array
void *readUntilEmpty(int fd, size_t element_size, size_t *out_count) {
    if (element_size == 0 || out_count == NULL) {
        return NULL;
    }
    int cap = 100;
    void *buff = malloc(element_size * cap);

    if (buff == NULL) {
        printErr("Malloc error");
        return NULL;
    }

    int i = 0;
    ssize_t bytesRead = 0;
    while ((bytesRead = read(fd, buff + (i * element_size), element_size)) > 0) {
        // Handle partial reads (for pipes)
        size_t totalRead = bytesRead;
        while (totalRead < element_size) {
            bytesRead = read(fd, buff + (i * element_size) + totalRead,
                              element_size - totalRead);
            if (bytesRead <= 0)
                break;
            totalRead += bytesRead;
        }

        if (totalRead < element_size) {
            // EOF or read error
            break;
        }

        i++;

        // Resize check before next read
        if (i == cap) {
            cap *= 2;
            char *temp = realloc(buff, cap * element_size);

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

    if (0 == i){
        free(buff);
        *out_count = 0;
        return NULL;
    }

    if (i < cap) {
        char *temp = realloc(buff, i * element_size);
        if (temp != NULL)
            buff = temp;
    }

    *out_count = i;
    return buff;
}
