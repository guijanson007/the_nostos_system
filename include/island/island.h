#ifndef __ISLAND_H_
#define __ISLAND_H_

#include "../lib/sphragis/sphragis.h"
#include "../include/utils/utils.h"

typedef struct {
    char *name;
    char *ip;
    int port;
} Rout;

typedef struct {
    char *name;
    char *file;
    char *ip;
    int port;
    int capacity;
    size_t numRouts;
    Rout *routs;
} Island;

typedef struct {
    char name[100];
    int quantity;
    int price;
} Stock;

int loadIsland(int fd, Island *island, SPHRAGIS_Island *sphragis);
void freeIsland(Island *island, SPHRAGIS_Island *sphragis);
int loadStock(int fd, Stock **stock, size_t *numStock);
void freeStock(Stock **stock);

#endif