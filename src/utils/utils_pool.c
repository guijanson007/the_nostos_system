#define _GNU_SOURCE

/***********************************************
 *
 * @File : utils_pool.c
 * @Purpose : fixed pool of worker threads. Jobs wait in a bounded ring
 *            buffer. Two semaphores guard it: tItems counts pending jobs
 *            (workers sleep on it instead of polling) and tMutex protects
 *            the buffer. Submitting never blocks, so a select() loop can
 *            use it: a full queue is reported and the caller decides.
 * @Author : Guilherme Rodrigues
 * @Date : 11/11/2026
 *
 ************************************************/

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/utils_pool.h"

// Values of the flags of the pool
#define POOL_FALSE 0
#define POOL_TRUE 1

// Initial value of each semaphore
#define POOL_ITEMS_INITIAL 0
#define POOL_MUTEX_INITIAL 1

// Marks a pipe end that is not open
#define POOL_NO_FD -1

/***********************************************
 *
 * @Name : poolWorker
 * @Def : Body of every worker thread. It sleeps until there is a job, runs
 *        it with executeJob and reports it through the notification pipe. It
 * leaves when it wakes up and finds the queue empty, which only happens on
 * shutdown (jobs already queued are still run first).
 * @Arg : In: pvArg = the ThreadPool
 * @Ret : NULL.
 *
 ************************************************/
static void *poolWorker(void *pvArg) {
    ThreadPool *ptPool = (ThreadPool *)pvArg;
    PoolJob *ptJob;
    int nRunning = POOL_TRUE;

    while (POOL_TRUE == nRunning) {
        if (SEM_WRAP_OK != semWait(&ptPool->tItems) ||
            SEM_WRAP_OK != semWait(&ptPool->tMutex)) {
            nRunning = POOL_FALSE;
        } else if (0 < ptPool->nCount) {
            ptJob = ptPool->pptQueue[ptPool->nHead];
            ptPool->nHead = (ptPool->nHead + 1) % ptPool->nCapacity;
            ptPool->nCount--;
            semSignal(&ptPool->tMutex);

            executeJob(ptJob);

            // One pointer is far below the atomic write size of a pipe
            if (sizeof(ptJob) !=
                (size_t)write(ptPool->anNotify[POOL_PIPE_WRITE], &ptJob,
                              sizeof(ptJob))) {
                nRunning = POOL_FALSE;
            }
        } else {
            semSignal(&ptPool->tMutex);
            nRunning = POOL_FALSE;
        }
    }

    return NULL;
}

/***********************************************
 *
 * @Name : poolInit
 * @Def : Puts the pool in a known empty state so poolDestroy can release
 *        it at any point of poolCreate.
 * @Arg : Out: ptPool = pool to initialize
 *        In: nWorkers = threads wanted
 *        In: nCapacity = size of the job queue
 * @Ret : None.
 *
 ************************************************/
static void poolInit(ThreadPool *ptPool, int nWorkers, int nCapacity) {
    memset(ptPool, 0, sizeof(*ptPool));
    ptPool->nWorkers = nWorkers;
    ptPool->nCapacity = nCapacity;
    ptPool->anNotify[POOL_PIPE_READ] = POOL_NO_FD;
    ptPool->anNotify[POOL_PIPE_WRITE] = POOL_NO_FD;
}

/***********************************************
 *
 * @Name : poolAllocate
 * @Def : Reserves the thread table and the job queue.
 * @Arg : In/Out: ptPool = pool to fill
 * @Ret : POOL_OK on success, POOL_ERROR if an allocation fails.
 *
 ************************************************/
static int poolAllocate(ThreadPool *ptPool) {
    ptPool->ptThreads = calloc((size_t)ptPool->nWorkers, sizeof(pthread_t));
    ptPool->pptQueue = calloc((size_t)ptPool->nCapacity, sizeof(PoolJob *));

    if (NULL == ptPool->ptThreads || NULL == ptPool->pptQueue) {
        return POOL_ERROR;
    }
    return POOL_OK;
}

/***********************************************
 *
 * @Name : poolOpenSync
 * @Def : Creates the two semaphores and the notification pipe.
 * @Arg : In/Out: ptPool = pool to fill
 * @Ret : POOL_OK on success, POOL_ERROR otherwise. What was created before
 *        the failure is released by poolDestroy.
 *
 ************************************************/
static int poolOpenSync(ThreadPool *ptPool) {
    if (SEM_WRAP_OK != semCreate(&ptPool->tItems, POOL_ITEMS_INITIAL)) {
        return POOL_ERROR;
    }
    ptPool->nHasItems = POOL_TRUE;

    if (SEM_WRAP_OK != semCreate(&ptPool->tMutex, POOL_MUTEX_INITIAL)) {
        return POOL_ERROR;
    }
    ptPool->nHasMutex = POOL_TRUE;

    if (-1 == pipe(ptPool->anNotify)) {
        ptPool->anNotify[POOL_PIPE_READ] = POOL_NO_FD;
        ptPool->anNotify[POOL_PIPE_WRITE] = POOL_NO_FD;
        return POOL_ERROR;
    }
    return POOL_OK;
}

/***********************************************
 *
 * @Name : poolStartWorkers
 * @Def : Starts the worker threads with every signal blocked, so SIGINT is
 *        always delivered to the main thread and never to a worker.
 * @Arg : In/Out: ptPool = pool whose workers are started
 * @Ret : POOL_OK if all the threads started, POOL_ERROR otherwise (the ones
 *        that did start are counted in nStarted and joined by poolDestroy).
 *
 ************************************************/
static int poolStartWorkers(ThreadPool *ptPool) {
    sigset_t tAll;
    sigset_t tOld;
    int nRet = POOL_OK;

    sigfillset(&tAll);
    pthread_sigmask(SIG_BLOCK, &tAll, &tOld);

    while (ptPool->nStarted < ptPool->nWorkers && POOL_OK == nRet) {
        if (0 != pthread_create(&ptPool->ptThreads[ptPool->nStarted], NULL,
                                poolWorker, ptPool)) {
            nRet = POOL_ERROR;
        } else {
            ptPool->nStarted++;
        }
    }

    pthread_sigmask(SIG_SETMASK, &tOld, NULL);
    return nRet;
}

/***********************************************
 *
 * @Name : poolCreate
 * @Def : Builds a pool: job queue, semaphores, notification pipe and
 *        worker threads. Nothing stays allocated if it fails.
 * @Arg : Out: ptPool = pool to build
 *        In: nWorkers = number of threads (1..POOL_MAX_WORKERS)
 *        In: nCapacity = pending jobs allowed (1..POOL_MAX_CAPACITY)
 * @Ret : POOL_OK on success, POOL_ERROR otherwise.
 *
 ************************************************/
int poolCreate(ThreadPool *ptPool, int nWorkers, int nCapacity) {
    if (NULL == ptPool || 0 >= nWorkers || POOL_MAX_WORKERS < nWorkers ||
        0 >= nCapacity || POOL_MAX_CAPACITY < nCapacity) {
        return POOL_ERROR;
    }

    poolInit(ptPool, nWorkers, nCapacity);

    if (POOL_OK == poolAllocate(ptPool) && POOL_OK == poolOpenSync(ptPool) &&
        POOL_OK == poolStartWorkers(ptPool)) {
        return POOL_OK;
    }

    poolDestroy(ptPool);
    return POOL_ERROR;
}

/***********************************************
 *
 * @Name : poolSubmit
 * @Def : Queues a job for the workers. It never waits for a free slot, so
 *        the caller (a select() loop) is never stalled by a busy pool.
 * @Arg : In/Out: ptPool = pool to use
 *        In: ptJob = job to run, owned by the caller until poolCollect
 *            returns it
 * @Ret : POOL_OK if queued, POOL_FULL if the queue is full (nothing was
 *        queued), POOL_ERROR otherwise.
 *
 ************************************************/
int poolSubmit(ThreadPool *ptPool, PoolJob *ptJob) {
    int nTail;

    if (NULL == ptPool || NULL == ptJob) {
        return POOL_ERROR;
    }

    if (SEM_WRAP_OK != semWait(&ptPool->tMutex)) {
        return POOL_ERROR;
    }

    if (POOL_TRUE == ptPool->nQuit) {
        semSignal(&ptPool->tMutex);
        return POOL_ERROR;
    }

    if (ptPool->nCapacity <= ptPool->nCount) {
        semSignal(&ptPool->tMutex);
        return POOL_FULL;
    }

    nTail = (ptPool->nHead + ptPool->nCount) % ptPool->nCapacity;
    ptPool->pptQueue[nTail] = ptJob;
    ptPool->nCount++;
    semSignal(&ptPool->tMutex);

    // Wakes up one sleeping worker
    if (SEM_WRAP_OK != semSignal(&ptPool->tItems)) {
        return POOL_ERROR;
    }
    return POOL_OK;
}

/***********************************************
 *
 * @Name : poolNotifyFd
 * @Def : Gives the descriptor that becomes readable when a job finishes,
 *        to be added to the select() set of the owner.
 * @Arg : In: ptPool = pool to ask
 * @Ret : The read end of the notification pipe, POOL_NO_FD if ptPool is
 *        NULL.
 *
 ************************************************/
int poolNotifyFd(const ThreadPool *ptPool) {
    if (NULL == ptPool) {
        return POOL_NO_FD;
    }
    return ptPool->anNotify[POOL_PIPE_READ];
}

/***********************************************
 *
 * @Name : poolCollect
 * @Def : Takes one finished job from the notification pipe. Call it when
 *        select() says poolNotifyFd is readable (it blocks otherwise).
 * @Arg : In/Out: ptPool = pool to ask
 *        Out: pptJob = the job given to poolSubmit, with nResult set
 * @Ret : POOL_OK on success, POOL_ERROR otherwise.
 *
 ************************************************/
int poolCollect(ThreadPool *ptPool, PoolJob **pptJob) {
    if (NULL == ptPool || NULL == pptJob) {
        return POOL_ERROR;
    }

    if (sizeof(*pptJob) != (size_t)read(ptPool->anNotify[POOL_PIPE_READ],
                                        pptJob, sizeof(*pptJob))) {
        return POOL_ERROR;
    }
    return POOL_OK;
}

/***********************************************
 *
 * @Name : poolDestroy
 * @Def : Stops the pool: refuses new jobs, wakes every worker, waits for
 *        the jobs already queued to finish, joins the threads and releases
 *        the queue, the semaphores and the pipe. Safe on a pool that was
 *        only partially built.
 * @Arg : In/Out: ptPool = pool to destroy
 * @Ret : None.
 *
 ************************************************/
void poolDestroy(ThreadPool *ptPool) {
    int i;

    if (NULL == ptPool) {
        return;
    }

    if (POOL_TRUE == ptPool->nHasMutex) {
        if (SEM_WRAP_OK == semWait(&ptPool->tMutex)) {
            ptPool->nQuit = POOL_TRUE;
            semSignal(&ptPool->tMutex);
        }
    }

    // One extra wake-up per worker: the one that finds no job leaves
    for (i = 0; i < ptPool->nStarted; i++) {
        semSignal(&ptPool->tItems);
    }
    for (i = 0; i < ptPool->nStarted; i++) {
        pthread_join(ptPool->ptThreads[i], NULL);
    }
    ptPool->nStarted = 0;

    if (POOL_TRUE == ptPool->nHasItems) {
        semDestroy(&ptPool->tItems);
    }
    if (POOL_TRUE == ptPool->nHasMutex) {
        semDestroy(&ptPool->tMutex);
    }
    for (i = 0; i < POOL_PIPE_ENDS; i++) {
        if (POOL_NO_FD != ptPool->anNotify[i]) {
            close(ptPool->anNotify[i]);
        }
    }

    free(ptPool->ptThreads);
    free(ptPool->pptQueue);
    poolInit(ptPool, 0, 0);
}
