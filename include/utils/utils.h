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
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

// Constants
#define READ_CHUNK_SIZE 4096
#define INITIAL_ELEMENT_CAPACITY 100
#define INITIAL_LINES_CAPACITY 1
#define INITIAL_BINARY_CAPACITY 1
#define MAX_INT_DIGITS 9
#define UPPER_CASE_OFFSET 32
#define NEW_LINE "\n"
#define WAIT_POLL_USEC 200000
#define WAIT_QUIT 0


#define ERROR_MALLOC "Malloc error"
#define ERROR_REALLOC "Realloc error"
#define ERROR_READ "Read error"

#include "utils.h"

// Functions
void freeLines(char **apsLines, size_t tCount);
void writeToFileAndStdOut(int nFd, const char *psMessage);
void println(const char *psMessage);
void print(const char *psMessage);
void printErr(const char *psMessage);
void strToUpper(char *psString);
char *readUntil(int nFd, char cEnd);
void *readUntilEmpty(int nFd, size_t tElementSize, size_t *ptOutCount);
int waitReadable(int nFd, volatile sig_atomic_t *pnQuit);
int readLine(int nFd, char **ppsBuffer);
char **readLines(int nFd, size_t *ptOutCount);
void *readBinary(int nFd, size_t tElementSize, size_t *ptOutCount);
int isAllDigits(const char *psString);
int isPositiveInteger(const char *psString);

#endif // __UTILS_H__
