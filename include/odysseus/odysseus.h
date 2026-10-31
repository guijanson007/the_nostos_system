#ifndef _ODYSSEUS_H
#define _ODYSSEUS_H

/***********************************************
 *
 * @File : odysseus.h
 * @Purpose : entrypoint of odysseus process
 * @Author : Guilherme Rodrigues
 * @Date : 31/10/2026
 *
 ************************************************/

// System includes
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
 
// Project includes
#include "odysseus/odysseus_status.h"
#include "sphragis.h"
#include "utils/utils.h"

// Constant declarations
#define EXPECTED_ARGC 2
#define AT_ITHACA -1
#define NO_SOCKET -1
#define ERROR_ARGC "Usage: odysseus <config_file>.dat"
#define ERROR_OPEN_FILE "Failed to open file: %s"
#define ERROR_INIT_STATE "Failed to initialise the odysseus state"


//Project Include
#include "odysseus/odysseus_cargo.h"


// Custom type definitions
typedef struct KnownIsland {
    char *psName;
    char *psIp;
    int nPort;
    int nNumNeighbors;
    int *anNeighbors;
} KnownIsland;
 
// Route received when buying a map, before it is merged into the Map
typedef struct {
    char *psName;
    char *psIp;
    int nPort;
} MapRoute;
 
typedef struct {
    // yellow[0] and green[1]
    KnownIsland *atConnections;
    int nNumConnections;
} Map;
 
typedef struct {
    int nId;
    char *psObject;
    char *psDestination;
    int nReward;
} OdysseusVoyage;
 
typedef struct Odysseus {
    char *psName;
    char *psPath;
    char *psIp;
    char *psPort;
    char *psInitialInfo;
    int nIthacaSocket;
    Map tMap;
    int nWhereAmI;  // Index of the current island in the connections array. If Ithaca, -1
    int nIslandSocket;
    Cargo tCargo;
    char *psLastError;
    OdysseusVoyage *atListedVoyages;
    int nNumListedVoyages;
    OdysseusVoyage tActiveVoyage;
} Odysseus;

#endif
