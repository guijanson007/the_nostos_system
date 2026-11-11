/***********************************************
 *
 * @File : ithaca.c
 * @Purpose : manages the ithaca process.
 *            Arguments: argv[1] = ithaca file
 *                       argv[2] = voyages file
 * @Author : Axel Boleda i Callizo
 * @Date : 22/09/2026
 *
 ************************************************/
#define _GNU_SOURCE

// header
#include "ithaca/ithaca.h"

// own headers
#include "ithaca/ithaca.h"
#include "ithaca/ithaca_conversetion.h"
#include "ithaca/ithaca_parser.h"
#include "utils/utils.h"

static volatile int nEvQuit = 0;

/***********************************************
 *
 * @Name : onSigint
 * @Def : SIGINT handler. Only sets the quit flag; printing and freeing are
 *        done in main, outside the handler.
 * @Arg : In: nSignum = signal number (unused)
 * @Ret : None.
 *
 ************************************************/
static void onSigint(int nSignum) {
    (void)nSignum;
    nEvQuit = 1;
}

/***********************************************
 *
 * @Name : freeIthaca
 * @Def : Frees the dynamic memory of the ithaca struct.
 * @Arg : In/Out: ptIthaca = ithaca to free
 * @Ret : None.
 *
 ************************************************/
void freeIthaca(Ithaca *ptIthaca) {
    size_t i = 0;

    if (NULL == ptIthaca) {
        return;
    }
    free(ptIthaca->psName);
    free(ptIthaca->psMission);
    free(ptIthaca->psIp);

    for (i = 0; i < ptIthaca->tNumClients; i++) {
        free(ptIthaca->atClients[i].psName);
    }
    free(ptIthaca->atClients);
    memset(ptIthaca, 0, sizeof(*ptIthaca));
}

/***********************************************
 *
 * @Name : freeVoyage
 * @Def : Frees the voyages dynamic array and the strings of each voyage.
 * @Arg : In/Out: patVoyages = pointer to the voyages array
 *        In/Out: ptNumVoyages = number of voyages, set to 0
 * @Ret : None.
 *
 ************************************************/
void freeVoyage(Voyage **patVoyages, size_t *ptNumVoyages) {
    size_t i = 0;

    if (NULL == patVoyages || NULL == *patVoyages) {
        return;
    }
    for (i = 0; i < *ptNumVoyages; i++) {
        free((*patVoyages)[i].psObject);
        free((*patVoyages)[i].psFile);
        free((*patVoyages)[i].psDestination);
        free((*patVoyages)[i].psOwner);
    }
    free(*patVoyages);
    *patVoyages = NULL;
    *ptNumVoyages = 0;
}

/***********************************************
 *
 * @Name : closeFiles
 * @Def : Closes the ithaca and voyages file descriptors.
 * @Arg : In/Out: pnFdIthaca = pointer to the ithaca file descriptor
 *        In/Out: pnFdVoyage = pointer to the voyages file descriptor
 * @Ret : None.
 *
 ************************************************/
void closeFiles(int *pnFdIthaca, int *pnFdVoyage) {
    close(*pnFdIthaca);
    close(*pnFdVoyage);
}

/***********************************************
 *
 * @Name : errorLoadingHandler
 * @Def : Prints a loading error and closes both files.
 * @Arg : In: psError = error message to print
 *        In/Out: pnFdIthaca = pointer to the ithaca file descriptor
 *        In/Out: pnFdVoyage = pointer to the voyages file descriptor
 * @Ret : None.
 *
 ************************************************/
void errorLoadingHandler(const char *psError, int *pnFdIthaca,
                         int *pnFdVoyage) {
    printErr((char *)psError);
    closeFiles(pnFdIthaca, pnFdVoyage);
}

/***********************************************
 *
 * @Name : loadData
 * @Def : Loads the ithaca and the voyages. On error it prints the message,
 * closes both files and frees
 * @Arg : In/Out: pnFdIthaca = pointer to the ithaca file descriptor
 *        In/Out: pnFdVoyage = pointer to the voyages file descriptor
 *        Out: ptIthaca = ithaca to fill
 *        Out: patVoyages = dynamic array of voyages to fill
 *        Out: ptNumVoyages = number of elements in the voyages array
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 ************************************************/
int loadData(int *pnFdIthaca, int *pnFdVoyage, Ithaca *ptIthaca,
             Voyage **patVoyages, size_t *ptNumVoyages) {
    if (-1 == loadIthacaData(*pnFdIthaca, ptIthaca)) {
        errorLoadingHandler(ERROR_LOAD_ITHACA, pnFdIthaca, pnFdVoyage);
        return -1;
    }

    if (-1 == loadVoyageData(*pnFdVoyage, patVoyages, ptNumVoyages)) {
        errorLoadingHandler(ERROR_LOAD_VOYAGE, pnFdIthaca, pnFdVoyage);
        freeIthaca(ptIthaca);
        return -1;
    }

    return 0;
}

/***********************************************
 *
 * @Name : printData
 * @Def : Prints the ithaca info and its voyages.
 * @Arg : In: ptIthaca = pointer to the ithaca struct
 *        In: atVoyages = dynamic array of voyages
 *        In: tNumVoyages = number of elements in atVoyages
 * @Ret : None.
 *
 ************************************************/
void printData(Ithaca *ptIthaca, Voyage *atVoyages, size_t tNumVoyages) {
    char *psBuffer = NULL;
    size_t i = 0;

    if (-1 != asprintf(&psBuffer, "%s - %s - %s - %d", ptIthaca->psName,
                       ptIthaca->psMission, ptIthaca->psIp, ptIthaca->nPort)) {
        println(psBuffer);
        free(psBuffer);
    }

    for (i = 0; i < tNumVoyages; i++) {
        if (-1 != asprintf(&psBuffer, "%d: %s - %s - %s - %d", atVoyages[i].nId,
                           atVoyages[i].psObject, atVoyages[i].psFile,
                           atVoyages[i].psDestination, atVoyages[i].nReward)) {
            println(psBuffer);
            free(psBuffer);
        }
    }
}

/***********************************************
 *
 * @Name : checkArgcIthaca
 * @Def : Checks the number and validity of command-line arguments.
 * @Arg : In: argc = number of arguments
 *        In: argv = arguments (ithaca file and voyages file)
 * @Ret : 0 if arguments are valid return 0, -1 otherwise.
 *
 ************************************************/
int checkArgcIthaca(int argc, char *argv[]) {
    if (EXPECTED_ARGC != argc || 0 != strcmp(argv[1], ITHACA_FILE) ||
        0 != strcmp(argv[2], VOYAGES_FILE)) {
        printErr(ERROR_ARGC);
        return -1;
    }
    return 0;
}

/***********************************************
 *
 * @Name : openFiles
 * @Def : Opens the ithaca and voyages files. If the voyages file fails,
 *        the ithaca file is closed.
 * @Arg : In: argv = arguments (ithaca file and voyages file)
 *        Out: pnFdIthaca = ithaca file descriptor
 *        Out: pnFdVoyage = voyages file descriptor
 * @Ret : 0 if both files were opened, -1 otherwise.
 *
 ************************************************/
static int openFiles(char *argv[], int *pnFdIthaca, int *pnFdVoyage) {
    *pnFdIthaca = open(argv[1], O_RDONLY);
    if (-1 == *pnFdIthaca) {
        printErr(ERROR_FILE_ITHACA);
        return -1;
    }

    *pnFdVoyage = open(argv[2], O_RDONLY);
    if (-1 == *pnFdVoyage) {
        printErr(ERROR_FILE_VOYAGE);
        close(*pnFdIthaca);
        return -1;
    }

    return 0;
}

/***********************************************
 *
 * @Name : startServer
 * @Def : Opens the listening socket, gives the voyages to ithaca, prepares
 *        the set of descriptors and prints the welcome message. If the
 *        socket fails, it frees ithaca and the voyages.
 * @Arg : In/Out: ptIthaca = ithaca that will listen for connections
 *        In/Out: patVoyages = pointer to the voyages array
 *        In/Out: ptNumVoyages = number of voyages
 * @Ret : 0 if the server is ready, -1 otherwise.
 *
 ************************************************/
static int startServer(Ithaca *ptIthaca, Voyage **patVoyages,
                       size_t *ptNumVoyages) {
    char *psBuff = NULL;

    ptIthaca->nServerFd = openListenConn(ptIthaca->psIp, ptIthaca->nPort);
    if (0 > ptIthaca->nServerFd) {
        printErr(ERROR_SOCKET);
        freeIthaca(ptIthaca);
        freeVoyage(patVoyages, ptNumVoyages);
        return -1;
    }

    ptIthaca->atVoyages = *patVoyages;
    ptIthaca->tNumVoyages = *ptNumVoyages;

    FD_ZERO(&ptIthaca->tFdsClients);
    FD_SET(ptIthaca->nServerFd, &ptIthaca->tFdsClients);

    ptIthaca->nMaxFd = ptIthaca->nServerFd;

    if (-1 == asprintf(&psBuff, WELCOME, (int)ptIthaca->tNumVoyages)) {
        close(ptIthaca->nServerFd);
        ptIthaca->atVoyages = NULL;
        freeIthaca(ptIthaca);
        freeVoyage(patVoyages, ptNumVoyages);
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
 * @Arg : In/Out: ptIthaca = ithaca that receives the connection
 * @Ret : None.
 *
 ************************************************/
static void acceptClient(Ithaca *ptIthaca) {
    int nFdClient = -1;

    nFdClient = accept(ptIthaca->nServerFd, NULL, NULL);
    if (0 > nFdClient) {
        printErr(ERROR_ACCEPT);
        return;
    }
    FD_SET(nFdClient, &ptIthaca->tFdsClients);
    if (nFdClient > ptIthaca->nMaxFd) {
        ptIthaca->nMaxFd = nFdClient;
    }
}

/***********************************************
 *
 * @Name : handleClient
 * @Def : Reads a message from a client and executes the matching option.
 *        Invalid messages get a NACK and closed connections are removed.
 * @Arg : In/Out: ptIthaca = ithaca that handles the client
 *        In: nFd = descriptor of the client
 * @Ret : None.
 *
 ************************************************/
static void handleClient(Ithaca *ptIthaca, int nFd) {
    int nResult = 0;
    Message tMsg = {0};

    nResult = receiveMessage(&tMsg, nFd);
    if (-2 == nResult) {
        sendNack(nFd, NACK_INVALID_LENGTH);
        return;
    }
    if (-3 == nResult) {
        sendNack(nFd, NACK_INVALID_FLAGS);
        return;
    }
    if (0 != nResult) {
        if (0 != removeClient(ptIthaca, nFd)) {
            // Never registered: drop the socket anyway
            close(nFd);
            FD_CLR(nFd, &ptIthaca->tFdsClients);
        }
        return;
    }

    choseOption(ptIthaca, tMsg, nFd);
}

/***********************************************
 *
 * @Name : runServer
 * @Def : Main loop of ithaca. Waits with select for new connections and
 *        messages until SIGINT is received.
 * @Arg : In/Out: ptIthaca = ithaca that runs the server
 * @Ret : None.
 *
 ************************************************/
static void runServer(Ithaca *ptIthaca) {
    int nFd = 0;
    fd_set tReadFds = {0};

    while (0 == nEvQuit) {
        // Only the server socket is in the set
        if (ptIthaca->nMaxFd == ptIthaca->nServerFd) {
            println(WAITING_FOR_CLIENTS);
        }

        tReadFds = ptIthaca->tFdsClients;
        if (-1 == select(ptIthaca->nMaxFd + 1, &tReadFds, NULL, NULL, NULL)) {
            break;
        }

        for (nFd = 0; nFd <= ptIthaca->nMaxFd; nFd++) {
            if (0 == FD_ISSET(nFd, &tReadFds)) {
                continue;
            }

            if (nFd == ptIthaca->nServerFd) {
                acceptClient(ptIthaca);
            } else {
                handleClient(ptIthaca, nFd);
            }
        }
    }
}

/***********************************************
 *
 * @Name : closeIthaca
 * @Def : Closes every open descriptor and frees ithaca and the voyages.
 * @Arg : In/Out: ptIthaca = ithaca to close
 *        In/Out: patVoyages = pointer to the voyages array
 *        In/Out: ptNumVoyages = number of voyages
 * @Ret : None.
 *
 ************************************************/
void closeIthaca(Ithaca *ptIthaca, Voyage **patVoyages, size_t *ptNumVoyages) {
    int nFd = 0;

    for (nFd = 0; nFd <= ptIthaca->nMaxFd; nFd++) {
        if (0 != FD_ISSET(nFd, &ptIthaca->tFdsClients)) {
            close(nFd);
        }
    }

    freeIthaca(ptIthaca);
    freeVoyage(patVoyages, ptNumVoyages);
    println(FAREWELL);
}

/***********************************************
 *
 * @Name : main
 * @Def : Manages the general flow of the ithaca process.
 * @Arg : In: argc = number of arguments
 *        In: argv = arguments (ithaca file and voyages file)
 * @Ret : EXIT_SUCCESS if everything went well, EXIT_FAILURE otherwise.
 *
 ************************************************/
int main(int argc, char *argv[]) {
    int nFdIthaca = -1;
    int nFdVoyage = -1;
    Ithaca tIthaca = {0};
    Voyage *atVoyages = NULL;
    size_t tNumVoyages = 0;

    if (0 != checkArgcIthaca(argc, argv)) {
        return EXIT_FAILURE;
    }

    if (-1 == openFiles(argv, &nFdIthaca, &nFdVoyage)) {
        return EXIT_FAILURE;
    }

    if (-1 ==
        loadData(&nFdIthaca, &nFdVoyage, &tIthaca, &atVoyages, &tNumVoyages)) {
        return EXIT_FAILURE;
    }
    closeFiles(&nFdIthaca, &nFdVoyage);

    // DEBUGGING
    // printData(&tIthaca, atVoyages, tNumVoyages);

    signal(SIGINT, onSigint);
    signal(SIGPIPE, SIG_IGN);

    if (-1 == startServer(&tIthaca, &atVoyages, &tNumVoyages)) {
        return EXIT_FAILURE;
    }

    runServer(&tIthaca);

    closeIthaca(&tIthaca, &atVoyages, &tNumVoyages);

    return EXIT_SUCCESS;
}
