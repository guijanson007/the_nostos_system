#define _GNU_SOURCE

// System headers
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


// event flag to indicate when to quit the program
int nEv_quit = 0;

/*******************************
 * @Name : quitProgram
 * @Def : Signal handler to set the quit event flag when SIGINT is received.
 * @Arg : In: signum = signal number (unused)
 * @Ret : None.
 *
 ********************************/
void quitProgram(int signum) {
    (void)signum;
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
    char sErrorMsg[MAX_ERROR_MSG_SIZE] = {'\0'};
    Odysseus tOdysseus;
 
    if (EXPECTED_ARGC != argc) {
        printErr(ERROR_ARGC);
        return EXIT_FAILURE;
    }
    int fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
        char *errorMsg;
        asprintf(&errorMsg, "Failed to open file: %s", argv[1]);
        printErr(errorMsg);
        free(errorMsg);
        return EXIT_FAILURE;
    }
    signal(SIGINT, quitProgram);

    Odysseus odysseus = odysseusParseFile(fd);
    close(fd);

    // something went wrong while parsing the file, we should exit
    if (!odysseus.name || !odysseus.path || !odysseus.ip || !odysseus.port ||
        !odysseus.initialInfo ||
        (odysseus.initialNumSupplies > 0 &&
         (!odysseus.supplies || !odysseus.supplies->name ||
          !odysseus.supplies->amount))) {
        freeOdysseus(&odysseus);
        return EXIT_FAILURE;
    }
 
    // odysseusPrintInfo(&tOdysseus);
 
    // Print the initial message
    odysseusStartMenu(tOdysseus);
 
    while (0 == gnEvQuit && 1 == odysseusShowMenu(tOdysseus)) {
    }

    freeOdysseus(&odysseus);
    return EXIT_SUCCESS;
}
