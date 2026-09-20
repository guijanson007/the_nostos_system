#ifndef __ODYSSEUS_H
#define __ODYSSEUS_H

/***********************************************
 *
 * @File : oduysseus.h
 * @Purpose : entrypoint of odysseus process
 * @Author : Guilherme Rodrigues
 * @Date : 20/09/2026
 *
 ************************************************/

typedef struct {
    char name[100];
    int amount;
} Supply;

typedef struct {
    char name[100];
    char path[100];
    char ipPort[100];
    char initialInfo[200];
    float initialMoney;
    int initialNumSupplies;
    Supply *supplies;
} Odysseus;

void odysseusDestroy(Odysseus *odysseus);
Odysseus parseFile();
void odysseusPrintInfo(Odysseus *odysseus);

#endif // !__ODYSSEUS_H
