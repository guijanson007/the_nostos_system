#ifndef _ODYSSEUS_CARGO_H_
#define _ODYSSEUS_CARGO_H_

#define CONSUME_WEIGHT 7

//Struct
typedef struct {
    char *name;
    int amount;
} Supply;

typedef struct {
    Supply *supplies;
    int numSupplies;
    int totalAmount;
} Cargo;

void consumeFood(Cargo *ptCargo);

#endif