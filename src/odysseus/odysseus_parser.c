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
static int buildSupplies(Odysseus *ptOdysseus, char **apsLines,
                         size_t tOutCount) {
    size_t tLine = SUPPLIES_FIRST_LINE;
    char *psToken = NULL;

    if (0 < ptOdysseus->cargo.numSupplies) {
        ptOdysseus->cargo.supplies =
            calloc(ptOdysseus->cargo.numSupplies, sizeof(Supply));
        if (NULL == ptOdysseus->cargo.supplies) {

            return 0;
        } else {
            while (tLine < tOutCount &&
                   (tLine - SUPPLIES_FIRST_LINE) <
                       (size_t)ptOdysseus->cargo.numSupplies) {
                psToken = strtok(apsLines[tLine], " ");
                if (NULL != psToken) {
                    ptOdysseus->cargo.supplies[tLine - SUPPLIES_FIRST_LINE]
                        .name = strdup(psToken);
                    if (NULL == ptOdysseus->cargo
                                    .supplies[tLine - SUPPLIES_FIRST_LINE]
                                    .name) {
                        return 0;
                    }
                    psToken = strtok(NULL, " ");
                    if (NULL != psToken) {
                        ptOdysseus->cargo.supplies[tLine - SUPPLIES_FIRST_LINE]
                            .amount = atoi(psToken);
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
 * @Ret : Returns a populated Odysseus struct. If parsing fails, returns an
 * empty Odysseus struct with all fields set to NULL or 0.
 *
 **************************************************/
Odysseus odysseusParseFile(int nFd) {
    Odysseus tOdysseus;
    size_t tOutCount = 0;
    char **apsLines = NULL;
    char *psIpPort = NULL;
    char *psToken = NULL;

    memset(&tOdysseus, 0, sizeof(tOdysseus));
    tOdysseus.ithaca_socket = -1;
    tOdysseus.island_socket = -1;

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

    tOdysseus.cargo.nGold = atoi(apsLines[4]);
    tOdysseus.cargo.numSupplies = atoi(apsLines[5]);

    if (0 == buildSupplies(&tOdysseus, apsLines, tOutCount)) {
        printErr(ERROR_ALLOC_SUPPLIES);
        freeLines(apsLines, tOutCount);
        return tOdysseus;
    }

    calculateTotalGold(&tOdysseus.cargo);
    // We can free the lines now since we have copied the data we need
    freeLines(apsLines, tOutCount);

    // tOdysseus is freed in main in case of error, or after the menu loop
    // ends, so we don't free it here
    return tOdysseus;
}

/**************************************************
 *
 * @Name : freeOdysseusVoyage
 * @Def : Frees the strings of a voyage and resets it to "no voyage".
 * @Arg : In/Out: ptVoyage = voyage to clear
 * @Ret : None.
 *
 **************************************************/
void freeOdysseusVoyage(OdysseusVoyage *ptVoyage) {
    free(ptVoyage->object);
    free(ptVoyage->destination);
    memset(ptVoyage, 0, sizeof(*ptVoyage));
}

/**************************************************
 *
 * @Name : freeListedVoyages
 * @Def : Frees the voyages of the last LIST VOYAGES answer.
 * @Arg : In/Out: ptOdysseus = Odysseus holding the listing
 * @Ret : None.
 *
 **************************************************/
void freeListedVoyages(Odysseus *ptOdysseus) {
    int i = 0;

    for (i = 0; i < ptOdysseus->numListedVoyages; i++) {
        freeOdysseusVoyage(&ptOdysseus->listedVoyages[i]);
    }
    free(ptOdysseus->listedVoyages);
    ptOdysseus->listedVoyages = NULL;
    ptOdysseus->numListedVoyages = 0;
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
    free(ptOdysseus->lastError);
    ptOdysseus->lastError = NULL;

    freeListedVoyages(ptOdysseus);
    freeOdysseusVoyage(&ptOdysseus->activeVoyage);

    if (NULL != ptOdysseus->cargo.supplies) {
        for (i = 0; i < ptOdysseus->cargo.numSupplies; ++i) {
            free(ptOdysseus->cargo.supplies[i].name);
            ptOdysseus->cargo.supplies[i].name = NULL;
        }
        free(ptOdysseus->cargo.supplies);
        ptOdysseus->cargo.supplies = NULL;
    }
    ptOdysseus->cargo.numSupplies = 0;
    ptOdysseus->cargo.totalAmount = 0;

    for (int i = 0; i < ptOdysseus->map.numConnections; i++) {
        free(ptOdysseus->map.connections[i].name);
        free(ptOdysseus->map.connections[i].ip);
        free(ptOdysseus->map.connections[i].neighbors);
    }
    free(ptOdysseus->map.connections);
    ptOdysseus->map.connections = NULL;
    ptOdysseus->map.numConnections = 0;

    if (-1 != ptOdysseus->ithaca_socket) {
        close(ptOdysseus->ithaca_socket);
        ptOdysseus->ithaca_socket = -1;
    }
    if (-1 != ptOdysseus->island_socket) {
        close(ptOdysseus->island_socket);
        ptOdysseus->island_socket = -1;
    }
}
