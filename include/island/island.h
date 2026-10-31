#ifndef __ISLAND_H__
#define __ISLAND_H__
 
/***********************************************
 *
 * @File : island.h
 * @Purpose : declarations for the island process
 * @Author : Axel Boleda i Callizo
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
#include "utils/utils.h"
#include "utils/utils_communication.h"
#include "../lib/sphragis/sphragis.h"
 
// Constant declarations
#define EXPECTED_ARGC 3
#define MAX_STOCK_NAME 100
#define EXTENSION_DELIMITER "."
#define PATH_DELIMITER "/"
#define STOCKS_FOLDER "stocks"
#define ERROR_ARGC "Usage: ./island <config.dat> <stocks/stock.db>"
#define ERROR_FILE_CONFIG "ERROR: config file couldn't be open"
#define ERROR_FILE_STOCK "ERROR: stock file couldn't be open"
#define ERROR_LOAD_CONFIG "ERROR: data from the config file could not been open"
#define MSG_ISLAND_FINISHED "\nIsland process finished."
 
#define WELCOME "Island %s initialized.\nPort capacity: %d ships.\n%d sea routes loaded.\n%d products available."
#define FAREWELL "\%s closes its port."
 
// Custom type definitions
typedef enum {
    CLIENT_NEW = 0,
    CLIENT_WAITING,
    CLIENT_DOCKED,
    CLIENT_LEFT
} ClientState;
 
typedef struct {
    char *psName;
    char *psIp;
    int nPort;
} Route;
 
typedef struct {
    int nFd;
    char *psName;
    size_t tQueued;
    ClientState eState;
} Client;
 
typedef struct {
    char sName[MAX_STOCK_NAME];
    int nQuantity;
    int nPrice;
} Stock;
 
typedef struct {
    char *psName;
    char *psFile;
    char *psIp;
    int nPort;
    int nCapacity;
    size_t tNumRoutes;
    Route *atRoutes;
    fd_set tFdsClients;
    int nMaxFd;
    int nServerFd;
    Client *atClients;
    int nNumDocked;
    size_t tNumClients;
    size_t tArrivalCounter;
    Stock *atStock;
    size_t tNumStock;
    int nFdStock;
} Island;
 
// Project includes
#include "island_parser.h"
#include "island_conversetion.h"
 
// Functions
void freeRoute(Route *ptRoute);
void freeIsland(Island *ptIsland, SPHRAGIS_Island *ptSphragis);
void freeStock(Stock **patStock);
void closeFiles(int *pnFdConfig, int *pnFdStock);
void errorLoadingHandler(const char *psError, int *pnFdConfig, int *pnFdStock);
void printData(Island tIsland, Stock *atStock, size_t tNumStock);
int loadData(int *pnFdConfig, int *pnFdStock, Island *ptIsland, SPHRAGIS_Island *ptSphragis, Stock **patStock,size_t *ptNumStock);
 
// End of __ISLAND_H__
#endif
