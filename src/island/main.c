#include "utils/utils.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printErr("Usage: ./island <config.dat> <stock.db>");
        return EXIT_FAILURE;
    }

    (void)argv;
    println("Island process started.");
    return EXIT_SUCCESS;
}
