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
#define LOG_OBJECT_FAILED " did not receive the object. The voyage is free again."
#define CLAIM_OK_FORMAT "OK&%d" 
#define CLAIM_TEMP_FORMAT "%s/.claim_%d.tmp"
#define MSG_VOYAGE_COMPLETED ">>> Voyage %d completed by %s."
#define MSG_SENDING_OBJECT ">>> Sending %s to %s..."
#define MSG_OBJECT_SENT ">>> Object transferred successfully."
#define MSG_CLAIMS_VOYAGE ">>> %s claims voyage %d."
#define MSG_RECEIVING_STAMPED ">>> Receiving stamped %s..."
#define MSG_OBJECT_RECEIVED ">>> Object received correctly."
#define MSG_GLYPH_VERIFIED ">>> Glyph of %s verified."
#define MSG_REWARD_GRANTED ">>> Reward granted: %d gold."
#define PATH_SEPARATOR '/'
#define OBJECT_PATH_FORMAT "%s/%s"
 
#define ERROR_MESSAGE_FORMAT "ERROR: The message that you send has the wrong format"
#define ERROR_DISCONECTING "ERROR: you couldn't be diconected"
#define ERROR_CLIENT_ALREADY_CONNECTED "ERROR: The client is already connected"
#define ERROR_MEMORY_ALLOCATION "ERROR: Memory allocation failed"
#define ERROR_NOT_CONNECTED "ERROR: You must connect to Ithaca first"
#define ERROR_NO_VOYAGES "ERROR: No voyages available"
#define ERROR_INVALID_VOYAGE_ID "ERROR: Invalid voyage id"
#define ERROR_VOYAGE_NOT_AVAILABLE "ERROR: The voyage is not available"
#define ERROR_ACTIVE_VOYAGE "ERROR: You already have an active voyage"
#define ERROR_CLAIM_NO_VOYAGE "ERROR: You have no active voyage"
#define ERROR_CLAIM_WRONG_VOYAGE "ERROR: That is not your active voyage"
#define ERROR_ITHACA_BUSY "ERROR: Ithaca is busy, try again later"
#define ERROR_CLAIM_HEADER "ERROR: Invalid file header"
#define ERROR_CLAIM_WRONG_FILE "ERROR: That is not the object of your voyage"
#define ERROR_CLAIM_BUSY "ERROR: Ithaca is busy, try again later"
#define ERROR_CLAIM_PEER_GONE "ERROR: The destination is missing"
#define ERROR_CLAIM_MD5_ERROR "ERROR: MD5 checksum mismatch"
#define ERROR_CLAIM_SIZE_ERROR "ERROR: File size mismatch"
#define ERROR_CLAIM_NO_REQUEST "ERROR: You have no pending claim request"
#define ERROR_CLAIM_FAILED "ERROR: The claim failed"
#define ERROR_OBJECT_MISSING "ERROR: The object of this voyage is not available"
#define OK_MESSAGE "OK"
 
// Functions
void finishJob(Ithaca *ptIthaca, PoolJob *ptJob);
void consoleLog(const char *psOdysseusName, const char *psPostFix);
void choseOption(Ithaca *ptIthaca, Message tMsg, int nFdClient);
int removeClient(Ithaca *ptIthaca, int nFdClient);

#endif
