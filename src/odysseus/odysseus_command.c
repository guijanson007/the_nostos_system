/***********************************************
 *
 * @File : odysseus_command.c
 * @Purpose : parses, validates and executes the commands typed in the odysseus menu
 * @Author : Guilherme Rodrigues
 * @Date : 25/10/2026
 *
 ************************************************/

#define _GNU_SOURCE

// own
#include "odysseus/odysseus_command.h"

/***********************************************
 *
 * @Name : commandTypeFromName
 * @Def : Converts a command name to its corresponding CommandType enum value.
 * @Arg : In: psName = command name (case-insensitive)
 * @Ret : Corresponding CommandType enum value, or COMMAND_UNKNOWN if not found.
 *
 ************************************************/
static CommandType commandTypeFromName(char *psName) {
    CommandType eOut = COMMAND_UNKNOWN;
 
    // Ignore case when comparing command names
    strToUpper(psName);
 
    if (0 == strcmp(psName, CMD_MAP)) {
        return COMMAND_MAP;
    }
    if (0 == strcmp(psName, CMD_STATUS)) {
        return COMMAND_STATUS;
    }
    if (0 == strcmp(psName, CMD_DELIVER)) {
        return COMMAND_DELIVER;
    }
    if (0 == strcmp(psName, CMD_CLAIM)) {
        return COMMAND_CLAIM;
    }
    if (0 == strcmp(psName, CMD_CONNECT)) {
        return COMMAND_CONNECT;
    }
    if (0 == strcmp(psName, CMD_LIST)) {
        return COMMAND_LIST;
    }
    if (0 == strcmp(psName, CMD_ACCEPT)) {
        return COMMAND_ACCEPT;
    }
    if (0 == strcmp(psName, CMD_SAIL)) {
        return COMMAND_SAIL;
    }
    if (0 == strcmp(psName, CMD_BUY)) {
        return COMMAND_BUY;
    }
    if (0 == strcmp(psName, CMD_SELL)) {
        return COMMAND_SELL;
    }
    return eOut;
}

/*/*************************************************
 *
 * @Name : parseOdysseusCommand
 * @Def : Parses a command line into an OdysseusCommand structure.
 * @Arg : In: psLine = command line string
 *        Out: ptCommand = pointer to OdysseusCommand structure to fill
 * @Ret : Returns 1 if the command was parsed successfully, 0 if the line is
 * empty.
 *
 **************************************************/
int parseOdysseusCommand(char *psLine, OdysseusCommand *ptCommand) {
    char *psToken = NULL;
 
    ptCommand->type = COMMAND_UNKNOWN;
    ptCommand->argc = 0;
 
    psToken = strtok(psLine, COMMAND_DELIMITERS);
    if (NULL == psToken) {
        return 0;
    }
 
    ptCommand->type = commandTypeFromName(psToken);
 
    while (NULL != (psToken = strtok(NULL, COMMAND_DELIMITERS))) {
        if (ptCommand->argc < ODYSSEUS_MAX_ARGS) {
            ptCommand->argv[ptCommand->argc] = psToken;
        }
        ptCommand->argc++;
    }
 
    return 1;
}

/*****************************************************************
 *
 * @Name : hasExactlyZeroArguments
 * @Def : Checks if the command has exactly zero arguments.
 * @Arg : In: ptCommand = pointer to OdysseusCommand structure
 * @Ret : Returns 1 if the command has exactly zero arguments, 0 otherwise.
 *
 ****************************************************************/
static int hasExactlyZeroArguments(const OdysseusCommand *ptCommand) {
    return 0 == ptCommand->argc;
}
/*****************************************************************
 *
 * @Name : hasExactlyOneArgument
 * @Def : Checks if the command has exactly one argument.
 * @Arg : In: ptCommand = pointer to OdysseusCommand structure
 * @Ret : Returns 1 if the command has exactly one argument, 0 otherwise.
 *
 ****************************************************************/
static int hasExactlyOneArgument(const OdysseusCommand *ptCommand) {
    return 1 == ptCommand->argc;
}

/*****************************************************************
 *
 * @Name : hasExactlyTwoArguments
 * @Def : Checks if the command has exactly two arguments.
 * @Arg : In: ptCommand = pointer to OdysseusCommand structure
 * @Ret : Returns 1 if the command has exactly two arguments, 0 otherwise.
 *
 ****************************************************************/
static int hasExactlyTwoArguments(const OdysseusCommand *ptCommand) {
    return 2 == ptCommand->argc;
}
/*************************************************
 *
 * @Name : validateOdysseusCommand
 * @Def : Validates the arguments of an OdysseusCommand.
 * @Arg : In: ptCommand = pointer to OdysseusCommand structure to validate
 *        Out: ppsErrorMessage = pointer to a string describing the error, if any
 * @Ret : Returns COMMAND_VALID if the command is valid, or an appropriate
 * CommandValidationResult value if invalid.
 *
 **************************************************/
CommandValidationResult validateOdysseusCommand(const OdysseusCommand *ptCommand,
                                                const char **ppsErrorMessage) {
    *ppsErrorMessage = NULL;
 
    if (0 < ptCommand->argc) {
        // Convert argv[0] to uppercase for case-insensitive comparison
        strToUpper(ptCommand->argv[0]);
    }
 
    if (COMMAND_UNKNOWN == ptCommand->type) {
        *ppsErrorMessage = ERROR_UNKNOWN_COMMAND;
        return COMMAND_UNKNOWN_COMMAND;
    }
 
    switch (ptCommand->type) {
        case COMMAND_MAP:
            if (0 == hasExactlyZeroArguments(ptCommand)) {
                *ppsErrorMessage = USAGE_MAP;
            }
            break;
 
        case COMMAND_STATUS:
            if (0 == hasExactlyZeroArguments(ptCommand)) {
                *ppsErrorMessage = USAGE_STATUS;
            }
            break;
 
        case COMMAND_DELIVER:
            if (0 == hasExactlyZeroArguments(ptCommand)) {
                *ppsErrorMessage = USAGE_DELIVER;
            }
            break;
 
        case COMMAND_CLAIM:
            if (0 == hasExactlyZeroArguments(ptCommand)) {
                *ppsErrorMessage = USAGE_CLAIM;
            }
            break;
 
        case COMMAND_CONNECT:
            if (0 == hasExactlyOneArgument(ptCommand) ||
                0 != strcmp(ptCommand->argv[0], ARG_ITHACA)) {
                *ppsErrorMessage = USAGE_CONNECT;
            }
            break;
 
        case COMMAND_LIST:
            if (0 == hasExactlyOneArgument(ptCommand) ||
                (0 != strcmp(ptCommand->argv[0], ARG_VOYAGES) &&
                 0 != strcmp(ptCommand->argv[0], ARG_MARKET))) {
                *ppsErrorMessage = USAGE_LIST;
            }
            break;
 
        case COMMAND_ACCEPT:
            if (0 == hasExactlyOneArgument(ptCommand)) {
                *ppsErrorMessage = USAGE_ACCEPT;
            } else if (0 == isPositiveInteger(ptCommand->argv[0])) {
                *ppsErrorMessage = ERROR_VOYAGE_ID;
            }
            break;
 
        case COMMAND_SAIL:
            if (0 == hasExactlyOneArgument(ptCommand)) {
                *ppsErrorMessage = USAGE_SAIL;
            }
            break;
 
        case COMMAND_BUY:
            if (0 == hasExactlyTwoArguments(ptCommand)) {
                *ppsErrorMessage = USAGE_BUY;
            } else if (0 == isPositiveInteger(ptCommand->argv[1])) {
                *ppsErrorMessage = ERROR_AMOUNT;
            }
            break;
 
        case COMMAND_SELL:
            if (0 == hasExactlyTwoArguments(ptCommand)) {
                *ppsErrorMessage = USAGE_SELL;
            } else if (0 == isPositiveInteger(ptCommand->argv[1])) {
                *ppsErrorMessage = ERROR_AMOUNT;
            }
            break;
 
        case COMMAND_UNKNOWN:
            break;
 
        default:
            break;
    }
 
    return NULL == *ppsErrorMessage ? COMMAND_VALID : COMMAND_INVALID_ARGUMENTS;
}

/*************************************************
 *
 * @Name : executeOdysseusCommand
 * @Def : Executes the given OdysseusCommand using the provided Odysseus
 * instance.
 * @Arg : In: tOdysseus = Odysseus instance to use for command execution
 *        In: ptCommand = pointer to OdysseusCommand structure to execute
 * @Ret : ODY_OK on success, otherwise the status code of the failed handler.
 *
 **************************************************/
OdysseusStatus executeOdysseusCommand(Odysseus *tOdysseus,
                                      const OdysseusCommand *ptCommand) {
    switch (ptCommand->type) {
        case COMMAND_MAP:
            odysseusMap(tOdysseus);
            break;
        case COMMAND_STATUS:
            odysseusStatus(tOdysseus);
            break;
        case COMMAND_DELIVER:
            odysseusDeliver(tOdysseus);
            break;
        case COMMAND_CLAIM:
            odysseusClaim(tOdysseus);
            break;
        case COMMAND_CONNECT:
            return odysseusConnect(tOdysseus);
        case COMMAND_LIST:
            if (0 == strcmp(ptCommand->argv[0], ARG_VOYAGES)) {
                return odysseusListVoyages(tOdysseus);
            } else {
                return odysseusListMarket(tOdysseus);
            }
            break;
        case COMMAND_ACCEPT:
            return odysseusAcceptVoyage(tOdysseus, ptCommand->argv[0]);
        case COMMAND_SAIL:
            return odysseusSail(tOdysseus, ptCommand->argv[0]);
        case COMMAND_BUY:
            return odysseusBuy(tOdysseus, ptCommand->argv[0],
                               ptCommand->argv[1]);
        case COMMAND_SELL:
            odysseusSell(tOdysseus, ptCommand->argv[0], ptCommand->argv[1]);
            break;
        case COMMAND_UNKNOWN:
            break;
        default:
            break;
    }
    return ODY_OK;
}
