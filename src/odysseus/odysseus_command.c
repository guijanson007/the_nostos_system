#define _GNU_SOURCE

// header
#include "odysseus/odysseus_command.h"

// system
#include <string.h>

// own
#include "odysseus/odysseus_handler.h"
#include "utils/utils.h"

/*************************************************
 *
 * @Name : commandTypeFromName
 * @Def : Converts a command name to its corresponding CommandType enum value.
 * @Arg : In: name = command name (case-insensitive)
 * @Ret : Corresponding CommandType enum value, or COMMAND_UNKNOWN if not found.
 *
 **************************************************/
static CommandType commandTypeFromName(char *name) {
    // ignore case when comparing command names
    CommandType out = COMMAND_UNKNOWN;
    strToUpper(name);

    if (strcmp(name, "MAP") == 0) {
        return COMMAND_MAP;
    }
    if (strcmp(name, "STATUS") == 0) {
        return COMMAND_STATUS;
    }
    if (strcmp(name, "DELIVER") == 0) {
        return COMMAND_DELIVER;
    }
    if (strcmp(name, "CLAIM") == 0) {
        return COMMAND_CLAIM;
    }
    if (strcmp(name, "CONNECT") == 0) {
        return COMMAND_CONNECT;
    }
    if (strcmp(name, "LIST") == 0) {
        return COMMAND_LIST;
    }
    if (strcmp(name, "ACCEPT") == 0) {
        return COMMAND_ACCEPT;
    }
    if (strcmp(name, "SAIL") == 0) {
        return COMMAND_SAIL;
    }
    if (strcmp(name, "BUY") == 0) {
        return COMMAND_BUY;
    }
    if (strcmp(name, "SELL") == 0) {
        return COMMAND_SELL;
    }
    return out;
}

/*/*************************************************
 *
 * @Name : parseOdysseusCommand
 * @Def : Parses a command line into an OdysseusCommand structure.
 * @Arg : In: line = command line string
 *        Out: command = pointer to OdysseusCommand structure to fill
 * @Ret : Returns 1 if the command was parsed successfully, 0 if the line is
 * empty.
 *
 **************************************************/
int parseOdysseusCommand(char *line, OdysseusCommand *command) {
    char *token;

    command->type = COMMAND_UNKNOWN;
    command->argc = 0;

    token = strtok(line, " \t");
    if (token == NULL) {
        return 0;
    }

    command->type = commandTypeFromName(token);

    while ((token = strtok(NULL, " \t")) != NULL) {
        if (command->argc < ODYSSEUS_MAX_ARGS) {
            command->argv[command->argc] = token;
        }
        command->argc++;
    }

    return 1;
}

/*****************************************************************
 *
 * @Name : hasExactlyZeroArguments
 * @Def : Checks if the command has exactly zero arguments.
 * @Arg : In: command = pointer to OdysseusCommand structure
 * @Ret : Returns 1 if the command has exactly zero arguments, 0 otherwise.
 *
 ****************************************************************/
static int hasExactlyZeroArguments(const OdysseusCommand *command) {
    return command->argc == 0;
}

/*****************************************************************
 *
 * @Name : hasExactlyOneArgument
 * @Def : Checks if the command has exactly one argument.
 * @Arg : In: command = pointer to OdysseusCommand structure
 * @Ret : Returns 1 if the command has exactly one argument, 0 otherwise.
 *
 ****************************************************************/
static int hasExactlyOneArgument(const OdysseusCommand *command) {
    return command->argc == 1;
}

/*****************************************************************
 *
 * @Name : hasExactlyTwoArguments
 * @Def : Checks if the command has exactly two arguments.
 * @Arg : In: command = pointer to OdysseusCommand structure
 * @Ret : Returns 1 if the command has exactly two arguments, 0 otherwise.
 *
 ****************************************************************/
static int hasExactlyTwoArguments(const OdysseusCommand *command) {
    return command->argc == 2;
}
/*************************************************
 *
 * @Name : validateOdysseusCommand
 * @Def : Validates the arguments of an OdysseusCommand.
 * @Arg : In: command = pointer to OdysseusCommand structure to validate
 *        Out: errorMessage = pointer to a string describing the error, if any
 * @Ret : Returns COMMAND_VALID if the command is valid, or an appropriate
 * CommandValidationResult value if invalid.
 *
 **************************************************/
CommandValidationResult validateOdysseusCommand(const OdysseusCommand *command,
                                                const char **errorMessage) {
    *errorMessage = NULL;

    if (command->argc > 0) {
        // convert argv[0] to uppercase for case-insensitive comparison
        strToUpper(command->argv[0]);
    }

    if (command->type == COMMAND_UNKNOWN) {
        *errorMessage = "Unknown command";
        return COMMAND_UNKNOWN_COMMAND;
    }

    switch (command->type) {
        case COMMAND_MAP:
            if (!hasExactlyZeroArguments(command)) {
                *errorMessage = "Usage: MAP";
            }
            break;
        case COMMAND_STATUS:
            if (!hasExactlyZeroArguments(command)) {
                *errorMessage = "Usage: STATUS";
            }
            break;
        case COMMAND_DELIVER:
            if (!hasExactlyZeroArguments(command)) {
                *errorMessage = "Usage: DELIVER";
            }
            break;
        case COMMAND_CLAIM:
            if (!hasExactlyZeroArguments(command)) {
                *errorMessage = "Usage: CLAIM";
            }
            break;

        case COMMAND_CONNECT:
            if (!hasExactlyOneArgument(command) ||
                strcmp(command->argv[0], "ITHACA") != 0) {
                *errorMessage = "Usage: CONNECT ITHACA";
            }
            break;

        case COMMAND_LIST:
            if (!hasExactlyOneArgument(command) ||
                (strcmp(command->argv[0], "VOYAGES") != 0 &&
                 strcmp(command->argv[0], "MARKET") != 0)) {
                *errorMessage = "Usage: LIST VOYAGES | LIST MARKET";
            }
            break;

        case COMMAND_ACCEPT:
            if (!hasExactlyOneArgument(command)) {
                *errorMessage = "Usage: ACCEPT <voyage_id>";
            } else if (!isPositiveInteger(command->argv[0])) {
                *errorMessage = "Voyage ID must be a positive integer";
            }
            break;

        case COMMAND_SAIL:
            if (!hasExactlyOneArgument(command)) {
                *errorMessage = "Usage: SAIL <ISLAND>";
            }
            break;

        case COMMAND_BUY:
        case COMMAND_SELL:
            if (!hasExactlyTwoArguments(command)) {
                *errorMessage = command->type == COMMAND_BUY
                                    ? "Usage: BUY <PRODUCT> <AMOUNT>"
                                    : "Usage: SELL <PRODUCT> <AMOUNT>";
            } else if (!isPositiveInteger(command->argv[1])) {
                *errorMessage = "Amount must be a positive integer";
            }
            break;

        case COMMAND_UNKNOWN:
            break;
    }

    return *errorMessage == NULL ? COMMAND_VALID : COMMAND_INVALID_ARGUMENTS;
}

/*************************************************
 *
 * @Name : executeOdysseusCommand
 * @Def : Executes the given OdysseusCommand using the provided Odysseus
 * instance.
 * @Arg : In: odysseus = Odysseus instance to use for command execution
 *        In: command = pointer to OdysseusCommand structure to execute
 * @Ret : None.
 *
 **************************************************/
void executeOdysseusCommand(Odysseus odysseus, const OdysseusCommand *command) {
    switch (command->type) {
        case COMMAND_MAP:
            odysseusMap(odysseus);
            break;
        case COMMAND_STATUS:
            odysseusStatus(odysseus);
            break;
        case COMMAND_DELIVER:
            odysseusDeliver(odysseus);
            break;
        case COMMAND_CLAIM:
            odysseusClaim(odysseus);
            break;
        case COMMAND_CONNECT:
            odysseusConnect(odysseus, command->argv[0]);
            break;
        case COMMAND_LIST:
            if (strcmp(command->argv[0], "VOYAGES") == 0) {
                odysseusListVoyages(odysseus);
            } else {
                odysseusListMarket(odysseus);
            }
            break;
        case COMMAND_ACCEPT:
            odysseusAcceptVoyage(odysseus, command->argv[0]);
            break;
        case COMMAND_SAIL:
            odysseusSail(odysseus, command->argv[0]);
            break;
        case COMMAND_BUY:
            odysseusBuy(odysseus, command->argv[0], command->argv[1]);
            break;
        case COMMAND_SELL:
            odysseusSell(odysseus, command->argv[0], command->argv[1]);
            break;
        case COMMAND_UNKNOWN:
            break;
    }
}
