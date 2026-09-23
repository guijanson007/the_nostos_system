#define _GNU_SOURCE 1

// header
#include "odysseus/odysseus.h"

// system
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// own
#include "utils/utils.h"

Odysseus odysseusParseFile(int fd) {
    Odysseus odysseus;
    memset(&odysseus, 0, sizeof(Odysseus));

    if (fd == -1) {
        printErr("Failed to open odysseus.dat");
        return odysseus;
    }

    size_t outCount = 0;
    char **lines = readLines(fd, &outCount);

    close(fd);

    // Verify readLines didn't fail and we have the minimum required lines
    if (!lines || outCount < 6) {
        printErr("Invalid or incomplete odysseus.dat file");
        if (lines) {
            for (size_t j = 0; j < outCount; j++)
                free(lines[j]);
            free(lines);
        }
        return odysseus;
    }

    odysseus.name = strdup(lines[0]);
    odysseus.path = strdup(lines[1]);

    char *ipPort = strdup(lines[2]);
    char *token = ipPort ? strtok(ipPort, " ") : NULL;
    odysseus.ip = token ? strdup(token) : NULL;
    token = token ? strtok(NULL, " ") : NULL;
    odysseus.port = token ? strdup(token) : NULL;
    free(ipPort); // only needed as strtok scratch space; ip/port are
                  // independent copies now

    odysseus.initialInfo = strdup(lines[3]);

    if (!odysseus.name || !odysseus.path || !odysseus.ip || !odysseus.port ||
        !odysseus.initialInfo) {
        printErr("Failed to allocate/parse odysseus fields");
        free(odysseus.name);
        free(odysseus.path);
        free(odysseus.ip);
        free(odysseus.port);
        free(odysseus.initialInfo);
        for (size_t j = 0; j < outCount; j++)
            free(lines[j]);
        free(lines);
        memset(&odysseus, 0, sizeof(Odysseus));
        return odysseus;
    }

    odysseus.initialMoney = atof(lines[4]);
    odysseus.initialNumSupplies = atoi(lines[5]);

    if (odysseus.initialNumSupplies > 0) {
        odysseus.supplies =
            malloc(odysseus.initialNumSupplies * sizeof(Supply));
        if (!odysseus.supplies) {
            printErr("Failed to allocate memory for supplies");
            odysseus.initialNumSupplies =
                0; // keep struct internally consistent
        } else {
            size_t i = 6;
            while (i < outCount &&
                   (i - 6) < (size_t)odysseus.initialNumSupplies) {
                char *arg1 = strtok(lines[i], " ");
                if (arg1) {
                    odysseus.supplies[i - 6].name = strdup(arg1);
                    arg1 = strtok(NULL, " ");
                    if (arg1) {
                        odysseus.supplies[i - 6].amount = atoi(arg1);
                    }
                }
                ++i;
            }
        }
    }

    for (size_t j = 0; j < outCount; j++) {
        free(lines[j]);
    }
    free(lines);

    return odysseus;
}

void freeOdysseus(Odysseus *odysseus) {
    free(odysseus->name);
    free(odysseus->path);
    free(odysseus->ip);
    free(odysseus->port);
    free(odysseus->initialInfo);

    if (odysseus->supplies) {
        for (int i = 0; i < odysseus->initialNumSupplies; ++i) {
            free(odysseus->supplies[i].name);
        }
        free(odysseus->supplies);
        odysseus->supplies = NULL;
    }
}

void odysseusPrintInfo(Odysseus *odysseus) {
    println("Odysseus Info:");
    char nameStr[200];
    sprintf(nameStr, "Name: %s", odysseus->name);
    println(nameStr);
    char pathStr[200];
    sprintf(pathStr, "Path: %s", odysseus->path);
    println(pathStr);
    char ipPortStr[200];
    sprintf(ipPortStr, "IP:%s | Port: %s", odysseus->ip, odysseus->port);
    println(ipPortStr);
    char initialInfoStr[300];
    sprintf(initialInfoStr, "Initial Info: %s", odysseus->initialInfo);
    println(initialInfoStr);
    char moneyStr[100];
    sprintf(moneyStr, "Initial money: %f", odysseus->initialMoney);
    println(moneyStr);
    char numSuppliesStr[100];
    sprintf(numSuppliesStr, "Initial number of supplies: %d",
            odysseus->initialNumSupplies);
    println(numSuppliesStr);

    for (int i = 0; i < odysseus->initialNumSupplies; ++i) {
        char supplyHeader[200];
        sprintf(supplyHeader, "Supply %d:", i + 1);
        println(supplyHeader);
        sprintf(supplyHeader, "\tName: %s", odysseus->supplies[i].name);
        println(supplyHeader);
        sprintf(supplyHeader, "\tAmount: %d", odysseus->supplies[i].amount);
        println(supplyHeader);
    }
}
