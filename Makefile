all: island ithaca odysseus

island: build/island.o build/island_parser.o build/utils.o lib/sphragis/sphragis.o
	gcc build/island.o build/island_parser.o build/utils.o lib/sphragis/sphragis.o -o island -Wall -Wextra -lpthread

ithaca: build/ithaca.o build/ithaca_parser.o build/utils.o lib/sphragis/sphragis.o
	gcc build/ithaca.o build/ithaca_parser.o build/utils.o lib/sphragis/sphragis.o -o ithaca -Wall -Wextra -lpthread

odysseus: build/odysseus.o build/odysseus_command.o build/odysseus_handler.o build/odysseus_menu.o build/odysseus_parser.o build/utils.o lib/sphragis/sphragis.o
	gcc build/odysseus.o build/odysseus_command.o build/odysseus_handler.o build/odysseus_menu.o build/odysseus_parser.o build/utils.o lib/sphragis/sphragis.o -o odysseus -Wall -Wextra -lpthread

build:
	mkdir -p build

build/utils.o: src/utils/utils.c include/utils/utils.h | build
	gcc -c src/utils/utils.c -o build/utils.o -Wall -Wextra -Iinclude

build/island.o: src/island/island.c include/island/island.h include/island/island_parser.h include/utils/utils.h lib/sphragis/sphragis.h | build
	gcc -c src/island/island.c -o build/island.o -Wall -Wextra -Iinclude

build/island_parser.o: src/island/island_parser.c include/island/island.h include/island/island_parser.h lib/sphragis/sphragis.h | build
	gcc -c src/island/island_parser.c -o build/island_parser.o -Wall -Wextra -Iinclude

build/ithaca.o: src/ithaca/ithaca.c include/ithaca/ithaca.h include/ithaca/ithaca_parser.h | build
	gcc -c src/ithaca/ithaca.c -o build/ithaca.o -Wall -Wextra -Iinclude

build/ithaca_parser.o: src/ithaca/ithaca_parser.c include/ithaca/ithaca_parser.h | build
	gcc -c src/ithaca/ithaca_parser.c -o build/ithaca_parser.o -Wall -Wextra -Iinclude

build/odysseus.o: src/odysseus/odysseus.c include/odysseus/odysseus.h include/odysseus/odysseus_parser.h include/odysseus/odysseus_menu.h include/odysseus/odysseus_command.h include/odysseus/odysseus_handler.h include/utils/utils.h | build
	gcc -c src/odysseus/odysseus.c -o build/odysseus.o -Wall -Wextra -Iinclude

build/odysseus_command.o: src/odysseus/odysseus_command.c include/odysseus/odysseus_command.h include/odysseus/odysseus.h include/odysseus/odysseus_handler.h | build
	gcc -c src/odysseus/odysseus_command.c -o build/odysseus_command.o -Wall -Wextra -Iinclude

build/odysseus_handler.o: src/odysseus/odysseus_handler.c include/odysseus/odysseus_handler.h include/odysseus/odysseus.h | build
	gcc -c src/odysseus/odysseus_handler.c -o build/odysseus_handler.o -Wall -Wextra -Iinclude

build/odysseus_menu.o: src/odysseus/odysseus_menu.c include/odysseus/odysseus_menu.h include/odysseus/odysseus.h | build
	gcc -c src/odysseus/odysseus_menu.c -o build/odysseus_menu.o -Wall -Wextra -Iinclude

build/odysseus_parser.o: src/odysseus/odysseus_parser.c include/odysseus/odysseus_parser.h include/odysseus/odysseus.h | build
	gcc -c src/odysseus/odysseus_parser.c -o build/odysseus_parser.o -Wall -Wextra -Iinclude

.PHONY: clean
clean:
	rm -f build/*.o
	rm -f island ithaca odysseus