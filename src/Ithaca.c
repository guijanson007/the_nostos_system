#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define ERROR_ARGC "ERROR IRHACA: Invalid number of arguments"



int main (int argc, char *argv[]) {
    if (argc != 2) {
        println(ERROR_ARGC);
        return 1;
    }



    return 0;
}