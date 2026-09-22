#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "../include/utils/utils.h"

#define ERROR_ARGC "ERROR ITHACA: Invalid number of arguments"
#define ERROR_FILE_ITHACA "ERROR ITHACA: The file couldn't been open"
#define ERROR_FILE_VOYAGE "ERROR VOYAGE: The file couldn't been open"
#define ERROR_LOAD_ITHACA                                                      \
    "ERROR ITHACA: The data couldn't been loaded from the file"
#define ERROR_LOAD_VOYAGE                                                      \
    "ERROR VOYAGE: The data couldn't been loaded from the file"
#define FARWEL "\nFinishing the Ithaca prosses"

typedef struct {
    char *name;
    char *mision;
    char *ipAddress;
    int port;
} Ithaca;

typedef struct {
    int id;
    char *object;
    char *file;
    char *destination;
    int reward;
} Voyage;

static int running = 1;

static void onSigint(int sig) {
    (void)sig;
    println(FARWEL);
    running = 0;
}

void freeIthaca(Ithaca *ithaca) {
    free(ithaca->name);
    free(ithaca->mision);
    free(ithaca->ipAddress);
    memset(ithaca, 0, sizeof *ithaca);
}

int loadIthacaData(int fd, Ithaca *ithaca) {
    size_t size;
    char **lines = readLines(fd, &size);

    if (lines == NULL)
        return -1;
    if (size != 3) {
        freeLines(lines, size);
        return -1;
    }

    ithaca->name = lines[0];
    ithaca->mision = lines[1];

    char *save = NULL;
    char *ip = strtok_r(lines[2], " ", &save);
    char *portStr = strtok_r(NULL, " ", &save);
    if (ip == NULL || portStr == NULL) {
        freeLines(lines, size);
        return -1;
    }
    ithaca->ipAddress = strdup(ip);
    ithaca->port = atoi(portStr);

    free(lines[2]);
    free(lines);
    return 0;
}

void freeVoyage(Voyage *voyages, size_t *numVoyage) {
    if (voyages == NULL)
        return;
    for (size_t i = 0; i < *numVoyage; i++) {
        free(voyages[i].object);
        free(voyages[i].file);
        free(voyages[i].destination);
    }
    *numVoyage = 0;
    free(voyages);
}

int loadVoyageData(int fd, Voyage **voyage, size_t *numVoyage) {
    *voyage = NULL;
    *numVoyage = 0;

    size_t count = 0;

    char **lines = readLines(fd, &count);
    if (lines == NULL)
        return -1;

    Voyage *list = malloc((count + 1) * sizeof(Voyage));
    if (list == NULL) {
        freeLines(lines, count);
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        char *save = NULL;
        char *object = strtok_r(lines[i], " ", &save);

        if (object == NULL)
            continue;

        char *file = strtok_r(NULL, " ", &save);
        char *destination = strtok_r(NULL, " ", &save);
        char *reward = strtok_r(NULL, " ", &save);
        if (file == NULL || destination == NULL || reward == NULL) {
            freeLines(lines, count);
            return -1;
        }

        list[i].id = (int)i + 1;
        list[i].object = strdup(object);
        list[i].file = strdup(file);
        list[i].destination = strdup(destination);
        list[i].reward = atoi(reward);
    }

    *voyage = list;
    *numVoyage = count;
    freeLines(lines, count);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printErr(ERROR_ARGC);
        return 1;
    }

    signal(SIGINT, onSigint);

    int fd_ithaca = open(argv[1], O_RDONLY);
    if (fd_ithaca == -1) {
        printErr(ERROR_FILE_ITHACA);
        return 1;
    }

    int fd_voyage = open(argv[2], O_RDONLY);
    if (fd_voyage == -1) {
        printErr(ERROR_FILE_VOYAGE);
        return 1;
    }

    Ithaca ithaca;
    Voyage *voyage;
    size_t numVoyage = 0;

    if (loadIthacaData(fd_ithaca, &ithaca) == -1) {
        printErr(ERROR_LOAD_ITHACA);
        return 1;
    }

    if (loadVoyageData(fd_voyage, &voyage, &numVoyage) == -1) {
        printErr(ERROR_LOAD_VOYAGE);
        return 1;
    }

    close(fd_ithaca);


    // DEBUGING //
    char *dbg = NULL;
    if (asprintf(&dbg, "%s - %s - %s - %d", ithaca.name, ithaca.mision,
                 ithaca.ipAddress, ithaca.port) >= 0) {
        println(dbg);
        free(dbg);
    }

    for (size_t i = 0; i < numVoyage; i++) {
        char *dbg = NULL;
        if (asprintf(&dbg, "%d: %s - %s - %s - %d", voyage[i].id,
                     voyage[i].object, voyage[i].file, voyage[i].destination,
                     voyage[i].reward) >= 0) {
            println(dbg);
            free(dbg);
        }
    }
    /////////////

    while (running) {
        pause();
    }

    freeIthaca(&ithaca);
    freeVoyage(voyage, &numVoyage);
    return 0;
}