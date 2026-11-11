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
 * @Def : Allocates and populates the supplies array in the Odysseus
 *        struct.
 * @Arg : In/Out: ptOdysseus = pointer to Odysseus struct to populate
 *        In: apsLines = array of strings read from the odysseus.dat file
 *        In: tOutCount = number of lines read
 * @Ret : Returns 1 on success, 0 on failure (e.g., memory allocation
 *        failure).
 *
 ************************************************/
static int buildSupplies(Odysseus *ptOdysseus, char **apsLines, size_t tOutCount) {
    size_t tLine = SUPPLIES_FIRST_LINE;
    char *psToken = NULL;
    Supply *ptSupply = NULL;
 
    if (0 < ptOdysseus->tCargo.nNumSupplies) {
        ptOdysseus->tCargo.atSupplies = calloc(ptOdysseus->tCargo.nNumSupplies, sizeof(Supply));
        if (NULL == ptOdysseus->tCargo.atSupplies) {
            return 0;
        } else {
            while (tLine < tOutCount &&
                   (tLine - SUPPLIES_FIRST_LINE) <
                       (size_t)ptOdysseus->tCargo.nNumSupplies) {
                ptSupply =
                    &ptOdysseus->tCargo.atSupplies[tLine - SUPPLIES_FIRST_LINE];
                psToken = strtok(apsLines[tLine], " ");
                if (NULL != psToken) {
                    ptSupply->psName = strdup(psToken);
                    if (NULL == ptSupply->psName) {
                        return 0;
                    }
                    psToken = strtok(NULL, " ");
                    if (NULL != psToken) {
                        ptSupply->nAmount = atoi(psToken);
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
 * @Name : parseIpPort
 * @Def : Reads the "<ip> <port>" line of the config file and copies the
 *        ip and the port into the Odysseus struct. A field that can't be
 *        read is left as NULL.
 * @Arg : In/Out: ptOdysseus = pointer to Odysseus struct to populate
 *        In: psLine = line with the ip and the port
 * @Ret : None.
 *
 ************************************************/
static void parseIpPort(Odysseus *ptOdysseus, const char *psLine) {
    char *psIpPort = NULL;
    char *psToken = NULL;
 
    psIpPort = strdup(psLine);
    if (NULL == psIpPort) {
        return;
    }
 
    psToken = strtok(psIpPort, " ");
    if (NULL != psToken) {
        ptOdysseus->psIp = strdup(psToken);
        psToken = strtok(NULL, " ");
    }
    if (NULL != psToken) {
        ptOdysseus->psPort = strdup(psToken);
    }
    free(psIpPort);
}

/***********************************************
 *
 * @Name : odysseusParseFile
 * @Def : Parses the odysseus.dat file and populates an Odysseus struct.
 * @Arg : In: nFd = file descriptor of the odysseus.dat file
 * @Ret : Returns a populated Odysseus struct. If parsing fails, returns an
 *        empty Odysseus struct with all fields set to NULL or 0.
 *
 ************************************************/
Odysseus odysseusParseFile(int nFd) {
    Odysseus tOdysseus = {0};
    size_t tOutCount = 0;
    char **apsLines = NULL;
 
    tOdysseus.nIthacaSocket = NO_SOCKET;
    tOdysseus.nIslandSocket = NO_SOCKET;
 
    apsLines = readLines(nFd, &tOutCount);
 
    // Verify readLines didn't fail and we have the minimum required lines
    if (NULL == apsLines || MIN_CONFIG_LINES > tOutCount) {
        printErr(ERROR_INVALID_FILE);
        freeLines(apsLines, tOutCount);
        return tOdysseus;
    }
 
    tOdysseus.psName = strdup(apsLines[0]);
    tOdysseus.psPath = strdup(apsLines[1]);
    parseIpPort(&tOdysseus, apsLines[2]);
    tOdysseus.psInitialInfo = strdup(apsLines[3]);
 
    if (NULL == tOdysseus.psName || NULL == tOdysseus.psPath ||
        NULL == tOdysseus.psIp || NULL == tOdysseus.psPort ||
        NULL == tOdysseus.psInitialInfo) {
        printErr(ERROR_PARSE_FIELDS);
        freeLines(apsLines, tOutCount);
        return tOdysseus;
    }
 
    tOdysseus.tCargo.nGold = atoi(apsLines[4]);
    tOdysseus.tCargo.nNumSupplies = atoi(apsLines[5]);
 
    if (0 == buildSupplies(&tOdysseus, apsLines, tOutCount)) {
        printErr(ERROR_ALLOC_SUPPLIES);
        freeLines(apsLines, tOutCount);
        return tOdysseus;
    }
 
    calculateTotalAmount(&tOdysseus.tCargo);
    // We can free the lines now since we have copied the data we need
    freeLines(apsLines, tOutCount);
 
    // tOdysseus is freed in main in case of error, or after the menu loop
    // ends, so we don't free it here
    return tOdysseus;
}


/***********************************************
 *
 * @Name : freeOdysseusVoyage
 * @Def : Frees the strings of a voyage and resets it to "no voyage".
 * @Arg : In/Out: ptVoyage = voyage to clear
 * @Ret : None.
 *
 ************************************************/
void freeOdysseusVoyage(OdysseusVoyage *ptVoyage) {
    free(ptVoyage->psObject);
    free(ptVoyage->psDestination);
    memset(ptVoyage, 0, sizeof(*ptVoyage));
}


/***********************************************
 *
 * @Name : freeListedVoyages
 * @Def : Frees the voyages of the last LIST VOYAGES answer.
 * @Arg : In/Out: ptOdysseus = Odysseus holding the listing
 * @Ret : None.
 *
 ************************************************/
void freeListedVoyages(Odysseus *ptOdysseus) {
    int i = 0;
 
    for (i = 0; i < ptOdysseus->nNumListedVoyages; i++) {
        freeOdysseusVoyage(&ptOdysseus->atListedVoyages[i]);
    }
    free(ptOdysseus->atListedVoyages);
    ptOdysseus->atListedVoyages = NULL;
    ptOdysseus->nNumListedVoyages = 0;
}



/***********************************************
 *
 * @Name : freeCargo
 * @Def : Frees the supplies of the cargo and resets its counters.
 * @Arg : In/Out: ptCargo = cargo to free
 * @Ret : None.
 *
 ************************************************/
static void freeCargo(Cargo *ptCargo) {
    int i = 0;
 
    if (NULL != ptCargo->atSupplies) {
        for (i = 0; i < ptCargo->nNumSupplies; ++i) {
            free(ptCargo->atSupplies[i].psName);
            ptCargo->atSupplies[i].psName = NULL;
        }
        free(ptCargo->atSupplies);
        ptCargo->atSupplies = NULL;
    }
    ptCargo->nNumSupplies = 0;
    ptCargo->nTotalAmount = 0;
}


/***********************************************
 *
 * @Name : freeMap
 * @Def : Frees every island of the known map and resets it.
 * @Arg : In/Out: ptMap = map to free
 * @Ret : None.
 *
 ************************************************/
static void freeMap(Map *ptMap) {
    int i = 0;
 
    for (i = 0; i < ptMap->nNumConnections; i++) {
        free(ptMap->atConnections[i].psName);
        free(ptMap->atConnections[i].psIp);
        free(ptMap->atConnections[i].anNeighbors);
    }
    free(ptMap->atConnections);
    ptMap->atConnections = NULL;
    ptMap->nNumConnections = 0;
}

/***********************************************
 *
 * @Name : freeOdysseus
 * @Def : Frees all dynamically allocated memory in the Odysseus struct and
 *        closes its sockets.
 * @Arg : In/Out: ptOdysseus = pointer to Odysseus struct to free
 * @Ret : None.
 *
 ************************************************/
void freeOdysseus(Odysseus *ptOdysseus) {
    if (NULL == ptOdysseus) {
        return;
    }
 
    free(ptOdysseus->psName);
    ptOdysseus->psName = NULL;
    free(ptOdysseus->psPath);
    ptOdysseus->psPath = NULL;
    free(ptOdysseus->psIp);
    ptOdysseus->psIp = NULL;
    free(ptOdysseus->psPort);
    ptOdysseus->psPort = NULL;
    free(ptOdysseus->psInitialInfo);
    ptOdysseus->psInitialInfo = NULL;
    free(ptOdysseus->psLastError);
    ptOdysseus->psLastError = NULL;
 
    freeListedVoyages(ptOdysseus);
    freeOdysseusVoyage(&ptOdysseus->tActiveVoyage);
    freeCargo(&ptOdysseus->tCargo);
    freeMap(&ptOdysseus->tMap);
 
    if (NO_SOCKET != ptOdysseus->nIthacaSocket) {
        close(ptOdysseus->nIthacaSocket);
        ptOdysseus->nIthacaSocket = NO_SOCKET;
    }
    if (NO_SOCKET != ptOdysseus->nIslandSocket) {
        close(ptOdysseus->nIslandSocket);
        ptOdysseus->nIslandSocket = NO_SOCKET;
    }
}