#ifndef __ISLAND_H_
#define __ISLAND_H_
 
/***********************************************
 *
 * @File : island.h
 * @Purpose : declarations for the island process
 * @Author : Axel Boleda i Callizo
 * @Date : 24/09/2026
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
 
// Constants
#define EXPECTED_ARGC 3
#define ERROR_ARGC "Usage: ./island <config.dat> <stocks/stock.db>"
#define ERROR_FILE_CONFIG "ERROR: config file couldn't be open"
#define ERROR_FILE_STOCK "ERROR: stock file couldn't be open"
#define ERROR_LOAD_CONFIG "ERROR: data from the config file could not been open"
#define FAREWELL "\nFinishing the Island process"

//Structs
typedef struct {
    char *psName;
    char *psIp;
    int nPort;
} Route;
 
typedef struct {
    char *psName;
    char *psFile;
    char *psIp;
    int nPort;
    int nCapacity;
    size_t tNumRoutes;
    Route *atRoutes;
} Island;
 
typedef struct {
    char sName[100];
    int nQuantity;
    int nPrice;
} Stock;

// Local Includes
#include "island_parser.h"
#include "../utils/utils.h"
#include "../lib/sphragis/sphragis.h"
 
// Functions
void freeIsland(Island *island, SPHRAGIS_Island *sphragis);
void freeStock(Stock **stock);
void freeRoute(Route *ptRoute);
void closeFiles(int *pnFdConfig, int *pnFdStock); 
#endif
 