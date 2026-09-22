#include "utils/utils.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printErr("Usage: ./ithaca <config.dat> <voyages.dat>");
        return EXIT_FAILURE;
    }

    (void)argv;
    println("Ithaca process started.");
    return EXIT_SUCCESS;
}
