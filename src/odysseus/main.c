// System headers
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

// Own headers
#include "odysseus/menu.h"
#include "odysseus/odysseus.h"
#include "utils/utils.h"

Odysseus gOdysseus; // Global variable to hold the Odysseus instance

void quitProgram(int signum) {
    (void)signum; // Unused parameter
                  // println("\nReceived SIGINT. Exiting...");

    // first check if the odysseus struct has been initialized before trying to
    // free it
    if (gOdysseus.name)
        freeOdysseus(&gOdysseus);

    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printErr("Usage: odysseus <config_file>.dat");
        return EXIT_FAILURE;
    }
    int fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
        char errorMsg[256];
        sprintf(errorMsg, "Failed to open file: %s", argv[1]);
        printErr(errorMsg);
        return EXIT_FAILURE;
    }
    signal(SIGINT, quitProgram);

    gOdysseus = odysseusParseFile(fd);
    odysseusPrintInfo(&gOdysseus);

    odysseusStartMenu(gOdysseus);

    freeOdysseus(&gOdysseus);
    close(fd);
    return EXIT_SUCCESS;
}
