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
    int anPipe[2];
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
    if (-1 == pipe(anPipe)) {
        free(psMd5);
        return NULL;
    }

    nPid = fork();
    if (0 > nPid) {
        close(anPipe[0]);
        close(anPipe[1]);
        free(psMd5);
        return NULL;
    }
    else if (0 == nPid) {
        // Child: nothing but fd plumbing before exec (we may come from a thread)
        dup2(anPipe[1], STDOUT_FILENO);
        close(anPipe[0]);
        close(anPipe[1]);
        nNull = open("/dev/null", O_WRONLY);
        if (0 <= nNull) {
            dup2(nNull, STDERR_FILENO);
            close(nNull);
        }
        execlp("md5sum", "md5sum", psPath, NULL);
        _exit(EXIT_FAILURE);
    }

    // The parent must close its write end, otherwise a failing md5sum would
    // leave read() blocked forever instead of reaching end of file
    close(anPipe[1]);
    while (nRead < MD5_LENGTH) {
        nBytes = read(anPipe[0], psMd5 + nRead, MD5_LENGTH - nRead);
        if (0 >= nBytes) {
            break;
        }
        nRead += nBytes;
    }
    close(anPipe[0]);

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
