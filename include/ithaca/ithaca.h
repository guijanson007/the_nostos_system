#ifndef __ITHACA_H_
#define __ITHACA_H_

/***********************************************
 *
 * @File : ithaca.h
 * @Purpose : declarations for the ithaca process
 * @Author : Axel Boleda Callizo
 * @Date : 30/10/2026
 *
 ************************************************/

// System includes
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

// Constants
#define EXPECTED_ARGC 3
#define ITHACA_NUM_LINES 3
#define ITHACA_FILE "ithaca.dat"
#define VOYAGES_FILE "voyages.dat"
 
#define WELCOME "Ithaca initialized. %d voyages loaded."
#define FAREWELL "\nIthaca closes the harbor."
 
#define ERROR_ARGC "Usage: ./ithaca <ithaca.dat> <voyages.dat>"
#define ERROR_FILE_ITHACA "ERROR ITHACA: the file couldn't be opened"
#define ERROR_FILE_VOYAGE "ERROR VOYAGE: the file couldn't be opened"
#define ERROR_LOAD_ITHACA "ERROR ITHACA: the data couldn't be loaded from the file"
#define ERROR_LOAD_VOYAGE "ERROR VOYAGE: the data couldn't be loaded from the file"
#define ERROR_SOCKET "ERROR: the socket could not be created\n"
#define ERROR_ACCEPT "ERROR: The conection to the socket\n"
#define ERROR_RECEIVE_MESSAGE "ERROR: There was an error receiving the message from the client\n"
#define WAITING_FOR_CLIENTS "Waiting for Odysseus...\n"

// Custom type
typedef enum {
    VOYAGE_AVAILABLE = 0,
    VOYAGE_ASSIGNED,
    VOYAGE_FAILED,
} VoyageState;

typedef struct {
    int nFd;
    char *psName;
} Client;
 
 
typedef struct {
    int nId;
    char *psObject;
    char *psFile;
    char *psDestination;
    int nReward;
    VoyageState eState;
    char *psOwner;
} Voyage;
 
typedef struct {
    char *psName;
    char *psMission;
    char *psIp;
    int nPort;
    fd_set tFdsClients;
    int nMaxFd;
    int nServerFd;
    Client *atClients;
    size_t tNumClients;
    Voyage *atVoyages;
    size_t tNumVoyages;
} Ithaca;

// Local includes
#include "utils/utils.h"
#include "utils/utils_communication.h"

// Functions
void freeVoyage(Voyage **voyages, size_t *numVoyage);
void freeIthaca(Ithaca *ithaca);
void closeFiles(int *pnFdIthaca, int *pnFdVoyage);

#endif
