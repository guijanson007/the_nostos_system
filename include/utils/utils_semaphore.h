#ifndef _UTILS_SEMAPHORE_H
#define _UTILS_SEMAPHORE_H

/***********************************************
 *
 * @File : utils_semaphore.h
 * @Purpose : thin wrapper over the provided semaphore library
 *            (lib/sphragis/semaphore_v2.h). The library is a header with
 *            function bodies, so it is included only by utils_semaphore.c
 *            and the rest of the project uses this interface instead.
 * @Author : Guilherme Rodrigues
 * @Date : 11/11/2026
 *
 ************************************************/

// Return values of the wrapper functions
#define SEM_WRAP_OK 0
#define SEM_WRAP_ERROR -1

// Semaphore handle. It does not expose the library's own types, so no file
// besides utils_semaphore.c ever sees them.
typedef struct {
    int nId;
} Sem;

int semCreate(Sem *ptSem, int nInitial);
int semWait(const Sem *ptSem);
int semSignal(const Sem *ptSem);
int semDestroy(const Sem *ptSem);

#endif
