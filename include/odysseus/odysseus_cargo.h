#ifndef _ODYSSEUS_CARGO_H_
#define _ODYSSEUS_CARGO_H_

#define CONSUME_WEIGHT 7

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// Struct
typedef struct {
    char *name;
    int amount;
} Supply;

typedef struct {
    Supply *supplies;
    int numSupplies;
    int totalAmount;
    int nGold;
} Cargo;

void consumeFood(Cargo *ptCargo);
void calculateTotalGold(Cargo *ptCargo);
int addSupply(Cargo *ptCargo, const char *psName, int nAmount);
int removeSuplies(Cargo *ptCargo, const char *psName, int nAmount);


#endif