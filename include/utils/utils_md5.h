#ifndef _UTILS_MD5_H
#define _UTILS_MD5_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>


#define MD5_LENGTH 32

char *computeMd5(const char *psPath);

#endif