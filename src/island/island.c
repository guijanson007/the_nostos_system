/***********************************************
 *
 * @File : island.c
 * @Purpose : manages the island process.
 *            Arguments: argv[1] = config file
 *                       argv[2] = stock file
 * @Author : Axel Boleda Callizo
 * @Date : 31/10/2026
 *
 ************************************************/

#define _GNU_SOURCE

#include "island/island.h"

static volatile int nEvQuit = 0;

/***********************************************
 *
 * @Name : onSigint
 * @Def : SIGINT handler. Sets the quit flag so the main loop ends.
 * @Arg : In: nSignum = number of the signal received (unused)
 * @Ret : None.
 *
 ************************************************/
static void onSigint(int nSignum) {
    (void)nSignum;
    nEvQuit = 1;
}

/***********************************************
 *
 * @Name : freeRoute
 * @Def : Frees the strings of one route.
 * @Arg : In/Out: ptRoute = route to free
 * @Ret : None.
 *
 ************************************************/
void freeRoute(Route *ptRoute) {
    free(ptRoute->psName);
    free(ptRoute->psIp);
    ptRoute->psName = NULL;
    ptRoute->psIp = NULL;
}

/***********************************************
 *
 * @Name : freeIsland
 * @Def : Frees all the dynamic memory of the island and the sphragis
 *        island. Safe to call more than once.
 * @Arg : In/Out: ptIsland = island to free
 *        In/Out: ptSphragis = sphragis island to free
 * @Ret : None.
 *
 ************************************************/
void freeIsland(Island *ptIsland, SPHRAGIS_Island *ptSphragis) {
    size_t i = 0;

    if (NULL != ptIsland) {
        free(ptIsland->psName);
        free(ptIsland->psFile);
        free(ptIsland->psIp);
        if (NULL != ptIsland->atRoutes) {
            for (i = 0; i < ptIsland->tNumRoutes; i++) {
                freeRoute(&ptIsland->atRoutes[i]);
            }
            free(ptIsland->atRoutes);
        }
        memset(ptIsland, 0, sizeof(*ptIsland));
    }

    if (NULL != ptSphragis) {
        free(ptSphragis->name);
        ptSphragis->name = NULL;
        if (NULL != ptSphragis->known_islands) {
            for (i = 0; i < (size_t)ptSphragis->known_island_count; i++) {
                free(ptSphragis->known_islands[i]);
            }
            free(ptSphragis->known_islands);
            ptSphragis->known_islands = NULL;
        }
        ptSphragis->known_island_count = 0;
    }
}

/***********************************************
 *
 * @Name : freeStock
 * @Def : Frees the stock dynamic array.
 * @Arg : In/Out: patStock = pointer to the stock array
 * @Ret : None.
 *
 ************************************************/
void freeStock(Stock **patStock) {
    if (NULL == patStock || NULL == *patStock) {
        return;
    }
    free(*patStock);
    *patStock = NULL;
}

/***********************************************
 *
 * @Name : closeFiles
 * @Def : Closes the config and stock file descriptors.
 * @Arg : In/Out: pnFdConfig = pointer to the config file descriptor
 *        In/Out: pnFdStock = pointer to the stock file descriptor
 * @Ret : None.
 *
 ************************************************/
void closeFiles(int *pnFdConfig, int *pnFdStock) {
    close(*pnFdConfig);
    close(*pnFdStock);
}

/***********************************************
 *
 * @Name : errorLoadingHandler
 * @Def : Prints a loading error and closes both files.
 * @Arg : In: psError = error message to print
 *        In/Out: pnFdConfig = pointer to the config file descriptor
 *        In/Out: pnFdStock = pointer to the stock file descriptor
 * @Ret : None.
 *
 ************************************************/
void errorLoadingHandler(const char *psError, int *pnFdConfig, int *pnFdStock) {
    printErr((char *)psError);
    closeFiles(pnFdConfig, pnFdStock);
}

/***********************************************
 *
 * @Name : printData
 * @Def : Prints the island info, its routes and its stock (debugging).
 * @Arg : In: tIsland = copy of the island struct
 *        In: atStock = dynamic array of stock items
 *        In: tNumStock = number of elements in atStock
 * @Ret : None.
 *
 ************************************************/
void printData(Island tIsland, Stock *atStock, size_t tNumStock) {
    char *psBuffer = NULL;
    size_t i = 0;

    if (-1 != asprintf(&psBuffer, "%s - %s - %s %d - %d", tIsland.psName,
                       tIsland.psFile, tIsland.psIp, tIsland.nPort,
                       tIsland.nCapacity)) {
        println(psBuffer);
        free(psBuffer);
    }

    for (i = 0; i < tIsland.tNumRoutes; i++) {
        if (-1 != asprintf(&psBuffer, "\t%s - %s - %d",
                           tIsland.atRoutes[i].psName, tIsland.atRoutes[i].psIp,
                           tIsland.atRoutes[i].nPort)) {
            println(psBuffer);
            free(psBuffer);
        }
    }

    for (i = 0; i < tNumStock; i++) {
        if (-1 != asprintf(&psBuffer, "%s - %d - %d", atStock[i].sName,
                           atStock[i].nQuantity, atStock[i].nPrice)) {
            println(psBuffer);
            free(psBuffer);
        }
    }
}

/***********************************************
 *
 * @Name : loadData
 * @Def : Loads the island (config file) and the stock (stock file). On
 *        error it prints the message, closes both files and frees.
 * @Arg : In/Out: pnFdConfig = pointer to the config file descriptor
 *        In/Out: pnFdStock = pointer to the stock file descriptor
 *        Out: ptIsland = island to fill
 *        Out: ptSphragis = sphragis island to fill
 *        Out: patStock = dynamic array of stock to fill
 *        Out: ptNumStock = number of elements in the stock array
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 ************************************************/
int loadData(int *pnFdConfig, int *pnFdStock, Island *ptIsland,
             SPHRAGIS_Island *ptSphragis, Stock **patStock,
             size_t *ptNumStock) {
    if (-1 == loadIsland(*pnFdConfig, ptIsland, ptSphragis)) {
        errorLoadingHandler(ERROR_LOAD_CONFIG, pnFdConfig, pnFdStock);
        freeIsland(ptIsland, ptSphragis);
        return -1;
    }

    if (-1 == loadStock(*pnFdStock, patStock, ptNumStock)) {
        errorLoadingHandler(ERROR_FILE_STOCK, pnFdConfig, pnFdStock);
        freeIsland(ptIsland, ptSphragis);
        freeStock(patStock);
        return -1;
    }

    return 0;
}

/***********************************************
 *
 * @Name : checkArgcIsland
 * @Def : Checks the number and validity of command-line arguments.
 * @Arg : In: argc = number of arguments
 *        In: argv = arguments (island config file and stock file)
 * @Ret : 0 if the arguments are valid, -1 otherwise.
 *
 ************************************************/
int checkArgcIsland(int argc, char *argv[]) {
    char *psConfigAux = NULL;
    char *psStockAux = NULL;
    char *psIslandConfigFile = NULL;
    char *psStockFolder = NULL;
    char *psAux = NULL;
    char *psIslandStockFile = NULL;

    if (EXPECTED_ARGC != argc) {
        return -1;
    }

    psConfigAux = strdup(argv[1]);
    psStockAux = strdup(argv[2]);

    if (NULL == psConfigAux || NULL == psStockAux) {
        free(psConfigAux);
        free(psStockAux);
        return -1;
    }

    psIslandConfigFile = strtok(psConfigAux, EXTENSION_DELIMITER);
    psStockFolder = strtok(psStockAux, PATH_DELIMITER);
    psAux = strtok(NULL, PATH_DELIMITER);
    if (NULL == psIslandConfigFile || NULL == psStockFolder || NULL == psAux) {
        free(psConfigAux);
        free(psStockAux);
        return -1;
    }
    psIslandStockFile = strtok(psAux, EXTENSION_DELIMITER);
    if (NULL == psIslandStockFile) {
        free(psConfigAux);
        free(psStockAux);
        return -1;
    }

    if (0 != strcasecmp(psIslandConfigFile, psIslandStockFile) ||
        0 != strcmp(psStockFolder, STOCKS_FOLDER)) {
        free(psConfigAux);
        free(psStockAux);
        return -1;
    }

    free(psConfigAux);
    free(psStockAux);

    return 0;
}

/***********************************************
 *
 * @Name : openFiles
 * @Def : Opens the config file (read only) and the stock file (read and
 *        write). If the stock file fails, the config file is closed.
 * @Arg : In: argv = arguments (config file and stock file)
 *        Out: pnFdConfig = config file descriptor
 *        Out: pnFdStock = stock file descriptor
 * @Ret : 0 if both files were opened, -1 otherwise.
 *
 ************************************************/
static int openFiles(char *argv[], int *pnFdConfig, int *pnFdStock) {
    *pnFdConfig = open(argv[1], O_RDONLY);
    if (-1 == *pnFdConfig) {
        printErr(ERROR_FILE_CONFIG);
        return -1;
    }

    *pnFdStock = open(argv[2], O_RDWR);
    if (-1 == *pnFdStock) {
        printErr(ERROR_FILE_STOCK);
        close(*pnFdConfig);
        return -1;
    }

    return 0;
}

/***********************************************
 *
 * @Name : startServer
 * @Def : Opens the listening socket, prepares the set of descriptors and
 *        prints the welcome message. If the socket fails, it closes the
 *        files and frees the island.
 * @Arg : In/Out: ptIsland = island that will listen for connections
 *        In/Out: pnFdConfig = pointer to the config file descriptor
 *        In/Out: ptSphragis = sphragis island to free on error
 * @Ret : 0 if the server is ready, -1 otherwise.
 *
 ************************************************/
static int startServer(Island *ptIsland, int *pnFdConfig,
                       SPHRAGIS_Island *ptSphragis) {
    char *psBuff = NULL;

    ptIsland->nServerFd = openListenConn(ptIsland->psIp, ptIsland->nPort);
    if (0 > ptIsland->nServerFd) {
        write(STDOUT_FILENO, ERROR_SOCKET, strlen(ERROR_SOCKET));
        closeFiles(pnFdConfig, &ptIsland->nFdStock);
        freeStock(&ptIsland->atStock);
        freeIsland(ptIsland, ptSphragis);
        return -1;
    }

    FD_ZERO(&ptIsland->tFdsClients);
    FD_SET(ptIsland->nServerFd, &ptIsland->tFdsClients);

    ptIsland->nMaxFd = ptIsland->nServerFd;

    if (-1 == asprintf(&psBuff, WELCOME, ptIsland->psName, ptIsland->nCapacity,
                       (int)ptIsland->tNumRoutes, (int)ptIsland->tNumStock)) {
        close(ptIsland->nServerFd);
        closeFiles(pnFdConfig, &ptIsland->nFdStock);
        freeStock(&ptIsland->atStock);
        freeIsland(ptIsland, ptSphragis);
        return -1;
    }
    println(psBuff);
    free(psBuff);

    return 0;
}

/***********************************************
 *
 * @Name : acceptClient
 * @Def : Accepts a new connection and adds it to the set of descriptors.
 * @Arg : In/Out: ptIsland = island that receives the connection
 * @Ret : None.
 *
 ************************************************/
static void acceptClient(Island *ptIsland) {
    int nFdClient = -1;

    nFdClient = accept(ptIsland->nServerFd, NULL, NULL);
    if (0 > nFdClient) {
        write(STDOUT_FILENO, ERROR_SOCKET, strlen(ERROR_SOCKET));
        return;
    }
    FD_SET(nFdClient, &ptIsland->tFdsClients);
    if (nFdClient > ptIsland->nMaxFd) {
        ptIsland->nMaxFd = nFdClient;
    }
}

/***********************************************
 *
 * @Name : handleClient
 * @Def : Reads a message from a client and executes the matching option.
 *        If the client disconnected, it is removed.
 * @Arg : In/Out: ptIsland = island that handles the client
 *        In: nFd = descriptor of the client
 * @Ret : None.
 *
 ************************************************/
static void handleClient(Island *ptIsland, int nFd) {
    int nResult = -1;
    Message tMsg = {0};

    nResult = receiveMessage(&tMsg, nFd);

    if (-1 == nResult) {
        clientLeaves(ptIsland, nFd);
        return;
    } else if (-2 == nResult) {
        sendNack(nFd, NACK_INVALID_LENGTH);
        return;
    } else if (-3 == nResult) {
        sendNack(nFd, NACK_INVALID_FLAGS);
        return;
    }

    choseOption(ptIsland, tMsg, nFd);
}

/***********************************************
 *
 * @Name : runServer
 * @Def : Main loop of the island. Waits with select for new connections
 *        and messages until SIGINT is received.
 * @Arg : In/Out: ptIsland = island that runs the server
 * @Ret : None.
 *
 ************************************************/
static void runServer(Island *ptIsland) {
    int nFd = 0;
    fd_set tReadFds = {0};

    while (0 == nEvQuit) {
        tReadFds = ptIsland->tFdsClients;
        if (-1 == select(ptIsland->nMaxFd + 1, &tReadFds, NULL, NULL, NULL)) {
            break;
        }

        for (nFd = 0; nFd <= ptIsland->nMaxFd; nFd++) {
            if (0 == FD_ISSET(nFd, &tReadFds) ||
                0 == FD_ISSET(nFd, &ptIsland->tFdsClients)) {
                continue;
            }

            if (nFd == ptIsland->nServerFd) {
                acceptClient(ptIsland);
            } else {
                handleClient(ptIsland, nFd);
            }
        }
    }
}

/***********************************************
 *
 * @Name : closeIsland
 * @Def : Closes every open descriptor, frees the clients, closes the files
 *        and frees the island and its stock.
 * @Arg : In/Out: ptIsland = island to close
 *        In/Out: pnFdConfig = pointer to the config file descriptor
 *        In/Out: ptSphragis = sphragis island to free
 * @Ret : None.
 *
 ************************************************/
static void closeIsland(Island *ptIsland, int *pnFdConfig,
                        SPHRAGIS_Island *ptSphragis) {
    int nFd = 0;
    size_t i = 0;
    char *psFarewell = NULL;

    for (nFd = 0; nFd <= ptIsland->nMaxFd; nFd++) {
        if (0 != FD_ISSET(nFd, &ptIsland->tFdsClients)) {
            close(nFd);
        }
    }

    for (i = 0; i < ptIsland->tNumClients; i++) {
        free(ptIsland->atClients[i].psName);
    }
    free(ptIsland->atClients);

    closeFiles(pnFdConfig, &ptIsland->nFdStock);
    freeStock(&ptIsland->atStock);
    if (-1 != asprintf(&psFarewell, FAREWELL, ptIsland->psName)) {
        println(psFarewell);
        free(psFarewell);
    }
    freeIsland(ptIsland, ptSphragis);
}

/***********************************************
 *
 * @Name : main
 * @Def : Manages the general flow of the island process.
 * @Arg : In: argc = number of arguments
 *        In: argv = arguments (config file and stock file)
 * @Ret : EXIT_SUCCESS if everything went well, EXIT_FAILURE otherwise.
 *
 ************************************************/
int main(int argc, char *argv[]) {
    int nFdConfig = -1;
    Island tIsland = {0};
    SPHRAGIS_Island tSphragis = {0};

    tIsland.nFdStock = -1;

    if (0 != checkArgcIsland(argc, argv)) {
        printErr(ERROR_ARGC);
        return EXIT_FAILURE;
    }

    if (-1 == openFiles(argv, &nFdConfig, &tIsland.nFdStock)) {
        return EXIT_FAILURE;
    }

    if (-1 == loadData(&nFdConfig, &tIsland.nFdStock, &tIsland, &tSphragis,
                       &tIsland.atStock, &tIsland.tNumStock)) {
        return EXIT_FAILURE;
    }

    // DEBUGGING
    // printData(tIsland, tIsland.atStock, tIsland.tNumStock);

    signal(SIGINT, onSigint);
    signal(SIGPIPE, SIG_IGN);

    if (-1 == startServer(&tIsland, &nFdConfig, &tSphragis)) {
        return EXIT_FAILURE;
    }

    runServer(&tIsland);

    closeIsland(&tIsland, &nFdConfig, &tSphragis);

    return EXIT_SUCCESS;
}
