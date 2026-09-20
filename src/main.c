#include "odysseus/odysseus.h"
#include <sphragis.h>
#include <unistd.h>

int main() {
    Odysseus odysseus = parseFile();
    odysseusPrintInfo(&odysseus);
}
