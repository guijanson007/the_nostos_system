#include <sphragis.h>
#include <string.h>
#include <unistd.h>

int main() {
    write(STDOUT_FILENO, "Hello, World!\n", strlen("Hello, World!\n"));
}
