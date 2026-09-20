// header
#include "odysseus/odysseus.h"

// system
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// own
#include "utils/utils.h"

Odysseus odysseusParseFile() {
    Odysseus odysseus;

    int fd = open("src/odysseus/odysseus.dat", O_RDONLY);
    if (fd == -1) {
        printErr("Failed to open odysseus.dat");
        return (Odysseus){0}; // Return an empty Odysseus struct on error
    }

    size_t outCount;
    char **lines = readLines(fd, &outCount);
    strcpy(odysseus.name, lines[0]);
    strcpy(odysseus.path, lines[1]);
    strcpy(odysseus.ipPort, lines[2]);
    strcpy(odysseus.initialInfo, lines[3]);
    odysseus.initialMoney = atof(lines[4]);
    odysseus.initialNumSupplies = atoi(lines[5]);

    odysseus.supplies = malloc(odysseus.initialNumSupplies * sizeof(Supply));

    size_t i = 6;
    while (i < outCount) {
        char *token = strtok(lines[i], " ");
        strcpy(odysseus.supplies[i - 6].name, token);
        token = strtok(NULL, " ");
        odysseus.supplies[i - 6].amount = atoi(token);
        ++i;
    }

    return odysseus;
}

void odysseusDestroy(Odysseus *odysseus) {
    if (odysseus->supplies) {
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
    sprintf(ipPortStr, "IP:Port: %s", odysseus->ipPort);
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
