#ifndef __ODYSSEUS_HANDLER_H__
#define __ODYSSEUS_HANDLER_H__

/***********************************************
 *
 * @File : odysseus_handler.h
 * @Purpose : handler for the odysseus process
 * @Author : Guilherme Rodrigues
 * @Date : 22/09/2026
 *
 ************************************************/

// own headers
#include "odysseus/odysseus.h"

// Functions
OdysseusStatus odysseusConnect(Odysseus *tOdysseus);
void odysseusMap(Odysseus *tOdysseus);
void odysseusStatus(Odysseus *tOdysseus);
void odysseusDeliver(Odysseus *tOdysseus);
void odysseusClaim(Odysseus *tOdysseus);
OdysseusStatus odysseusListMarket(Odysseus *tOdysseus);
OdysseusStatus odysseusBuy(Odysseus *tOdysseus, const char *psProduct,
                           const char *psAmount);
OdysseusStatus odysseusListVoyages(Odysseus *tOdysseus);
OdysseusStatus odysseusAcceptVoyage(Odysseus *tOdysseus,
                                    const char *psVoyageId);
OdysseusStatus odysseusSail(Odysseus *tOdysseus, const char *psDestination);
OdysseusStatus odysseusSell(Odysseus *tOdysseus, const char *psProduct,
                  const char *psAmount);

#endif // !#ifndef __ODYSSEUS_HANDLER_H__
