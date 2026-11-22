# Lab runbook: final validation on three machines

This runbook walks through every row of the four test PDFs (Phase 1, Phase 2, Phase 3, Global) on the real lab layout. `tests/lab/lab.sh` writes the configuration files and starts each process from its own folder.

Machine tags used below: **[MO]** Montserrat, **[MA]** Matagalls, **[PU]** Puigpedros.

| Machine | Servers (port) | Ships |
| --- | --- | --- |
| Montserrat | Ithaca (8625), Thrinacia cap 1 (8626) | Polyphemus, Eumaeus |
| Matagalls | Aeaea cap 1 (8625), Aeolia cap 1 (8626) | Telemachus |
| Puigpedros | Scheria cap 2 (8625), Ogygia cap 1 (8626), Ismarus cap 1 (8627) | Penelope |

Direct routes (bidirectional, only these): Aeaea–Scheria, Aeaea–Thrinacia, Thrinacia–Ogygia, Scheria–Aeolia, Scheria–Ismarus. Every ship starts with 250 gold, Barley 40, DriedFigs 60 (100 kg).

Write `L=tests/lab/lab.sh` in every shell (all commands are run from the repo root).

---

## 0. Preparation (once)

1. **Code** (each machine, or once if `$HOME` is shared over NFS):
   ```
   git checkout phase3 && git pull && git log -1 --oneline
   make rebuild 2>&1 | tee build.log; grep -ci warning build.log     # must print 0
   ```
2. **Tools**: `which python3 valgrind tmux md5sum ipcs`. python3 is needed by `lab.sh stock`, by the `scheria_race` stock and by `load_test.py`.
3. **IPs**: on each machine run `hostname -I` (or `getent hosts montserrat matagalls puigpedros`). Never use `localhost`, `127.*` or `0.0.0.0`.
4. **Ports**: if 8625–8627 are not the ones assigned to you, override them on every machine with the same values, for example `PORT_ITHACA=8629 $L setup ...`.
5. **Setup** (each machine, same arguments; if `$HOME` is shared, once is enough):
   ```
   $L setup <montserrat_ip> <matagalls_ip> <puigpedros_ip>
   ```
   This writes `~/nostos_lab/<machine>/<process>/` (one folder per process, each with its own config, `stocks/<Island>.db` copy, ship folder, and Ithaca's own `objects/` copy). `$L list` shows the layout.
6. **Start the servers**, one terminal each, in this order. Use tmux: `tmux new -s nostos`, then `Ctrl-b c` for a new window, or `Ctrl-b %` / `Ctrl-b "` to split.
   - [MO] `$L run ithaca`
   - [MO] `$L run thrinacia`
   - [MA] `$L run aeaea`
   - [MA] `$L run aeolia`
   - [PU] `$L run scheria`
   - [PU] `$L run ogygia`
   - [PU] `$L run ismarus`

   Check:
   - Ithaca says **24 voyages**.
   - Each island says **8 products available**.
   - Each island shows the capacity from the table.
   - Sea routes loaded: Aeaea 2, Aeolia 1, Scheria 3, Thrinacia 2, Ogygia 1, Ismarus 1.
7. **Ships**: `$L run polyphemus` [MO], `$L run eumaeus` [MO], `$L run telemachus` [MA], `$L run penelope` [PU].

### Rules that apply to every row

- **Reset between independent rows.** Stop the processes involved with CTRL+C, then run `$L reset` (everything) or `$L reset <process ...>`. This restores stock.db, ship files and kept images. **Do not reset** inside a persistence row; the PDFs require reusing the modified files.
- **IDs are not fixed.** Always run `LIST VOYAGES` in the same session before `ACCEPT <id>`. Ithaca only accepts IDs from your last listing.
- **Aeaea has one berth.** A second ship arriving while one is docked waits, and waiting **eats 7 kg every 5 s**. For exact numbers, move ships through Aeaea one at a time: the first one leaves (`BUY MAP 1`, then `SAIL Scheria` or `SAIL Thrinacia`) before the next arrives.
- **Simultaneous commands**: put two ships of the same machine (Polyphemus + Eumaeus on [MO]) in two tmux panes. Turn on `Ctrl-b :setw synchronize-panes on`, type the command once, and press Enter. Turn it off afterwards.
- **Stopping a server** means CTRL+C (clean) or `kill -9 <pid>` (crash). Do **not** use Ctrl+Z: a stopped-but-alive peer is a known open issue (a client can block waiting for a reply that never comes).
- **`kill -9` leaks semaphores.** `kill -9` of a server leaves its System V semaphores behind. This is expected, because no handler runs. Check with `$L check`, remove them with `$L ipcclean`. After **clean** runs, `ipcs -s` must be empty for your user.
- **Useful checks**:
  - `$L stock <island>` prints the stock.db.
  - `$L files <process>` lists the files with their md5sum, plus any `.tmp`/hidden file, which must be none.
  - `$L check` shows semaphores, zombies, RSS/CPU/fds/threads of each process.
- **Valgrind**: start any process with `VALGRIND=1 $L run <process>`.

---

## 1. Automated pass (one lab machine, ~4 min)

These scripted scenarios cover the fault-injection rows that cannot be typed by hand: bad TYPE/FLAGS/LENGTH frames, wrong size/MD5, a byte flipped with the right MD5, connections cut in the middle of each transfer type, and a slow client. They run on the real lab compiler and kernel, using the machine's real IP.

```
python3 tests/load_test.py 2>&1 | tee load_test.log
ipcs -s            # nothing of yours left
```
Record the final line (passed/failed per scenario). If one fails, rerun it alone with `python3 tests/load_test.py <name> --keep` and keep the temp folder.

Rows covered here **and** by the manual steps below: frames, badmd5, badsize, flipbyte, cutoff, peerdies, badobject, fileerrors, slowclient, cycles, static, valgrind, valgrind_faults.

---

## 2. Phase 1

> In the final version, valid commands **execute**. `Command OK` was Phase-1-only, so it must **not** appear. That is the expected result, not a failure.

| # | Steps | Expected | Result |
| --- | --- | --- | --- |
| 1.1 | Start everything (§0). | Ithaca: 24 voyages. Every island: 8 products. Only Odysseus shows a `$ ` prompt. | ☐ |
| 1.2 | [MO] Polyphemus, in this order: `CONNECT ITHACA`, `LIST VOYAGES`, `ACCEPT <id>`, `SAIL Aeaea`, `MAP`, `LIST MARKET`, `BUY DriedFish 3`, `SELL Barley 10`, `STATUS`, `DELIVER`, `CLAIM`. | All 11 commands are recognised and executed (or refused because of the state). None prints `Unknown command` or a usage line. | ☐ |
| 1.3 | `cOnNeCt iThAcA`, `sTaTuS`. | Executed as normal. | ☐ |
| 1.4 | `ACCEPT` / `BUY DriedFish` / `SELL Barley` / `MAP now` / `SAIL` / `something else`. | `Usage: ACCEPT <voyage_id>` / `Usage: BUY <product> <amount>` / `Usage: SELL <product> <amount>` / `Usage: MAP` / `Usage: SAIL <island>` / `Unknown command`. | ☐ |
| 1.5 | CTRL+C on the ship. Then, with `VALGRIND=1 $L run polyphemus`, repeat a few commands and CTRL+C. | Clean exit. Valgrind: `All heap blocks were freed`, 0 errors, only fds 0-2 open. | ☐ |

---

## 3. Phase 2

Reset before each row (`$L reset`, then restart the servers).

**2.1 Connect and list voyages**
- [MO] Polyphemus: `LIST VOYAGES`, `ACCEPT 1`. Expected: both refused (`not connected`).
- `CONNECT ITHACA`, `LIST VOYAGES`, then `ACCEPT` an **even** ID. Expected: assigned, and the object is received (Phase 3 behaviour).
- CTRL+C, `$L reset polyphemus`, restart Ithaca. Repeat with an **odd** ID.
- Expected: only available voyages are listed.
- ☐

**2.2 Concurrent acceptance**
- [MO] Polyphemus and Eumaeus in synchronized panes: `CONNECT ITHACA`, `LIST VOYAGES`, then the same `ACCEPT <id>` simultaneously.
- [MA] Telemachus: `CONNECT ITHACA`, `LIST VOYAGES`.
- The winner then tries `LIST VOYAGES` and `ACCEPT <other id>`.
- Expected:
  - Only one ship gets the voyage and the OBJECT; the other gets an `ERROR`.
  - Telemachus does not see the ID.
  - The winner is refused: `active voyage`.
  - The Ithaca log shows one `assigned to` line.
- ☐

**2.3 Leave Ithaca and reach the destination**
- [MO] Polyphemus: `CONNECT ITHACA`, `LIST VOYAGES`, `ACCEPT <TridentOfPoseidon id>`.
- `SAIL Scheria`. Expected: `We can't sail to Scheria from here...`.
- `SAIL Thrinacia` / `SAIL Ogygia`. Expected: also refused.
- `SAIL Aeaea`, `BUY MAP 1`, `SAIL Scheria`, `STATUS`.
- Expected: the mission is still `TridentOfPoseidon -> Scheria`, location Scheria, gold 200, object on board.
- ☐

**2.4 Full port and FIFO queue** (Aeaea capacity 1)
- Ship A = Polyphemus [MO]. It runs `CONNECT ITHACA`, `SAIL Aeaea`, and docks.
- Ship B = Telemachus [MA]. It runs `CONNECT ITHACA`, `SAIL Aeaea`. Expected: `The harbor is full. We drop anchor and wait.`
- Ship C = Penelope [PU], a few seconds later: the same commands and the same wait.
- A: `BUY MAP 1`, `SAIL Scheria`. Expected: B (not C) gets `granted access`.
- B: `BUY MAP 1`, `SAIL Scheria`. Expected: C docks.
- The Aeaea log shows DOCKED in order B, then C. There is never more than 1 ship docked.
- ☐

**2.5 Provisions and death at sea**
- (a) Waiting timing:
  - Polyphemus docks at Aeaea.
  - Telemachus `SAIL Aeaea` (waits). Start a stopwatch at the `harbor is full` line.
  - `STATUS` at 4 s: 100 kg. At 5 s it prints `Provisions remaining: 93 kg`, at 10 s `86 kg`.
  - A ship sailing or docked keeps its kg (`STATUS` on Polyphemus shows 100).
- (b) Death at sea: see **G.12** below (9 kg ship).
- ☐

**2.6 Commands while waiting and disconnection**
- While Telemachus waits (as in 2.5):
  - `STATUS` and `MAP`. Expected: allowed; MAP draws the chart.
  - `LIST MARKET`, `BUY DriedFish 1`, `SELL Barley 1`. Expected: `ERROR: You are not docked yet. Only STATUS and MAP are available`.
- Queue Penelope too. Then **kill -9** Telemachus (queued).
- Then CTRL+C Polyphemus (docked).
- Expected:
  - Aeaea logs both departures.
  - Penelope gets DOCKED. The server keeps running.
  - `$L check` on [MA]: island CPU ~0 % while idle (no busy waiting).
- ☐

**2.7 Market, trade and map** (fresh reset; Polyphemus alone)
- `CONNECT ITHACA`, `SAIL Aeaea`, `LIST MARKET`.
  - Expected: 8 foods plus MAP at 50. DriedFish: 90 kg at 15.
- `BUY DriedFish 5`, `STATUS`. Expected: 175.
- `SELL DriedFish 2`, `STATUS`. Expected: 205.
- `BUY MAP 1`, `STATUS`. Expected: 155.
- `BUY MAP 1`, `STATUS`. Expected: 105.
- Expected at the end:
  - 3 kg DriedFish aboard, provisions 103 kg, `MAP` draws.
  - `LIST MARKET`: DriedFish 87.
- CTRL+C Aeaea, then `$L stock aeaea`. Expected: DriedFish 87, 8 records, no MAP record.
- ☐

**2.8 Errors and concurrent purchase**
- (a) At Aeaea, note `STATUS` + `LIST MARKET`, then try each of:
  - `BUY Unicorn 1`
  - `BUY DriedFish 0`
  - `BUY DriedFish -1`
  - `BUY DriedFish 1000`
  - `SELL WildHoney 1`
  - `SELL Barley 1000`
  - `BUY MAP 2`
  - `SELL MAP 1`
  - `BUY EnchantedHerbs 20` (440 gold: insufficient funds)

  Insufficient **stock** costs more than 250 gold at every product, so it is checked by the `market` scenario in §1.

  Expected: all refused, and `STATUS` and `LIST MARKET` are unchanged.
- (b) Concurrent purchase:
  - [PU] stop Scheria, then `$L run scheria_race` (DriedFish = 5 kg).
  - [MO] Polyphemus: `CONNECT ITHACA`, `SAIL Aeaea`, `BUY MAP 1`, `SAIL Scheria`.
  - Then Eumaeus does the same (after Polyphemus left Aeaea).
  - Synchronized panes: `BUY DriedFish 4`.
  - Expected: exactly one `Bought 4 kg`, and `LIST MARKET` shows 1.
  - CTRL+C scheria_race, then `$L stock scheria_race`. Expected: DriedFish 1.
- ☐

**2.9 Maps and direct routes**
- (a) Polyphemus: `CONNECT ITHACA`, `SAIL Aeaea`, `BUY MAP 1`, `SAIL Scheria`, `BUY MAP 1`, `SAIL Aeolia`.
  - From Aeolia: `SAIL Ismarus`. Expected: `We can't sail to Ismarus from here...`.
  - `SAIL Scheria`. Expected: OK.
- (b) [MA] stop Aeaea, then `$L run aeaea_filter` (config lists Aeolia too).
  - Expected: the log says **2 sea routes loaded** (SPHRAGIS dropped Aeaea–Aeolia).
  - A ship buys the Aeaea map. `MAP` shows no Aeaea–Aeolia link, and `SAIL Aeolia` from Aeaea is refused.
- ☐

**2.10 Frames and server errors**
- Format errors (NACK 0x7F/0xC0, operation errors TYPE/0xC0) are verified by the `frames` scenario in §1.
- Manually:
  - (a) With Polyphemus connected to Ithaca, CTRL+C Ithaca. Then `LIST VOYAGES`. Expected: an error, no hang, the ship stays alive.
  - (b) Docked at Aeaea, `kill -9` Aeaea. Then `LIST MARKET`. Expected: an error, no hang.
  - (c) Make a copy of polyphemus.dat with a port where nothing listens (e.g. 8999), `CONNECT ITHACA`. Expected: a connection error, the prompt returns.
  - (d) CTRL+C each process. Expected: clean exit, and `$L check` shows no semaphores (except after kill -9; run `$L ipcclean`).
- ☐

---

## 4. Phase 3

**3.1–3.3 Full mission** (fresh reset; run in order; Polyphemus alone)

1. `CONNECT ITHACA`, `LIST VOYAGES`, `ACCEPT <TridentOfPoseidon id>`.
   - Expected: `Object received correctly`.
   - [MO] `md5sum ~/nostos_lab/montserrat/polyphemus/polyphemus_files/Polyphemus.object ~/nostos_lab/montserrat/ithaca/objects/trident_of_poseidon.png`. Expected: equal.
2. `SAIL Aeaea`, `BUY MAP 1`, `SAIL Scheria`, `STATUS`. Expected: mission active.
3. `DELIVER`. Expected order:
   - `Object transferred correctly`
   - `Stamped object received`
   - `Delivery completed`
   - `The mark of Scheria has been obtained`

   The Scheria log shows `sealed with the glyph of Scheria`.
4. `SAIL Ithaca`. Expected: `Returning directly to Ithaca...`. It does not connect yet; `CLAIM` here says `not connected`.
5. `CONNECT ITHACA`, `CLAIM`.
   - Expected: `Glyph of Scheria verified`, `Reward received: 1500 gold`.
   - `STATUS`: **1700 gold, 100 kg, Current voyage: None**.
6. `CLAIM` again. Expected: an `ERROR`, gold stays 1700.
7. `$L files ithaca` and `$L files scheria`:
   - Kept images: `Polyphemus_<id>_trident_of_poseidon.png` (+ `.signed` on Scheria).
   - No `.tmp` files.

☐

**3.4 Delivery or claim in the wrong state**
- No mission: `DELIVER`. Expected: ERROR.
- With a mission:
  - At Ithaca, `DELIVER`. Expected: ERROR.
  - At Aeaea (not the destination), `DELIVER`. Expected: `not the destination`.
  - While waiting at Aeaea (another ship docked), `DELIVER`. Expected: `not docked yet`.
- Before delivering, back at Ithaca, `CLAIM`. Expected: refused.
- After `SAIL Ithaca` without `CONNECT ITHACA`, `CLAIM`. Expected: `not connected`.
- In all cases the gold is unchanged and the Ithaca log shows no `completed`.
- ☐

**3.5 Wrong object or glyph** (works because CLAIM sends `<Ship>.signed` from disk)
- Get a mission delivered (any Scheria voyage). Then `SAIL Ithaca` and CTRL+C the ship. The receipt is persisted.
- Overwrite `~/nostos_lab/montserrat/polyphemus/polyphemus_files/Polyphemus.signed` with:
  - (a) **No glyph**: `cp Polyphemus.object Polyphemus.signed`.
  - (b) **Wrong island glyph**: Eumaeus completes a delivery to another island (e.g. a Thrinacia voyage). Copy `eumaeus_files/Eumaeus.signed` over it.
  - (c) **Another mission's sealed image**: Eumaeus delivers a different Scheria voyage. Copy its `.signed` over it.
- Each time: restart Polyphemus, `CONNECT ITHACA`, `CLAIM`.
- Expected:
  - `Object transferred correctly`, then `ERROR: The claim failed`.
  - No reward; `STATUS` still shows the mission.
  - No kept `Polyphemus_<id>_*` image appears in Ithaca's objects.
- ☐

**3.6 File transfer protocol**
- Covered by §1: every frame read is checked for 256 bytes, DATA_LENGTH ≤ 253, zero padding and FLAGS.
- Manually: the size and md5sum of `.object` and `.signed` match `md5sum` of the sources.
- Code check: `grep -n execlp src/utils/utils_md5.c` shows `md5sum` is executed with `fork`/`exec`, with no MD5 implementation of our own.
- ☐

**3.7 Corrupt or interrupted transfer**
- SIZE_ERROR/MD5_ERROR and corruption are covered by `badmd5`, `badsize`, `flipbyte` and `badobject` (§1).
- Manually, cut a connection during each transfer type with `kill -9`:
  - OBJECT: the ship during ACCEPT. Expected: the voyage is free again, and Ithaca logs `did not receive the object`.
  - DELIVERY: the ship during DELIVER.
  - SIGNED: Scheria during DELIVER.
  - CLAIM: the ship during CLAIM.
- The images are small, so use the largest object or valgrind to slow it down.
- Expected:
  - No partial file is kept (`$L files ...`).
  - No delivery or reward is confirmed.
  - The servers stay up.
- ☐

**3.8 File errors and simultaneous transfers**
- (a) Ithaca missing source:
  - `mv ~/nostos_lab/montserrat/ithaca/objects/trident_of_poseidon.png /tmp/`, then ACCEPT the Trident voyage.
  - Expected: an error, and the voyage stays free. Move the file back.
- (b) Unwritable ship folder:
  - `chmod a-w .../polyphemus_files`, then ACCEPT.
  - Expected: an error, no crash. Run `chmod u+w` afterwards.
- (c) Unwritable island folder:
  - `chmod a-w ~/nostos_lab/puigpedros/scheria/scheria_files`, then DELIVER.
  - Expected: an error reported, and Scheria stays up.
- (d) Polyphemus and Eumaeus each accept a different Scheria voyage and dock at Scheria (capacity 2; pass Aeaea one at a time). Synchronized `DELIVER`, then both `SAIL Ithaca`, `CONNECT ITHACA`, synchronized `CLAIM`.
  - Expected: each gets its own reward.
  - The kept files are `Polyphemus_<id1>_...` and `Eumaeus_<id2>_...`, with nothing mixed.
- ☐

**3.9 Odysseus persistence** (no reset during this row)
- (a) Trade at Aeaea, then `STATUS`. CTRL+C and restart. Expected: the same gold and cargo, and location Ithaca (a new connection is needed).
- (b) `ACCEPT` a voyage, then CTRL+C. Restart and `STATUS`.
  - Expected: the mission is shown, `Polyphemus.object` still exists, and its md5 is unchanged.
  - `CONNECT ITHACA`, `LIST VOYAGES`. Expected: refused (`active voyage`).
- (c) Deliver, then CTRL+C. Restart and `STATUS`.
  - Expected: the mission is shown and `Polyphemus.signed` exists.
  - `CONNECT ITHACA`, `CLAIM`. Expected: paid once.
- (d) CTRL+C after the reward. Restart.
  - Expected: the gold includes the reward, `Current voyage: None`, and the `.object`/`.signed` files are gone.
  - `CLAIM`: nothing paid.
- ☐

**3.10 Island persistence**
- At Aeaea, `BUY DriedFish 5` and `SELL Barley 3`, then `LIST MARKET`.
- CTRL+C Aeaea and restart it with the same files (`$L run aeaea`, **no reset**).
- Another ship docks and runs `LIST MARKET`. Expected: the same quantities (DriedFish 85, Barley 143).
- `$L stock aeaea`: no MAP record.
- `$L stock scheria`: untouched (independent stocks).
- ☐

**3.11 Death at sea and shutdown**: see G.12, G.10 and G.11. In addition:
- CTRL+C each server while a ship is waiting, while it is trading, and while a transfer is running.
- Afterwards, `$L check` on every machine. Expected: no zombies, no semaphores, no leftover processes.
- ☐

---

## 5. Global Testing

| # | Row | Do | Expected | Result |
| --- | --- | --- | --- | --- |
| G.1 | Environment | `$L list`. Check that Ithaca [MO] and Aeaea [MA] are on different machines, that no config has `localhost`/`127.*`, and that every process has its own folder. | As listed. | ☐ |
| G.2 | Startup and terminal | §0 step 6 and rows 1.1–1.4, with all four ships started. | As in Phase 1. | ☐ |
| G.3 | Concurrent mission assignment | Row 2.2, then the winner sails to Aeaea. Telemachus runs `LIST VOYAGES` again. | The ID is still not listed: sailing does not cancel the assignment. | ☐ |
| G.4 | Capacity, queue and provisions | Rows 2.4 + 2.5(a): waiting ships start at 100 kg. | 93 at 5 s, 86 at 10 s; FIFO; waiting ships cannot trade. | ☐ |
| G.5 | Trade and stock consistency | Row 2.7 (only BUY DriedFish 5, SELL DriedFish 2, STATUS) and row 2.8(b). | 205 gold, 103 kg, DriedFish 3, stock 87; race: 1 kg left in the server **and** in stock.db. | ☐ |
| G.6 | Known island, no direct connection | Polyphemus: `CONNECT ITHACA`, `SAIL Aeaea`, `BUY MAP 1`, `SAIL Thrinacia`, `BUY MAP 1`, `SAIL Ogygia`, `SAIL Thrinacia`, `SAIL Aeaea`, `SAIL Ogygia`, `SAIL Thrinacia`. | `SAIL Ogygia` from Aeaea is refused and the ship stays at Aeaea; `SAIL Thrinacia` works. | ☐ |
| G.7 | Maps and navigation | At Aeaea, `BUY MAP 1` twice, then `MAP`, `STATUS`, `LIST MARKET`. From Ithaca, try every island except Aeaea. Try a known non-adjacent island. | -50 each, no duplicated routes, no cargo or stock change, MAP opens no connection (servers log nothing). From Ithaca only Aeaea works. | ☐ |
| G.8 | Complete mission and reward | Rows 3.1–3.3 exactly, with no extra purchases or waiting. | 1700 gold, 100 kg, no mission; a second CLAIM pays nothing. | ☐ |
| G.9 | Persistence across runs | Row 3.9, plus 3.10 for the market. Also: exit after the receipt (SIGNED) and before CLAIM, **keeping Ithaca running**, then restart and `CONNECT ITHACA`, `CLAIM`. | Recovered; paid exactly once. | ☐ |
| G.10 | Client disconnection or crash | With other ships active, stop one ship at each of these points: connected to Ithaca, docked, queued, during OBJECT/DELIVER/CLAIM. Do each once with CTRL+C and once with `kill -9`. | Servers stay up; the berth is freed or the ship leaves the queue; the survivors keep FIFO; nothing is confirmed early. | ☐ |
| G.11 | Ithaca or island crash | (a) Ships connected: CTRL+C Ithaca, then a ship sends `LIST VOYAGES`. Restart Ithaca and `CONNECT ITHACA`. (b) A ship docked at Scheria: `kill -9` Scheria, the ship sends `LIST MARKET`; `$L ipcclean` on [PU] is fine here; restart Scheria and dock a new ship. | Clients report the error without hanging; the other processes stay up; restarted servers accept new connections; Scheria restores its saved stock. | ☐ |
| G.12 | Death at sea and resource exhaustion | **(a) Death at sea:** Polyphemus docks at Aeaea and stays. [MO] Eumaeus with `$L run eumaeus_9kg`: `CONNECT ITHACA`, `LIST VOYAGES`, `ACCEPT <id>`, `SAIL Aeaea` (waits). Telemachus also `CONNECT ITHACA`, `SAIL Aeaea` (queued behind it). Wait until Eumaeus is lost and exits. Polyphemus: `BUY MAP 1`, `SAIL Scheria`. Penelope: `CONNECT ITHACA`, `LIST VOYAGES`. Then restart Eumaeus with `$L run eumaeus_9kg`: `STATUS`, `CONNECT ITHACA`, `LIST VOYAGES`. Penelope: `LIST VOYAGES` again. **(b) Stranded:** `$L reset polyphemus`, then `$L run polyphemus_broke` (50 gold, no cargo): `CONNECT ITHACA`, `LIST VOYAGES`, `ACCEPT <id>`, `SAIL Aeaea`, `BUY MAP 1`. Try `BUY SeaSalt 1` and `SELL Barley 1`, then CTRL+C. Restart with `$L run polyphemus_broke`, `STATUS`, `CONNECT ITHACA`, `LIST VOYAGES`. | **(a)** 2 kg at 5 s, 0 kg at 10 s, `lost at sea`, the process exits and `Eumaeus.object` is deleted. Telemachus (not Eumaeus) docks when Polyphemus leaves. After restart: 250 gold, 9 kg, no voyage. Penelope's first listing does not show the ID (Ithaca still holds the assignment). After Eumaeus lists, the voyage is failed and appears in neither listing. **(b)** Trades refused; Aeaea logs `Polyphemus has left`; `.object` deleted; restart shows 50 gold, 0 kg, no voyage; the voyage is not listed again. | ☐ |
| G.13 | File integrity and mission proof | `flipbyte`/`badmd5` (§1) for MD5_ERROR; row 3.5 for no-glyph, wrong-glyph and other-object proofs. | MD5_ERROR; no reward. | ☐ |
| G.14 | Protocol and resource errors | `frames` (§1), rows 3.8(a)(b)(c), 2.10(c). | NACK 0x7F/0xC0; TYPE/0xC0; controlled errors. | ☐ |
| G.15 | Resources and repeated runs | Run 3–5 full missions (3.1–3.3) in a row with Polyphemus + Eumaeus, plus waiting and trading. After each cycle run `$L check` on every machine and note RSS, fds and threads of every server. Also run one server under `VALGRIND=1`, do a cycle, then CTRL+C. `slowclient` and `cycles` in §1. | RSS and fds stable across cycles; CPU ~0 % when idle; no zombies; valgrind: no leaks, no invalid accesses, only std fds open; a slow client does not block others. | ☐ |
| G.16 | Final review | `grep -ci warning build.log` → 0; `static` scenario (§1); see below for the .tar. | | ☐ |

### Final .tar check
```
mkdir /tmp/tarcheck && tar -xf <delivery>.tar -C /tmp/tarcheck && cd /tmp/tarcheck/<folder> && make && ls
```
- **Must contain**: sources, `Makefile`, the `.dat` configs, `voyages.dat`, `stocks/`, `objects/`, `lib/sphragis/`, and the PDF report.
- **Must not contain**:
  - Binaries or `build/`.
  - The stray files `lazygit`, `name`, `src/ithaca~HEAD`. They are tracked in git, so `git archive` would include them.

---

## Results log

| Row | Pass/Fail | Observed / modified files | Notes |
| --- | --- | --- | --- |
| §1 load_test | | | |
| 1.1 – 1.5 | | | |
| 2.1 – 2.10 | | | |
| 3.1 – 3.11 | | | |
| G.1 – G.16 | | | |
