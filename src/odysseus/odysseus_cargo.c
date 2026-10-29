/***********************************************
 *
 * @File : odysseus_cargo.c
 * @Purpose : Manages the logic for the suplies of the odysseus
 * @Author : Axel Boleda Callizo
 * @Date : 24/10/2026
 *
 ************************************************/

#define _GNU_SOURCE
#include "odysseus/odysseus_cargo.h"

void consumeFood(Cargo *ptCargo) {
    int foodToConsume = CONSUME_WEIGHT;

    for (int i = 0; i < ptCargo->numSupplies && foodToConsume > 0; i++) {
        if (ptCargo->supplies[i].amount <= 0)
            continue;

        if (ptCargo->supplies[i].amount >= foodToConsume) {
            ptCargo->supplies[i].amount -= foodToConsume;
            ptCargo->totalAmount -= foodToConsume;
            foodToConsume = 0;
        } else {
            ptCargo->totalAmount -= ptCargo->supplies[i].amount;
            foodToConsume -= ptCargo->supplies[i].amount;
            ptCargo->supplies[i].amount = 0;
        }
    }
}

void calculateTotalGold(Cargo *ptCargo) {
    for (int i = 0; i < ptCargo->numSupplies; i++) {
        ptCargo->totalAmount += ptCargo->supplies[i].amount;
    }
}

int addSupply(Cargo *ptCargo, const char *psName, int nAmount) {
    Supply *atTmp = NULL;
    char *psCopy = NULL;
    int i = 0;

    for (i = 0; i < ptCargo->numSupplies; i++) {
        if (0 == strcasecmp(ptCargo->supplies[i].name, psName)) {
            ptCargo->supplies[i].amount += nAmount;
            ptCargo->totalAmount += nAmount;
            return 0;
        }
    }

    psCopy = strdup(psName);
    if (NULL == psCopy) {
        return -1;
    }

    atTmp =
        realloc(ptCargo->supplies, sizeof(Supply) * (ptCargo->numSupplies + 1));
    if (NULL == atTmp) {
        free(psCopy);
        return -1;
    }

    ptCargo->supplies = atTmp;
    ptCargo->supplies[ptCargo->numSupplies].name = psCopy;
    ptCargo->supplies[ptCargo->numSupplies].amount = nAmount;
    ptCargo->numSupplies++;
    ptCargo->totalAmount += nAmount;

    return 0;
}


int removeSuplies(Cargo *ptCargo, const char *psName, int nAmount) {
    int i;
    
    for (i = 0; i < ptCargo->numSupplies; i++) {
        if (0 == strcasecmp(ptCargo->supplies[i].name, psName)) {
            if (i == ptCargo->numSupplies) {
                return -1
            }

            if (ptCargo->supplies[i].amount < nAmount) {
                return -1;
            }
            
            ptCargo->supplies[i].amount -= nAmount;
            ptCargo->totalAmount -= nAmount;

            if (0 == ptCargo->supplies[i].amount) {
                    free(ptCargo->supplies[i].name);
            for (j = i; j < ptCargo->numSupplies - 1; j++) {
                ptCargo->supplies[j] = ptCargo->supplies[j + 1];
            }
            ptCargo->numSupplies--;
        }
    }

    return 0;
}