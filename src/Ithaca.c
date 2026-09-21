#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "include/utils.h"

#define ERROR_ARGC "ERROR ITHACA: Invalid number of arguments"
#define ERROR_FILE_ITHACA "ERROR ITHACA: The file couldn't been open"
#define ERROR_FILE_VOYAGE "ERROR VOYAGE: The file couldn't been open"
#define ERROR_LOAD_ITHACA "ERROR ITHACA: The data couldn't been loaded from the file"
#define ERROR_LOAD_VOYAGE "ERROR VOYAGE: The data couldn't been loaded from the file"

typedef struct {
    char *name;
    char *mision;
    char *ipAddress;
    int port;
}Ithaca;

typedef struct {
    int id;
    char *object;
    char *file;
    char *destination;
    int reward;
}Voyage;

void freeIthica (Ithaca *ithaca) {
    free(ithaca->name);
    free(ithaca->mision);
    free(ithaca->ipAddress);
    memset(ithaca, 0, sizeof *ithaca);
}

int loadIthacaData(int fd, Ithaca *ithaca) {
    size_t size;
    char **lines = readLines(fd, &size);

    if (lines == NULL) return -1;    
    if (size != 3) {
        freeLines(lines, size);
        return -1;
    }

    ithaca->name = lines[0];
    ithaca->mision = lines[1];
    ithaca->ipAddress = strtok_r(lines[2], " ");
    ithaca->port = atoi(strtok_r(NULL, " "));


    free(lines)
    return 0;
}

void freeVoyage (Voyage *voyage, size_t *numVoyage) {
    if (voyages == NULL) return;
        for (size_t i = 0; i < count; i++) {
            free(voyages[i].object);
            free(voyages[i].file);
            free(voyages[i].destination);
        }
    free(voyages);
}

int loadVoyageData(int fd, Voyage **voyage, size_t *numVoyage) {
    *voyages = NULL;
    *numVoyages = 0;
    
    char **lines = readLines(fd, numVoyage);
    if (lines == NULL) return -1;

    voyage = malloc(*numVoyage * sizeof(Voyage));
     if (list == NULL) {
        freeLines(lines, numVoyage);
        return -1;
    }

    for (int i = 0; i < *numVoyage; i++) {
        char *save = NULL;
        char *object = strtok_r(lines[i], " ", &save);

        if (object == NULL) continue;

        char *file = strtok_r(NULL, " ", &save);
        char *destination = strtok_r(NULL, " ", &save);
        char *reward = strtok_r(NULL, " ", &save);
        if (file == NULL || destination == NULL || reward == NULL) {
            freeLines(lines, numVoyage);
            return -1;
        }

        voyage[i].id = (int)i + 1;
        voyage[i].object = strdup(object);;
        voyage[i].file = strdup(file);
        voyage[i].destination = strdup(descriptor);;
        voyage[i].reward = atoi(reward);
    }

    freeLines(lines, numVoyage);

    return 0;
}

int main (int argc, char *argv[]) {
    if (argc != 3) {
        printErr(ERROR_ARGC);
        return 1;
    }

    int fd_ithaca = open(argv[1], O_RDONLY);
    int fd_voyage = open(argv[2], O_RDONLY);

    if (fd_ithaca == -1) {
        printErr(ERROR_FILE_ITHACA);
        return 1;
    }
    if(fd_voyage == -1) {
        printErr();
        return 1;
    }

    Ithaca ithaca;
    Voyage *voyage;
    size_t numVoyage = 0;

    if (loadIthacaData(fd_ithaca, &ithaca) == -1) {
        printErr(ERROR_LOAD_ITHACA);
    }

    if (loadVoyageData(fd_voyage, voyage, &numVoyage) == -1) {
        printErr(ERROR_LOAD_VOYAGE);
    }

    close(fd_ithaca);

    while(1){
        pause();
    }

    freeIthaca(&ithaca);
    freeVoyage(voyage, numVoyage);
    return 0;
}