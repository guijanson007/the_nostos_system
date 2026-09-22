#ifndef __HANDLER_H__
#define __HANDLER_H__

/***********************************************
 *
 * @File : menu.h
 * @Purpose : console menu for odysseus process
 * @Author : Guilherme Rodrigues
 * @Date : 22/09/2026
 *
 ************************************************/

// own headers
#include "odysseus/odysseus.h"

void odysseusConnect(Odysseus odysseus, const char *destination);
void odysseusMap(Odysseus odysseus);
void odysseusStatus(Odysseus odysseus);
void odysseusDeliver(Odysseus odysseus);
void odysseusClaim(Odysseus odysseus);
void odysseusListVoyages(Odysseus odysseus);
void odysseusListMarket(Odysseus odysseus);
void odysseusAcceptVoyage(Odysseus odysseus, const char *voyageId);
void odysseusSail(Odysseus odysseus, const char *voyageId);
void odysseusBuy(Odysseus odysseus, const char *product, const char *amount);
void odysseusSell(Odysseus odysseus, const char *product, const char *amount);

#endif // !#ifndef __HANDLER_H__
