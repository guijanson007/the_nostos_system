
#include "utils/utils_md5.h"


char *computeMd5(const char *psPath) {
    pid_t nPid = 0;
    int anPipe[2];
    int nStatus = 0;
    int nRead = 0;
    int nBytes = 0;
    char *psMd5 = malloc((MD5_LENGTH + 1) * sizeof(char));

    if (NULL == psPath) {
        return NULL;
    }
    if (-1 == pipe(anPipe)) {
        return NULL;
    }

    nPid = fork();
    if (0 > nPid) {
        close(anPipe[0]);
        close(anPipe[1]);
        return NULL;
    }
    else if (0 == nPid) {
        dup2(anPipe[1], STDOUT_FILENO);
        close(anPipe[0]);
        close(anPipe[1]);
        execlp("md5sum", "md5sum", psPath, NULL);
        exit(EXIT_FAILURE);
    }
    else {
        while (-1 == waitpid(nPid, &nStatus, 0)) {
            if (EINTR != errno) {
                return NULL;
            }
        }

        close(anPipe[1]);
        while (nRead < MD5_LENGTH) {
            nBytes = read(anPipe[0], psMd5 + nRead, MD5_LENGTH - nRead);
            if (0 >= nBytes) {
                break;
            }
            nRead += (int)nBytes;
        }
        close(anPipe[0]);

        if (!WIFEXITED(nStatus) || 0 != WEXITSTATUS(nStatus) ||
            MD5_LENGTH != nRead) {
            return NULL;
        }

        psMd5[MD5_LENGTH] = '\0';
    }
    
    return psMd5;
}


