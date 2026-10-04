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

/***********************************************
 *
 * @Name : buildSupplies
 * @Def : Allocates the supplies array and fills it with the supply lines of
 *        the config file (name and amount separated by a space).
 * @Arg : In/Out: ptOdysseus = struct whose supplies are filled
 *        In: apsLines = lines read from the config file
 *        In: tOutCount = number of lines read
 * @Ret : Returns 1 if successful, 0 if the allocation failed.
 *
 ************************************************/
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



/***********************************************
 *
 * @Name : odysseusParseFile
 * @Def : Reads the config file and builds the Odysseus struct from it.
 * @Arg : In: nFd = file descriptor of the config file
 * @Ret : The Odysseus struct. If something fails, the fields that could not
 *        be filled are NULL/0. The caller must free it with freeOdysseus.
 *
 ************************************************/
Odysseus odysseusParseFile(int nFd) {
    Odysseus tOdysseus;
    size_t tOutCount = 0;
    char **apsLines = NULL;
    char *psIpPort = NULL;
    char *psToken = NULL;

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

/***********************************************
 *
 * @Name : freeOdysseus
 * @Def : Frees every dynamically allocated field of an Odysseus struct and
 *        sets them to NULL.
 * @Arg : In/Out: ptOdysseus = struct to free
 * @Ret : None.
 *
 ************************************************/
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

/***********************************************
 *
 * @Name : odysseusPrintInfo
 * @Def : Prints the contents of the Odysseus struct. Useful for debugging.
 * @Arg : In: ptOdysseus = struct to print
 * @Ret : None.
 *
 ************************************************/
void odysseusPrintInfo(Odysseus *ptOdysseus) {
    char sName[PRINT_BUFFER_SIZE] = {'\0'};
    char sPath[PRINT_BUFFER_SIZE] = {'\0'};
    char sIpPort[PRINT_BUFFER_SIZE] = {'\0'};
    char sInitialInfo[PRINT_LONG_BUFFER_SIZE] = {'\0'};
    char sMoney[PRINT_SHORT_BUFFER_SIZE] = {'\0'};
    char sNumSupplies[PRINT_SHORT_BUFFER_SIZE] = {'\0'};
    char sSupplyHeader[PRINT_BUFFER_SIZE] = {'\0'};
    int i = 0;
 
    println("Odysseus Info:");
    sprintf(sName, "Name: %s", ptOdysseus->name);
    println(sName);
    sprintf(sPath, "Path: %s", ptOdysseus->path);
    println(sPath);
    sprintf(sIpPort, "IP:%s | Port: %s", ptOdysseus->ip, ptOdysseus->port);
    println(sIpPort);
    sprintf(sInitialInfo, "Initial Info: %s", ptOdysseus->initialInfo);
    println(sInitialInfo);
    sprintf(sMoney, "Initial money: %f", ptOdysseus->initialMoney);
    println(sMoney);
    sprintf(sNumSupplies, "Initial number of supplies: %d",
            ptOdysseus->initialNumSupplies);
    println(sNumSupplies);
 
    for (i = 0; i < ptOdysseus->initialNumSupplies; ++i) {
        sprintf(sSupplyHeader, "Supply %d:", i + 1);
        println(sSupplyHeader);
        sprintf(sSupplyHeader, "\tName: %s", ptOdysseus->supplies[i].name);
        println(sSupplyHeader);
        sprintf(sSupplyHeader, "\tAmount: %d", ptOdysseus->supplies[i].amount);
        println(sSupplyHeader);
    }
}
