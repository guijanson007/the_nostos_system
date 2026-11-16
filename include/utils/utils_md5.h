#ifndef _UTILS_MD5_H
#define _UTILS_MD5_H

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#define MD5_LENGTH 32

char *computeMd5(const char *psPath);

#endif