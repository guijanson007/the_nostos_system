#define _GNU_SOURCE

#include <fcntl.h>
#include "utils/utils_md5.h"

/***********************************************
 *
 * @Name : computeMd5
 * @Def : Runs the OS md5sum on a file (fork + pipe + exec, no system or
 *        popen) and returns the 32 hex characters it prints.
 * @Arg : In: psPath = path of the file
 * @Ret : Dynamic string with the MD5 (free it), or NULL if the file does
 *        not exist, md5sum failed or memory ran out.
 *
 ************************************************/
char *computeMd5(const char *psPath) {
    pid_t nPid = 0;
    int anPipe[MD5_PIPE_ENDS];
    int nStatus = 0;
    int nRead = 0;
    int nBytes = 0;
    int nNull = -1;
    char *psMd5 = NULL;

    if (NULL == psPath) {
        return NULL;
    }
    psMd5 = malloc((MD5_LENGTH + 1) * sizeof(char));
    if (NULL == psMd5) {
        return NULL;
    }
    // Close-on-exec: other workers' md5sum children must not inherit this
    // pipe (they would delay its end of file) nor any other descriptor
    if (-1 == pipe2(anPipe, O_CLOEXEC)) {
        free(psMd5);
        return NULL;
    }

    nPid = fork();
    if (0 > nPid) {
        close(anPipe[MD5_PIPE_READ]);
        close(anPipe[MD5_PIPE_WRITE]);
        free(psMd5);
        return NULL;
    }
    else if (0 == nPid) {
        // Child: nothing but fd plumbing before exec (we may come from a thread)
        dup2(anPipe[MD5_PIPE_WRITE], STDOUT_FILENO);
        close(anPipe[MD5_PIPE_READ]);
        close(anPipe[MD5_PIPE_WRITE]);
        nNull = open(MD5_NULL_DEVICE, O_WRONLY);
        if (0 <= nNull) {
            dup2(nNull, STDERR_FILENO);
            close(nNull);
        }
        execlp(MD5_COMMAND, MD5_COMMAND, psPath, NULL);
        // Only exit: the child of a threaded process must not touch malloc
        exit(EXIT_FAILURE);
    }

    // The parent must close its write end, otherwise a failing md5sum would
    // leave read() blocked forever instead of reaching end of file
    close(anPipe[MD5_PIPE_WRITE]);
    while (nRead < MD5_LENGTH) {
        nBytes = read(anPipe[MD5_PIPE_READ], psMd5 + nRead, MD5_LENGTH - nRead);
        if (0 >= nBytes) {
            break;
        }
        nRead += nBytes;
    }
    close(anPipe[MD5_PIPE_READ]);

    while (-1 == waitpid(nPid, &nStatus, 0)) {
        if (EINTR != errno) {
            free(psMd5);
            return NULL;
        }
    }

    if (!WIFEXITED(nStatus) || 0 != WEXITSTATUS(nStatus) ||
        MD5_LENGTH != nRead) {
        free(psMd5);
        return NULL;
    }

    psMd5[MD5_LENGTH] = '\0';
    return psMd5;
}
