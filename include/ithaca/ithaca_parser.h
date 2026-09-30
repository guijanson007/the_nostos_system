#ifndef __ITHACA_H_
#define __ITHACA_H_

/***********************************************
 *
 * @File : ithaca.h
 * @Purpose : entrypoint of ithaca prosses
 * @Author : Axel Boleda
 * @Date : 22/09/2026
 *
 ************************************************/

typedef struct {
    char *name;
    char *mision;
    char *ipAddress;
    int port;
} Ithaca;

typedef struct {
    int id;
    char *object;
    char *file;
    char *destination;
    int reward;
} Voyage;

int loadIthacaData(int fd, Ithaca *ithaca);
void freeIthaca(Ithaca *ithaca);
int loadVoyageData(int fd, Voyage **voyage, size_t *numVoyage);
void freeVoyage(Voyage *voyages, size_t *numVoyage);

#endif