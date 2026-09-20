#ifndef __UTILS_H__
#define __UTILS_H__

/***********************************************
 *
 * @File : utils.h
 * @Purpose : collection of utility functions for file and console operations
 * @Author : Guilherme Rodrigues
 * @Date : 20/09/2026
 *
 ************************************************/

// System includes
#include <stdio.h>

void writeToFileAndStdOut(int fd, const char *message);
void println(const char *message);
void printErr(const char *message);
void *readUntilEmpty(int fd, size_t elementSize, size_t *outCount);
char **readLines(int fd, size_t *outCount);

#endif // __UTILS_H__
