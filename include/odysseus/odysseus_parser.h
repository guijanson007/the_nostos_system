#ifndef __ODYSSEUS_H
#define __ODYSSEUS_H

/***********************************************
 *
 * @File : oduysseus_parser.h
 * @Purpose : parser for the odysseus configuration file
 * @Author : Guilherme Rodrigues
 * @Date : 20/09/2026
 *
 ************************************************/

typedef struct {
    char *name;
    int amount;
} Supply;

typedef struct {
    char *name;
    char *path;
    char *ip;
    char *port;
    char *initialInfo;
    float initialMoney;
    int initialNumSupplies;
    Supply *supplies;
} Odysseus;

void freeOdysseus(Odysseus *odysseus);
Odysseus odysseusParseFile(int fd);
void odysseusPrintInfo(Odysseus *odysseus);

#endif // !__ODYSSEUS_H
