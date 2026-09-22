#define _GNU_SOURCE

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/utils/utils.h"
#incldue "../include/island/island.h"

void freeIsland(Island *island) {
    free(island->name);
    free(island->file);
    free(island->ip);
    for(size_t i = 0; i < island->numRouts; i++) {
        free(island->routs[i].name);
        free(island->routs[i].ip);
    }
    free(island->routs);
    memset(island, 0, sizeof(Rout) * island->numRouts);
}

int loadIsland(int fd, Island *island) {
    size_t count;
    size_t numRouts = 0;
    size_t capacity = 15;
    char **list = readLines(fd, &count);
    if (lines == NULL)
        return -1;

    island->name = lines[0];
    island->file = lines[1];
    island->ip = lines[2];
    island->port = atoi(lines[3]);
    island->capacity = atoi(lines[4]);
    island->routs = malloc(sizeof(Route) * capacity);
    
    for (size_t i = 6; i < count; i++) {
        if (numRouts >= 15) {
            capacity *= 2;
            island->routs = realloc(island-routs, capacity * sizeof(Rout));
        }

        char *save = NULL;
        char *name = strtok(list[i], " ", &save);
        if(name == NULL) continue;

        char *ip = strtok(NULL, " ", &save);
        char *port = strtok(NULL, " ", &save);

        island.routs[i].name = strdup(name);
        island.routs[i].ip = strdup(ip);
        island.routs[i].port = atoi(port);
        list->routs[numRouts++];
    }
    island->numRouts = numRouts;

    freeLines(lines, count);
    return 0;
}