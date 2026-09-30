#define _GNU_SOURCE

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/ithaca/ithaca_parser.h"
#include "../include/utils/utils.h"

#define ITHACA_FILE "ithaca.dat"
#define VOYAGES_FILE "voyages.dat"
#define ERROR_ARGC "Usage: ./ithaca <ithaca.dat> <voyages.dat>"
#define ERROR_FILE_ITHACA "ERROR ITHACA: The file couldn't been open"
#define ERROR_FILE_VOYAGE "ERROR VOYAGE: The file couldn't been open"
#define ERROR_LOAD_ITHACA                                                      \
    "ERROR ITHACA: The data couldn't been loaded from the file"
#define ERROR_LOAD_VOYAGE                                                      \
    "ERROR VOYAGE: The data couldn't been loaded from the file"
#define FARWEL "\nFinishing the Ithaca prosses"

static volatile int run = 1;

void onSigint(int sig) {
    (void)sig;
    println(FARWEL);
    run = 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], ITHACA_FILE) != 0 || strcmp(argv[2], VOYAGES_FILE) != 0) {
        printErr(ERROR_ARGC);
        return 1;
    }

    signal(SIGINT, onSigint);

    int fd_ithaca = open(argv[1], O_RDONLY);
    if (fd_ithaca == -1) {
        printErr(ERROR_FILE_ITHACA);
        return 1;
    }

    int fd_voyage = open(argv[2], O_RDONLY);
    if (fd_voyage == -1) {
        printErr(ERROR_FILE_VOYAGE);
        return 1;
    }

    Ithaca ithaca;
    Voyage *voyage;
    size_t numVoyage = 0;

    if (loadIthacaData(fd_ithaca, &ithaca) == -1) {
        printErr(ERROR_LOAD_ITHACA);
        return 1;
    }

    if (loadVoyageData(fd_voyage, &voyage, &numVoyage) == -1) {
        printErr(ERROR_LOAD_VOYAGE);
        return 1;
    }

    close(fd_ithaca);

    // DEBUGING //
    char *dbg = NULL;
    if (asprintf(&dbg, "%s - %s - %s - %d", ithaca.name, ithaca.mision,
                 ithaca.ipAddress, ithaca.port) >= 0) {
        println(dbg);
        free(dbg);
    }

    for (size_t i = 0; i < numVoyage; i++) {
        char *dbg = NULL;
        if (asprintf(&dbg, "%d: %s - %s - %s - %d", voyage[i].id,
                     voyage[i].object, voyage[i].file, voyage[i].destination,
                     voyage[i].reward) >= 0) {
            println(dbg);
            free(dbg);
        }
    }
    /////////////

    while (run) {
        pause();
    }

    freeIthaca(&ithaca);
    freeVoyage(voyage, &numVoyage);
    return EXIT_SUCCESS;
}
