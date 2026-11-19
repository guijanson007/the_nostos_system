# Code audit: Phase 3 (2026-11-19)

Audit of the `phase3` branch at commit `8aaa988` ("fix: Do not close the sockets of the island twice on exit"), done before the final report. Every finding below was fixed in small commits on top of that commit (§7 maps each commit to the findings it fixes), except the ones listed in §6.

## How the audit was done

- **Four independent read-only reviews**, one per area:
  - `odysseus`;
  - `ithaca`;
  - `island`;
  - the shared `src/utils` code. It is linked by all three processes, so it was reviewed once.
- **What each review read.** `CLAUDE.md` (project rules) and `nostos_system_spec.md` (the statement). It then went through every source file line by line looking for:
  1. constructs forbidden by the statement or the project rules: brace initialization, `calloc`, ternary operator, non-`sig_atomic_t` globals, magic numbers, VLAs, `printf` family, `system`/`popen`/`stat`, `goto`, `_exit`/`abort`, own function pointers;
  2. memory leaks (every allocation and every error path);
  3. file descriptor, process and System V IPC leaks;
  4. differences with the statement (protocol frames, commands, log messages, file locations);
  5. concurrency and general correctness bugs.
- **Checking.** The most important findings were checked again by hand against the code, and the forbidden constructs were counted with `grep`.
- **Build.** The build (`-Wall -Wextra -std=c99`) was warning-free before the audit and still is after the fixes.

## 1. Forbidden constructs

| # | Rule | Where | Fix |
|---|------|-------|-----|
| F1 | Brace initialization `= {0}` (31 places) | `odysseus_conversation.c` (15), `odysseus.c`, `odysseus_parser.c`, `ithaca.c` (3), `island.c` (3), `island_conversetion.c` (3), `island_parser.c`, `utils_communication.c` (3) | Variable declared, then `memset(&x, 0, sizeof(x))` (`FD_ZERO` for the `fd_set`) |
| F2 | `calloc` (12 places) | `odysseus.c`, `odysseus_parser.c`, `odysseus_conversation.c` (4), `odysseus_map.c` (2), `utils_communication.c` (2), `utils_pool.c` (2) | `malloc` + check + `memset` |
| F3 | Ternary operator `?:` (9 places) | `odysseus_command.c`, `odysseus_conversation.c`, `utils_semaphore.c` (3), `utils_transfer.c` (4) | `if`/`else` |
| F4 | Global quit flag declared `volatile int` | `ithaca.c`, `island.c` | `volatile sig_atomic_t` (the only global the statement allows) |
| F5 | Magic numbers | raw `-1/-2/-3` results of `receiveMessage` compared in Ithaca and the island; pipe ends `[0]/[1]`, `"md5sum"` and `"/dev/null"` in `utils_md5.c`; literal `'/'` in Ithaca | Named `#define`s (`RECEIVE_OK/ERROR/BAD_LENGTH/BAD_FLAGS`, `MD5_PIPE_*`, `MD5_COMMAND`, `PATH_SEPARATOR`) |

After the fixes, `grep` finds no `= {0}`, `calloc` or `?:` in `src/` or `include/`.

The audit found none of these anywhere: VLAs, `printf`/`scanf`/`puts`/`gets`, `goto`, `system`/`popen`/`stat`, `_exit`/`abort`, `vasprintf`, or function pointers of our own. The fixed `char[]` buffers that exist are the protocol frame and transfer buffers, whose size comes from the statement.

## 2. Bugs in the shared utils (they affect the three processes)

| # | Sev. | Where | Problem | Fix |
|---|------|-------|---------|-----|
| U1 | High | `utils_transfer.c` `waitFrame` | The "bad length" and "reserved flag bits" results of `receiveMessage` were ignored, and the type of a frame that was never filled (stale or uninitialized) was checked. A malformed DATA frame could make the previous chunk be written to the file again. | Any result other than OK is an error; the frame is zeroed before each read |
| U2 | Medium | `utils_communication.c` `readFull` | Once the first byte of a frame arrived, the rest was read with no time limit. A peer that sent half a frame froze the whole `select()` loop of Ithaca or an island (SIGINT could not stop it, because `read` is restarted) or held a pool worker forever. | The rest of a frame must arrive within `FRAME_READ_TIMEOUT_SECONDS` (5 s), otherwise the frame fails and the client is dropped. Waiting for the first byte is still unlimited. |
| U3 | Medium | `utils_md5.c` `computeMd5` | The pipe was created without close-on-exec. Concurrent `md5sum` children inherited the other workers' pipe ends (delaying their end of file) and the server sockets. | `pipe2(..., O_CLOEXEC)` |
| U4 | Low | `utils.c` `readUntil` | Unchecked `malloc`, and `p = realloc(p, ...)` lost the buffer on failure. The function was not used anywhere. | Removed |
| U5 | Low | `utils_pool.c` `poolSubmit` | If `semSignal` failed after the job was queued, `POOL_ERROR` was returned and the caller freed a job that a worker could still run. | The job is taken out of the queue before the error is returned |
| U6 | Low | `utils_communication.c` `readFull`/`writeFull` | No retry on `EINTR`. | Retry on `EINTR` |
| U7 | Low | `utils_md5.c`, child after a failed `exec` | `free()` in the child of a multithreaded process (not async-signal-safe). | The child only calls `exit` |

## 3. Bugs in Odysseus

| # | Sev. | Where | Problem | Fix |
|---|------|-------|---------|-----|
| O1 | High | `odysseus_cargo.c` `consumeFood` + `odysseus_persistence.c` `decodeState` | A product used up while waiting offshore stayed in the cargo with 0 kg. The state file saved it, and loading rejects 0 kg slots, so at the next start the whole saved state (gold, cargo) was discarded. STATUS also showed `0 kg` lines. | Products that reach 0 kg are removed from the cargo |
| O2 | Medium | `odysseus_conversation.c`, `odysseus_persistence.c` | The ship folder (line 2 of `odysseus.dat`, statement §3.1) was never used. `.object`, `.signed` and `.state` were written in the current directory. | The files are created inside the ship folder, which is created at start-up if it is missing |
| O3 | Medium | `odysseus_handler.c` `odysseusDeliver`, `odysseusSell` | After an I/O or protocol error (or a transfer that died half way) the island socket stayed open, so a late frame from the island was read as the answer to the next command. | The island connection is closed on those errors, as `BUY` already did |
| O4 | Medium | `odysseus_conversation.c` `receiveSigned` | When the island cannot seal the object, it answers `DELIVER` with an error instead of the SIGNED transfer. Odysseus took that frame as an unexpected one, lost the reason and waited one more second for a frame that never came. | Odysseus reads the first frame itself (new `receiveFileFromHeader` in utils) and shows the island's reason |
| O5 | Low | `odysseus_command.c` | The first argument of every command was uppercased (`SELL DRIEDFISH 2`, "We can't sail to AEOLIA"). | Only the keywords (`ITHACA`, `VOYAGES`, `MARKET`) are compared ignoring case; the other arguments are kept as typed |
| O6 | Low | `odysseus.c` `isOdysseusValid` | Only the first supply was checked. A missing supply line, or a failed allocation of a later supply, started the ship with `NULL` product names. | Every supply must have a name and a positive amount; an invalid file is reported |
| O7 | Low | `odysseus_menu.c` | On a read error the terminal was left in bold. | Input style reset on every path |
| O8 | Low | `odysseus_conversation.c`, market/trade parsing | Numbers from the island were parsed with `atoi` without a length bound. | Checked with the new `isBoundedNumber` (max 9 digits, accepts 0) |

## 4. Bugs in Ithaca

| # | Sev. | Where | Problem | Fix |
|---|------|-------|---------|-----|
| I1 | Medium | `ithaca.c` `checkArgcIthaca` | The arguments had to be exactly `ithaca.dat` and `voyages.dat`, so `./ithaca ithaca.local.dat voyages.dat` or any path was refused. Statement §2 says `<config.dat> <voyages.dat>`. | Only the number of arguments is checked |
| I2 | Medium | `ithaca_conversetion.c` `claimRequest`/`claimHeader` | A refused CLAIM got two error frames: `0x04` for the request and `0x22` for the file HEADER that Odysseus sends right after it. The second frame was read as the answer to the next command. | The refusal is stored and answered once, on the READY of the HEADER, which is what Odysseus waits for |
| I3 | Medium | `ithaca_conversetion.c` ACCEPT | `ACCEPT OK` was sent before the transfer job existed, so with a full pool the ship was disconnected right after its OK. | The job is created and the pool checked for room (new `poolHasRoom`) before answering. With a full pool, ACCEPT answers "Ithaca is busy" and the voyage stays available. The select loop is the only one submitting jobs, so the room is still there when the job is submitted. |
| I4 | Low | `ithaca_conversetion.c` `assignVoyage` | If the reply could not be built, the voyage stayed assigned to the client. | The reply is built before the voyage is touched |
| I5 | Low | `ithaca_conversetion.c` `finishClaim` | The voyage was marked COMPLETED before the reply was built. | Reply built first |
| I6 | Low | `ithaca_conversetion.c` `sendAvailableVoyages` | If a list line could not be built, the client waited forever for the missing frames. | Error frame sent |
| I7 | Low | `ithaca_conversetion.c` `claimRequest` | The voyage ID in the CLAIM payload was not checked. | It must match the active voyage of the ship |
| I8 | Low | `ithaca.c` | "Waiting for Odysseus..." was printed on every loop pass, and several messages printed an extra blank line. | Printed once each time the last ship leaves; extra `\n` removed |
| I9 | Low | `ithaca_conversetion.c` | The server log lacked the CLAIM lines of statement §7.4. | Added: "X claims voyage N", "Receiving stamped ...", "Object received correctly", "Glyph of ... verified", "Reward granted: N gold", "Sending ... to X..." |

## 5. Bugs in the island

| # | Sev. | Where | Problem | Fix |
|---|------|-------|---------|-----|
| S1 | Medium | `island.c` `loadData` / `freeIsland` | On a bad config file, `freeIsland` zeroed the struct and the stock fd became 0. `close(0)` then closed **stdin**, and the real stock fd leaked (confirmed with `valgrind --track-fds`). | `freeIsland` keeps the stock fd; `closeFiles` closes it |
| S2 | Medium | `island.c` `startServer` | If the welcome message could not be built, the pool was not destroyed: 4 threads, 2 System V semaphores and a pipe leaked. | `poolDestroy` on that path |
| S3 | Low | `island.c` `startServer` | `nMaxFd` was set from the notify pipe and then overwritten, so the pipe was not watched until a client connected. | Order fixed |
| S4 | Low | `island_conversetion.c` BUY/SELL | Amounts of up to 9 digits are accepted, and `stock + amount` and `amount * price` could overflow `int`. | Refused with an error if the result does not fit |
| S5 | Low | `island_conversetion.c` `completeTrade` | If writing `stock.db` failed, the change stayed in memory and OK was sent. | Change undone and an error sent |
| S6 | Low | `island_conversetion.c` `listMarket` | If a line could not be built, the client waited forever for the missing frames. | Error frame sent |
| S7 | Low | `island_conversetion.c` | Delivery temporary files were written in the current directory, and the island folder (line 2 of the config, §3.4) was unused. | Temporary files go to the island folder, which is created at start-up |
| S8 | Low | `island_conversetion.c` `deliverRequest` | The `<VoyageID>&<Object>` payload of DELIVER was not checked, and the delivery log lines of §7.4 were missing. | Payload validated; log lines added |
| S9 | Low | `island.c` `acceptClient` | A descriptor at or past `FD_SETSIZE` would be passed to `FD_SET` (undefined behaviour). | Such connections are closed |

## 6. Left open (with reason)

- **A half-sent frame still pauses the server loop for up to 5 s.** After U2, a client that sends part of a frame and stops no longer blocks Ithaca or an island for good. The `select()` loop still waits up to `FRAME_READ_TIMEOUT_SECONDS` for the rest before dropping that client, and the other ships are not served meanwhile. Removing this pause completely would need a per-client buffer for partial frames in the select loop. Well-behaved clients always send whole frames with one `write`, so the pause only happens with a broken or hostile peer.
- **Odysseus blocks waiting for a server reply that never comes**, and SIGINT does not interrupt it. The servers answer at once in practice, and since U2 they can no longer hang mid-frame.
- **A SIGINT that arrives between the quit-flag check and `select()`** is only seen at the next event. `pselect` would close the gap; it is not a leak, because the server still exits cleanly.
- **`semWait` does not retry on `EINTR`.** Workers block all signals, so only the main thread is affected, and there an interrupted wait means the server is closing.
- **When Ithaca refuses a CLAIM transfer, Odysseus shows a generic "could not be transferred" message.** `sendFile` does not return the reason of a refused READY. Odysseus checks the same conditions before sending CLAIM, so this only happens if Ithaca lost the voyage (for example after a restart).
- **Style:**
  - some headers have the wrong `@File` name;
  - some identifiers have typos (`diconected`, `ALREDY`, `arravalIsland`, `choseOption`);
  - some error messages still end in `\n` while being printed with `printErr`.

## 7. Commits

Fixes on top of `8aaa988`, oldest first:

1. `fix: Time out half-received frames and name the receive results`: U2, U6, F1/F2/F5 in `utils_communication.c`
2. `fix: Treat malformed frames as errors in waitFrame`: U1, F3 in `utils_transfer.c`
3. `Replace the ternary operators of the semaphore wrapper with if/else`: F3
4. `fix: Use malloc in the pool and unqueue a job whose wake-up failed`: U5, F2
5. `fix: Keep the md5sum pipe out of other children`: U3, U7, F5
6. `Remove the unused readUntil`: U4
7. `Add poolHasRoom to check the queue before promising a transfer`
8. `fix: Accept any config file names in Ithaca and clean up its main loop`: I1, I8, F1, F4
9. `fix: Answer a refused CLAIM once and promise ACCEPT only with room in the pool`: I2–I7, I9
10. `fix: Stop the island from closing stdin on a bad config file`: S1–S3, S9, F1, F4
11. `fix: Keep the island stock consistent and check DELIVER requests`: S4–S8, F1
12. `Remove brace initialization, calloc and ternaries from Odysseus`: F1–F3
13. `fix: Remove used-up products so the saved state stays loadable`: O1
14. `fix: Keep the ship files in the folder of its config file`: O2
15. `Add receiveFileFromHeader for receivers that read the HEADER themselves`
16. `fix: Show the island's sealing error and drop a desynced island connection`: O3, O4
17. `fix: Keep the case of command arguments as typed`: O5
18. `fix: Check every supply of the ship config file`: O6
19. `Reset the terminal style when reading a command fails`: O7
20. `fix: Bound the numbers Odysseus reads from the island before atoi`: O8

## 8. Verification

- `make` is warning-free after every commit.
- `tests/load_test.py` passes all 17 scenarios. These include the two valgrind runs (no leaks, no open descriptors) and its check that no System V semaphores are left behind. The local copy of the script was updated for O2 and S7, because the ship and island files moved into their folders:
  - `<Ship>_files/<Ship>.object` / `.signed`;
  - `.delivery_*` inside the island folder.
- **Extra checks done by hand:**
  - **O1:** a ship waiting offshore until a product runs out restarts with its saved state; before the fix, the state was rejected.
  - **O4:** with an object the island cannot seal, `DELIVER` shows "ERROR: The object could not be sealed", and `LIST MARKET` works right after.
  - **O5:** `sell barley 5` and `SAIL Atlantis` keep the typed names.
  - **U2:** a client that sends 6 bytes of a frame and stops is dropped after 5 s, and Ithaca keeps serving other ships. Before the fix, Ithaca blocked for good.
  - **S1:** an island with a bad config exits with only the three standard descriptors open. The old binary closed stdin and leaked the stock file.

## 9. Checked and found correct

- **Worker pool:**
  - ring buffer indices and full/empty handling;
  - shutdown (queued jobs finish first);
  - destruction of a half-built pool;
  - both semaphores destroyed and the notify pipe closed once.
- **Semaphores:** the library is included from one file only, only private semaphores are used, and `ipcs -s` is clean after runs.
- **Frames:** always serialized into one 256-byte buffer and sent with one `write`. Unused data bytes are zeroed, and received data is NUL-terminated.
- **File transfer:**
  - frame types and flags match §8.4;
  - MD5 compared ignoring case;
  - partial files deleted on failure;
  - the `md5sum` child is always waited for (no zombies).
- **Island berths and FIFO queue:**
  - the docked count goes down exactly once;
  - waiting ships that leave are removed from the queue;
  - ships are docked in arrival order.
- **Odysseus:**
  - state checks for every command;
  - waiting offshore uses `select` with a timeout (no busy waiting);
  - every `realloc` goes through a temporary pointer.
- **SIGPIPE** is ignored in the three processes.
