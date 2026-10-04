/***********************************************
 *
 * @File : odysseus_handler.c
 * @Purpose : handlers for the odysseus commands. Not yet implemented, will
 *            be used starting from phase 2 of the project.
 * @Author : Guilherme Rodrigues
 * @Date : 04/10/2026
 *
 ************************************************/
 

// header
#include "odysseus/odysseus_handler.h"

void odysseusConnect(Odysseus tOdysseus, const char *psDestination) {
    (void)tOdysseus;
    (void)psDestination;
}

void odysseusMap(Odysseus tOdysseus) { (void)tOdysseus; }

void odysseusStatus(Odysseus tOdysseus) { (void)tOdysseus; }

void odysseusDeliver(Odysseus tOdysseus) { (void)tOdysseus; }

void odysseusClaim(Odysseus tOdysseus) { (void)tOdysseus; }

void odysseusListVoyages(Odysseus tOdysseus) { (void)tOdysseus; }

void odysseusListMarket(Odysseus tOdysseus) { (void)tOdysseus; }

void odysseusAcceptVoyage(Odysseus tOdysseus, const char *psVoyageId) {
    (void)tOdysseus;
    (void)psVoyageId;
}

void odysseusSail(Odysseus tOdysseus, const char *psVoyageId) {
    (void)tOdysseus;
    (void)psVoyageId;
}

void odysseusBuy(Odysseus tOdysseus, const char *psProduct, const char *psAmount) {
    (void)tOdysseus;
    (void)psProduct;
    (void)psAmount;
}

void odysseusSell(Odysseus tOdysseus, const char *psProduct, const char *psAmount) {
    (void)tOdysseus;
    (void)psProduct;
    (void)psAmount;
}
