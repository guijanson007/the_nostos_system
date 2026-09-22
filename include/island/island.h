#ifndef __ISLAND_H_
#define __ISSLAND_H_

typedef struct {
    char *name;
    char *ip;
    int port;
}Rout;

typedef struct{
    char *name;
    char *file;
    char *ip;
    int port;
    int capacity;
    size_t numRoutes;
    Rout *routs;
}Island;

int loadIsland(int fd, Island *island);
void freeIsland(Island *island);

#endif