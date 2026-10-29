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

// Functions
int mergeRoutes(Odysseus *ptOdysseus, const MapRoute *atRoutes, int nRoutes);
void freeMapRoutes(MapRoute *atRoutes, int nRoutes);
void displayMap(const Odysseus *ptOdysseus);

#endif // !#ifndef __ODYSSEUS_MAP_H__
