#define _GNU_SOURCE

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/island/island.h"
#include "../include/utils/utils.h"
#include "../lib/sphragis/sphragis.h"

#define CONFIG_FILE "aeaea.dat"
#define STOCK_FILE "stocks/Aeaea.db"
#define ERROR_ARGC "Usage: ./island <config.dat> <stocks/stock.db>"
#define ERROR_FILE_CONFIG "ERROR: config file couldn't be open"
#define ERROR_FILE_STOCK "ERROR: stock file couldn't be open"
#define ERROR_LOAD_CONFIG "ERROR: data from the config file could not been open"

static volatile int run = 1;

void onSigint () {
    run = 0;
}

static void closeFiles(int *fd_config, int *fd_stock) {
    close(*fd_config);
    close(*fd_stock);
}

static void errorLoadingHandeler(int *fd_config, int *fd_stock) {
    printErr(ERROR_LOAD_CONFIG);
    closeFiles(fd_config, fd_stock);
}

int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], CONFIG_FILE) != 0 ||
        strcmp(argv[2], STOCK_FILE) != 0) {
        printErr(ERROR_ARGC);
        return EXIT_FAILURE;
    }

    signal(SIGINT, onSigint);

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
    Stock *stock;
    size_t numStock = 0;
    SPHRAGIS_Island sphragis;

    if (loadIsland(fd_config, &island, &sphragis) == -1) {
        errorLoadingHandeler(&fd_config, &fd_stock);
        return EXIT_FAILURE;
    }

    if (loadStock(fd_stock, &stock, &numStock) == -1) {
        errorLoadingHandeler(&fd_config, &fd_stock);
        return EXIT_FAILURE;
    }

    // DEBUGIGN

    char *buffer;
    asprintf(&buffer, "%s - %s - %s %d - %d", island.name, island.file,
             island.ip, island.port, island.capacity);
    println(buffer);
    free(buffer);
    for (size_t i = 0; i < island.numRouts; i++) {
        asprintf(&buffer, "\t%s - %s -%d", island.routs[i].name, island.routs[i].ip,
                 island.routs[i].port);
        println(buffer);
        free(buffer);
    }
    for (size_t i = 0; i < numStock; i++) {
        asprintf(&buffer, "%s - %d -%d", stock[i].name, stock[i].quantity,
                 stock[i].price);
        println(buffer);
        free(buffer);
    }

    ///////////

    while(run) {
        pause();
    }

    closeFiles(&fd_config, &fd_stock);
    freeIsland(&island, &sphragis);
    freeStock(&stock);
    println("\nIsland process finished.");
    return EXIT_SUCCESS;
}
