#define _GNU_SOURCE 1

// header
#include "odysseus/odysseus_parser.h"

// system
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// own
#include "utils/utils.h"

/*************************************************
 *
 * @Name : buildSupplies
 * @Def : Allocates and populates the supplies array in the Odysseus struct.
 * @Arg : In: odysseus = pointer to Odysseus struct to populate
 *        In: lines = array of strings read from the odysseus.dat file
 *        In: outCount = number of lines read
 * @Ret : Returns 1 on success, 0 on failure (e.g., memory allocation failure).
 *
 **************************************************/
static int buildSupplies(Odysseus *odysseus, char **lines, size_t outCount) {
    if (odysseus->initialNumSupplies > 0) {
        odysseus->supplies =
            calloc(odysseus->initialNumSupplies, sizeof(Supply));
        if (!odysseus->supplies) {

            return 0;
        } else {
            size_t i = 6;
            while (i < outCount &&
                   (i - 6) < (size_t)odysseus->initialNumSupplies) {
                char *arg1 = strtok(lines[i], " ");
                if (arg1) {
                    odysseus->supplies[i - 6].name = strdup(arg1);
                    arg1 = strtok(NULL, " ");
                    if (arg1) {
                        odysseus->supplies[i - 6].amount = atoi(arg1);
                    }
                }
                ++i;
            }
        }
    }
    return 1;
}

/**************************************************
 *
 * @Name : odysseusParseFile
 * @Def : Parses the odysseus.dat file and populates an Odysseus struct.
 * @Arg : In: fd = file descriptor of the odysseus.dat file
 * @Ret : Returns a populated Odysseus struct. If parsing fails, returns an
 * empty Odysseus struct with all fields set to NULL or 0.
 *
 **************************************************/
Odysseus odysseusParseFile(int fd) {
    Odysseus odysseus;
    memset(&odysseus, 0, sizeof(Odysseus));

    size_t outCount = 0;
    char **lines = readLines(fd, &outCount);

    // Verify readLines didn't fail and we have the minimum required lines
    if (!lines || outCount < 6) {
        printErr("Invalid or incomplete odysseus.dat file");
        freeLines(lines, outCount);
        return odysseus;
    }

    odysseus.name = strdup(lines[0]);
    odysseus.path = strdup(lines[1]);

    char *ipPort = strdup(lines[2]);
    char *token = ipPort ? strtok(ipPort, " ") : NULL;
    odysseus.ip = token ? strdup(token) : NULL;
    token = token ? strtok(NULL, " ") : NULL;
    odysseus.port = token ? strdup(token) : NULL;
    free(ipPort);

    odysseus.initialInfo = strdup(lines[3]);

    if (!odysseus.name || !odysseus.path || !odysseus.ip || !odysseus.port ||
        !odysseus.initialInfo) {
        printErr("Failed to allocate/parse odysseus fields");
        freeLines(lines, outCount);
        return odysseus;
    }

    odysseus.initialMoney = atof(lines[4]);
    odysseus.initialNumSupplies = atoi(lines[5]);

    if (buildSupplies(&odysseus, lines, outCount) == 0) {
        printErr("Failed to allocate memory for supplies");
        freeLines(lines, outCount);
        return odysseus;
    };

    // we can free the lines now since we have copied the data we need
    freeLines(lines, outCount);

    return odysseus;
    // odysseus is freed in main.c in case of error, or after the menu loop
    // ends, so we don't free it here
}

/**************************************************
 *
 * @Name : freeOdysseus
 * @Def : Frees all dynamically allocated memory in the Odysseus struct.
 * @Arg : In: odysseus = pointer to Odysseus struct to free
 * @Ret : None.
 *
 **************************************************/
void freeOdysseus(Odysseus *odysseus) {
    if (odysseus == NULL) {
        return;
    }

    free(odysseus->name);
    odysseus->name = NULL;
    free(odysseus->path);
    odysseus->path = NULL;
    free(odysseus->ip);
    odysseus->ip = NULL;
    free(odysseus->port);
    odysseus->port = NULL;
    free(odysseus->initialInfo);
    odysseus->initialInfo = NULL;

    if (odysseus->supplies) {
        for (int i = 0; i < odysseus->initialNumSupplies; ++i) {
            free(odysseus->supplies[i].name);
            odysseus->supplies[i].name = NULL;
        }
        free(odysseus->supplies);
        odysseus->supplies = NULL;
    }
    odysseus->initialNumSupplies = 0;
}

/*************************************************
 *
 * @Name : odysseusPrintInfo
 * @Def : Prints the information of the Odysseus struct to standard output.
 * (Solely used for debugging purposes)
 * @Arg : In: odysseus = pointer to Odysseus struct to print
 * @Ret : None.
 *
 **************************************************/
void odysseusPrintInfo(Odysseus *odysseus) {
    char *str;

    println("Odysseus Info:");

    asprintf(&str, "Name: %s", odysseus->name);
    println(str);
    free(str);

    asprintf(&str, "Path: %s", odysseus->path);
    println(str);
    free(str);

    asprintf(&str, "IP:%s | Port: %s", odysseus->ip, odysseus->port);
    println(str);
    free(str);

    asprintf(&str, "Initial Info: %s", odysseus->initialInfo);
    println(str);
    free(str);

    asprintf(&str, "Initial money: %f", odysseus->initialMoney);
    println(str);
    free(str);

    asprintf(&str, "Initial number of supplies: %d",
             odysseus->initialNumSupplies);
    println(str);
    free(str);

    for (int i = 0; i < odysseus->initialNumSupplies; ++i) {
        asprintf(&str, "Supply %d:", i + 1);
        println(str);
        free(str);

        asprintf(&str, "\tName: %s", odysseus->supplies[i].name);
        println(str);
        free(str);

        asprintf(&str, "\tAmount: %d", odysseus->supplies[i].amount);
        println(str);
        free(str);
    }
}
