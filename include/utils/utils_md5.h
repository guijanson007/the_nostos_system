#ifndef _UTILS_MD5_H
#define _UTILS_MD5_H

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#define MD5_LENGTH 32
#define MD5_COMMAND "md5sum"
#define MD5_NULL_DEVICE "/dev/null"

// Ends of the pipe that carries the output of md5sum
#define MD5_PIPE_ENDS 2
#define MD5_PIPE_READ 0
#define MD5_PIPE_WRITE 1

char *computeMd5(const char *psPath);

#endif
