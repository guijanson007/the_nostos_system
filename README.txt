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


4. NETWORK AND PORTS
--------------------
Every process is configured only through its .dat file (IP and port); nothing
is hardcoded, and nothing assumes that two processes share a machine.

Lab layout (Global Testing table; each server has its own port per machine):

  Machine                     Process      Port   Config
  Montserrat 172.16.214.21    Ithaca       8625   ithaca.dat
                              Thrinacia    8626   thrinacia.dat
                              Polyphemus          odysseus.dat
                              Eumaeus             eumaeus.dat
  Matagalls  172.16.214.20    Aeaea        8625   aeaea.dat
                              Aeolia       8626   aeolia.dat
                              Telemachus          telemachus.dat
  Puigpedros 172.16.214.18    Scheria      8625   scheria.dat
                              Ogygia       8626   ogygia.dat
                              Ismarus      8627   ismarus.dat
                              Penelope            penelope.dat

- Ithaca and Aeaea are on different machines, as required.
- Capacities: 1 berth on every island except Scheria (2).
- Direct routes: Aeaea-Scheria, Aeaea-Thrinacia, Aeolia-Scheria,
  Scheria-Ismarus, Thrinacia-Ogygia. Every ship connects to Ithaca on
  Montserrat and starts at Aeaea.
- A port is only unique per machine, so servers on different machines reuse
  8625-8627.
- Each server listens on the IP and port of its own .dat file, so it must be
  started on the machine whose IP it lists (or use 0.0.0.0 to listen anywhere).
- Odysseus connects with plain IP addresses (no host names).
- Every process uses its own folder (line 2 of its config) and every island
  its own stocks/<Island>.db.
