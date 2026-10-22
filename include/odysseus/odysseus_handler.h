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
void odysseusConnect(Odysseus *tOdysseus, const char *psDestination);
void odysseusMap(Odysseus *tOdysseus);
void odysseusStatus(Odysseus *tOdysseus);
void odysseusDeliver(Odysseus *tOdysseus);
void odysseusClaim(Odysseus *tOdysseus);
void odysseusListVoyages(Odysseus *tOdysseus);
void odysseusListMarket(Odysseus *tOdysseus);
void odysseusAcceptVoyage(Odysseus *tOdysseus, const char *psVoyageId);
void odysseusSail(Odysseus *tOdysseus, const char *psVoyageId);
void odysseusBuy(Odysseus *tOdysseus, const char *psProduct,
                 const char *psAmount);
void odysseusSell(Odysseus *tOdysseus, const char *psProduct,
                  const char *psAmount);

#endif // !#ifndef __ODYSSEUS_HANDLER_H__
