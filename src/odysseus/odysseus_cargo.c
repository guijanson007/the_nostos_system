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

/***********************************************
 *
 * @Name : consumeFood
 * @Def : Consumes CONSUME_WEIGHT kg of supplies, taking them from the
 *        first supplies of the cargo that still have some amount.
 * @Arg : In/Out: ptCargo = cargo whose supplies are consumed
 * @Ret : None.
 *
 ************************************************/
void consumeFood(Cargo *ptCargo) {
    int nFoodToConsume = CONSUME_WEIGHT;
    int i = 0;
 
    for (i = 0; i < ptCargo->nNumSupplies && 0 < nFoodToConsume; i++) {
        if (0 >= ptCargo->atSupplies[i].nAmount) {
            continue;
        }
 
        if (ptCargo->atSupplies[i].nAmount >= nFoodToConsume) {
            ptCargo->atSupplies[i].nAmount -= nFoodToConsume;
            ptCargo->nTotalAmount -= nFoodToConsume;
            nFoodToConsume = 0;
        } else {
            ptCargo->nTotalAmount -= ptCargo->atSupplies[i].nAmount;
            nFoodToConsume -= ptCargo->atSupplies[i].nAmount;
            ptCargo->atSupplies[i].nAmount = 0;
        }
    }
}

/***********************************************
 *
 * @Name : calculateTotalAmount
 * @Def : Adds the amount of every supply to the total amount of the cargo.
 * @Arg : In/Out: ptCargo = cargo whose total amount is calculated
 * @Ret : None.
 *
 ************************************************/
void calculateTotalAmount(Cargo *ptCargo) {
    int i = 0;
 
    for (i = 0; i < ptCargo->nNumSupplies; i++) {
        ptCargo->nTotalAmount += ptCargo->atSupplies[i].nAmount;
    }
}

/***********************************************
 *
 * @Name : addSupply
 * @Def : Adds an amount of a supply to the cargo. If the supply already
 *        exists (case-insensitive) its amount is increased, otherwise a
 *        new supply is added.
 * @Arg : In/Out: ptCargo = cargo to modify
 *        In: psName = name of the supply
 *        In: nAmount = amount to add
 * @Ret : 0 if the supply was added, -1 if the memory couldn't be
 *        allocated.
 *
 ************************************************/
int addSupply(Cargo *ptCargo, const char *psName, int nAmount) {
    Supply *atTmp = NULL;
    char *psCopy = NULL;
    int i = 0;
 
    for (i = 0; i < ptCargo->nNumSupplies; i++) {
        if (0 == strcasecmp(ptCargo->atSupplies[i].psName, psName)) {
            ptCargo->atSupplies[i].nAmount += nAmount;
            ptCargo->nTotalAmount += nAmount;
            return 0;
        }
    }
 
    psCopy = strdup(psName);
    if (NULL == psCopy) {
        return -1;
    }
 
    atTmp = realloc(ptCargo->atSupplies, sizeof(Supply) * (ptCargo->nNumSupplies + 1));
    if (NULL == atTmp) {
        free(psCopy);
        return -1;
    }
 
    ptCargo->atSupplies = atTmp;
    ptCargo->atSupplies[ptCargo->nNumSupplies].psName = psCopy;
    ptCargo->atSupplies[ptCargo->nNumSupplies].nAmount = nAmount;
    ptCargo->nNumSupplies++;
    ptCargo->nTotalAmount += nAmount;
 
    return 0;
}


/***********************************************
 *
 * @Name : removeSuplies
 * @Def : Removes an amount of a supply from the cargo (case-insensitive).
 *        If its amount reaches 0, the supply is removed from the array.
 * @Arg : In/Out: ptCargo = cargo to modify
 *        In: psName = name of the supply
 *        In: nAmount = amount to remove
 * @Ret : 0 if the amount was removed, -1 if the supply doesn't exist or
 *        there isn't enough.
 *
 ************************************************/
int removeSuplies(Cargo *ptCargo, const char *psName, int nAmount) {
    int i = 0;
    int j = 0;
 
    for (i = 0; i < ptCargo->nNumSupplies; i++) {
        if (0 == strcasecmp(ptCargo->atSupplies[i].psName, psName)) {
            break;
        }
    }
 
    if (i == ptCargo->nNumSupplies) {
        return -1;
    }
    if (ptCargo->atSupplies[i].nAmount < nAmount) {
        return -1;
    }
 
    ptCargo->atSupplies[i].nAmount -= nAmount;
    ptCargo->nTotalAmount -= nAmount;
 
    if (0 == ptCargo->atSupplies[i].nAmount) {
        free(ptCargo->atSupplies[i].psName);
 
        for (j = i; j < ptCargo->nNumSupplies - 1; j++) {
            ptCargo->atSupplies[j] = ptCargo->atSupplies[j + 1];
        }
 
        ptCargo->nNumSupplies--;
    }
 
    return 0;
}