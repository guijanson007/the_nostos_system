// header
#include "odysseus/menu.h"

// system
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// own
#include "odysseus/command.h"
#include "utils/utils.h"

#define ODYSSEUS_INPUT_SIZE 1028

void odysseusStartMenu(Odysseus odysseus) {
    char buffer[ODYSSEUS_INPUT_SIZE];

    snprintf(buffer, sizeof(buffer), "Odysseus %s is ready to sail.\n",
             odysseus.name);
    println(buffer);
}

int odysseusShowMenu(Odysseus odysseus) {
    OdysseusCommand command;
    const char *errorMessage;

    char input[ODYSSEUS_INPUT_SIZE];

    if (!readLine(STDIN_FILENO, input, ODYSSEUS_INPUT_SIZE)) {
        if (errno == EINTR) {
            return 0; // Interrupted by signal
        } else {
            printErr("Failed to read command");
            return 0; // Error reading input
        }
    }

    if (!parseOdysseusCommand(input, &command)) {
        return 1;
    }

    if (COMMAND_VALID != validateOdysseusCommand(&command, &errorMessage)) {
        printErr(errorMessage);
        printErr(""); // Print an empty line for better readability
        return 1; // Return 1 to continue the menu even after an invalid command
    }
    executeOdysseusCommand(odysseus, &command);
    println("Command OK");
    println(""); // Print an empty line for better readability
    return 1;
}
