// System headers
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

// Own headers
#include "odysseus/menu.h"
#include "odysseus/odysseus.h"
#include "utils/utils.h"

// event flag to indicate when to quit the program
int gEv_quit = 0;

void quitProgram(int signum) {
    (void)signum; // Unused parameter
    gEv_quit = 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printErr("Usage: odysseus <config_file>.dat");
        return EXIT_FAILURE;
    }
    int fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
        char errorMsg[256];
        snprintf(errorMsg, sizeof(errorMsg), "Failed to open file: %s",
                 argv[1]);
        printErr(errorMsg);
        return EXIT_FAILURE;
    }
    signal(SIGINT, quitProgram);

    Odysseus odysseus = odysseusParseFile(fd);

    // something went wrong while parsing the file, we should exit
    if (!odysseus.name || !odysseus.path || !odysseus.ip || !odysseus.port ||
        !odysseus.initialInfo ||
        (odysseus.initialNumSupplies > 0 &&
         (!odysseus.supplies || !odysseus.supplies->name ||
          !odysseus.supplies->amount))) {
        freeOdysseus(&odysseus);
        close(fd);
        return EXIT_FAILURE;
    }

    // odysseusPrintInfo(&odysseus);

    // Print the initial message
    odysseusStartMenu(odysseus);

    while (!gEv_quit && odysseusShowMenu(odysseus) == 1) {
    }

    freeOdysseus(&odysseus);
    close(fd);
    return EXIT_SUCCESS;
}
