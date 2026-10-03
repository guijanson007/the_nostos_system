#define _GNU_SOURCE

// header
#include "odysseus/odysseus_menu.h"

// system
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// own
#include "odysseus/odysseus_command.h"
#include "utils/utils.h"

void odysseusStartMenu(Odysseus odysseus) {
    char *buffer;
    asprintf(&buffer, "Odysseus %s is ready to sail.\n", odysseus.name);
    println(buffer);
    free(buffer);
}

int odysseusShowMenu(Odysseus odysseus) {
    OdysseusCommand command;
    const char *errorMessage;

    char *input = NULL;

    if (-1 == readLine(STDIN_FILENO, &input)) {
        if (errno == EINTR) {
            return 0; // Interrupted by signal
        } else {
            printErr("Failed to read command");
            return 0; // Error reading input
        }
    }

    if (NULL == input) {
        return 0;
    }

    if (!parseOdysseusCommand(input, &command)) {
        free(input);
        return 1;
    }

    if (COMMAND_VALID != validateOdysseusCommand(&command, &errorMessage)) {
        printErr(errorMessage);
        printErr(""); // Print an empty line for better readability
        free(input);
        return 1; // Return 1 to continue the menu even after an invalid command
    }
    free(input); // Free the input buffer after parsing
    executeOdysseusCommand(odysseus, &command);
    println("Command OK");
    println(""); // Print an empty line for better readability
    return 1;
}
