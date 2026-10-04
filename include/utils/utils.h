#ifndef __UTILS_H__
#define __UTILS_H__

/***********************************************
 *
 * @File : utils.h
 * @Purpose : collection of utility functions for file and console operations
 * @Author : Guilherme Rodrigues and Axel Boleda Callizo
 * @Date : 20/09/2026
 *
 ************************************************/

// System includes
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Constants
#define READ_CHUNK_SIZE 4096
#define INITIAL_ELEMENT_CAPACITY 100
#define INITIAL_LINES_CAPACITY 1
#define INITIAL_BINARY_CAPACITY 1

// Functions
void freeLines(char **apsLines, size_t tCount);
void writeToFileAndStdOut(int nFd, const char *psMessage);
void println(const char *psMessage);
void printErr(const char *psMessage);
void strToUpper(char *psString);
void *readUntilEmpty(int nFd, size_t tElementSize, size_t *ptOutCount);
int readLine(int nFd, char **ppsBuffer);
char **readLines(int nFd, size_t *ptOutCount);
void *readBinary(int nFd, size_t tElementSize, size_t *ptOutCount);
int isAllDigits(const char *psString);
int isPositiveInteger(const char *psString);

#endif // __UTILS_H__
