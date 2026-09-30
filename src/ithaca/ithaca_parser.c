#define _GNU_SOURCE

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/ithaca/ithaca_parser.h"
#include "../include/utils/utils.h"


/***********************************************
 *
 * @Name : loadIthacaData
 * @Def : Reads the ithaca file (name, mission, "ip port") and fills the
 *        ithaca struct. On error everything allocated is freed.
 * @Arg : In: nFd = ithaca file descriptor
 *        Out: ptIthaca = ithaca to fill
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 ************************************************/
int loadIthacaData(int nFd, Ithaca *ptIthaca) {
    size_t tCount = 0;
    char **apsLines = NULL;
    char *psIp = NULL;
    char *psPort = NULL;
 
    apsLines = readLines(nFd, &tCount);
    if (NULL == apsLines) {
        return -1;
    }
    if (ITHACA_NUM_LINES != tCount) {
        freeLines(apsLines, tCount);
        return -1;
    }
 
    psIp = strtok(apsLines[2], " ");
    psPort = strtok(NULL, " ");
    if (NULL == psIp || NULL == psPort) {
        freeLines(apsLines, tCount);
        return -1;
    }
 
    ptIthaca->psName = strdup(apsLines[0]);
    ptIthaca->psMission = strdup(apsLines[1]);
    ptIthaca->psIp = strdup(psIp);
    ptIthaca->nPort = atoi(psPort);
    freeLines(apsLines, tCount);
 
    if (NULL == ptIthaca->psName || NULL == ptIthaca->psMission ||
        NULL == ptIthaca->psIp) {
        freeIthaca(ptIthaca);
        return -1;
    }
    return 0;
}


/***********************************************
 *
 * @Name : parseVoyage
 * @Def : Parses one voyage line
 * @Arg : In: psLine = line to parse
 *        Out: ptVoyage = voyage to fill
 *        In: nId = id given to the voyage
 * @Ret : 1 if a voyage was read, 0 if the line was empty, -1 on error.
 *
 ************************************************/
static int parseVoyage(char *psLine, Voyage *ptVoyage, int nId) {
    char *psObject = strtok(psLine, " ");
    char *psFile = NULL;
    char *psDestination = NULL;
    char *psReward = NULL;
 
    if (NULL == psObject) {
        return 0;   // empty line
    }
    psFile = strtok(NULL, " ");
    psDestination = strtok(NULL, " ");
    psReward = strtok(NULL, " ");
    if (NULL == psFile || NULL == psDestination || NULL == psReward) {
        return -1;
    }
 
    ptVoyage->nId = nId;
    ptVoyage->psObject = strdup(psObject);
    ptVoyage->psFile = strdup(psFile);
    ptVoyage->psDestination = strdup(psDestination);
    ptVoyage->nReward = atoi(psReward);
    if (NULL == ptVoyage->psObject || NULL == ptVoyage->psFile ||
        NULL == ptVoyage->psDestination) {
        free(ptVoyage->psObject);
        free(ptVoyage->psFile);
        free(ptVoyage->psDestination);
        return -1;
    }
    return 1;
}

/***********************************************
 *
 * @Name : loadVoyageData
 * @Def : Reads the voyages file into a dynamic array of Voyage. Empty lines are skipped. On error everything allocated is freed.
 * @Arg : In: nFd = voyages file descriptor
 *        Out: patVoyages = pointer to the voyages array to fill
 *        Out: ptNumVoyages = number of voyages read
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 ************************************************/
int loadVoyageData(int nFd, Voyage **patVoyages, size_t *ptNumVoyages) {
    size_t tCount = 0;
    size_t i = 0;
    char **apsLines = NULL;
    int nResult = 0;
 
    *patVoyages = NULL;
    *ptNumVoyages = 0;
 
    apsLines = readLines(nFd, &tCount);
    if (NULL == apsLines) {
        return -1;
    }
    
    *patVoyages = malloc(sizeof(Voyage) * (tCount + 1));
    if (NULL == *patVoyages) {
        freeLines(apsLines, tCount);
        return -1;
    }
 
    for (i = 0; i < tCount; i++) {
        nResult = parseVoyage(apsLines[i], &(*patVoyages)[*ptNumVoyages], (int)*ptNumVoyages + 1);
        if (-1 == nResult) {
            freeLines(apsLines, tCount);
            freeVoyage(patVoyages, ptNumVoyages);
            return -1;
        }
        *ptNumVoyages += (size_t)nResult;
    }
 
    freeLines(apsLines, tCount);
    return 0;
}