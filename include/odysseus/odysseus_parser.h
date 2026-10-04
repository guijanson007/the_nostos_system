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

void freeOdysseus(Odysseus *odysseus);
Odysseus odysseusParseFile(int fd);
void odysseusPrintInfo(Odysseus *odysseus);


#endif 
