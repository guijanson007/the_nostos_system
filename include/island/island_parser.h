#ifndef __ISLAND_PARSER_H_
#define __ISLAND_PARSER_H_

/***********************************************
 *
 * @File : island.h
 * @Purpose : declarations for the island process
 * @Author : Axel Boleda Callizo
 * @Date : 24/09/2026
 *
 ************************************************/

//Local includes
#include "island/island.h"
#include "sphragis.h"

//Constants
#define START_ROUTS 5
#define INITIAL_ROUTES_CAPACITY 15


//Functions
int loadIsland(int fd, Island *island, SPHRAGIS_Island *sphragis);
int loadStock(int fd, Stock **stock, size_t *numStock);

#endif // __ISLAND_PARSER_H