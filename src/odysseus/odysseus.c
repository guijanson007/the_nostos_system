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

    // Copy data using strncpy to prevent buffer overflows
    strncpy(odysseus.name, lines[0], sizeof(odysseus.name) - 1);
    strncpy(odysseus.path, lines[1], sizeof(odysseus.path) - 1);
    strncpy(odysseus.ipPort, lines[2], sizeof(odysseus.ipPort) - 1);
    strncpy(odysseus.initialInfo, lines[3], sizeof(odysseus.initialInfo) - 1);

    odysseus.initialMoney = atof(lines[4]);
    odysseus.initialNumSupplies = atoi(lines[5]);

    if (odysseus.initialNumSupplies > 0) {
        odysseus.supplies =
            malloc(odysseus.initialNumSupplies * sizeof(Supply));
        if (!odysseus.supplies) {
            printErr("Failed to allocate memory for supplies");
            // Fallthrough to cleanup
        } else {
            size_t i = 6;
            // Ensure we don't read beyond outCount or initialNumSupplies
            while (i < outCount &&
                   (i - 6) < (size_t)odysseus.initialNumSupplies) {
                char *arg1 = strtok(lines[i], " ");
                if (arg1) {
                    strncpy(odysseus.supplies[i - 6].name, arg1,
                            sizeof(odysseus.supplies[i - 6].name) - 1);
                    arg1 = strtok(NULL, " ");
                    if (arg1) {
                        odysseus.supplies[i - 6].amount = atoi(arg1);
                    }
                }
                ++i;
            }
        }
    }

    // Free the dynamically allocated memory from readLines
    for (size_t j = 0; j < outCount; j++) {
        free(lines[j]);
    }
    free(lines);

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
