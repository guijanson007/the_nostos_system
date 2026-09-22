#define _GNU_SOURCE

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/ithaca/ithaca.h"
#include "../include/utils/utils.h"



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