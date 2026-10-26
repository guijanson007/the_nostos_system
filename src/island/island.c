/***********************************************
 *
 * @File : island.c
 * @Purpose : manages the island process.
 *            Arguments: argv[1] = config file
 *                       argv[2] = stock file
 * @Author : Axel Boleda Callizo
 * @Date : 24/09/2026
 *
 ************************************************/

#define _GNU_SOURCE

#include "island/island.h"

int nEv_quit = 0;

static void onSigint(int nSignum) {
    (void)nSignum;
    nEv_quit = 1;
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
 * @Arg : In: ptIsland = copy of the island struct
 *        In: atStock = dynamic array of stock items
 *        In: tNumStock = number of elements in atStock
 * @Ret : None.
 *
 ************************************************/
void printData(Island ptIsland, Stock *atStock, size_t tNumStock) {
    char *psBuffer = NULL;
    size_t i = 0;

    if (-1 != asprintf(&psBuffer, "%s - %s - %s %d - %d", ptIsland.psName,
                       ptIsland.psFile, ptIsland.psIp, ptIsland.nPort,
                       ptIsland.nCapacity)) {
        println(psBuffer);
        free(psBuffer);
    }

    for (i = 0; i < ptIsland.tNumRoutes; i++) {
        if (-1 !=
            asprintf(&psBuffer, "\t%s - %s - %d", ptIsland.atRoutes[i].psName,
                     ptIsland.atRoutes[i].psIp, ptIsland.atRoutes[i].nPort)) {
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
 * @Def : Loads the island (config file) and the stock (stock file).  On error
 * it prints the message, closes both files and frees.
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
 * @Ret : 0 if arguments are valid return 0, -1 otherwise.
 *
 ************************************************/
int checkArgcIsland(int argc, char *argv[]) {
    if (EXPECTED_ARGC != argc) {
        return -1;
    }

    char *configAux = strdup(argv[1]);
    char *stockAux = strdup(argv[2]);

    if (configAux == NULL || stockAux == NULL) {
        free(configAux);
        free(stockAux);
        return -1;
    }

    char *islandConfigFile = strtok(configAux, ".");
    char *stockFoler = strtok(stockAux, "/");
    char *aux = strtok(NULL, "/");
    if (NULL == islandConfigFile || NULL == stockFoler || NULL == aux) {
        free(configAux);
        free(stockAux);
        return -1;
    }
    char *islandStockFile = strtok(aux, ".");
    if (NULL == islandStockFile) {
        free(configAux);
        free(stockAux);
        return -1;
    }

    if (0 != strcasecmp(islandConfigFile, islandStockFile) ||
        0 != strcmp(stockFoler, "stocks")) {
        free(configAux);
        free(stockAux);
        return -1;
    }

    free(configAux);
    free(stockAux);

    return 0;
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
    int nFdStock = -1;
    int nResult = -1;
    Island tIsland = {0};
    SPHRAGIS_Island tSphragis = {0};

    if (0 != checkArgcIsland(argc, argv)) {
        printErr(ERROR_ARGC);
        return EXIT_FAILURE;
    }

    nFdConfig = open(argv[1], O_RDONLY);
    if (-1 == nFdConfig) {
        printErr(ERROR_FILE_CONFIG);
        return EXIT_FAILURE;
    }

    nFdStock = open(argv[2], O_RDONLY);
    if (-1 == nFdStock) {
        printErr(ERROR_FILE_STOCK);
        close(nFdConfig);
        return EXIT_FAILURE;
    }

    if (-1 == loadData(&nFdConfig, &nFdStock, &tIsland, &tSphragis,
                       &tIsland.atStock, &tIsland.tNumStock)) {
        return EXIT_FAILURE;
    }

    // DEBUGGING
    printData(tIsland, tIsland.atStock, tIsland.tNumStock);
    signal(SIGINT, onSigint);
    signal(SIGPIPE, SIG_IGN);

    tIsland.nServerFd = openListenConn(tIsland.psIp, tIsland.nPort);
    if (tIsland.nServerFd < 0) {
        write(STDOUT_FILENO, ERROR_SOCKET, strlen(ERROR_SOCKET));
        closeFiles(&nFdConfig, &nFdStock);
        freeStock(&tIsland.atStock);
        freeIsland(&tIsland, &tSphragis);
        return EXIT_FAILURE;
    }

    FD_ZERO(&tIsland.fds_clients);
    FD_SET(tIsland.nServerFd, &tIsland.fds_clients);

    tIsland.max_fd = tIsland.nServerFd;

    while (!nEv_quit) {
        fd_set read_fds = tIsland.fds_clients;
        if (select(tIsland.max_fd + 1, &read_fds, NULL, NULL, NULL) == -1)
            break;

        for (int fd = 0; fd <= tIsland.max_fd; fd++) {
            if (!FD_ISSET(fd, &read_fds))
                continue;

            if (fd == tIsland.nServerFd) {
                int fd_client = accept(tIsland.nServerFd, NULL, NULL);
                if (fd_client < 0) {
                    write(STDOUT_FILENO, ERROR_SOCKET, strlen(ERROR_SOCKET));
                    continue;
                }
                FD_SET(fd_client, &tIsland.fds_clients);
                if (fd_client > tIsland.max_fd)
                    tIsland.max_fd = fd_client;
            } else {
                Message msg;
                nResult = receiveMessage(&msg, fd);

                if (-1 == nResult) {
                    clientLeaves(&tIsland, fd);
                    continue;
                } else if (-2 == nResult) {
                    continue;
                }

                choseOption(&tIsland, msg, fd);
            }
        }
    }

    for (int fd = 0; fd <= tIsland.max_fd; fd++) {
        if (FD_ISSET(fd, &tIsland.fds_clients))
            close(fd);
    }

    closeFiles(&nFdConfig, &nFdStock);
    freeStock(&tIsland.atStock);
    freeIsland(&tIsland, &tSphragis);
    println("\nIsland process finished.");
    return EXIT_SUCCESS;
}
