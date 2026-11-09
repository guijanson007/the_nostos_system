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

Lab layout (all machines are in 172.16.214.0/24, ports 8625-8629):

  Machine          Process     Port
  172.16.214.18    Ithaca      8625
  172.16.214.18    Aeolia      8628
  172.16.214.20    Aeaea       8626
  172.16.214.20    Scheria     8627
  172.16.214.21    Ismarus     8629
  172.16.214.21    Thrinacia   8625
  172.16.214.21    Ogygia      8626

- Ithaca and Aeaea are on different machines, as required.
- There are 7 servers and only 5 ports. A port is only unique per machine, so
  Thrinacia and Ogygia reuse 8625 and 8626 on a machine where they are free.
- Each server listens on the IP and port of its own .dat file, so it must be
  started on the machine whose IP it lists (or use 0.0.0.0 to listen anywhere).
- Odysseus connects with plain IP addresses (no host names).
