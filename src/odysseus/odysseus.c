#define _GNU_SOURCE

// System headers
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "odysseus/odysseus.h"

// event flag to indicate when to quit the program
int nEv_quit = 0;

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
 * @Def : Opens and parses the config file, then shows the menu until the user quits or SIGINT is received.
 * @Arg : In: argc = number of arguments
 *        In: argv = arguments (config file)
 * @Ret : Returns EXIT_SUCCESS if everything went well, EXIT_FAILURE otherwise.
 *
 ************************************************/
int main(int argc, char *argv[]) {
    int nFd = -1;
    char *psErrorMsg = NULL;
    Odysseus tOdysseus;
 
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
    signal(SIGINT, quitProgram);
    
    memset(&tOdysseus, 0, sizeof(Odysseus));
    tOdysseus = odysseusParseFile(nFd);
    close(nFd);
 
    // Something went wrong while parsing the file, we should exit
    if (NULL == tOdysseus.name || NULL == tOdysseus.path ||
        NULL == tOdysseus.ip || NULL == tOdysseus.port ||
        NULL == tOdysseus.initialInfo ||
        (0 < tOdysseus.cargo.numSupplies &&
         (NULL == tOdysseus.cargo.supplies || NULL == tOdysseus.cargo.supplies->name ||
          0 == tOdysseus.cargo.supplies->amount))) {
        freeOdysseus(&tOdysseus);
        return EXIT_FAILURE;
    }

    // odysseusPrintInfo(&tOdysseus);
    
    // Print the initial message
    odysseusStartMenu(tOdysseus);
 
    while (0 == nEv_quit && 1 == odysseusShowMenu(&tOdysseus)) {
    }
 
    freeOdysseus(&tOdysseus);
    return EXIT_SUCCESS;
}