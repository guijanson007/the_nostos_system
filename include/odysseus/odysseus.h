#ifndef _ODYSSEUS_H
#define _ODYSSEUS_H

/***********************************************
 *
 * @File : odysseus.h
 * @Purpose : entrypoint of odysseus process
 * @Author : Guilherme Rodrigues
 * @Date : 20/09/2026
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

// own includes
#include "odysseus/odysseus_status.h"
#include "sphragis.h"
#include "utils/utils.h"

// Constant declarations
#define EXPECTED_ARGC 2
#define ERROR_ARGC "Usage: odysseus <config_file>.dat"
#define ERROR_OPEN_FILE "Failed to open file: %s"
#define ERROR_INIT_STATE "Failed to initialise the odysseus state"

#include "odysseus/odysseus_cargo.h"

// Structs
typedef struct KnownIsland {
    char *name;
    char *ip;
    int port;
    int num_neighbors;
    int *neighbors; // index of the neighbors in the connection
} KnownIsland;

typedef struct {
    KnownIsland *connections; // yellow[0] and green[1]
    int numConnections;
} Map;

typedef struct {
    int id; // 0 when there is no voyage
    char *object;
    char *destination;
    int reward;
} OdysseusVoyage;

typedef struct Odysseus {
    char *name;
    char *path;
    char *ip;
    char *port;
    char *initialInfo;
    int ithaca_socket;
    Supply *supplies;
    Map map;
    int whereAmI; // index of the current island in the connections array If
                  // Ithaca -1
    int island_socket;
    Cargo cargo;
    char *lastError;
    OdysseusVoyage *listedVoyages; // Last LIST VOYAGES answer
    int numListedVoyages;
    OdysseusVoyage activeVoyage;
} Odysseus;

#endif
