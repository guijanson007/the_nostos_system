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
#include "ithaca/ithaca_conversetion.h"

int nEv_quit = 0;

/***********************************************
 *
 * @Name : onSigint
 * @Def : SIGINT handler. Only sets the quit flag; printing and freeing are done
 * in main, outside the handler.
 * @Arg : In: nSignum = signal number (unused)
 * @Ret : None.
 *
 ************************************************/
static void onSigint(int nSignum) {
    (void)nSignum;
    nEv_quit = 1;
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
    if (NULL == ptIthaca) {
        return;
    }
    free(ptIthaca->psName);
    free(ptIthaca->psMission);
    free(ptIthaca->psIp);

    for (size_t i = 0; i < ptIthaca->numClients; i++) {
        free(ptIthaca->clients[i].psName);
    }
    free(ptIthaca->clients);
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

    nFdIthaca = open(argv[1], O_RDONLY);
    if (-1 == nFdIthaca) {
        printErr(ERROR_FILE_ITHACA);
        return EXIT_FAILURE;
    }

    nFdVoyage = open(argv[2], O_RDONLY);
    if (-1 == nFdVoyage) {
        printErr(ERROR_FILE_VOYAGE);
        close(nFdIthaca);
        return EXIT_FAILURE;
    }

    if (-1 ==
        loadData(&nFdIthaca, &nFdVoyage, &tIthaca, &atVoyages, &tNumVoyages)) {
        return EXIT_FAILURE;
    }
    closeFiles(&nFdIthaca, &nFdVoyage);

    // DEBUGGING
    printData(&tIthaca, atVoyages, tNumVoyages);

    signal(SIGINT, onSigint);

    tIthaca.nServerFd = openListenConn(tIthaca.psIp, tIthaca.nPort);
    if (tIthaca.nServerFd < 0) {
        write(STDOUT_FILENO, ERROR_SOCKET, strlen(ERROR_SOCKET));
        freeIthaca(&tIthaca);
        freeVoyage(&atVoyages, &tNumVoyages);
        return EXIT_FAILURE;
    }

    FD_ZERO(&tIthaca.fds_clients);
    FD_SET(tIthaca.nServerFd, &tIthaca.fds_clients);

    tIthaca.max_fd = tIthaca.nServerFd;

    while (!nEv_quit) {
        fd_set read_fds = tIthaca.fds_clients;
        if (select(tIthaca.max_fd + 1, &read_fds, NULL, NULL, NULL) == -1)
            break;

        for (int fd = 0; fd <= tIthaca.max_fd; fd++) {
            if (!FD_ISSET(fd, &read_fds))
                continue;

            if (fd == tIthaca.nServerFd) {
                int fd_client = accept(tIthaca.nServerFd, NULL, NULL);
                if (fd_client < 0) {
                    printErr(ERROR_ACCEPT);
                    continue;
                }
                FD_SET(fd_client, &tIthaca.fds_clients);
                if (fd_client > tIthaca.max_fd)
                    tIthaca.max_fd = fd_client;
            } else {
                Message msg;
                if (receiveMessage(&msg, fd) != 0) {
                    printErr(ERROR_RECEIVE_MESSAGE);
                    if (0 != removeClient(&tIthaca, fd)) {
                        // Never registered: drop the socket anyway
                        close(fd);
                        FD_CLR(fd, &tIthaca.fds_clients);
                    }
                    continue;
                }
                // receiveMessage(&msg, fd);
                //  Option to handel the options
                choseOption(&tIthaca, msg, fd);
            }
        }
    }

    for (int fd = 0; fd <= tIthaca.max_fd; fd++) {
        if (FD_ISSET(fd, &tIthaca.fds_clients))
            close(fd);
    }

    freeIthaca(&tIthaca);
    freeVoyage(&atVoyages, &tNumVoyages);
    println(FAREWELL);

    return EXIT_SUCCESS;
}
