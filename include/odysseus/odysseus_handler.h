#ifndef __ODYSSEUS_HANDLER_H__
#define __ODYSSEUS_HANDLER_H__

/***********************************************
 *
 * @File : odysseus_handler.h
 * @Purpose : handler for the odysseus process
 * @Author : Guilherme Rodrigues, Axel Boleda
 * @Date : 31/10/2026
 *
 ************************************************/

// own headers
#include "odysseus/odysseus.h"


// Constant declarations
#define ITHACA_NAME "Ithaca"
#define MSG_LEAVING "Leaving %s..."
#define MSG_LEAVING_ITHACA "Leaving Ithaca..."
#define MSG_RETURNING_ITHACA "Returning directly to Ithaca..."
#define MSG_ITHACA_CONNECTED "Connected to Ithaca."
#define MSG_STATUS_HEADER "--- %s ---\nGold: %d\n\nCargo:"
#define MSG_SUPPLY_LINE "%s: %d kg"
#define MSG_TOTAL_PROVISIONS "\nTotal provisions: %d kg\n"
#define MSG_NO_VOYAGE "Current voyage: None"
#define MSG_CURRENT_VOYAGE "Current voyage: %s -> %s\nReward: %d"
#define MSG_LOCATION "Current location: %s"
#define MSG_VOYAGES_HEADER "--- AVAILABLE VOYAGES ---"
#define MSG_VOYAGE_LINE "%d. %-20s -> %-10s [%d]"
#define MSG_VOYAGE_ACCEPTED "Voyage %d accepted.\nDestination: %s."
#define MSG_CANT_SAIL "We can't sail to %s from here..."
#define MSG_SAILING_TO "Sailing to %s..."
#define MSG_CONNECTED_TO "\nConnected to %s."
#define MSG_HARBOR_FULL "The harbor is full. We drop anchor and wait."
#define MSG_LOST_AT_SEA "\nThe crew has run out of provisions.\nOdysseus %s has been lost at sea."
#define MSG_GRANTED_ACCESS "%s has granted access to the port."
#define MSG_VOYAGE_DESTINATION "%s is the destination of your voyage (%s)."

// Functions
OdysseusStatus odysseusConnect(Odysseus *ptOdysseus);
void odysseusMap(Odysseus *ptOdysseus);
void odysseusStatus(Odysseus *ptOdysseus);
void odysseusDeliver(Odysseus *ptOdysseus);
void odysseusClaim(Odysseus *ptOdysseus);
OdysseusStatus odysseusListMarket(Odysseus *ptOdysseus);
OdysseusStatus odysseusBuy(Odysseus *ptOdysseus, const char *psProduct, const char *psAmount);
OdysseusStatus odysseusListVoyages(Odysseus *ptOdysseus);
OdysseusStatus odysseusAcceptVoyage(Odysseus *ptOdysseus, const char *psVoyageId);
OdysseusStatus odysseusSail(Odysseus *ptOdysseus, const char *psDestination);
OdysseusStatus odysseusSell(Odysseus *ptOdysseus, const char *psProduct, const char *psAmount);

#endif // !#ifndef __ODYSSEUS_HANDLER_H__
