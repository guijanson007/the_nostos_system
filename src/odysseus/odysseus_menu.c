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

void setupInputField() {
    // make text bold
    write(STDOUT_FILENO, "\033[1m", strlen("\033[1m"));
    write(STDOUT_FILENO, "$ ", strlen("$ "));
}

void resetInputField() {
    // reset text formatting
    write(STDOUT_FILENO, "\033[0m", strlen("\033[0m"));
}

int odysseusShowMenu(Odysseus odysseus) {
    OdysseusCommand command;
    const char *errorMessage;

    char *input = NULL;

    setupInputField();

    if (-1 == readLine(STDIN_FILENO, &input)) {
        if (errno == EINTR) {
            return 0; // Interrupted by signal
        } else {
            printErr("Failed to read command");
            return 0; // Error reading input
        }
    }

    resetInputField();

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
    executeOdysseusCommand(odysseus, &command);
    free(input);
    println("Command OK");
    println(""); // Print an empty line for better readability
    return 1;
}
