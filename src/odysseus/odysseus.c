#define _GNU_SOURCE

// System headers
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "odysseus/odysseus.h"

// event flag to indicate when to quit the program
volatile sig_atomic_t nEv_quit = 0;

/*******************************
 * @Name : quitProgram
 * @Def : Signal handler to set the quit event flag when SIGINT is received.
 * @Arg : In: nSignum = signal number (unused)
 * @Ret : None.
 *
 ********************************/
void quitProgram(int nSignum) {
    (void)nSignum;
    nEv_quit = 1;
}

/***********************************************
 *
 * @Name : main
 * @Def : Opens and parses the config file, then shows the menu until the user
 * quits or SIGINT is received.
 * @Arg : In: argc = number of arguments
 *        In: argv = arguments (config file)
 * @Ret : Returns EXIT_SUCCESS if everything went well, EXIT_FAILURE otherwise.
 *
 ************************************************/
int main(int argc, char *argv[]) {
    int nFd = -1;
    char *psErrorMsg = NULL;
    Odysseus tOdysseus;
    struct sigaction tAction;

    if (EXPECTED_ARGC != argc) {
        printErr(ERROR_ARGC);
        return EXIT_FAILURE;
    }
    nFd = open(argv[1], O_RDONLY);

    if (-1 == nFd) {
        asprintf(&psErrorMsg, ERROR_OPEN_FILE, argv[1]);
        printErr(psErrorMsg);
        free(psErrorMsg);
        return EXIT_FAILURE;
    }
    // No SA_RESTART: a blocking read() must return EINTR when SIGINT arrives
    sigemptyset(&tAction.sa_mask);
    tAction.sa_handler = quitProgram;
    tAction.sa_flags = 0;
    sigaction(SIGINT, &tAction, NULL);

    memset(&tOdysseus, 0, sizeof(Odysseus));
    tOdysseus = odysseusParseFile(nFd);
    close(nFd);

    // Something went wrong while parsing the file, we should exit
    if (NULL == tOdysseus.name || NULL == tOdysseus.path ||
        NULL == tOdysseus.ip || NULL == tOdysseus.port ||
        NULL == tOdysseus.initialInfo ||
        (0 < tOdysseus.initialNumSupplies &&
         (NULL == tOdysseus.supplies || NULL == tOdysseus.supplies->name ||
          0 == tOdysseus.supplies->amount))) {
        freeOdysseus(&tOdysseus);
        return EXIT_FAILURE;
    }

    // odysseusPrintInfo(&tOdysseus);

    // Print the initial message
    odysseusStartMenu(tOdysseus);

    while (0 == nEv_quit) {
        if (1 != odysseusShowMenu(&tOdysseus))
            break;
    }

    freeOdysseus(&tOdysseus);
    return EXIT_SUCCESS;
}
