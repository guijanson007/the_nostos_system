#include "odysseus/odysseus.h"
#include <sphragis.h>
#include <unistd.h>

int main() {
    Odysseus odysseus = odysseusParseFile();
    odysseusPrintInfo(&odysseus);
}
