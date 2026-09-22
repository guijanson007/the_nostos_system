// System headers
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

// Own headers
#include "odysseus/menu.h"
#include "odysseus/odysseus.h"
#include "utils/utils.h"

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

    Odysseus odysseus = odysseusParseFile(fd);
    // odysseusPrintInfo(&odysseus);

    odysseusStartMenu(odysseus);

    freeOdysseus(&odysseus);
    close(fd);
    return EXIT_SUCCESS;
}
