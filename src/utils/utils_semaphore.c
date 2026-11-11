/***********************************************
 *
 * @File : utils_semaphore.c
 * @Purpose : wrapper over the provided semaphore library. This is the ONLY
 *            file allowed to include semaphore_v2.h: the library defines
 *            its functions inside the header, so a second include in the
 *            same executable causes multiple definition errors. It must
 *            also not define _GNU_SOURCE (the library defines __USE_XOPEN
 *            itself and the build would warn), so no asprintf here.
 * @Author : Guilherme Rodrigues
 * @Date : 11/11/2026
 *
 ************************************************/

// The library goes first, before any other system header
#include "semaphore_v2.h"

#include "utils/utils_semaphore.h"

// The library returns 0 on success
#define SEM_LIB_OK 0

/***********************************************
 *
 * @Name : semCreate
 * @Def : Creates a private semaphore and sets its initial value. A value of
 *        1 makes it a mutex, a higher one a counting semaphore.
 * @Arg : Out: ptSem = handle of the new semaphore
 *        In: nInitial = initial value of the counter
 * @Ret : SEM_WRAP_OK on success, SEM_WRAP_ERROR otherwise (nothing is left
 *        created).
 *
 ************************************************/
int semCreate(Sem *ptSem, int nInitial) {
    semaphore tLib;

    if (NULL == ptSem) {
        return SEM_WRAP_ERROR;
    }

    if (SEM_LIB_OK != SEM_constructor(&tLib)) {
        return SEM_WRAP_ERROR;
    }

    if (SEM_LIB_OK != SEM_init(&tLib, nInitial)) {
        SEM_destructor(&tLib);
        return SEM_WRAP_ERROR;
    }

    ptSem->nId = tLib.shmid;
    return SEM_WRAP_OK;
}

/***********************************************
 *
 * @Name : semWait
 * @Def : Decreases the semaphore, blocking while it is zero.
 * @Arg : In: ptSem = semaphore to wait on
 * @Ret : SEM_WRAP_OK on success, SEM_WRAP_ERROR otherwise.
 *
 ************************************************/
int semWait(const Sem *ptSem) {
    semaphore tLib;

    if (NULL == ptSem) {
        return SEM_WRAP_ERROR;
    }

    tLib.shmid = ptSem->nId;
    return (SEM_LIB_OK == SEM_wait(&tLib)) ? SEM_WRAP_OK : SEM_WRAP_ERROR;
}

/***********************************************
 *
 * @Name : semSignal
 * @Def : Increases the semaphore, waking up one waiter if there is any.
 * @Arg : In: ptSem = semaphore to signal
 * @Ret : SEM_WRAP_OK on success, SEM_WRAP_ERROR otherwise.
 *
 ************************************************/
int semSignal(const Sem *ptSem) {
    semaphore tLib;

    if (NULL == ptSem) {
        return SEM_WRAP_ERROR;
    }

    tLib.shmid = ptSem->nId;
    return (SEM_LIB_OK == SEM_signal(&tLib)) ? SEM_WRAP_OK : SEM_WRAP_ERROR;
}

/***********************************************
 *
 * @Name : semDestroy
 * @Def : Removes the semaphore from the system. System V semaphores outlive
 *        the process, so this must run on every exit path.
 * @Arg : In: ptSem = semaphore to remove
 * @Ret : SEM_WRAP_OK on success, SEM_WRAP_ERROR otherwise.
 *
 ************************************************/
int semDestroy(const Sem *ptSem) {
    semaphore tLib;

    if (NULL == ptSem) {
        return SEM_WRAP_ERROR;
    }

    tLib.shmid = ptSem->nId;
    return (SEM_LIB_OK == SEM_destructor(&tLib)) ? SEM_WRAP_OK : SEM_WRAP_ERROR;
}
