/***********************************************
 *
 * @File : odysseus_map.c
 * @Purpose : known map of the odysseus: merging bought routes and showing it
 * @Author : Guilherme Rodrigues
 * @Date : 29/10/2026
 *
 ************************************************/
#define _GNU_SOURCE

// header
#include "odysseus/odysseus_map.h"

#define ERROR_MAP_WIDTH                                                        \
    "ERROR: The terminal must be at least 80 columns wide to show the map"
#define ERROR_MAP_DISPLAY "ERROR: Could not display the map"

/***********************************************
 *
 * @Name : findIsland
 * @Def : Looks for an island by name (case-insensitive) in the known map.
 * @Arg : In: ptMap = known map
 *        In: psName = name to look for
 * @Ret : Index of the island in the map, or -1 if it isn't known.
 *
 ************************************************/
static int findIsland(const Map *ptMap, const char *psName) {
    int i = 0;
 
    for (i = 0; i < ptMap->nNumConnections; i++) {
        if (0 == strcasecmp(ptMap->atConnections[i].psName, psName)) {
            return i;
        }
    }
    return -1;
}

/***********************************************
 *
 * @Name : findDestinationFromIsland
 * @Def : Looks for a destination that can be reached from where odysseus
 *        is. From Ithaca only the first island of the map can be reached;
 *        from an island, only its neighbors.
 * @Arg : In: ptOdysseus = Odysseus instance
 *        In: psDestination = name of the destination (case-insensitive)
 * @Ret : Index of the destination in the map, or -1 if it can't be
 *        reached.
 *
 ************************************************/
int findDestinationFromIsland(Odysseus *ptOdysseus, const char *psDestination) {
    const KnownIsland *ptHere = NULL;
    int nNeighbor = 0;
    int i = 0;
 
    if (AT_ITHACA == ptOdysseus->nWhereAmI) {
        if (0 == strcasecmp(ptOdysseus->tMap.atConnections[0].psName,
                            psDestination)) {
            return 0;
        }
        return -1;
    }
 
    ptHere = &ptOdysseus->tMap.atConnections[ptOdysseus->nWhereAmI];
 
    for (i = 0; i < ptHere->nNumNeighbors; i++) {
        nNeighbor = ptHere->anNeighbors[i];
        if (0 == strcasecmp(ptOdysseus->tMap.atConnections[nNeighbor].psName,
                            psDestination)) {
            return nNeighbor;
        }
    }
    return -1;
}


/***********************************************
 *
 * @Name : addIsland
 * @Def : Appends an island without neighbors to the known map.
 * @Arg : In/Out: ptMap = known map
 *        In: ptRoute = route holding the name, ip and port of the island
 * @Ret : Index of the new island, or -1 if memory ran out (map unchanged).
 *
 ************************************************/
static int addIsland(Map *ptMap, const MapRoute *ptRoute) {
    KnownIsland *atTmp = NULL;
    KnownIsland *ptNew = NULL;
    char *psName = strdup(ptRoute->psName);
    char *psIp = strdup(ptRoute->psIp);
 
    atTmp = realloc(ptMap->atConnections,
                    sizeof(KnownIsland) * (ptMap->nNumConnections + 1));
    if (NULL == psName || NULL == psIp || NULL == atTmp) {
        free(psName);
        free(psIp);
        if (NULL != atTmp) {
            ptMap->atConnections = atTmp;
        }
        return -1;
    }
 
    ptMap->atConnections = atTmp;
    ptNew = &ptMap->atConnections[ptMap->nNumConnections];
    ptNew->psName = psName;
    ptNew->psIp = psIp;
    ptNew->nPort = ptRoute->nPort;
    ptNew->nNumNeighbors = 0;
    ptNew->anNeighbors = NULL;
 
    return ptMap->nNumConnections++;
}

/***********************************************
 *
 * @Name : addNeighbor
 * @Def : Links an island to a neighbor, ignoring links already known.
 * @Arg : In/Out: ptIsland = island that gets the neighbor
 *        In: nNeighbor = index of the neighbor in the map
 * @Ret : 0 if everything went well, -1 if memory ran out.
 *
 ************************************************/
static int addNeighbor(KnownIsland *ptIsland, int nNeighbor) {
    int *anTmp = NULL;
    int i = 0;
 
    for (i = 0; i < ptIsland->nNumNeighbors; i++) {
        if (nNeighbor == ptIsland->anNeighbors[i]) {
            return 0;
        }
    }
 
    anTmp = realloc(ptIsland->anNeighbors,
                    sizeof(int) * (ptIsland->nNumNeighbors + 1));
    if (NULL == anTmp) {
        return -1;
    }
    ptIsland->anNeighbors = anTmp;
    ptIsland->anNeighbors[ptIsland->nNumNeighbors++] = nNeighbor;
 
    return 0;
}


/***********************************************
 *
 * @Name : mergeRoutes
 * @Def : Incorporates the routes of the island we are docked at into the
 *        known map. Routes are bidirectional, so both ends get the link.
 *        Islands we already know keep the address they have.
 * @Arg : In/Out: ptOdysseus = odysseus whose map is updated
 *                (nWhereAmI >= 0)
 *        In: atRoutes = routes received
 *        In: nRoutes = number of routes
 * @Ret : 0 if everything went well, -1 if memory ran out.
 *
 ************************************************/
int mergeRoutes(Odysseus *ptOdysseus, const MapRoute *atRoutes, int nRoutes) {
    Map *ptMap = &ptOdysseus->tMap;
    int nHere = ptOdysseus->nWhereAmI;
    int nDestination = 0;
    int i = 0;
 
    for (i = 0; i < nRoutes; i++) {
        if (0 == strcasecmp(ptMap->atConnections[nHere].psName,
                            atRoutes[i].psName)) {
            continue;
        }
        nDestination = findIsland(ptMap, atRoutes[i].psName);
        if (-1 == nDestination) {
            nDestination = addIsland(ptMap, &atRoutes[i]);
        }
        if (-1 == nDestination ||
            0 != addNeighbor(&ptMap->atConnections[nHere], nDestination) ||
            0 != addNeighbor(&ptMap->atConnections[nDestination], nHere)) {
            return -1;
        }
    }
    return 0;
}

/***********************************************
 *
 * @Name : freeMapRoutes
 * @Def : Frees a list of routes and its strings.
 * @Arg : In: atRoutes = routes to free (may be NULL)
 *        In: nRoutes = number of routes
 * @Ret : None.
 *
 ************************************************/
void freeMapRoutes(MapRoute *atRoutes, int nRoutes) {
    int i = 0;
 
    for (i = 0; i < nRoutes; i++) {
        free(atRoutes[i].psName);
        free(atRoutes[i].psIp);
    }
    free(atRoutes);
}


/***********************************************
 *
 * @Name : freeSphragisIslands
 * @Def : Frees the islands built to show the map with SPHRAGIS. The names
 *        belong to the known map, so only the arrays are freed.
 * @Arg : In: atIslands = islands to free
 *        In: nCount = number of islands
 * @Ret : None.
 *
 ************************************************/
static void freeSphragisIslands(SPHRAGIS_Island *atIslands, int nCount) {
    int i = 0;
 
    for (i = 0; i < nCount; i++) {
        free(atIslands[i].known_islands);
    }
    free(atIslands);
}

/***********************************************
 *
 * @Name : displayMap
 * @Def : Shows the known routes with SPHRAGIS. It discovers nothing.
 * @Arg : In: ptOdysseus = odysseus whose map is shown
 * @Ret : None.
 *
 ************************************************/
void displayMap(const Odysseus *ptOdysseus) {
    SPHRAGIS_Island *atIslands = NULL;
    const KnownIsland *ptKnown = NULL;
    int nCount = ptOdysseus->tMap.nNumConnections;
    int nResult = SPHRAGIS_OK;
    int i = 0;
    int j = 0;
 
    atIslands = calloc(nCount, sizeof(SPHRAGIS_Island));
    if (NULL == atIslands) {
        printErr(ERROR_MAP_DISPLAY);
        return;
    }
 
    for (i = 0; i < nCount; i++) {
        ptKnown = &ptOdysseus->tMap.atConnections[i];
 
        atIslands[i].name = ptKnown->psName;
        atIslands[i].known_island_count = ptKnown->nNumNeighbors;
        if (0 == ptKnown->nNumNeighbors) {
            continue;
        }
        atIslands[i].known_islands = calloc(ptKnown->nNumNeighbors,
                                            sizeof(char *));
        if (NULL == atIslands[i].known_islands) {
            printErr(ERROR_MAP_DISPLAY);
            nCount = i;
            nResult = SPHRAGIS_ERROR_INVALID_CONNECTION;
            break;
        }
        for (j = 0; j < ptKnown->nNumNeighbors; j++) {
            atIslands[i].known_islands[j] =
                ptOdysseus->tMap.atConnections[ptKnown->anNeighbors[j]].psName;
        }
    }
 
    if (SPHRAGIS_OK == nResult) {
        nResult = SPHRAGIS_display_map(atIslands, nCount);
        if (SPHRAGIS_ERROR == nResult) {
            printErr(ERROR_MAP_WIDTH);
        } else if (SPHRAGIS_OK != nResult) {
            printErr(ERROR_MAP_DISPLAY);
        }
    }
 
    freeSphragisIslands(atIslands, nCount);
}
