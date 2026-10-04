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


/*************************************************
 *
 * @Name : buildSupplies
 * @Def : Allocates and populates the supplies array in the Odysseus struct.
 * @Arg : In: ptOdysseus = pointer to Odysseus struct to populate
 *        In: apsLines = array of strings read from the odysseus.dat file
 *        In: tOutCount = number of lines read
 * @Ret : Returns 1 on success, 0 on failure (e.g., memory allocation failure).
 *
 **************************************************/
static int buildSupplies(Odysseus *ptOdysseus, char **apsLines, size_t tOutCount) {
    size_t tLine = SUPPLIES_FIRST_LINE;
    char *psToken = NULL;
 
    if (0 < ptOdysseus->initialNumSupplies) {
        ptOdysseus->supplies =
            calloc(ptOdysseus->initialNumSupplies, sizeof(Supply));
        if (NULL == ptOdysseus->supplies) {
 
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
 * @Arg : In: nFd = file descriptor of the odysseus.dat file
 * @Ret : Returns a populated Odysseus struct. If parsing fails, returns an empty Odysseus struct with all fields set to NULL or 0.
 *
 **************************************************/
Odysseus odysseusParseFile(int nFd) {
    Odysseus tOdysseus = {0};
    size_t tOutCount = 0;
    char **apsLines = NULL;
    char *psIpPort = NULL;
    char *psToken = NULL;
 
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
 * @Arg : In: ptOdysseus = pointer to Odysseus struct to free
 * @Ret : None.
 *
 **************************************************/
void freeOdysseus(Odysseus *ptOdysseus) {
    int i = 0;
 
    if (NULL == ptOdysseus) {
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
 * @Arg : In: ptOdysseus = pointer to Odysseus struct to print
 * @Ret : None.
 *
 **************************************************/
void odysseusPrintInfo(Odysseus *ptOdysseus) {
    char *psStr = NULL;
    int i = 0;
 
    println("Odysseus Info:");
 
    asprintf(&psStr, "Name: %s", ptOdysseus->name);
    println(psStr);
    free(psStr);
 
    asprintf(&psStr, "Path: %s", ptOdysseus->path);
    println(psStr);
    free(psStr);
 
    asprintf(&psStr, "IP:%s | Port: %s", ptOdysseus->ip, ptOdysseus->port);
    println(psStr);
    free(psStr);
 
    asprintf(&psStr, "Initial Info: %s", ptOdysseus->initialInfo);
    println(psStr);
    free(psStr);
 
    asprintf(&psStr, "Initial money: %f", ptOdysseus->initialMoney);
    println(psStr);
    free(psStr);
 
    asprintf(&psStr, "Initial number of supplies: %d",
             ptOdysseus->initialNumSupplies);
    println(psStr);
    free(psStr);
 
    for (i = 0; i < ptOdysseus->initialNumSupplies; ++i) {
        asprintf(&psStr, "Supply %d:", i + 1);
        println(psStr);
        free(psStr);
 
        asprintf(&psStr, "\tName: %s", ptOdysseus->supplies[i].name);
        println(psStr);
        free(psStr);
 
        asprintf(&psStr, "\tAmount: %d", ptOdysseus->supplies[i].amount);
        println(psStr);
        free(psStr);
    }
}
