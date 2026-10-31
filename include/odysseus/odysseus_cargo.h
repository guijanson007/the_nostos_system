#ifndef _ODYSSEUS_CARGO_H_
#define _ODYSSEUS_CARGO_H_

/***********************************************
 *
 * @File : odysseus_cargo.h
 * @Purpose : declarations for the supplies (cargo) of the odysseus
 * @Author : Axel Boleda Callizo
 * @Date : 26/10/2026
 *
 ************************************************/

// System includes
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// Constant declarations
#define CONSUME_WEIGHT 7
 
// Custom type definitions
typedef struct {
    char *psName;
    int nAmount;
} Supply;
 
typedef struct {
    Supply *atSupplies;
    int nNumSupplies;
    int nTotalAmount;
    int nGold;
} Cargo;


// Functions
void consumeFood(Cargo *ptCargo);
void calculateTotalAmount(Cargo *ptCargo);
int addSupply(Cargo *ptCargo, const char *psName, int nAmount);
int removeSuplies(Cargo *ptCargo, const char *psName, int nAmount);

#endif