// header
#include "odysseus/menu.h"

// System includes
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Own includes
#include "odysseus/handler.h"
#include "odysseus/odysseus.h"
#include "utils/utils.h"

static void informCommandUnknow() { printErr("Unknown command"); }

void odysseusStartMenu(Odysseus odysseus) {
    char buff[1028];

    snprintf(buff, sizeof(buff), "Odysseus %s is ready to sail.\n",
             odysseus.name);
    println(buff);

    while (1) {
        int error = 0;
        ssize_t bytesRead = read(STDIN_FILENO, buff, sizeof(buff) - 1);
        if (bytesRead <= 0) {
            // EOF or read error
            break;
        }

        buff[bytesRead] = '\0';

        // remove possible newline characters
        buff[strcspn(buff, "\n")] = '\0';

        // grab first part of the command
        char *cmd = strtok(buff, " \t");
        if (cmd == NULL) {
            continue; // Empty line
        }

        // grab the rest of the command
        char *arg1 = strtok(NULL, " \t");
        char *arg2 = strtok(NULL, " \t");

        // no argument commands
        if (strcmp(cmd, "MAP") == 0) {
            odysseusMap(odysseus);
        } else if (strcmp(cmd, "STATUS") == 0) {
            odysseusStatus(odysseus);
        } else if (strcmp(cmd, "DELIVER") == 0) {
            odysseusDeliver(odysseus);
        } else if (strcmp(cmd, "CLAIM") == 0) {
            odysseusClaim(odysseus);
        }
        // 1 and 2 argument commands
        else if (strcmp(cmd, "CONNECT") == 0) {
            if (arg1 != NULL && strcmp(arg1, "ITHACA") == 0) {
                odysseusConnect(odysseus, arg1);
            } else {
                error = 1;
                printErr("Usage: CONNECT ITHACA\n");
            }
        } else if (strcmp(cmd, "LIST") == 0) {
            if (arg1 != NULL && strcmp(arg1, "VOYAGES") == 0) {
                odysseusListVoyages(odysseus);
            } else if (arg1 != NULL && strcmp(arg1, "MARKET") == 0) {
                odysseusListMarket(odysseus);
            } else {
                error = 1;
                printErr("Usage: LIST VOYAGES | LIST MARKET\n");
            }
        } else if (strcmp(cmd, "ACCEPT") == 0) {
            if (arg1 != NULL) {
                if (!isPositiveInteger(arg1)) {
                    error = 1;
                    printErr("Voyage ID must be a positive integer\n");
                    continue;
                }
                odysseusAcceptVoyage(odysseus, arg1);
            } else {
                error = 1;
                printErr("Usage: ACCEPT <voyage_id>\n");
            }
        } else if (strcmp(cmd, "SAIL") == 0) {
            if (arg1 != NULL) {
                odysseusSail(odysseus, arg1);
            } else {
                error = 1;
                printErr("Usage: SAIL <ISLAND>\n");
            }
        }
        // 2-argument commands
        else if (strcmp(cmd, "BUY") == 0) {
            if (arg1 != NULL && arg2 != NULL) {
                if (!isPositiveInteger(arg2)) {
                    error = 1;
                    printErr("Amount must be a positive integer\n");
                    continue;
                }
                odysseusBuy(odysseus, arg1, arg2);
            } else {
                error = 1;
                printErr("Usage: BUY <PRODUCT> <AMOUNT>\n");
            }
        } else if (strcmp(cmd, "SELL") == 0) {
            if (arg1 != NULL && arg2 != NULL) {
                if (!isPositiveInteger(arg2)) {
                    error = 1;
                    printErr("Amount must be a positive integer\n");
                    continue;
                }
                odysseusSell(odysseus, arg1, arg2);
            } else {
                error = 1;
                printErr("Usage: SELL <PRODUCT> <AMOUNT>\n");
            }
        } else {
            error = 1;
            informCommandUnknow();
        }
        if (!error) {
            println("Command OK\n");
        }
    }
}
