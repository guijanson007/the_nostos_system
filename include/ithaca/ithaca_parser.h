#ifndef __ITHACA_PARSER_H_
#define __ITHACA_PARSER_H_

/***********************************************
 *
 * @File : ithaca.h
 * @Purpose : entrypoint of ithaca prosses
 * @Author : Axel Boleda
 * @Date : 22/09/2026
 *
 ************************************************/

#include "ithaca.h"

int loadIthacaData(int nFd, Ithaca *ptIthaca);
int loadVoyageData(int nFd, Voyage **patVoyages, size_t *ptNumVoyages);

#endif