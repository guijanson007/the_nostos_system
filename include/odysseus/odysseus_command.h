#ifndef ODYSSEUS_COMMAND_H
#define ODYSSEUS_COMMAND_H

/***********************************************
 *
 * @File : odysseus_command.h
 * @Purpose : responsible for parsing and executing commands for the odysseus
 * process
 * @Author : Guilherme Rodrigues
 * @Date : 26/09/2026
 *
 ************************************************/

// System includes
#include <string.h>
 
// Project includes
#include "odysseus/odysseus.h"
#include "odysseus/odysseus_handler.h"

// Constant
#define ODYSSEUS_MAX_ARGS 3
#define COMMAND_DELIMITERS " \t"
 
#define CMD_MAP "MAP"
#define CMD_STATUS "STATUS"
#define CMD_DELIVER "DELIVER"
#define CMD_CLAIM "CLAIM"
#define CMD_CONNECT "CONNECT"
#define CMD_LIST "LIST"
#define CMD_ACCEPT "ACCEPT"
#define CMD_SAIL "SAIL"
#define CMD_BUY "BUY"
#define CMD_SELL "SELL"

#define ARG_ITHACA "ITHACA"
#define ARG_VOYAGES "VOYAGES"
#define ARG_MARKET "MARKET"
 
#define ERROR_UNKNOWN_COMMAND "Unknown command"
#define USAGE_MAP "Usage: MAP"
#define USAGE_STATUS "Usage: STATUS"
#define USAGE_DELIVER "Usage: DELIVER"
#define USAGE_CLAIM "Usage: CLAIM"
#define USAGE_CONNECT "Usage: CONNECT ITHACA"
#define USAGE_LIST "Usage: LIST VOYAGES | LIST MARKET"
#define USAGE_ACCEPT "Usage: ACCEPT <voyage_id>"
#define USAGE_SAIL "Usage: SAIL <ISLAND>"
#define USAGE_BUY "Usage: BUY <PRODUCT> <AMOUNT>"
#define USAGE_SELL "Usage: SELL <PRODUCT> <AMOUNT>"
#define ERROR_VOYAGE_ID "Voyage ID must be a positive integer"
#define ERROR_AMOUNT "Amount must be a positive integer"


//Enumerations
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

//Structures
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

//Functions
int parseOdysseusCommand(char *psLine, OdysseusCommand *ptCommand);
CommandValidationResult validateOdysseusCommand(const OdysseusCommand *ptCommand, const char **ppsErrorMessage);
void executeOdysseusCommand(Odysseus tOdysseus, const OdysseusCommand *ptCommand);

#endif // ODYSSEUS_COMMAND_H
