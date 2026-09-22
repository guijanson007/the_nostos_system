#define _GNU_SOURCE

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#incldue "../include/island/island.h"

#define ERROR_ARGC "Usage: ./island <config.dat> <stock.db>"
#define ERROR_FILE_CONFIG "ERROR: config file couldn't be open"
#define ERROR_FILE_STOCK "ERROR: stock file couldn't be open"
#define ERROR_LOAD_CONFIG "ERROR: data from the config file could not been open"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printErr(ERROR_ARGC);
        return EXIT_FAILURE;
    }

    int fd_config = open(argv[1], O_RDONLY);
    if (fd_config == -1) {
        printErr(ERROR_FILE_CONFIG);
        return EXIT_FAILURE;
    }

    int fd_stock = open(argv[2], O_RDONLY);
    if (fd_stock == -1) {
        printErr(ERROR_FILE_STOCK);
        close(fd_config);
        return EXIT_FAILURE;
    }

    Island island;

    if(loadIsland(fd_config, &island) == -1) {
        printErr(ERROR_LOAD_CONFIG);
        close(fd_config);
        close(fd_stock);
        return EXIT_FAILURE;
    }


    close(fd_config);
    close(fd_stock);
    freeIsland(&island);
    println("Island process finished.");
    return EXIT_SUCCESS;
}
