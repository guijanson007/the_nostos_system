#ifndef _UTILS_POOL_H
#define _UTILS_POOL_H

/***********************************************
 *
 * @File : utils_pool.h
 * @Purpose : fixed pool of worker threads with a bounded job queue guarded
 *            by semaphores. The owner (the select() loop of Ithaca or an
 *            island) submits jobs without blocking, and learns which jobs
 *            finished through a pipe it can put inside its select() set.
 *            A job is plain data (a type and its parameters): the worker
 *            picks the code to run with a switch inside executeJob, so the
 *            pool needs no function pointers of its own.
 * @Author : Guilherme Rodrigues
 * @Date : 11/11/2026
 *
 ************************************************/

// System includes
#include <pthread.h>

// Project includes
#include "utils/utils_semaphore.h"

// Return values of the pool functions
#define POOL_OK 0
#define POOL_ERROR -1
#define POOL_FULL -2

// Limits accepted by poolCreate. Every queued or running job leaves one
// pointer in the notification pipe, so capacity + workers must fit in it.
#define POOL_MAX_WORKERS 64
#define POOL_MAX_CAPACITY 1024

// Suggested sizes for the servers
#define POOL_DEFAULT_WORKERS 4
#define POOL_DEFAULT_CAPACITY 16

// Position of each end of the notification pipe
#define POOL_PIPE_READ 0
#define POOL_PIPE_WRITE 1
#define POOL_PIPE_ENDS 2

// Result of a job, written by the worker in PoolJob.nResult
#define JOB_RESULT_OK 0
#define JOB_RESULT_ERROR -1

// What a worker has to do. executeJob has one case per type.
typedef enum {
    JOB_SEND_FILE,     // send psPath through nFd
    JOB_RECEIVE_FILE,  // receive a file from nFd and store it in psPath
    JOB_SEAL_DELIVERY, // island: receive, seal with its glyph, send back
    JOB_VERIFY_CLAIM   // ithaca: receive and check the glyph of psOwner
} JobType;

// A job. The caller owns the struct and the strings it points to and must
// keep them alive until poolCollect returns the job. During the job the
// worker is the only one using nFd.
typedef struct {
    JobType eType;
    int nFd;          // socket of the client
    char *psPath;     // file to send, or where to store the received one
    char *psFileName; // name of the file in the transfer
    char *psOwner;    // island whose glyph is applied or expected
    int nResult;      // JOB_RESULT_OK or JOB_RESULT_ERROR, set by the worker
} PoolJob;

// Runs one job (a switch over ptJob->eType) and sets ptJob->nResult. It is
// implemented in utils_transfer.c, which every process links.
void executeJob(PoolJob *ptJob);

typedef struct {
    pthread_t *ptThreads; // worker threads
    int nWorkers;         // threads requested
    int nStarted;         // threads really running
    PoolJob **pptQueue;   // ring buffer of pending jobs (owned by callers)
    int nCapacity;        // size of the ring buffer
    int nHead;            // oldest pending job
    int nCount;           // pending jobs (protected by tMutex)
    int nQuit;            // 1 once poolDestroy started (protected by tMutex)
    Sem tItems;           // counts pending jobs, workers sleep on it
    Sem tMutex;           // protects nHead, nCount and nQuit
    int nHasItems;        // 1 if tItems was created
    int nHasMutex;        // 1 if tMutex was created
    int anNotify[POOL_PIPE_ENDS]; // finished jobs, one pointer each
} ThreadPool;

int poolCreate(ThreadPool *ptPool, int nWorkers, int nCapacity);
int poolSubmit(ThreadPool *ptPool, PoolJob *ptJob);
int poolNotifyFd(const ThreadPool *ptPool);
int poolCollect(ThreadPool *ptPool, PoolJob **pptJob);
void poolDestroy(ThreadPool *ptPool);

#endif
