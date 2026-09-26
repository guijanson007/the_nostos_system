#ifndef ODYSSEUS_COMMAND_H
#define ODYSSEUS_COMMAND_H

/***********************************************
 *
 * @File : menu.h
 * @Purpose : responsible for parsing and executing commands for the odysseus
 * process
 * @Author : Guilherme Rodrigues
 * @Date : 25/09/2026
 *
 ************************************************/

#include "odysseus/odysseus.h"

#define ODYSSEUS_MAX_ARGS 3

typedef enum {
    COMMAND_UNKNOWN,
    COMMAND_MAP,
    COMMAND_STATUS,
    COMMAND_DELIVER,
    COMMAND_CLAIM,
    COMMAND_CONNECT,
    COMMAND_LIST,
    COMMAND_ACCEPT,
    COMMAND_SAIL,
    COMMAND_BUY,
    COMMAND_SELL
} CommandType;

typedef struct {
    CommandType type;
    int argc;
    char *argv[ODYSSEUS_MAX_ARGS];
} OdysseusCommand;

typedef enum {
    COMMAND_VALID,
    COMMAND_UNKNOWN_COMMAND,
    COMMAND_INVALID_ARGUMENTS
} CommandValidationResult;

int parseOdysseusCommand(char *line, OdysseusCommand *command);
CommandValidationResult validateOdysseusCommand(const OdysseusCommand *command,
                                                const char **errorMessage);
void executeOdysseusCommand(Odysseus odysseus, const OdysseusCommand *command);

#endif
