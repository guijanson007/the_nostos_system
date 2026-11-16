#define _GNU_SOURCE

// System headers
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// own headers
#include "odysseus/odysseus.h"
#include "odysseus/odysseus_conversation.h"
#include "odysseus/odysseus_menu.h"

// event flag to indicate when to quit the program
volatile sig_atomic_t gtEvQuit = 0;

/***********************************************
 *
 * @Name : quitProgram
 * @Def : Signal handler to set the quit event flag when SIGINT is
 *        received.
 * @Arg : In: nSignum = signal number (unused)
 * @Ret : None.
 *
 ************************************************/
void quitProgram(int nSignum) {
    (void)nSignum;
    gtEvQuit = 1;
}

/***********************************************
 *
 * @Name : odysseusInitState
 * @Def : Sets the initial state of odysseus: at Ithaca, without sockets
 *        and with a map that only contains the island of the initial info
 *        (Aeaea).
 * @Arg : In/Out: ptOdysseus = odysseus to initialise
 * @Ret : 0 if everything went well, -1 otherwise.
 *
 ************************************************/
static int odysseusInitState(Odysseus *ptOdysseus) {
    char *psCopy = NULL;
    char *psName = NULL;
    char *psIp = NULL;
    char *psPort = NULL;
    KnownIsland *ptAeaea = NULL;

    // We start at Ithaca
    ptOdysseus->nWhereAmI = AT_ITHACA;
    ptOdysseus->nIthacaSocket = NO_SOCKET;
    ptOdysseus->nIslandSocket = NO_SOCKET;

    // Map with one island: Aeaea
    ptOdysseus->tMap.atConnections = calloc(1, sizeof(KnownIsland));
    if (NULL == ptOdysseus->tMap.atConnections) {
        return -1;
    }
    psCopy = strdup(ptOdysseus->psInitialInfo);
    if (NULL == psCopy) {
        return -1;
    }
    psName = strtok(psCopy, " ");
    psIp = strtok(NULL, " ");
    psPort = strtok(NULL, " ");
    if (NULL == psName || NULL == psIp || NULL == psPort) {
        free(psCopy);
        return -1;
    }

    ptAeaea = &ptOdysseus->tMap.atConnections[0];
    ptAeaea->psName = strdup(psName);
    ptAeaea->psIp = strdup(psIp);
    ptAeaea->nPort = atoi(psPort);
    ptAeaea->nNumNeighbors = 0;
    ptAeaea->anNeighbors = NULL;
    ptOdysseus->tMap.nNumConnections = 1;
    free(psCopy);

    if (NULL == ptAeaea->psName || NULL == ptAeaea->psIp) {
        return -1;
    }
    return 0;
}

/***********************************************
 *
 * @Name : openConfigFile
 * @Def : Opens the config file in read only mode. If it fails, it prints
 *        the error with the name of the file.
 * @Arg : In: psPath = path of the config file
 * @Ret : File descriptor of the config file, or -1 if it couldn't be
 *        opened.
 *
 ************************************************/
int openConfigFile(char *psPath) {
    int nFd = -1;
    char *psErrorMsg = NULL;

    nFd = open(psPath, O_RDONLY);
    if (-1 == nFd) {
        if (-1 != asprintf(&psErrorMsg, ERROR_OPEN_FILE, psPath)) {
            printErr(psErrorMsg);
            free(psErrorMsg);
        }
    }
    return nFd;
}

/***********************************************
 *
 * @Name : setupSignals
 * @Def : Installs the SIGINT handler and ignores SIGPIPE.
 * @Arg : None.
 * @Ret : None.
 *
 ************************************************/
void setupSignals(void) {
    // signal() restarts read(), so stdin is waited on with select() instead
    // (see waitReadable), which always returns when a signal arrives
    signal(SIGINT, quitProgram);
    signal(SIGPIPE, SIG_IGN);
}

/***********************************************
 *
 * @Name : isOdysseusValid
 * @Def : Checks that every field read from the config file was parsed.
 * @Arg : In: ptOdysseus = odysseus to check
 * @Ret : 1 if the odysseus is valid, 0 if something went wrong while
 *        parsing the file.
 *
 ************************************************/
int isOdysseusValid(Odysseus *ptOdysseus) {
    if (NULL == ptOdysseus->psName || NULL == ptOdysseus->psPath ||
        NULL == ptOdysseus->psIp || NULL == ptOdysseus->psPort ||
        NULL == ptOdysseus->psInitialInfo ||
        (0 < ptOdysseus->tCargo.nNumSupplies &&
         (NULL == ptOdysseus->tCargo.atSupplies ||
          NULL == ptOdysseus->tCargo.atSupplies->psName ||
          0 == ptOdysseus->tCargo.atSupplies->nAmount))) {
        return 0;
    }
    return 1;
}

/***********************************************
 *
 * @Name : closeConnections
 * @Def : Disconnects from Ithaca and departs from the island if the
 *        sockets are still open.
 * @Arg : In/Out: ptOdysseus = odysseus whose connections are closed
 * @Ret : None.
 *
 ************************************************/
void closeConnections(Odysseus *ptOdysseus) {
    if (NO_SOCKET != ptOdysseus->nIthacaSocket) {
        disconnectFromIthaca(ptOdysseus);
    }
    if (NO_SOCKET != ptOdysseus->nIslandSocket) {
        departFromIsland(ptOdysseus);
    }
}

/***********************************************
 *
 * @Name : main
 * @Def : Opens and parses the config file, then shows the menu until the
 *        user quits or SIGINT is received.
 * @Arg : In: argc = number of arguments
 *        In: argv = arguments (config file)
 * @Ret : Returns EXIT_SUCCESS if everything went well, EXIT_FAILURE
 *        otherwise.
 *
 ************************************************/
int main(int argc, char *argv[]) {
    int nFd = -1;
    Odysseus tOdysseus = {0};

    if (EXPECTED_ARGC != argc) {
        printErr(ERROR_ARGC);
        return EXIT_FAILURE;
    }

    nFd = openConfigFile(argv[1]);
    if (-1 == nFd) {
        return EXIT_FAILURE;
    }
    setupSignals();

    tOdysseus = odysseusParseFile(nFd);
    close(nFd);

    // Something went wrong while parsing the file, we should exit
    if (0 == isOdysseusValid(&tOdysseus)) {
        freeOdysseus(&tOdysseus);
        return EXIT_FAILURE;
    }

    if (0 != odysseusInitState(&tOdysseus)) {
        printErr(ERROR_INIT_STATE);
        freeOdysseus(&tOdysseus);
        return EXIT_FAILURE;
    }

    // odysseusPrintInfo(&tOdysseus);

    // Print the initial message
    odysseusStartMenu(tOdysseus);

    while (0 == gtEvQuit) {
        if (1 != odysseusShowMenu(&tOdysseus, &gtEvQuit)) {
            break;
        }
    }

    closeConnections(&tOdysseus);
    freeOdysseus(&tOdysseus);
    return EXIT_SUCCESS;
}
