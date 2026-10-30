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

    for (i = 0; i < ptMap->numConnections; i++) {
        if (0 == strcasecmp(ptMap->connections[i].name, psName)) {
            return i;
        }
    }
    return -1;
}

int findDestinationFromIsland(Odysseus *ptOdysseus, const char *psDestination) {
    const KnownIsland *ptHere = NULL;
    int nNeighbor = 0;
    int i = 0;

    if (-1 == ptOdysseus->whereAmI) {
        if (0 ==
            strcasecmp(ptOdysseus->map.connections[0].name, psDestination)) {
            return 0;
        }
        return -1;
    }

    ptHere = &ptOdysseus->map.connections[ptOdysseus->whereAmI];
    
    for (i = 0; i < ptHere->num_neighbors; i++) {
        nNeighbor = ptHere->neighbors[i];
        if (0 == strcasecmp(ptOdysseus->map.connections[nNeighbor].name,
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
    char *psName = strdup(ptRoute->name);
    char *psIp = strdup(ptRoute->ip);

    atTmp = realloc(ptMap->connections,
                    sizeof(KnownIsland) * (ptMap->numConnections + 1));
    if (NULL == psName || NULL == psIp || NULL == atTmp) {
        free(psName);
        free(psIp);
        if (NULL != atTmp) {
            ptMap->connections = atTmp;
        }
        return -1;
    }

    ptMap->connections = atTmp;
    ptNew = &ptMap->connections[ptMap->numConnections];
    ptNew->name = psName;
    ptNew->ip = psIp;
    ptNew->port = ptRoute->port;
    ptNew->num_neighbors = 0;
    ptNew->neighbors = NULL;

    return ptMap->numConnections++;
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

    for (i = 0; i < ptIsland->num_neighbors; i++) {
        if (nNeighbor == ptIsland->neighbors[i]) {
            return 0;
        }
    }

    anTmp = realloc(ptIsland->neighbors,
                    sizeof(int) * (ptIsland->num_neighbors + 1));
    if (NULL == anTmp) {
        return -1;
    }
    ptIsland->neighbors = anTmp;
    ptIsland->neighbors[ptIsland->num_neighbors++] = nNeighbor;

    return 0;
}

/***********************************************
 *
 * @Name : mergeRoutes
 * @Def : Incorporates the routes of the island we are docked at into the known
 *        map. Routes are bidirectional, so both ends get the link. Islands we
 *        already know keep the address they have.
 * @Arg : In/Out: ptOdysseus = odysseus whose map is updated (whereAmI >= 0)
 *        In: atRoutes = routes received
 *        In: nRoutes = number of routes
 * @Ret : 0 if everything went well, -1 if memory ran out.
 *
 ************************************************/
int mergeRoutes(Odysseus *ptOdysseus, const MapRoute *atRoutes, int nRoutes) {
    Map *ptMap = &ptOdysseus->map;
    int nHere = ptOdysseus->whereAmI;
    int nDestination = 0;
    int i = 0;

    for (i = 0; i < nRoutes; i++) {
        if (0 == strcasecmp(ptMap->connections[nHere].name, atRoutes[i].name)) {
            continue;
        }
        nDestination = findIsland(ptMap, atRoutes[i].name);
        if (-1 == nDestination) {
            nDestination = addIsland(ptMap, &atRoutes[i]);
        }
        if (-1 == nDestination ||
            0 != addNeighbor(&ptMap->connections[nHere], nDestination) ||
            0 != addNeighbor(&ptMap->connections[nDestination], nHere)) {
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
        free(atRoutes[i].name);
        free(atRoutes[i].ip);
    }
    free(atRoutes);
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
    int nCount = ptOdysseus->map.numConnections;
    int nResult = SPHRAGIS_OK;
    int i = 0;
    int j = 0;

    atIslands = calloc(nCount, sizeof(SPHRAGIS_Island));
    if (NULL == atIslands) {
        printErr(ERROR_MAP_DISPLAY);
        return;
    }

    for (i = 0; i < nCount; i++) {
        const KnownIsland *ptKnown = &ptOdysseus->map.connections[i];

        atIslands[i].name = ptKnown->name;
        atIslands[i].known_island_count = ptKnown->num_neighbors;
        if (0 == ptKnown->num_neighbors) {
            continue;
        }
        atIslands[i].known_islands =
            calloc(ptKnown->num_neighbors, sizeof(char *));
        if (NULL == atIslands[i].known_islands) {
            printErr(ERROR_MAP_DISPLAY);
            nCount = i;
            nResult = SPHRAGIS_ERROR_INVALID_CONNECTION;
            break;
        }
        for (j = 0; j < ptKnown->num_neighbors; j++) {
            atIslands[i].known_islands[j] =
                ptOdysseus->map.connections[ptKnown->neighbors[j]].name;
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

    for (i = 0; i < nCount; i++) {
        free(atIslands[i].known_islands);
    }
    free(atIslands);
}
