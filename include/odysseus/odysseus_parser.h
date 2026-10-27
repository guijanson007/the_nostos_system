#ifndef __ODYSSEUS_PARSER_H
#define __ODYSSEUS_PARSER_H

/***********************************************
 *
 * @File : oduysseus_parser.h
 * @Purpose : parser for the odysseus configuration file
 * @Author : Guilherme Rodrigues
 * @Date : 20/09/2026
 *
 ************************************************/

#include "odysseus.h"

// Constantº
#define MIN_CONFIG_LINES 6
#define SUPPLIES_FIRST_LINE 6
#define ERROR_INVALID_FILE "Invalid or incomplete odysseus.dat file"
#define ERROR_PARSE_FIELDS "Failed to allocate/parse odysseus fields"
#define ERROR_ALLOC_SUPPLIES "Failed to allocate memory for supplies"
 
// Projects
void freeOdysseus(Odysseus *ptOdysseus);
void freeOdysseusVoyage(OdysseusVoyage *ptVoyage);
void freeListedVoyages(Odysseus *ptOdysseus);
Odysseus odysseusParseFile(int nFd);
void odysseusPrintInfo(Odysseus *ptOdysseus);

#endif 
