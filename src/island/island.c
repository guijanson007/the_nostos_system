#define _GNU_SOURCE

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/island/island.h"
#include "../include/utils/utils.h"
#include "../lib/sphragis/sphragis.h"

#define START_ROUTS 5

static int filterRouts(SPHRAGIS_Island *island, Rout **routs,
                       size_t *numRouts) {
    int newCount = SPHRAGIS_filter_island_configuration(island);
    if (newCount < 0)
        return -1;

    size_t numMatches = 0;
    for (size_t i = 0; i < *numRouts; i++) {
        if (numMatches < (size_t)newCount &&
            strcmp(island->known_islands[numMatches], (*routs)[i].name) == 0) {
            (*routs)[numMatches++] = (*routs)[i];
        } else {
            free((*routs)[i].name);
            free((*routs)[i].ip);
        }
    }

    *numRouts = newCount;
    return 0;
}

void freeIsland(Island *island, SPHRAGIS_Island *sphragis) {
    if (island == NULL)
        return;

    free(island->name);
    free(island->file);
    free(island->ip);
    free(sphragis->name);
    for (size_t i = 0; i < island->numRouts; i++) {
        free(island->routs[i].name);
        free(island->routs[i].ip);
        free(sphragis->known_islands[i]);
    }
    free(island->routs);
    free(sphragis->known_islands);
    memset(island, 0, sizeof *island);
}

static void errorHandeler(Island *island, SPHRAGIS_Island *sphragis,
                           Rout *routs, size_t numRouts,
                           char **lines, size_t count) {
    island->routs = routs;
    island->numRouts = numRouts;
    freeLines(lines, count);
    freeIsland(island, sphragis);
}

int loadIsland(int fd, Island *island, SPHRAGIS_Island *sphragis) {
    size_t count;
    size_t numRouts = 0;
    size_t capacity = 15;
    char **lines = readLines(fd, &count);
    if (lines == NULL)
        return -1;

    sphragis->name = strdup(lines[0]);

    island->name = strdup(lines[0]);
    island->file = strdup(lines[1]);

    char *ip = strtok(lines[2], " ");
    if (ip == NULL) {
        freeLines(lines, count);
        return -1;
    }
    char *port = strtok(NULL, " ");
    island->ip = strdup(ip);
    island->port = atoi(port);
    island->capacity = atoi(lines[3]);

    Rout *routs = malloc(sizeof(Rout) * capacity);
    sphragis->known_islands = malloc(sizeof(char *) * capacity);
    if (routs == NULL || island->name == NULL || island->file == NULL ||
        island->ip == NULL || sphragis->known_islands == NULL ||  sphragis->name == NULL) {
        freeLines(lines, count);
        return -1;
    }

    if (count < START_ROUTS) {
        freeLines(lines, count);
        return -1;
    }

    for (size_t i = START_ROUTS; i < count; i++) {
        if (numRouts >= capacity) {
            capacity *= 2;
            Rout *tmp_r = realloc(routs, capacity * sizeof(Rout));
            char **tmp_c =
                realloc(sphragis->known_islands, capacity * sizeof(char *));
            if (tmp_r == NULL || tmp_c == NULL) {
                errorHandeler(island, sphragis, routs, numRouts, lines, count);
                return -1;
            }
            routs = tmp_r;
            sphragis->known_islands = tmp_c;
        }

        char *name = strtok(lines[i], " ");
        if (name == NULL)
            continue;

        char *ip = strtok(NULL, " ");
        char *port = strtok(NULL, " ");

        if (ip == NULL || port == NULL) {
            errorHandeler(island, sphragis, routs, numRouts, lines, count);
            return -1;
        }

        sphragis->known_islands[numRouts] = strdup(name);

        Rout current;
        current.name = strdup(name);
        current.ip = strdup(ip);
        current.port = atoi(port);
        if (current.name == NULL || current.ip == NULL ||
            sphragis->known_islands[numRouts] == NULL) {
            errorHandeler(island, sphragis, routs, numRouts, lines, count);
            return -1;
        }
        routs[numRouts] = current;
        numRouts++;
    }
    sphragis->known_island_count = numRouts;
    freeLines(lines, count);

    if (filterRouts(sphragis, &routs, &numRouts) < 0) {
        freeIsland(island, sphragis);
        return -1;
    }

    island->routs = routs;
    island->numRouts = numRouts;
    return 0;
}

void freeStock(Stock **stock) {
    if (stock == NULL || *stock == NULL)
        return;
    free(*stock);
    *stock = NULL;
}

int loadStock(int fd, Stock **stock, size_t *numStock) {

    *stock = readBinary(fd, sizeof(Stock), numStock);
    if (*stock == NULL)
        return -1;

    return 0;
}