#ifndef __ODYSSEUS_MAP_H__
#define __ODYSSEUS_MAP_H__

/***********************************************
 *
 * @File : odysseus_map.h
 * @Purpose : known map of the odysseus: merging bought routes and showing it
 * @Author : Guilherme Rodrigues
 * @Date : 29/10/2026
 *
 ************************************************/

// own headers
#include "odysseus/odysseus.h"

#define ERROR_MAP_WIDTH "ERROR: The terminal must be at least 80 columns wide to show the map"
#define ERROR_MAP_DISPLAY "ERROR: Could not display the map"

// Functions
int findDestinationFromIsland(Odysseus *ptOdysseus, const char *psDestination);
int mergeRoutes(Odysseus *ptOdysseus, const MapRoute *atRoutes, int nRoutes);
void freeMapRoutes(MapRoute *atRoutes, int nRoutes);
void displayMap(const Odysseus *ptOdysseus);

#endif // !#ifndef __ODYSSEUS_MAP_H__
