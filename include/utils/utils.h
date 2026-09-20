#ifndef __UTILS_H__
#define __UTILS_H__




// System includes
#include <stdio.h>

void writeToFileAndStdOut(int fd, const char *message);
void println(const char *message);
void printErr(const char *message);
void *readUntilEmpty(int fd, size_t element_size, size_t *out_count);

#endif // __UTILS_H__
