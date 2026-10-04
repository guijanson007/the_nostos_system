/***********************************************
 *
 * @File : odysseus_parser.c
 * @Purpose : parses the odysseus config file into an Odysseus struct
 * @Author : Guilherme Rodrigues
 * @Date : 20/09/2026
 *
 ************************************************/

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
            while (tLine < tOutCount &&
                   (tLine - SUPPLIES_FIRST_LINE) <
                       (size_t)ptOdysseus->initialNumSupplies) {
                psToken = strtok(apsLines[tLine], " ");
                if (NULL != psToken) {
                    ptOdysseus->supplies[tLine - SUPPLIES_FIRST_LINE].name =
                        strdup(psToken);
                    psToken = strtok(NULL, " ");
                    if (NULL != psToken) {
                        ptOdysseus->supplies[tLine - SUPPLIES_FIRST_LINE].amount =
                            atoi(psToken);
                    }
                }
                ++tLine;
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
 
    apsLines = readLines(nFd, &tOutCount);
 
    // Verify readLines didn't fail and we have the minimum required lines
    if (NULL == apsLines || MIN_CONFIG_LINES > tOutCount) {
        printErr(ERROR_INVALID_FILE);
        freeLines(apsLines, tOutCount);
        return tOdysseus;
    }
 
    tOdysseus.name = strdup(apsLines[0]);
    tOdysseus.path = strdup(apsLines[1]);
 
    psIpPort = strdup(apsLines[2]);
    psToken = psIpPort ? strtok(psIpPort, " ") : NULL;
    tOdysseus.ip = psToken ? strdup(psToken) : NULL;
    psToken = psToken ? strtok(NULL, " ") : NULL;
    tOdysseus.port = psToken ? strdup(psToken) : NULL;
    free(psIpPort);
 
    tOdysseus.initialInfo = strdup(apsLines[3]);
 
    if (NULL == tOdysseus.name || NULL == tOdysseus.path ||
        NULL == tOdysseus.ip || NULL == tOdysseus.port ||
        NULL == tOdysseus.initialInfo) {
        printErr(ERROR_PARSE_FIELDS);
        freeLines(apsLines, tOutCount);
        return tOdysseus;
    }
 
    tOdysseus.initialMoney = atof(apsLines[4]);
    tOdysseus.initialNumSupplies = atoi(apsLines[5]);
 
    if (0 == buildSupplies(&tOdysseus, apsLines, tOutCount)) {
        printErr(ERROR_ALLOC_SUPPLIES);
        freeLines(apsLines, tOutCount);
        return tOdysseus;
    }
 
    // We can free the lines now since we have copied the data we need
    freeLines(apsLines, tOutCount);
 
    // tOdysseus is freed in main in case of error, or after the menu loop
    // ends, so we don't free it here
    return tOdysseus;
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
 
    free(ptOdysseus->name);
    ptOdysseus->name = NULL;
    free(ptOdysseus->path);
    ptOdysseus->path = NULL;
    free(ptOdysseus->ip);
    ptOdysseus->ip = NULL;
    free(ptOdysseus->port);
    ptOdysseus->port = NULL;
    free(ptOdysseus->initialInfo);
    ptOdysseus->initialInfo = NULL;
 
    if (NULL != ptOdysseus->supplies) {
        for (i = 0; i < ptOdysseus->initialNumSupplies; ++i) {
            free(ptOdysseus->supplies[i].name);
            ptOdysseus->supplies[i].name = NULL;
        }
        free(ptOdysseus->supplies);
        ptOdysseus->supplies = NULL;
    }
    ptOdysseus->initialNumSupplies = 0;
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
