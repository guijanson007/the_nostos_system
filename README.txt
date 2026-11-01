THE NOSTOS SYSTEM

Authors:
  Axel Boleda (a.boleda)
  Guilherme Rodrigues (gj.rodriguesferreira)


1. COMPILE
From the root folder of the project:

  make            Compiles ithaca, island and odysseus


2. EXECUTE
----------
Start Ithaca first, then the islands, and then Odysseus.
Each process runs in its own terminal, from the root folder of the project.

  Ithaca:
    ./ithaca ithaca.dat voyages.dat

  Island:
    ./island <island>.dat stocks/<Island>.db
    Example: ./island aeaea.dat stocks/Aeaea.db
    (the stock file must be inside stocks/ and have the same name as the island config file)

  Odysseus:
    ./odysseus <odysseus>.dat
    Example: ./odysseus odysseus.dat

To stop any process, press CTRL+C.


3. EXECUTE WITH VALGRIND
------------------------
Same arguments, with valgrind in front:

  valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes ./ithaca ithaca.dat voyages.dat

  valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes ./island aeaea.dat stocks/Aeaea.db

  valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes ./odysseus odysseus.dat

Stop the process with CTRL+C to see the valgrind summary.
