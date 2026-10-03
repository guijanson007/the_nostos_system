/***********************************************
 *
 * @File : island_parser.c
 * @Purpose : loading and freeing of the island config and stock files
 * @Author : Axel Boleda i Callizo
 * @Date : 24/09/2026
 *
 ************************************************/

#define _GNU_SOURCE

#include "island/island_parser.h"

/***********************************************
 *
 * @Name : filterRoutes
 * @Def : Asks sphragis which known islands are valid and removes the routes that were filtered out.
 * @Arg : In/Out: ptSphragis = sphragis island with the known islands
 *        In/Out: atRoutes = dynamic array of routes
 *        In/Out: ptNumRoutes = number of routes in atRoutes
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 ************************************************/

static int filterRoutes(SPHRAGIS_Island *ptSphragis, Route *atRoutes,
                        size_t *ptNumRoutes) {
    int nNewCount = SPHRAGIS_filter_island_configuration(ptSphragis);
    size_t tMatches = 0;
    size_t i = 0;
 
    if (nNewCount < 0) {
        return -1;
    }
 
    for (i = 0; i < *ptNumRoutes; i++) {
        if (tMatches < (size_t)nNewCount &&
            0 == strcmp(ptSphragis->known_islands[tMatches], atRoutes[i].psName)) {
            atRoutes[tMatches++] = atRoutes[i];
        } else {
            freeRoute(&atRoutes[i]);
        }
    }
 
    *ptNumRoutes = tMatches;
    return 0;
}

/***********************************************
 *
 * @Name : loadError
 * @Def : Frees everything loaded after an error.
 * @Arg : In/Out: ptIsland = island being loaded
 *        In/Out: ptSphragis = sphragis island being loaded
 *        In: apsLines = lines read from the config file
 *        In: tCount = number of lines
 * @Ret : Always -1, so callers can write "return loadError()".
 *
 ************************************************/

static int loadError(Island *ptIsland, SPHRAGIS_Island *ptSphragis,
                     char **apsLines, size_t tCount) {
    freeLines(apsLines, tCount);
    freeIsland(ptIsland, ptSphragis);
    return -1;
}

/***********************************************
 *
 * @Name : loadIslandInfo
 * @Def : Fills the island info from the first lines of the config file (name, file, ip and port, capacity).
 * @Arg : In: apsLines = lines read from the config file
 *        Out: ptIsland = island to fill
 *        Out: ptSphragis = sphragis island to fill
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 ************************************************/
static int loadIslandInfo(char **apsLines, Island *ptIsland,
                          SPHRAGIS_Island *ptSphragis) {
    char *psIp = NULL;
    char *psPort = NULL;
 
    ptSphragis->name = strdup(apsLines[0]);
    ptIsland->psName = strdup(apsLines[0]);
    ptIsland->psFile = strdup(apsLines[1]);
 
    psIp = strtok(apsLines[2], " ");
    psPort = strtok(NULL, " ");
    if (NULL == psIp || NULL == psPort) {
        return -1;
    }
    ptIsland->psIp = strdup(psIp);
    ptIsland->nPort = atoi(psPort);
    ptIsland->nCapacity = atoi(apsLines[3]);
 
    if (NULL == ptSphragis->name || NULL == ptIsland->psName ||
        NULL == ptIsland->psFile || NULL == ptIsland->psIp) {
        return -1;
    }
    return 0;
}
 
/***********************************************
 *
 * @Name : initRoutes
 * @Def : Allocates the routes array and the known islands array.
 * @Arg : Out: ptIsland = island whose routes array is allocated
 *        Out: ptSphragis = sphragis island whose known islands are allocated
 *        In: tCapacity = initial number of elements
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 ************************************************/
static int initRoutes(Island *ptIsland, SPHRAGIS_Island *ptSphragis,
                      size_t tCapacity) {
    ptIsland->atRoutes = malloc(sizeof(Route) * tCapacity);
    ptIsland->tNumRoutes = 0;
    ptSphragis->known_islands = malloc(sizeof(char *) * tCapacity);
    ptSphragis->known_island_count = 0;
 
    if (NULL == ptIsland->atRoutes || NULL == ptSphragis->known_islands) {
        return -1;
    }
    return 0;
}
 
/***********************************************
 *
 * @Name : growRoutes
 * @Def : Doubles the size of the routes and known islands arrays.
 * @Arg : In/Out: ptIsland = island whose routes array grows
 *        In/Out: ptSphragis = sphragis island whose known islands grow
 *        In/Out: ptCapacity = current capacity, doubled on success
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 ************************************************/
static int growRoutes(Island *ptIsland, SPHRAGIS_Island *ptSphragis,
                      size_t *ptCapacity) {
    Route *atTmpRoutes = NULL;
    char **apsTmpKnown = NULL;
 
    atTmpRoutes = realloc(ptIsland->atRoutes, sizeof(Route) * *ptCapacity * 2);
    if (NULL == atTmpRoutes) {
        return -1;
    }
    ptIsland->atRoutes = atTmpRoutes;
 
    apsTmpKnown = realloc(ptSphragis->known_islands,
                          sizeof(char *) * *ptCapacity * 2);
    if (NULL == apsTmpKnown) {
        return -1;
    }
    ptSphragis->known_islands = apsTmpKnown;
 
    *ptCapacity *= 2;
    return 0;
}
 
/***********************************************
 *
 * @Name : addRoute
 * @Def : Parses one route line and appends it to the routes array and to the sphragis known islands.
 * @Arg : In: psLine = line to parse
 *        In/Out: ptIsland = island that receives the route
 *        In/Out: ptSphragis = sphragis island that receives the name
 *        In/Out: ptCapacity = current capacity of both arrays
 * @Ret : 0 if the route was added or the line was empty, -1 on error.
 *
 ************************************************/
static int addRoute(char *psLine, Island *ptIsland, SPHRAGIS_Island *ptSphragis,
                    size_t *ptCapacity) {
    char *psName = strtok(psLine, " ");
    char *psIp = NULL;
    char *psPort = NULL;
    char *psKnown = NULL;
    Route tRoute = {0};
 
    if (NULL == psName) {
        return 0;   // empty line
    }
    psIp = strtok(NULL, " ");
    psPort = strtok(NULL, " ");
    if (NULL == psIp || NULL == psPort) {
        return -1;
    }
 
    if (ptIsland->tNumRoutes >= *ptCapacity &&
        -1 == growRoutes(ptIsland, ptSphragis, ptCapacity)) {
        return -1;
    }
 
    tRoute.psName = strdup(psName);
    tRoute.psIp = strdup(psIp);
    tRoute.nPort = atoi(psPort);
    psKnown = strdup(psName);
    if (NULL == tRoute.psName || NULL == tRoute.psIp || NULL == psKnown) {
        freeRoute(&tRoute);
        free(psKnown);
        return -1;
    }
 
    ptIsland->atRoutes[ptIsland->tNumRoutes] = tRoute;
    ptSphragis->known_islands[ptIsland->tNumRoutes] = psKnown;
    ptIsland->tNumRoutes++;
    ptSphragis->known_island_count = ptIsland->tNumRoutes;
    return 0;
}
 
/***********************************************
 *
 * @Name : loadIsland
 * @Def : Reads the config file and fills the island and the sphragis island. On error everything allocated is freed.
 * @Arg : In: nFd = config file descriptor
 *        Out: ptIsland = island to fill
 *        Out: ptSphragis = sphragis island to fill
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 ************************************************/
int loadIsland(int nFd, Island *ptIsland, SPHRAGIS_Island *ptSphragis) {
    size_t tCount = 0;
    size_t tCapacity = INITIAL_ROUTES_CAPACITY;
    size_t i = 0;
    char **apsLines = NULL;
 
    apsLines = readLines(nFd, &tCount);
    if (NULL == apsLines) {
        return -1;
    }
    // Check BEFORE touching lines[0..4]
    if (tCount < START_ROUTS) {
        freeLines(apsLines, tCount);
        return -1;
    }
 
    if (-1 == loadIslandInfo(apsLines, ptIsland, ptSphragis) ||
        -1 == initRoutes(ptIsland, ptSphragis, tCapacity)) {
        return loadError(ptIsland, ptSphragis, apsLines, tCount);
    }
 
    for (i = START_ROUTS; i < tCount; i++) {
        if (-1 == addRoute(apsLines[i], ptIsland, ptSphragis, &tCapacity)) {
            return loadError(ptIsland, ptSphragis, apsLines, tCount);
        }
    }
    freeLines(apsLines, tCount);
 
    if (-1 == filterRoutes(ptSphragis, ptIsland->atRoutes, &ptIsland->tNumRoutes)) {
        freeIsland(ptIsland, ptSphragis);
        return -1;
    }
    return 0;
}

/***********************************************
 *
 * @Name : loadStock
 * @Def : Reads the binary stock file into a dynamic array of Stock.
 * @Arg : In: nFd = stock file descriptor
 *        Out: patStock = pointer to the stock array to fill
 *        Out: ptNumStock = number of elements read
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 ************************************************/

int loadStock(int nFd, Stock **patStock, size_t *ptNumStock) {
    size_t i = 0;
 
    *patStock = readBinary(nFd, sizeof(Stock), ptNumStock);
    if (NULL == *patStock) {
        return -1;
    }
 
    // The file may not NUL-terminate the names, so force it
    for (i = 0; i < *ptNumStock; i++) {
        (*patStock)[i].sName[sizeof((*patStock)[i].sName) - 1] = '\0';
    }
    return 0;
}