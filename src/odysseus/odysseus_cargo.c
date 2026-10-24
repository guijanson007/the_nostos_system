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
        if (ptCargo->supplies[i].amount <= 0) continue;

        if (ptCargo->supplies[i].amount >= foodToConsume) {
            ptCargo->supplies[i].amount -= foodToConsume;
            ptCargo->totalAmount -= foodToConsume;
            foodToConsume = 0;
        }
        else {
            ptCargo->totalAmount -= ptCargo->supplies[i].amount;
            foodToConsume -= ptCargo->supplies[i].amount;
            ptCargo->supplies[i].amount = 0;
        }
    }
}