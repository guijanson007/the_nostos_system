#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "include/utils.h"

#define ERROR_ARGC "ERROR ITHACA: Invalid number of arguments"
#define ERROR_FILE "ERROR ITHACA: The file couldn't been open"
#define ERROR_LOAD "ERROR ITHACA: The data couldn't been loaded from the file"

typedef struct {
    char *name;
    char *misionFilePath;
    char *ipAddress;
    int port;
}Ithaca;


void freeIthica (Ithaca *ithaca) {
    free(ithaca->name);
    free(ithaca->misionFilePath);
    free(ithaca->ipAddress);
    memset(ithaca, 0, sizeof *ithaca);
}

int loadIthacaData(int fd, Ithaca *ithaca) {
    size_t size;
    char **lines = readLines(fd, &size);

    if (lines == NULL) return -1;    
    if(size != 3) {
        freeLines(lines, size);
        return -1;
    }

    ithaca->name = lines[0];
    ithaca->misionFilePath = lines[1];
    ithaca->ipAddress = strtok(lines[2], " ");
    ithaca->port = atoi(strtok(NULL, " "));


    free(lines)
    return 0;
}

int main (int argc, char *argv[]) {
    if (argc != 2) {
        printErr(ERROR_ARGC);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
        printErr(ERROR_FILE);
        return 1;
    }

    Ithaca ithaca;

    if (loadIthacaData(fd, &ithaca) == -1) {
        printErr(ERROR_LOAD);
    }

    close(fd);

    while(1){
        pause();
    }

    freeIthaca(&ithaca);
    return 0;
}