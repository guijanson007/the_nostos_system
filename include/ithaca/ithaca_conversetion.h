#ifndef _ITHACA_CONVERSETION_H_
#define _ITHACA_CONVERSETION_H_

/***********************************************
 *
 * @File : ithaca_conversetion.h
 * @Purpose : declarations for the conversation between Ithaca and odysseus
 * @Author : Axel Boleda i Callizo, Guilherme Rodrigues
 * @Date : 31/10/2026
 *
 ************************************************/
 
// Project includes
#include "ithaca.h"
 
// Constant
#define MAX_VOYAGE_ID_DIGITS 9
#define NO_VOYAGES_MESSAGE "0"
#define VOYAGE_LINE_FORMAT "%d&%d&%d&%s&%s&%d"
#define VOYAGE_ACCEPTED_FORMAT "OK&%d&%s&%s&%d"
#define MSG_VOYAGE_ASSIGNED ">>> Voyage %d assigned to %s."
#define LOG_PREFIX ">>> "
#define LOG_LEFT_ITHACA " has left Ithaca. Fair winds!"
#define LOG_CONNECTED_ITHACA " connected to Ithaca."
#define LOG_REQUESTED_VOYAGES " requested available voyages."
 
#define ERROR_MESSAGE_FORMAT "ERROR: The message that you send has the wrong format"
#define ERROR_DISCONECTING "ERROR: you couldn't be diconected"
#define ERROR_CLIENT_ALREADY_CONNECTED "ERROR: The client is already connected"
#define ERROR_MEMORY_ALLOCATION "ERROR: Memory allocation failed"
#define ERROR_NOT_CONNECTED "ERROR: You must connect to Ithaca first"
#define ERROR_NO_VOYAGES "ERROR: No voyages available"
#define ERROR_INVALID_VOYAGE_ID "ERROR: Invalid voyage id"
#define ERROR_VOYAGE_NOT_AVAILABLE "ERROR: The voyage is not available"
#define ERROR_ACTIVE_VOYAGE "ERROR: You already have an active voyage"
#define OK_MESSAGE "OK"
 
// Functions
void consoleLog(const char *psOdysseusName, const char *psPostFix);
void choseOption(Ithaca *ptIthaca, Message tMsg, int nFdClient);
int removeClient(Ithaca *ptIthaca, int nFdClient);

#endif
