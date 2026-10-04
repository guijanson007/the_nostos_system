// System headers
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>


/***********************************************
 *
 * @Name : quitProgram
 * @Def : SIGINT handler. Sets the quit flag so the main loop ends.
 * @Arg : In: nSignum = number of the signal received (unused)
 * @Ret : None.
 *
 ************************************************/
void quitProgram(int nSignum) {
    (void)nSignum;
    gnEvQuit = 1;
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
    char sErrorMsg[MAX_ERROR_MSG_SIZE] = {'\0'};
    Odysseus tOdysseus;
 
    if (EXPECTED_ARGC != argc) {
        printErr(ERROR_ARGC);
        return EXIT_FAILURE;
    }
    nFd = open(argv[1], O_RDONLY);
 
    if (-1 == nFd) {
        snprintf(sErrorMsg, sizeof(sErrorMsg), ERROR_OPEN_FILE, argv[1]);
        printErr(sErrorMsg);
        return EXIT_FAILURE;
    }
    signal(SIGINT, quitProgram);
 
    tOdysseus = odysseusParseFile(nFd);
 
    // Something went wrong while parsing the file, we should exit
    if (NULL == tOdysseus.name || NULL == tOdysseus.path ||
        NULL == tOdysseus.ip || NULL == tOdysseus.port ||
        NULL == tOdysseus.initialInfo ||
        (0 < tOdysseus.initialNumSupplies &&
         (NULL == tOdysseus.supplies || NULL == tOdysseus.supplies->name ||
          0 == tOdysseus.supplies->amount))) {
        freeOdysseus(&tOdysseus);
        close(nFd);
        return EXIT_FAILURE;
    }
 
    // odysseusPrintInfo(&tOdysseus);
 
    // Print the initial message
    odysseusStartMenu(tOdysseus);
 
    while (0 == gnEvQuit && 1 == odysseusShowMenu(tOdysseus)) {
    }
 
    freeOdysseus(&tOdysseus);
    close(nFd);
    return EXIT_SUCCESS;
}
