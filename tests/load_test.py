#!/usr/bin/env python3
"""Load, concurrency, failure and spec-coverage tests for the Nostos system (Phases 1-3).

Starts ithaca + islands + odysseus processes in a temp directory. Not part of the C build.

  fullrun          accept -> deliver -> connect -> claim, files cleaned up
  badmd5/badsize   corrupted MD5 / short DATA frame in a DELIVERY and in a CLAIM
  wrongisland      CLAIM with an image sealed by another island
  othership        CLAIM of a voyage assigned to another ship
  otherobject      CLAIM with another object sealed by the right island, under the mission name
  peerdies         ship dies in the middle of the OBJECT transfer
  objectrefused    ship answers the OBJECT with an error ACK (voyage is freed)
  concurrent       several ships transfer at once (pool of workers)
  sigint_*         SIGINT while a server is in the middle of a transfer
  voyages, berths, stock, lost    Phase 2 concurrency checks
  valgrind         full run under valgrind (--track-fds)
  valgrind_faults  failure paths and SIGINT in a transfer under valgrind

Coverage of the test PDFs (tests/COVERAGE.md maps every row to a scenario):
  terminal         usage lines, Unknown command, mixed case, 24 voyages / 8 products
  frames           NACK 0x7F/0xC0 for bad TYPE/FLAGS/LENGTH, operation errors TYPE/0xC0
  wrongstate       commands in the wrong state, one mission per ship, only Aeaea from Ithaca
  waiting          FIFO, 100/93/86 kg timing, STATUS/MAP only while waiting, queued crash
  market, buyrace  exact trade sequence, rejected trades, 4+4 kg race on a 5 kg stock
  routes           SPHRAGIS route filter, known-but-not-adjacent islands (global topology)
  trident          TridentOfPoseidon mission: 1700 gold, 100 kg, CLAIM pays once
  deathmission     9 kg ship lost at sea with a mission; restart; voyage failed on LIST
  stranded         dead end (6.8): CTRL+C, restart with the initial resources
  persist_trade    gold/cargo/reward survive a restart
  persist_mission  mission + OBJECT / SIGNED survive a restart, CLAIM pays once
  islandrestart    island and Ithaca stopped and restarted, stock restored
  flipbyte, cutoff altered byte with the right MD5; connection cut in DELIVERY/SIGNED/CLAIM
  badobject        a fake Ithaca sends a bad OBJECT to the real Odysseus
  fileerrors       missing object, unwritable folders, no server listening
  slowclient       half a frame does not block the servers for others
  cycles           repeated missions: no fd/memory growth, no zombies, no busy waiting
  static           warning-free compile, forbidden calls, md5sum via exec, SPHRAGIS used

The whole run also fails if it leaves System V semaphores behind (ipcs -s).

Usage: python3 tests/load_test.py [scenario ...] [--keep]
"""
import os
import glob
import hashlib
import random
import re
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANSI = re.compile(r"\x1b\[[0-9;]*m")
PROMPT = "$ "
STOCK_RECORD = struct.Struct("100sii")
DEFAULT_TIMEOUT = 15
VALGRIND_SCALE = 8  # valgrind makes everything several times slower
FRAME_SIZE = 256
DATA_MAX = 253


def local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("192.0.2.1", 9))  # no packet is sent
        return s.getsockname()[0]
    finally:
        s.close()


def free_port():
    s = socket.socket()
    s.bind(("", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def write_png(path, size=200, noise=False):
    """Writes a small valid PNG (SPHRAGIS only accepts PNG or JPEG images). With noise the
    pixels are random and stored uncompressed, so the file is about 3*size*size bytes."""
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    if noise:
        packer = zlib.compressobj(0)
        parts = [packer.compress(b"\0" + os.urandom(size * 3)) for _ in range(size)]
        pixels = b"".join(parts) + packer.flush()
    else:
        pixels = zlib.compress((b"\0" + bytes(x % 256 for x in range(size * 3))) * size)
    ihdr = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
                + chunk(b"IDAT", pixels) + chunk(b"IEND", b""))


def md5_of(path):
    with open(path, "rb") as f:
        return hashlib.md5(f.read()).hexdigest()


def semaphore_count():
    """Number of System V semaphore arrays in the system (`ipcs -s`)."""
    out = subprocess.run(["ipcs", "-s"], capture_output=True, text=True).stdout
    return len([l for l in out.splitlines() if re.match(r"0x[0-9a-fA-F]+\s+\d+", l)])


def read_stock(path):
    stock = {}
    with open(path, "rb") as f:
        data = f.read()
    for i in range(0, len(data) - STOCK_RECORD.size + 1, STOCK_RECORD.size):
        name, amount, _price = STOCK_RECORD.unpack_from(data, i)
        stock[name.split(b"\0")[0].decode()] = amount
    return stock


def write_stock(path, products):
    """Writes a stock.db with products: list of (name, amount, price)."""
    with open(path, "wb") as f:
        for name, amount, price in products:
            f.write(STOCK_RECORD.pack(name.encode(), amount, price))


def read_stock_full(path):
    """[(name, amount, price)] of a stock.db, in file order."""
    with open(path, "rb") as f:
        data = f.read()
    assert len(data) % STOCK_RECORD.size == 0, "stock.db size %d is not whole records" % len(data)
    return [(n.split(b"\0")[0].decode(), a, p)
            for n, a, p in (STOCK_RECORD.unpack_from(data, i)
                            for i in range(0, len(data), STOCK_RECORD.size))]


# Global Testing: capacities and the only direct routes SPHRAGIS accepts
GLOBAL_ISLANDS = {
    "Aeaea": (1, ["Scheria", "Thrinacia"]),
    "Scheria": (2, ["Aeaea", "Aeolia", "Ismarus"]),
    "Thrinacia": (1, ["Aeaea", "Ogygia"]),
    "Ogygia": (1, ["Thrinacia"]),
    "Aeolia": (1, ["Scheria"]),
    "Ismarus": (1, ["Scheria"]),
}


class Cluster:
    """ithaca + islands running in a temp directory. By default Aeaea + Scheria, both with a
    copy of Aeaea.db (the original Phase 2/3 load tests). With islands=GLOBAL_ISLANDS (or any
    {name: (capacity, [routes])}) every island loads its own stocks/<Name>.db."""

    def __init__(self, aeaea_capacity=2, voyages=None, valgrind=False, islands=None,
                 stocks=None):
        self.valgrind = valgrind
        self.scale = VALGRIND_SCALE if valgrind else 1
        self.ip = local_ip()
        self.dir = tempfile.mkdtemp(prefix="nostos_")
        own_stocks = islands is not None
        if islands is None:
            islands = {"Aeaea": (aeaea_capacity, ["Scheria"]), "Scheria": (20, ["Aeaea"])}
        self.islands = islands
        self.ports = {n.lower(): free_port() for n in ["ithaca"] + list(islands)}
        self.procs = {}
        os.makedirs(os.path.join(self.dir, "stocks"))
        for island in islands:
            source = island if own_stocks else "Aeaea"
            shutil.copy(os.path.join(ROOT, "stocks", source + ".db"),
                        os.path.join(self.dir, "stocks", island + ".db"))
        for island, products in (stocks or {}).items():
            write_stock(os.path.join(self.dir, "stocks", island + ".db"), products)
        self.initial_stock = read_stock(os.path.join(self.dir, "stocks", "Aeaea.db"))
        self._write("ithaca.dat", "Ithaca\n./objects\n%s %d\n" % (self.ip, self.ports["ithaca"]))
        with open(os.path.join(ROOT, "voyages.dat")) as f:
            lines = f.read().splitlines()
        lines = lines[:voyages] if voyages else lines
        self._write("voyages.dat", "\n".join(lines) + "\n")
        # The object images are not in the repo: ithaca sends them after ACCEPT
        self.objects = {}  # object name -> path of its image
        self.destinations = {}  # voyage id -> (object, island, reward)
        for number, line in enumerate(lines, 1):
            fields = line.split()
            if len(fields) >= 4:
                os.makedirs(os.path.join(self.dir, os.path.dirname(fields[1])), exist_ok=True)
                write_png(os.path.join(self.dir, fields[1]))
                self.objects[fields[0]] = fields[1]
                self.destinations[number] = (fields[0], fields[2], int(fields[3]))
        for island, (capacity, routes) in islands.items():
            self.write_island(island, capacity, routes)

    def write_island(self, island, capacity, routes):
        """(Re)writes <island>.dat; routes may name islands outside the cluster (dead port)."""
        lines = ["%s %s %d" % (r, self.ip, self.ports.get(r.lower()) or free_port())
                 for r in routes]
        self._write(island.lower() + ".dat", "%s\n./%s_port\n%s %d\n%d\n--- ROUTES ---\n%s\n" % (
            island, island.lower(), self.ip, self.ports[island.lower()], capacity,
            "\n".join(lines)))

    def wrap(self, argv, tag):
        """Runs argv under valgrind (log in <tag>.vg) when the cluster asks for it."""
        if not self.valgrind:
            return argv
        return ["valgrind", "--leak-check=full", "--show-leak-kinds=all",
                "--track-fds=yes", "--log-file=" + os.path.join(self.dir, tag + ".vg")] + argv

    def object_of(self, obj):
        return self.objects[obj]

    def set_object(self, obj, nbytes):
        """Replaces the image of an object with a valid PNG of about nbytes (a big transfer)."""
        side = int((nbytes / 3) ** 0.5)
        write_png(os.path.join(self.dir, self.objects[obj]), side, noise=True)

    def _write(self, name, text):
        with open(os.path.join(self.dir, name), "w") as f:
            f.write(text)

    def log(self, name):
        with open(os.path.join(self.dir, name + ".log")) as f:
            return f.read().splitlines()

    def start(self):
        for name in ["ithaca"] + [i.lower() for i in self.islands]:
            self.start_proc(name, wait=False)
        time.sleep(self.scale)
        self.check_alive()

    def start_proc(self, name, wait=True):
        """Starts (or restarts, appending to its log) one server process."""
        if name == "ithaca":
            argv = ["ithaca", "ithaca.dat", "voyages.dat"]
        else:
            island = [i for i in self.islands if i.lower() == name][0]
            argv = ["island", name + ".dat", "stocks/%s.db" % island]
        argv = self.wrap([os.path.join(ROOT, argv[0])] + argv[1:], name)
        out = open(os.path.join(self.dir, name + ".log"), "a")
        self.procs[name] = subprocess.Popen(argv, cwd=self.dir, stdout=out,
                                            stderr=subprocess.STDOUT)
        out.close()
        if wait:
            time.sleep(self.scale)
            assert self.procs[name].poll() is None, "%s did not restart" % name

    def stop_proc(self, name, sig=signal.SIGINT):
        """Stops one server (SIGINT, or SIGKILL for a crash) and returns its exit code."""
        proc = self.procs[name]
        proc.send_signal(sig)
        return proc.wait(timeout=10 * self.scale)

    def check_alive(self):
        for name, p in self.procs.items():
            assert p.poll() is None, "%s died (exit %s)" % (name, p.poll())

    def stop(self):
        for p in self.procs.values():
            if p.poll() is None:
                p.send_signal(signal.SIGINT)
        for p in self.procs.values():
            try:
                p.wait(timeout=3 * self.scale)
            except subprocess.TimeoutExpired:
                p.kill()

    def cleanup(self, keep):
        if keep:
            print("  files kept in", self.dir)
        else:
            shutil.rmtree(self.dir, ignore_errors=True)


class Ship:
    """One odysseus process driven through stdin/stdout pipes."""

    def __init__(self, cluster, name, gold=250, cargo=None, ithaca_port=None, aeaea_port=None,
                 write_config=True):
        cargo = cargo if cargo is not None else [("Barley", 40), ("DriedFigs", 60)]
        self.name = name
        self.cluster = cluster
        self.folder = os.path.join(cluster.dir, name + "_files")
        cfg = "%s\n./%s_files\n%s %d\nAeaea %s %d\n%d\n%d\n%s" % (
            name, name, cluster.ip, ithaca_port or cluster.ports["ithaca"], cluster.ip,
            aeaea_port or cluster.ports["aeaea"], gold, len(cargo),
            "".join("%s %d\n" % c for c in cargo))
        self.path = os.path.join(cluster.dir, name + ".dat")
        if write_config:
            with open(self.path, "w") as f:
                f.write(cfg)
        self.scale = cluster.scale
        self.proc = None
        self._spawn()

    def _spawn(self):
        argv = self.cluster.wrap([os.path.join(ROOT, "odysseus"), self.path], self.name)
        self.proc = subprocess.Popen(argv, cwd=self.cluster.dir, stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.buf = ""
        self.pos = 0
        self.cond = threading.Condition()
        threading.Thread(target=self._reader, args=(self.proc,), daemon=True).start()
        self.wait_prompt()

    def restart(self):
        """Same config file and folder, new process (persistence checks)."""
        self.close()
        self._spawn()

    def file(self, suffix):
        return os.path.join(self.folder, self.name + suffix)

    def _reader(self, proc):
        while True:
            try:
                chunk = os.read(proc.stdout.fileno(), 4096)
            except (OSError, ValueError):
                break
            if not chunk:
                break
            with self.cond:
                self.buf += ANSI.sub("", chunk.decode(errors="replace"))
                self.cond.notify_all()

    def _wait(self, pred, timeout, what):
        deadline = time.time() + timeout * self.scale
        with self.cond:
            while not pred():
                left = deadline - time.time()
                if left <= 0:
                    raise TimeoutError("%s: timeout waiting for %s; got %r" % (
                        self.name, what, self.buf[self.pos:][-300:]))
                self.cond.wait(left)

    def cmd(self, line):
        self.proc.stdin.write((line + "\n").encode())
        self.proc.stdin.flush()

    def wait_for(self, text, timeout=DEFAULT_TIMEOUT):
        self._wait(lambda: text in self.buf[self.pos:], timeout, text)

    def wait_ever(self, text, timeout=DEFAULT_TIMEOUT):
        """Waits until text is anywhere in the output of this process."""
        self._wait(lambda: text in self.buf, timeout, text)

    def wait_prompt(self, timeout=DEFAULT_TIMEOUT):
        self._wait(lambda: self.buf[self.pos:].endswith(PROMPT), timeout, "prompt")
        with self.cond:
            out = self.buf[self.pos:]
            self.pos = len(self.buf)
        return out

    def send(self, line, timeout=DEFAULT_TIMEOUT):
        self.cmd(line)
        return self.wait_prompt(timeout)

    def ask(self, line, text, timeout=DEFAULT_TIMEOUT):
        """Sends a line and waits for text (no prompt is shown while waiting for a berth).
        Returns the output from the line on."""
        with self.cond:
            self.pos = len(self.buf)
        self.cmd(line)
        self.wait_for(text, timeout)
        with self.cond:
            out = self.buf[self.pos:]
            self.pos = len(self.buf)
        return out

    def alive(self):
        return self.proc.poll() is None

    def kill9(self):
        self.proc.kill()
        self.proc.wait()

    def close(self):
        if self.alive():
            self.proc.send_signal(signal.SIGINT)
            try:
                self.proc.wait(timeout=3 * self.scale)
            except subprocess.TimeoutExpired:
                self.proc.kill()
        self.proc.stdout.close()
        self.proc.stdin.close()


def run_parallel(fn, items):
    """Runs fn(item) in a thread per item, returns results, re-raises errors."""
    results = [None] * len(items)
    errors = []

    def work(i, item):
        try:
            results[i] = fn(item)
        except Exception as e:  # noqa: BLE001
            errors.append(e)

    threads = [threading.Thread(target=work, args=(i, it)) for i, it in enumerate(items)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    if errors:
        raise errors[0]
    return results


# ----------------------------------------------------------------- scenarios

def scenario_voyages(keep):
    """10 ships race for 3 voyages: each one must be assigned exactly once."""
    cluster = Cluster(voyages=3)
    ships = []
    try:
        cluster.start()
        ships = [Ship(cluster, "Ship%d" % i) for i in range(10)]
        barrier = threading.Barrier(len(ships))

        def race(ship):
            ship.send("CONNECT ITHACA")
            ship.send("LIST VOYAGES")
            barrier.wait()
            for vid in (1, 2, 3):
                if "accepted" in ship.send("ACCEPT %d" % vid):
                    return vid
            return None

        won = [v for v in run_parallel(race, ships) if v is not None]
        assert sorted(won) == [1, 2, 3], "ships won voyages %s, expected [1, 2, 3]" % sorted(won)
        assigned = [re.search(r"Voyage (\d+) assigned", l).group(1)
                    for l in cluster.log("ithaca") if "assigned to" in l]
        assert sorted(assigned) == ["1", "2", "3"], "ithaca log assigned %s" % assigned
        cluster.check_alive()
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_berths(keep):
    """Capacity 2, 6 ships: limit, FIFO order, berth freed by kill -9 and by SAIL."""
    cluster = Cluster(aeaea_capacity=2)
    ships = []
    try:
        cluster.start()
        ships = [Ship(cluster, "Ship%d" % i) for i in range(1, 7)]
        for s in ships:
            s.send("CONNECT ITHACA")
        for s in ships:  # staggered so the arrival order is known
            s.cmd("SAIL Aeaea")
            time.sleep(0.4)
        for s in ships[:2]:
            s.wait_for("granted access")
            s.wait_prompt()
        for s in ships[2:]:
            s.wait_for("harbor is full")

        ships[0].kill9()  # a docked ship crashes
        ships[2].wait_for("granted access")
        ships[2].wait_prompt()

        ships[1].send("BUY MAP 1")  # a docked ship leaves properly
        ships[1].cmd("SAIL Scheria")
        ships[3].wait_for("granted access")
        ships[3].wait_prompt()
        ships[1].wait_prompt()
        cluster.check_alive()
        log = cluster.log("aeaea")  # before the cleanup frees more berths
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)

    docked = 0
    for line in log:
        if "entered the port" in line or "DOCKED sent to" in line:
            docked += 1
        elif "has left" in line:
            docked -= 1
        assert docked <= 2, "more than 2 ships docked after: " + line
    waiting = re.findall(r"Port full\. (\S+) waiting", "\n".join(log))
    sent = re.findall(r"DOCKED sent to (\S+);", "\n".join(log))
    assert sent == ["Ship3", "Ship4"], "DOCKED order %s, expected [Ship3, Ship4]" % sent
    assert waiting[:2] == sent, "FIFO broken: waiting %s, admitted %s" % (waiting, sent)


def scenario_stock(keep):
    """10 docked ships buy concurrently: stock.db must match the reported buys."""
    rng = random.Random(42)
    cluster = Cluster(aeaea_capacity=10)
    ships = []
    try:
        cluster.start()
        gold = 5000
        ships = [Ship(cluster, "Ship%d" % i, gold=gold) for i in range(10)]
        for s in ships:
            s.send("CONNECT ITHACA")
            s.cmd("SAIL Aeaea")
            s.wait_for("granted access")
            s.wait_prompt()
        products = sorted(cluster.initial_stock)
        plans = [[(rng.choice(products), rng.randint(1, 10)) for _ in range(5)] for _ in ships]
        barrier = threading.Barrier(len(ships))

        def shop(args):
            ship, plan = args
            bought, spent = {}, 0
            barrier.wait()
            for product, kg in plan:
                out = ship.send("BUY %s %d" % (product, kg))
                m = re.search(r"Bought (\d+) kg of (\S+) for (\d+) gold", out)
                if m:
                    bought[m.group(2)] = bought.get(m.group(2), 0) + int(m.group(1))
                    spent += int(m.group(3))
            m = re.search(r"Gold: (-?\d+)", ship.send("STATUS"))
            assert m and int(m.group(1)) == gold - spent, "%s gold %s, expected %d" % (
                ship.name, m and m.group(1), gold - spent)
            return bought

        total = {}
        for bought in run_parallel(shop, list(zip(ships, plans))):
            for k, v in bought.items():
                total[k] = total.get(k, 0) + v
        cluster.check_alive()
    finally:
        for s in ships:
            s.close()
        cluster.stop()
    final = read_stock(os.path.join(cluster.dir, "stocks", "Aeaea.db"))
    expected = {k: v - total.get(k, 0) for k, v in cluster.initial_stock.items()}
    cluster.cleanup(keep)
    assert final == expected, "stock.db %s != expected %s" % (final, expected)
    assert sum(total.values()) > 0, "no purchase succeeded"


def scenario_lost(keep):
    """Waiting ship loses 7 kg every 5 s and is lost at sea at 0 (process exits)."""
    cluster = Cluster(aeaea_capacity=1)
    ships = []
    try:
        cluster.start()
        holder = Ship(cluster, "Holder")
        waiter = Ship(cluster, "Waiter", cargo=[("Barley", 14)])
        ships = [holder, waiter]
        holder.send("CONNECT ITHACA")
        holder.cmd("SAIL Aeaea")
        holder.wait_for("granted access")
        waiter.send("CONNECT ITHACA")
        waiter.cmd("SAIL Aeaea")
        waiter.wait_for("lost at sea", timeout=25)
        left = [int(x) for x in re.findall(r"Provisions remaining: (\d+) kg", waiter.buf)]
        assert left == [7, 0], "provisions seen %s, expected [7, 0]" % left
        deadline = time.time() + 5
        while waiter.alive() and time.time() < deadline:
            time.sleep(0.2)
        assert not waiter.alive(), "(spec 6.4) process did not exit after being lost at sea"
        cluster.check_alive()
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)


def run_delivery(cluster, ship, vid=1, obj="AeolusBagOfWinds", reward=1150):
    """accept -> sail -> deliver -> connect -> claim for a voyage to Scheria."""
    out = ship.send("CONNECT ITHACA") + ship.send("LIST VOYAGES")
    assert obj in out, "voyage %d not listed: %r" % (vid, out)
    out = ship.send("ACCEPT %d" % vid)
    assert "Object received correctly" in out, "no object: %r" % out
    mine = os.path.join(cluster.dir, ship.name + "_files", ship.name + ".object")
    assert md5_of(mine) == md5_of(os.path.join(cluster.dir, cluster.object_of(obj))), \
        "the object received differs from the one of Ithaca"

    assert "has granted access" in ship.send("SAIL Aeaea")
    assert "New routes" in ship.send("BUY MAP 1")
    assert "granted access" in ship.send("SAIL Scheria")
    assert "Reward" not in ship.send("CLAIM"), "CLAIM worked without a delivery"
    out = ship.send("DELIVER")
    for text in ("Object transferred correctly", "Stamped object received",
                 "The mark of Scheria has been obtained"):
        assert text in out, "DELIVER output lacks %r: %r" % (text, out)
    assert os.path.exists(os.path.join(cluster.dir, ship.name + "_files", ship.name + ".signed"))
    assert "already been delivered" in ship.send("DELIVER")

    assert "Connected to Ithaca" in ship.send("CONNECT ITHACA")
    out = ship.send("CLAIM")
    for text in ("Glyph of Scheria verified", "Voyage %d completed" % vid,
                 "Reward received: %d gold" % reward):
        assert text in out, "CLAIM output lacks %r: %r" % (text, out)
    gold = 250 - 50 + reward  # initial gold - map + reward
    out = ship.send("STATUS")
    assert "Gold: %d" % gold in out and "Current voyage: None" in out, out
    for leftover in (ship.name + "_files/" + ship.name + ".object", ship.name + "_files/" + ship.name + ".signed"):
        assert not os.path.exists(os.path.join(cluster.dir, leftover)), leftover + " not deleted"
    no_temp_files(cluster)
    check_records(cluster, ship.name, vid, obj)


def scenario_fullrun(keep):
    """accept -> deliver -> connect -> claim, then the ship and the servers are clean."""
    cluster = Cluster()
    ship = None
    try:
        cluster.start()
        ship = Ship(cluster, "Polyphemus", gold=250)
        run_delivery(cluster, ship)
        assert server_logged(cluster, "ithaca", "Voyage 1 completed by Polyphemus")
        assert server_logged(cluster, "scheria", "sealed with the glyph of Scheria")
        cluster.check_alive()
    finally:
        if ship:
            ship.close()
        cluster.stop()
        cluster.cleanup(keep)


# ------------------------------------------------- protocol-level fake client
# The faults of the transfer tests (wrong MD5, wrong size, stalled sender)
# cannot be produced by the real odysseus, so these scenarios speak the wire
# protocol of the spec (frames of 256 bytes: type, flags, length, 253 data).

MSG_CONNECT, MSG_LIST, MSG_ACCEPT, MSG_CLAIM = 0x01, 0x02, 0x03, 0x04
MSG_ARRIVAL, MSG_DELIVER, MSG_DEPARTURE = 0x10, 0x15, 0x16
MSG_HEADER, MSG_DATA, MSG_READY, MSG_ACK = 0x20, 0x21, 0x22, 0x23
FLAG_RESPONSE, FLAG_ERROR = 0x80, 0x40
# kind -> (flags of the HEADER, flags of the DATA frames)
TRANSFER_FLAGS = {"OBJECT": (0x80, 0xA0), "DELIVERY": (0x00, 0x20),
                  "SIGNED": (0x80, 0xA0), "CLAIM": (0x00, 0x20)}


class Proto:
    """One TCP connection that sends and reads raw protocol frames."""

    def __init__(self, ip, port, timeout=20):
        self.sock = socket.create_connection((ip, port), timeout=timeout)

    def close(self):
        self.sock.close()

    def send(self, kind, flag, data=b""):
        if isinstance(data, str):
            data = data.encode()
        assert len(data) <= DATA_MAX
        self.sock.sendall(bytes([kind, flag, len(data)]) + data.ljust(DATA_MAX, b"\0"))

    def recv(self, timeout=None):
        if timeout is not None:
            self.sock.settimeout(timeout)
        frame = b""
        while len(frame) < FRAME_SIZE:
            chunk = self.sock.recv(FRAME_SIZE - len(frame))
            if not chunk:
                raise EOFError("connection closed by the peer")
            frame += chunk
        # Annex II: DATA_LENGTH <= 253 and the unused DATA bytes are zero padding
        assert frame[2] <= DATA_MAX, "frame with DATA_LENGTH %d" % frame[2]
        assert not any(frame[3 + frame[2]:]), "frame %#x is not zero-padded" % frame[0]
        return frame[0], frame[1], frame[3:3 + frame[2]]

    def raw(self, kind, flag, length, data=b""):
        """Sends a frame whose DATA_LENGTH may lie (invalid-format tests)."""
        self.sock.sendall(bytes([kind, flag, length]) + data.ljust(DATA_MAX, b"\0")[:DATA_MAX])

    def maybe_recv(self, timeout):
        """Next frame, or None if nothing comes (or the peer closed) in timeout seconds."""
        try:
            return self.recv(timeout)
        except (socket.timeout, EOFError, ConnectionError):
            return None

    def request(self, kind, data=b""):
        self.send(kind, 0x00, data)
        return self.recv()

    def send_transfer(self, kind, name, data, md5=None, size=None, short_last=False,
                      stall_after=None, flip_byte=False):
        """Sender side. Returns "ok", "SIZE_ERROR", "MD5_ERROR", "rejected" (READY with an
        error), "stalled" (stopped after stall_after DATA frames, socket left open).
        flip_byte announces the real MD5 but alters one byte of the data sent."""
        header_flag, data_flag = TRANSFER_FLAGS[kind]
        md5 = md5 or hashlib.md5(data).hexdigest()
        size = len(data) if size is None else size
        if flip_byte:
            middle = len(data) // 2
            data = data[:middle] + bytes([data[middle] ^ 0xFF]) + data[middle + 1:]
        self.send(MSG_HEADER, header_flag, "%s&%s&%d&%s" % (kind, name, size, md5))
        _, flag, _ = self.recv()
        if flag != FLAG_RESPONSE:
            return "rejected"
        chunks = [data[i:i + DATA_MAX] for i in range(0, len(data), DATA_MAX)]
        if short_last:
            chunks[-1] = chunks[-1][:-1]
        for number, chunk in enumerate(chunks):
            if stall_after is not None and number >= stall_after:
                return "stalled"
            self.send(MSG_DATA, data_flag, chunk)
        _, flag, reply = self.recv()
        if flag == FLAG_RESPONSE:
            return "ok"
        return reply.decode()

    def receive_transfer(self, kind, stop_after=None, ack_error=None):
        """Receiver side. Returns (file name, bytes); with stop_after it returns None after
        that many DATA frames without answering (a receiver that stops reading); with
        ack_error the FINAL ACK is that error (SIZE_ERROR / MD5_ERROR) instead of OK."""
        _, data_flag = TRANSFER_FLAGS[kind]
        _, flag, header = self.recv()
        fields = header.decode().split("&")
        assert flag == TRANSFER_FLAGS[kind][0] and fields[0] == kind, "bad HEADER %r" % header
        name, size, md5 = fields[1], int(fields[2]), fields[3]
        self.send(MSG_READY, FLAG_RESPONSE, "READY")
        data = b""
        for number in range((size + DATA_MAX - 1) // DATA_MAX):
            if stop_after is not None and number >= stop_after:
                return None
            _, flag, chunk = self.recv()
            assert flag == data_flag, "DATA flags %#x, expected %#x" % (flag, data_flag)
            data += chunk
        assert len(data) == size and hashlib.md5(data).hexdigest() == md5, "corrupt transfer"
        if ack_error:
            self.send(MSG_ACK, FLAG_RESPONSE | FLAG_ERROR, ack_error)
        else:
            self.send(MSG_ACK, FLAG_RESPONSE, "OK")
        return name, data


def ithaca_login(cluster, name):
    conn = Proto(cluster.ip, cluster.ports["ithaca"])
    _, flag, reply = conn.request(MSG_CONNECT, name)
    assert flag == FLAG_RESPONSE and reply == b"OK", "CONNECT refused: %r" % reply
    return conn


def list_voyages(conn):
    """IDs of the voyages Ithaca lists."""
    conn.send(MSG_LIST, 0x00)
    _, flag, first = conn.recv()
    if flag & FLAG_ERROR:
        return []
    fields = first.decode().split("&")
    ids = [int(fields[2])]
    for _ in range(int(fields[1]) - 1):
        ids.append(int(conn.recv()[2].decode().split("&")[2]))
    return ids


def accept_voyage(conn, vid, stop_after=None, ack_error=None):
    """ACCEPT + OBJECT transfer. Returns (object file name, bytes), or None if refused."""
    list_voyages(conn)
    _, flag, reply = conn.request(MSG_ACCEPT, str(vid))
    if flag & FLAG_ERROR:
        return None
    return conn.receive_transfer("OBJECT", stop_after, ack_error)


def final_reply(conn, timeout=5):
    """The final response frame (type, flags, data) of an operation, or None."""
    return conn.maybe_recv(timeout)


def island_deliver(cluster, island, ship, vid, obj, name, data, **faults):
    """Docks at the island and runs DELIVER. Returns (transfer result, signed (name, bytes)
    or None, final DELIVER frame or None)."""
    conn = Proto(cluster.ip, cluster.ports[island])
    try:
        _, flag, reply = conn.request(MSG_ARRIVAL, ship)
        assert reply == b"DOCKED", "ship not docked: %r" % reply
        conn.send(MSG_DELIVER, 0x00, "%d&%s" % (vid, obj))
        result = conn.send_transfer("DELIVERY", name, data, **faults)
        if result != "ok":
            return result, None, final_reply(conn)
        signed = conn.receive_transfer("SIGNED")
        return result, signed, final_reply(conn)
    finally:
        conn.close()


def claim_voyage(conn, vid, name, data, **faults):
    """CLAIM request + transfer. Returns (transfer result, final CLAIM frame or None)."""
    conn.send(MSG_CLAIM, 0x00, str(vid))
    result = conn.send_transfer("CLAIM", name, data, **faults)
    return result, final_reply(conn)


def is_error(frame):
    return frame is not None and bool(frame[1] & FLAG_ERROR)


def is_ok(frame):
    return frame is not None and frame[1] == FLAG_RESPONSE


def completed(cluster, expect=0):
    """Log lines of the voyages Ithaca completed. With expect, waits (the server logs after it
    answers the ship) until that many are there."""
    deadline = time.time() + 5 * cluster.scale
    lines = [l for l in cluster.log("ithaca") if "completed by" in l]
    while len(lines) < expect and time.time() < deadline:
        time.sleep(0.1)
        lines = [l for l in cluster.log("ithaca") if "completed by" in l]
    return lines


def server_logged(cluster, server, text, expect=1):
    """True if the log of the server has the text expect times (waits a few seconds)."""
    deadline = time.time() + 5 * cluster.scale
    while time.time() < deadline:
        if len([l for l in cluster.log(server) if text in l]) >= expect:
            return True
        time.sleep(0.1)
    return False


def no_temp_files(cluster):
    """No temporary transfer files at all (PHASE3 / GLOBAL: "do not add temporary files"):
    the servers keep received images under their final names, never .tmp or hidden files."""
    stray = []
    for root, _, names in os.walk(cluster.dir):
        stray += [os.path.join(root, n) for n in names
                  if n.endswith(".tmp") or n.startswith(".claim") or n.startswith(".delivery")]
    assert not stray, "temporary files: %s" % stray


def records(cluster, server, ship):
    """Images a server keeps for a ship: <folder>/<Ship>_<VoyageID>_<file>[.signed]."""
    folder = "objects" if server == "ithaca" else server + "_port"
    return sorted(os.path.basename(f) for f in
                  glob.glob(os.path.join(cluster.dir, folder, ship + "_*")))


def check_records(cluster, ship, vid, obj, claimed=True):
    """After a good DELIVER (and CLAIM) the island keeps the received and the sealed image
    and Ithaca keeps the claimed one, which is the island's sealed image."""
    name = os.path.basename(cluster.object_of(obj))
    base = "%s_%d_%s" % (ship, vid, name)
    assert records(cluster, "scheria", ship) == [base, base + ".signed"], \
        "island records %s" % records(cluster, "scheria", ship)
    island = os.path.join(cluster.dir, "scheria_port", base)
    assert md5_of(island) == md5_of(os.path.join(cluster.dir, cluster.object_of(obj))), \
        "the island kept a different image than the object"
    if claimed:
        assert records(cluster, "ithaca", ship) == [base], \
            "Ithaca records %s" % records(cluster, "ithaca", ship)
        assert md5_of(os.path.join(cluster.dir, "objects", base)) == md5_of(island + ".signed"), \
            "Ithaca kept a different image than the one sealed"


def prepare_claim(cluster, ship, vid=1):
    """A fake ship that accepted voyage vid and got the object sealed by Scheria.
    Returns (Ithaca connection, object name, object, signed bytes)."""
    conn = ithaca_login(cluster, ship)
    got = accept_voyage(conn, vid)
    assert got, "ACCEPT %d refused" % vid
    obj_name, obj_data = got
    result, signed, final = island_deliver(cluster, "scheria", ship, vid,
                                           cluster.destinations[vid][0], obj_name, obj_data)
    assert result == "ok" and signed and is_ok(final), "DELIVER failed: %s %s" % (result, final)
    return conn, obj_name, obj_data, signed[1]


# ------------------------------------------------------ failure scenarios

def corrupt_scenario(keep, fault, expected_ack, valgrind=False):
    """DELIVER and CLAIM with a corrupted transfer (fault: kwargs of send_transfer): the
    receiver must answer the ACK error and a final error, keep no file and stay usable."""
    cluster = Cluster(valgrind=valgrind)
    conn = None
    try:
        cluster.start()
        conn = ithaca_login(cluster, "Corrupt")
        obj_name, obj_data = accept_voyage(conn, 1)

        # DELIVER to the island
        result, signed, final = island_deliver(cluster, "scheria", "Corrupt", 1,
                                               "AeolusBagOfWinds", obj_name, obj_data, **fault)
        assert result == expected_ack, "island ACK %r, expected %s" % (result, expected_ack)
        assert is_error(final) and final[0] == MSG_DELIVER, "no final DELIVER error: %s" % (final,)
        assert not any("sealed" in l for l in cluster.log("scheria")), "island sealed a bad image"
        time.sleep(0.5)
        no_temp_files(cluster)
        assert records(cluster, "scheria", "Corrupt") == [], "bad delivery kept"

        # the island is still fine: a good delivery works
        result, signed, final = island_deliver(cluster, "scheria", "Corrupt", 1,
                                               "AeolusBagOfWinds", obj_name, obj_data)
        assert result == "ok" and is_ok(final), "good DELIVER after a bad one failed"

        # CLAIM to Ithaca
        result, final = claim_voyage(conn, 1, obj_name, signed[1], **fault)
        assert result == expected_ack, "Ithaca ACK %r, expected %s" % (result, expected_ack)
        assert is_error(final) and final[0] == MSG_CLAIM, "no final CLAIM error: %s" % (final,)
        assert not completed(cluster), "voyage completed with a bad transfer"
        time.sleep(0.5)
        no_temp_files(cluster)
        assert records(cluster, "ithaca", "Corrupt") == [], "bad claim kept"

        # the voyage is still assigned: the good claim pays
        result, final = claim_voyage(conn, 1, obj_name, signed[1])
        assert result == "ok" and is_ok(final) and final[2] == b"OK&1150", \
            "good CLAIM after a bad one failed: %s %s" % (result, final)
        assert len(completed(cluster, 1)) == 1
        check_records(cluster, "Corrupt", 1, "AeolusBagOfWinds")
        cluster.check_alive()
        conn.close()
        cluster.stop()
        if valgrind:
            check_valgrind_logs(cluster, ("ithaca", "scheria"))
    finally:
        if conn:
            conn.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_badmd5(keep):
    """Corrupted MD5 in the HEADER of a DELIVERY and of a CLAIM."""
    corrupt_scenario(keep, {"md5": "0" * 32}, "MD5_ERROR")


def scenario_badsize(keep):
    """Last DATA frame shorter than announced in the DELIVERY and in the CLAIM."""
    corrupt_scenario(keep, {"short_last": True}, "SIZE_ERROR")


def scenario_wrongisland(keep):
    """CLAIM with an image sealed by another island: Ithaca must refuse it."""
    cluster = Cluster()
    conn = None
    try:
        cluster.start()
        conn = ithaca_login(cluster, "Cheater")
        obj_name, obj_data = accept_voyage(conn, 1)  # destination: Scheria
        result, signed, final = island_deliver(cluster, "aeaea", "Cheater", 1,
                                               "AeolusBagOfWinds", obj_name, obj_data)
        assert result == "ok" and is_ok(final), "Aeaea did not seal the image"
        assert any("glyph of Aeaea" in l for l in cluster.log("aeaea"))

        result, final = claim_voyage(conn, 1, obj_name, signed[1])
        assert is_error(final) and final[0] == MSG_CLAIM, "claim with the wrong glyph: %s" % (final,)
        assert not completed(cluster), "voyage completed with the glyph of another island"
        no_temp_files(cluster)
        assert records(cluster, "ithaca", "Cheater") == [], "a rejected claim image was kept"

        # the unsealed original has no glyph at all
        result, final = claim_voyage(conn, 1, obj_name, obj_data)
        assert is_error(final) and not completed(cluster), "claim with an unsealed image"

        # the right island's seal still works afterwards
        result, signed, final = island_deliver(cluster, "scheria", "Cheater", 1,
                                               "AeolusBagOfWinds", obj_name, obj_data)
        assert result == "ok"
        result, final = claim_voyage(conn, 1, obj_name, signed[1])
        assert is_ok(final) and len(completed(cluster, 1)) == 1, "right glyph refused: %s" % (final,)
        cluster.check_alive()
    finally:
        if conn:
            conn.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_othership(keep):
    """Claiming a voyage assigned to another ship, or without any voyage."""
    cluster = Cluster()
    owner = thief = idle = None
    try:
        cluster.start()
        owner, obj_name, _, signed = prepare_claim(cluster, "Owner", 1)

        # a ship without a voyage claims with the owner's sealed image
        idle = ithaca_login(cluster, "Idle")
        result, final = claim_voyage(idle, 1, obj_name, signed)
        assert result == "rejected", "claim without a voyage: %s %s" % (result, final)

        # a ship with another voyage (2, TridentOfPoseidon) claims with the owner's image
        thief = ithaca_login(cluster, "Thief")
        assert accept_voyage(thief, 2), "Thief could not accept voyage 2"
        result, final = claim_voyage(thief, 2, obj_name, signed)
        assert result == "rejected" or is_error(final), \
            "claim with the image of another voyage: %s %s" % (result, final)
        assert not completed(cluster), "a voyage was completed by the wrong ship"
        no_temp_files(cluster)

        # the owner is not affected by the attempts
        result, final = claim_voyage(owner, 1, obj_name, signed)
        assert is_ok(final) and final[2] == b"OK&1150", "owner refused: %s" % (final,)
        done = completed(cluster, 1)
        assert len(done) == 1 and "Owner" in done[0], done
        cluster.check_alive()
    finally:
        for c in (owner, thief, idle):
            if c:
                c.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_otherobject(keep):
    """CLAIM whose HEADER names the mission object and whose image carries the glyph of the
    destination, but is another object (sealed for another voyage to the same island)."""
    cluster = Cluster()
    owner = thief = None
    try:
        cluster.start()
        to_scheria = [v for v, d in sorted(cluster.destinations.items()) if d[1] == "Scheria"]
        assert len(to_scheria) >= 2, "need two voyages to Scheria"
        first, second = to_scheria[0], to_scheria[1]
        # the generated objects are all the same picture: make the first one different
        write_png(os.path.join(cluster.dir, cluster.objects[cluster.destinations[first][0]]), 150)
        owner, _, _, signed = prepare_claim(cluster, "Owner", first)

        thief = ithaca_login(cluster, "Thief")
        got = accept_voyage(thief, second)
        assert got, "ACCEPT %d refused" % second
        thief_name, thief_data = got
        result, final = claim_voyage(thief, second, thief_name, signed)
        assert result == "rejected" or is_error(final), \
            "claim with another sealed object: %s %s" % (result, final)
        assert not completed(cluster), "a voyage was completed with the wrong object"
        no_temp_files(cluster)
        kept = os.path.join(cluster.dir, "objects", "Thief_%d_%s" % (second, thief_name))
        assert not os.path.exists(kept), "the rejected image was kept"

        # the real sealed object of the voyage is still accepted
        result, real, final = island_deliver(cluster, "scheria", "Thief", second,
                                             cluster.destinations[second][0],
                                             thief_name, thief_data)
        assert result == "ok" and real and is_ok(final), "DELIVER failed: %s %s" % (result, final)
        result, final = claim_voyage(thief, second, thief_name, real[1])
        assert is_ok(final), "real claim refused: %s" % (final,)
        assert len(completed(cluster, 1)) == 1
        cluster.check_alive()
    finally:
        for c in (owner, thief):
            if c:
                c.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_peerdies(keep):
    """A ship dies in the middle of the OBJECT transfer: Ithaca survives and, as in 6.2, the
    voyage stays assigned to that ship until it lists voyages again."""
    cluster = Cluster()
    conn = other = None
    try:
        cluster.set_object("AeolusBagOfWinds", 500_000)  # more than the 3 frames it reads
        cluster.start()
        conn = ithaca_login(cluster, "Dying")
        assert accept_voyage(conn, 1, stop_after=3) is None
        conn.close()  # the ship disappears with the transfer half done
        conn = None
        deadline = time.time() + 15 * cluster.scale
        while not any("Dying has left" in l for l in cluster.log("ithaca")):
            assert time.time() < deadline, "Ithaca never noticed that the ship left"
            time.sleep(0.3)
        cluster.check_alive()
        other = ithaca_login(cluster, "Survivor")
        assert 1 not in list_voyages(other), "a voyage assigned to a dead ship is listed"
        assert accept_voyage(other, 2), "Ithaca stopped serving after the ship died"
        conn = ithaca_login(cluster, "Dying")  # the restarted ship lists again: voyage 1 fails
        assert 1 not in list_voyages(conn)
        cluster.check_alive()
    finally:
        for c in (conn, other):
            if c:
                c.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_objectrefused(keep):
    """The ship answers the OBJECT with an MD5_ERROR ack: Ithaca frees the voyage again."""
    cluster = Cluster()
    conn = other = None
    try:
        cluster.start()
        conn = ithaca_login(cluster, "Unlucky")
        assert accept_voyage(conn, 1, ack_error="MD5_ERROR")
        deadline = time.time() + 10 * cluster.scale
        while not any("voyage is free again" in l for l in cluster.log("ithaca")):
            assert time.time() < deadline, "Ithaca never freed the voyage"
            time.sleep(0.2)
        other = ithaca_login(cluster, "Lucky")
        assert 1 in list_voyages(other), "voyage 1 is not listed again"
        assert accept_voyage(other, 1), "voyage 1 cannot be accepted again"
        cluster.check_alive()
    finally:
        for c in (conn, other):
            if c:
                c.close()
        cluster.stop()
        cluster.cleanup(keep)


# --------------------------------------------------------- concurrent transfers

def scenario_concurrent(keep):
    """Ships do accept / deliver / claim at the same time (all transfers in the pool)."""
    cluster = Cluster(aeaea_capacity=10)
    ships = []
    try:
        cluster.destinations_to_scheria = [v for v, d in cluster.destinations.items()
                                           if d[1] == "Scheria"]
        voyages = cluster.destinations_to_scheria
        for vid in voyages:  # ~1 MB images: thousands of frames per transfer
            cluster.set_object(cluster.destinations[vid][0], 1_000_000)
        cluster.start()
        ships = [Ship(cluster, "Ship%d" % i, gold=250) for i in range(len(voyages))]
        barrier = threading.Barrier(len(ships))

        def journey(args):
            ship, vid = args
            obj, _, reward = cluster.destinations[vid]
            ship.send("CONNECT ITHACA")
            ship.send("LIST VOYAGES")
            barrier.wait()
            assert "Object received correctly" in ship.send("ACCEPT %d" % vid, timeout=60), \
                "%s: ACCEPT %d" % (ship.name, vid)
            assert md5_of(os.path.join(cluster.dir, ship.name + "_files", ship.name + ".object")) == \
                md5_of(os.path.join(cluster.dir, cluster.object_of(obj))), "corrupt object"
            ship.send("SAIL Aeaea")
            ship.send("BUY MAP 1")
            ship.send("SAIL Scheria")
            barrier.wait()
            out = ship.send("DELIVER", timeout=60)
            assert "The mark of Scheria has been obtained" in out, "%s: %r" % (ship.name, out)
            ship.send("CONNECT ITHACA")
            barrier.wait()
            out = ship.send("CLAIM", timeout=60)
            assert "Reward received: %d gold" % reward in out, "%s: %r" % (ship.name, out)
            return vid

        done = run_parallel(journey, list(zip(ships, voyages)))
        assert sorted(done) == sorted(voyages)
        assert len(completed(cluster, len(voyages))) == len(voyages), completed(cluster)
        assert server_logged(cluster, "scheria", "sealed with the glyph", len(voyages))
        no_temp_files(cluster)
        cluster.check_alive()
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)


# ------------------------------------------------------------ SIGINT in a transfer

SIGINT_LIMIT = 25  # seconds a server may take to exit (transfer timeout is 10 s)


def sigint_check(cluster, name):
    """Sends SIGINT to a server that has a transfer in progress and checks that it ends."""
    before = semaphore_count()
    proc = cluster.procs[name]
    start = time.time()
    proc.send_signal(signal.SIGINT)
    try:
        code = proc.wait(timeout=SIGINT_LIMIT * cluster.scale)
    except subprocess.TimeoutExpired:
        proc.kill()
        raise AssertionError("%s did not exit %ds after SIGINT during a transfer" % (
            name, SIGINT_LIMIT))
    took = time.time() - start
    assert code == 0, "%s exited with %s after SIGINT" % (name, code)
    assert semaphore_count() <= before, "%s left semaphores behind" % name
    print("      %s exited %.1fs after SIGINT" % (name, took))


def scenario_sigint_claim(keep, valgrind=False):
    """SIGINT while Ithaca receives a CLAIM that stopped halfway."""
    cluster = Cluster(valgrind=valgrind)
    conn = None
    try:
        cluster.start()
        conn, obj_name, _, signed = prepare_claim(cluster, "Stalled", 1)
        conn.send(MSG_CLAIM, 0x00, "1")
        assert conn.send_transfer("CLAIM", obj_name, signed, stall_after=2) == "stalled"
        sigint_check(cluster, "ithaca")
        no_temp_files(cluster)
        assert records(cluster, "ithaca", "Stalled") == [], "partial claim kept after SIGINT"
        if valgrind:
            check_valgrind_logs(cluster, ("ithaca", "scheria"))
    finally:
        if conn:
            conn.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_sigint_deliver(keep, valgrind=False):
    """SIGINT while an island receives a DELIVERY that stopped halfway."""
    cluster = Cluster(valgrind=valgrind)
    conn = isl = None
    try:
        cluster.start()
        conn = ithaca_login(cluster, "Stalled")
        obj_name, obj_data = accept_voyage(conn, 1)
        isl = Proto(cluster.ip, cluster.ports["scheria"])
        assert isl.request(MSG_ARRIVAL, "Stalled")[2] == b"DOCKED"
        isl.send(MSG_DELIVER, 0x00, "1&AeolusBagOfWinds")
        assert isl.send_transfer("DELIVERY", obj_name, obj_data, stall_after=2) == "stalled"
        sigint_check(cluster, "scheria")
        no_temp_files(cluster)
        assert records(cluster, "scheria", "Stalled") == [], "partial delivery kept after SIGINT"
        if valgrind:
            check_valgrind_logs(cluster, ("scheria",))
    finally:
        for c in (conn, isl):
            if c:
                c.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_sigint_object(keep, valgrind=False):
    """SIGINT while Ithaca sends a big OBJECT to a ship that stopped reading."""
    cluster = Cluster(valgrind=valgrind)
    conn = None
    try:
        cluster.set_object("AeolusBagOfWinds", 40_000_000)  # more than the socket buffers hold
        cluster.start()
        conn = ithaca_login(cluster, "Deaf")
        assert accept_voyage(conn, 1, stop_after=0) is None  # READY sent, DATA never read
        time.sleep(1)
        sigint_check(cluster, "ithaca")
        if valgrind:
            check_valgrind_logs(cluster, ("ithaca",))
    finally:
        if conn:
            conn.close()
        cluster.stop()
        cluster.cleanup(keep)


# ------------------------------------------------------------------- valgrind

def check_valgrind(cluster, tag):
    """The log of a process run under valgrind: no errors, no leaks, no open descriptors."""
    with open(os.path.join(cluster.dir, tag + ".vg")) as f:
        text = f.read()
    errors = re.search(r"ERROR SUMMARY: (\d+) errors", text)
    assert errors, "%s: valgrind did not finish (process killed?)" % tag
    assert errors.group(1) == "0", "%s: %s valgrind errors:\n%s" % (tag, errors.group(1), text)
    for kind in ("definitely lost", "indirectly lost", "possibly lost", "still reachable"):
        m = re.search(kind + r": ([\d,]+) bytes", text)
        assert m is None or m.group(1) == "0", "%s: memory %s: %s bytes\n%s" % (
            tag, kind, m.group(1), text)
    fds = re.search(r"FILE DESCRIPTORS: (\d+) open \((\d+) (?:std|inherited)\)", text)
    assert fds and fds.group(1) == fds.group(2), "%s: descriptors left open:\n%s" % (tag, text)


def check_valgrind_logs(cluster, tags):
    """check_valgrind for several processes, after stopping the ones still running."""
    for proc in cluster.procs.values():
        if proc.poll() is None:
            proc.send_signal(signal.SIGINT)
    for proc in cluster.procs.values():
        try:
            proc.wait(timeout=10 * cluster.scale)
        except subprocess.TimeoutExpired:
            raise AssertionError("a process did not exit after SIGINT")
    for tag in tags:
        check_valgrind(cluster, tag)


def scenario_valgrind_faults(keep):
    """The failure paths under valgrind: bad MD5, bad size, SIGINT in the middle of every kind
    of transfer. Each one must end with no memory errors, no leaks and no open descriptors."""
    corrupt_scenario(keep, {"md5": "0" * 32}, "MD5_ERROR", valgrind=True)
    corrupt_scenario(keep, {"short_last": True}, "SIZE_ERROR", valgrind=True)
    scenario_sigint_claim(keep, valgrind=True)
    scenario_sigint_deliver(keep, valgrind=True)
    scenario_sigint_object(keep, valgrind=True)


def scenario_valgrind(keep):
    """A full run with the three processes under valgrind (--track-fds), ended by SIGINT."""
    cluster = Cluster(valgrind=True)
    ship = None
    try:
        cluster.start()
        ship = Ship(cluster, "Polyphemus", gold=250)
        run_delivery(cluster, ship)
        ship.close()
        cluster.stop()
        for tag in ("ithaca", "aeaea", "scheria", "Polyphemus"):
            check_valgrind(cluster, tag)
    finally:
        if ship:
            ship.close()
        cluster.stop()
        cluster.cleanup(keep)


# ===================================================================================
# Coverage of tests/PHASE1-TESTS.pdf, PHASE2-TESTS.pdf, PHASE3-TESTS.pdf, GLOBAL_TESTS.pdf.
# Each scenario names the rows it covers; tests/COVERAGE.md has the full row -> test map.
# ===================================================================================

VOYAGE_LINE = re.compile(r"(\d+)\. (\S+)\s+-> (\S+)\s+\[(\d+)\]")


def status(ship):
    """(gold, total provisions, {product: kg}, full text) from STATUS."""
    out = ship.send("STATUS")
    gold = re.search(r"Gold: (-?\d+)", out)
    prov = re.search(r"Total provisions: (\d+) kg", out)
    assert gold and prov, "%s: unreadable STATUS %r" % (ship.name, out)
    cargo = {m.group(1): int(m.group(2)) for m in re.finditer(r"^(\w+): (\d+) kg$", out, re.M)}
    return int(gold.group(1)), int(prov.group(1)), cargo, out


def voyage_id(ship, obj):
    """ID of obj in a fresh LIST VOYAGES (None if not listed)."""
    for m in VOYAGE_LINE.finditer(ship.send("LIST VOYAGES")):
        if m.group(2) == obj:
            return int(m.group(1))
    return None


def listed_ids(ship):
    return [int(m.group(1)) for m in VOYAGE_LINE.finditer(ship.send("LIST VOYAGES"))]


def market(ship):
    """{product: (kg, price)} plus the map price from LIST MARKET."""
    out = ship.send("LIST MARKET")
    goods = {m.group(1): (int(m.group(2)), int(m.group(3)))
             for m in re.finditer(r"^(\w+)\t(\d+) kg\t ?(\d+) gold / kg$", out, re.M)}
    map_price = re.search(r"^Map\t+(\d+) gold$", out, re.M)
    return goods, map_price and int(map_price.group(1)), out


def rejected(out):
    """True if a command printed an error / refusal and no success text."""
    return ("ERROR" in out or "can't" in out or "Usage" in out or "must be" in out
            or out.strip().split("\n")[0].isupper()) and "Bought" not in out and "Sold" not in out


def children(pid):
    """[(pid, state)] of the children of pid (zombies show state Z)."""
    found = []
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        try:
            with open("/proc/%s/stat" % entry) as f:
                fields = f.read().rsplit(")", 1)[1].split()
        except OSError:
            continue
        if int(fields[1]) == pid:
            found.append((int(entry), fields[0]))
    return found


def no_zombies(cluster):
    for name, proc in cluster.procs.items():
        if proc.poll() is None:
            zombies = [c for c in children(proc.pid) if c[1] == "Z"]
            assert not zombies, "%s has zombie children %s" % (name, zombies)


def fd_count(pid):
    return len(os.listdir("/proc/%d/fd" % pid))


def cpu_ticks(pid):
    with open("/proc/%d/stat" % pid) as f:
        fields = f.read().rsplit(")", 1)[1].split()
    return int(fields[11]) + int(fields[12])  # utime + stime


def rss_kb(pid):
    with open("/proc/%d/status" % pid) as f:
        return int(re.search(r"VmRSS:\s+(\d+)", f.read()).group(1))


def sail_route(ship, *islands):
    """SAIL along islands, buying each map on the way (except the last island)."""
    for i, island in enumerate(islands):
        out = ship.send("SAIL %s" % island)
        assert "granted access" in out, "%s: SAIL %s: %r" % (ship.name, island, out)
        if i < len(islands) - 1:
            assert "Bought the map" in ship.send("BUY MAP 1")


# --------------------------------------------------------------- Phase 1 / startup

def scenario_terminal(keep):
    """PHASE1 all rows, GLOBAL 'Startup and terminal': files loaded (24 voyages, 8 products),
    usage lines, Unknown command, case-insensitive commands, errors leave state unchanged."""
    cluster = Cluster(islands=GLOBAL_ISLANDS)
    ship = None
    try:
        cluster.start()
        assert server_logged(cluster, "ithaca", "24 voyages loaded"), cluster.log("ithaca")
        for island in GLOBAL_ISLANDS:
            assert server_logged(cluster, island.lower(), "8 products available"), island
        ship = Ship(cluster, "Polyphemus")
        before = status(ship)[:3]
        cases = [
            ("ACCEPT", "Usage: ACCEPT <voyage_id>"),
            ("BUY DriedFish", "Usage: BUY <product> <amount>"),
            ("SELL Barley", "Usage: SELL <product> <amount>"),
            ("MAP now", "Usage: MAP"),
            ("STATUS now", "Usage: STATUS"),
            ("DELIVER x", "Usage: DELIVER"),
            ("CLAIM x", "Usage: CLAIM"),
            ("CONNECT", "Usage: CONNECT ITHACA"),
            ("SAIL", "Usage: SAIL <island>"),
            ("LIST", "Usage: LIST"),
            ("ACCEPT two", "positive integer"),
            ("BUY DriedFish -3", "positive integer"),
            ("something else", "Unknown command"),
            ("FLY Aeaea", "Unknown command"),
        ]
        for line, expected in cases:
            out = ship.send(line)
            assert expected in out, "%r printed %r, expected %r" % (
                line, out, expected)
            assert "Command OK" not in out, "final version still prints Command OK"
        assert status(ship)[:3] == before, "a rejected command changed the ship"
        assert "Connected to Ithaca" in ship.send("cOnNeCt iThAcA")
        assert "AVAILABLE VOYAGES" in ship.send("list voyages")
        assert "AVAILABLE VOYAGES" in ship.send("LiSt VoYaGeS")
        assert "--- POLYPHEMUS ---" in ship.send("status")
        ship.close()
        assert ship.proc.returncode == 0, "CTRL+C exit code %s" % ship.proc.returncode
        cluster.check_alive()
    finally:
        if ship:
            ship.close()
        cluster.stop()
        cluster.cleanup(keep)


# ----------------------------------------------------------------- frames (Annex II)

def scenario_frames(keep):
    """PHASE2 'Frames and server errors', GLOBAL 'Protocol and resource errors': invalid
    TYPE/FLAGS/DATA_LENGTH get NACK 0x7F/0xC0, operation errors their TYPE/0xC0, every frame
    received is 256 bytes zero-padded (checked in Proto.recv), servers keep serving."""
    cluster = Cluster()
    conns = []
    try:
        cluster.start()
        for server, hello in (("ithaca", MSG_CONNECT), ("aeaea", MSG_ARRIVAL)):
            for kind, flag, length, reason in ((0x55, 0x00, 0, "INVALID_TYPE"),
                                               (hello, 0x13, 4, "INVALID_FLAGS"),
                                               (hello, 0x00, 254, "INVALID_LENGTH")):
                conn = Proto(cluster.ip, cluster.ports[server])
                conns.append(conn)
                if kind == 0x55:  # a valid first frame, then one with an unknown TYPE
                    reply = conn.request(hello, "Framer")
                    assert reply[1] == FLAG_RESPONSE, "%s refused the hello: %s" % (server, reply)
                conn.raw(kind, flag, length, b"Nack")
                got = conn.maybe_recv(5)
                assert got and got[0] == 0x7F and got[1] == 0xC0 and got[2] == reason.encode(), \
                    "%s: %s frame answered %s, expected NACK %s" % (server, reason, got, reason)
                cluster.check_alive()
        # operation errors: that operation's TYPE with 0xC0
        ith = ithaca_login(cluster, "Operator")
        conns.append(ith)
        got = ith.request(MSG_ACCEPT, "999")
        assert got[0] == MSG_ACCEPT and got[1] == 0xC0, "ACCEPT 999 answered %s" % (got,)
        isl = Proto(cluster.ip, cluster.ports["aeaea"])
        conns.append(isl)
        assert isl.request(MSG_ARRIVAL, "Trader")[2] == b"DOCKED"
        got = isl.request(0x12, "Unicorn&1")
        assert got[0] == 0x12 and got[1] == 0xC0, "BUY Unicorn answered %s" % (got,)
        got = isl.request(0x13, "Unicorn&1")
        assert got[0] == 0x13 and got[1] == 0xC0, "SELL Unicorn answered %s" % (got,)
        # the servers still work for a real ship (the fake ships leave the berths first)
        for c in conns:
            c.close()
        conns = []
        time.sleep(0.5)
        ship = Ship(cluster, "Survivor")
        try:
            assert "AVAILABLE VOYAGES" in (ship.send("CONNECT ITHACA") + ship.send("LIST VOYAGES"))
            assert "granted access" in ship.send("SAIL Aeaea")
        finally:
            ship.close()
        cluster.check_alive()
    finally:
        for c in conns:
            c.close()
        cluster.stop()
        cluster.cleanup(keep)


# --------------------------------------------------------- states and assignment

def scenario_wrongstate(keep):
    """PHASE2 'Connect and list voyages', 'Concurrent acceptance' (active mission, third ship),
    'Leave Ithaca' (only Aeaea from Ithaca); PHASE3 'Delivery or claim in the wrong state';
    GLOBAL 'Concurrent mission assignment' (sailing does not cancel the assignment)."""
    cluster = Cluster(islands=GLOBAL_ISLANDS)
    ships = []
    try:
        cluster.start()
        a = Ship(cluster, "Polyphemus")
        b = Ship(cluster, "Telemachus")
        ships = [a, b]
        assert "not connected" in a.send("LIST VOYAGES")
        assert "not connected" in a.send("ACCEPT 1")
        assert "Reward" not in a.send("CLAIM")
        assert "ERROR" in a.send("DELIVER"), "DELIVER with no mission at Ithaca"
        a.send("CONNECT ITHACA")
        vid = voyage_id(a, "TridentOfPoseidon")
        assert vid, "TridentOfPoseidon not listed"
        other = [i for i in listed_ids(a) if i != vid][0]
        assert "can't sail" in a.send("SAIL Scheria"), "sailed from Ithaca to Scheria"
        assert "can't sail" in a.send("SAIL Ithaca"), "sailed from Ithaca to Ithaca"
        assert "accepted" in a.send("ACCEPT %d" % vid)
        assert "active voyage" in a.send("ACCEPT %d" % other), "second mission accepted"
        assert "ERROR" in a.send("DELIVER"), "DELIVER before arriving"
        assert "Reward" not in a.send("CLAIM"), "CLAIM before delivering"
        b.send("CONNECT ITHACA")
        assert vid not in listed_ids(b), "an assigned voyage is still listed"
        assert "ERROR" in b.send("ACCEPT %d" % vid), "the same voyage was assigned twice"
        assert "granted access" in a.send("SAIL Aeaea")
        assert "not the destination" in a.send("DELIVER")
        assert "not connected" in a.send("CLAIM"), "CLAIM without an Ithaca connection"
        assert vid not in listed_ids(b), "sailing cancelled the assignment"
        gold, _, _, out = status(a)
        assert gold == 250 and "TridentOfPoseidon" in out, out
        assert len([l for l in cluster.log("ithaca") if "assigned to" in l]) == 1
        cluster.check_alive()
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_waiting(keep):
    """PHASE2 'Full port and FIFO queue', 'Provisions' (100 at 4 s, 93 at 5 s, 86 at 10 s;
    docked and sailing ships eat nothing), 'Commands while waiting and disconnection' (STATUS
    and MAP only; a queued client disconnects); PHASE3 DELIVER while waiting for a berth;
    GLOBAL 'Capacity, queue and provisions'."""
    cluster = Cluster(islands=GLOBAL_ISLANDS)  # Aeaea capacity 1
    ships = []
    try:
        cluster.start()
        holder = Ship(cluster, "Polyphemus")
        first = Ship(cluster, "Telemachus")
        quitter = Ship(cluster, "Eumaeus")
        last = Ship(cluster, "Penelope")
        ships = [holder, first, quitter, last]
        for s in ships:
            s.send("CONNECT ITHACA")
        first.send("ACCEPT %d" % voyage_id(first, "AeolusBagOfWinds"))
        assert "granted access" in holder.send("SAIL Aeaea")
        start = time.time()
        for s in ships[1:]:
            s.ask("SAIL Aeaea", "harbor is full")
            time.sleep(0.3)
        assert time.time() - start < 4, "the WAIT replies were slow"
        time.sleep(max(0, start + 3.8 - time.time()))
        out = first.ask("STATUS", "Total provisions")
        assert "Total provisions: 100 kg" in out, "before 5 s of waiting: %r" % out
        for line in ("LIST MARKET", "BUY DriedFish 1", "SELL Barley 1", "DELIVER"):
            out = first.ask(line, "not docked yet", timeout=3)
            assert "Bought" not in out and "Sold" not in out, "%r while waiting: %r" % (line, out)
        out = first.ask("MAP", "", timeout=1)
        time.sleep(0.5)
        assert "not docked yet" not in first.buf[first.pos:], "MAP refused while waiting"
        first.wait_ever("Provisions remaining: 93 kg", timeout=3)
        t93 = time.time() - start
        quitter.kill9()  # a queued client disconnects
        first.wait_ever("Provisions remaining: 86 kg", timeout=7)
        t86 = time.time() - start
        assert 4.4 <= t93 < 6.5 and 9.4 <= t86 < 11.5, "93 kg at %.1fs, 86 kg at %.1fs" % (
            t93, t86)
        # the docked ship is still served and has eaten nothing
        goods, _, _ = market(holder)
        assert goods, "the docked ship is not served while others wait"
        assert status(holder)[1] == 100, "a docked ship consumed food"
        holder.send("BUY MAP 1")
        assert "granted access" in holder.send("SAIL Scheria")
        first.wait_ever("granted access", timeout=5)
        first.wait_prompt()
        assert status(holder)[1] == 100, "sailing consumed food"
        assert status(first)[1] == 86
        first.send("BUY MAP 1")
        first.send("SAIL Scheria")
        last.wait_ever("granted access", timeout=5)
        last.wait_prompt()
        log = cluster.log("aeaea")
        sent = re.findall(r"DOCKED sent to (\S+);", "\n".join(log))
        assert sent == ["Telemachus", "Penelope"], "DOCKED order %s" % sent
        docked = 0
        for line in log:
            if "entered the port" in line or "DOCKED sent to" in line:
                docked += 1
            elif "has left" in line:
                docked -= 1
            assert docked <= 1, "capacity 1 exceeded: " + line
        cluster.check_alive()
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)


# ----------------------------------------------------------------- market and stock

def scenario_market(keep):
    """PHASE2 'Market, trade and map' and 'Errors' (invalid product/amount, no funds, no stock,
    selling more than carried, BUY MAP 2, SELL MAP); GLOBAL 'Trade and stock consistency'
    (Aeaea part), 'Maps and navigation' (each map 50, no cargo or stock change)."""
    cluster = Cluster(islands=GLOBAL_ISLANDS)
    ships = []
    try:
        cluster.start()
        ship = Ship(cluster, "Polyphemus")
        rich = Ship(cluster, "Croesus", gold=100000)
        ships = [ship, rich]
        ship.send("CONNECT ITHACA")
        assert "granted access" in ship.send("SAIL Aeaea")
        goods, map_price, out = market(ship)
        assert len(goods) == 8, "LIST MARKET shows %d foods: %r" % (len(goods), out)
        assert map_price == 50 and goods["DriedFish"] == (90, 15), out
        gold = []
        ship.send("BUY DriedFish 5")
        gold.append(status(ship)[0])
        ship.send("SELL DriedFish 2")
        gold.append(status(ship)[0])
        ship.send("BUY MAP 1")
        gold.append(status(ship)[0])
        ship.send("BUY MAP 1")
        g, prov, cargo, _ = status(ship)
        gold.append(g)
        assert gold == [175, 205, 155, 105], "balances %s" % gold
        assert cargo.get("DriedFish") == 3 and prov == 103, "cargo %s provisions %d" % (cargo, prov)
        assert "MAP" not in cargo and "Map" not in cargo, "a map became cargo"
        assert market(ship)[0]["DriedFish"][0] == 87

        snapshot = status(ship)[:3]
        stock_before = market(ship)[0]
        bad = ["BUY Unicorn 1", "BUY DriedFish 0", "BUY DriedFish -1", "BUY DriedFish 1000",
               "SELL WildHoney 1", "SELL Barley 1000", "SELL Unicorn 1", "BUY MAP 2",
               "SELL MAP 1", "SELL DriedFish 4"]
        for line in bad:
            out = ship.send(line)
            assert "Bought" not in out and "Sold" not in out, "%r succeeded: %r" % (line, out)
        assert status(ship)[:3] == snapshot, "a rejected trade changed the ship"
        assert market(ship)[0] == stock_before, "a rejected trade changed the stock"
        # not enough stock (with plenty of gold)
        rich.send("CONNECT ITHACA")
        holder_leaves = ship.send("SAIL Scheria")  # capacity 1: make room for the rich ship
        assert "granted access" in holder_leaves
        assert "granted access" in rich.send("SAIL Aeaea")
        out = rich.send("BUY EnchantedHerbs 61")
        assert "Bought" not in out, "bought more than the stock: %r" % out
        assert status(rich)[0] == 100000
        rich.close()
        cluster.stop()
        records = read_stock_full(os.path.join(cluster.dir, "stocks", "Aeaea.db"))
        names = [r[0] for r in records]
        assert len(records) == 8 and "MAP" not in [n.upper() for n in names], records
        assert dict((r[0], r[1]) for r in records)["DriedFish"] == 87, records
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_buyrace(keep):
    """PHASE2 'Errors and concurrent purchase', GLOBAL 'Trade and stock consistency' (Scheria):
    two ships buy 4 kg of a 5 kg stock at once: one succeeds, 1 kg left in server and file."""
    scheria = [(n, 5 if n == "DriedFish" else a, p)
               for n, a, p in read_stock_full(os.path.join(ROOT, "stocks", "Scheria.db"))]
    cluster = Cluster(islands=GLOBAL_ISLANDS, stocks={"Scheria": scheria})
    ships = []
    try:
        cluster.start()
        ships = [Ship(cluster, "Polyphemus"), Ship(cluster, "Telemachus")]
        for s in ships:  # Aeaea has one berth: one ship at a time
            s.send("CONNECT ITHACA")
            sail_route(s, "Aeaea", "Scheria")
        barrier = threading.Barrier(2)

        def buy(ship):
            barrier.wait()
            return ship.send("BUY DriedFish 4")

        outs = run_parallel(buy, ships)
        ok = [o for o in outs if "Bought 4 kg of DriedFish" in o]
        assert len(ok) == 1, "purchases that succeeded: %r" % outs
        assert market(ships[0])[0]["DriedFish"][0] == 1
        cluster.check_alive()
        for s in ships:
            s.close()
        cluster.stop()
        final = dict((r[0], r[1]) for r in
                     read_stock_full(os.path.join(cluster.dir, "stocks", "Scheria.db")))
        assert final["DriedFish"] == 1, "stock.db has %d kg" % final["DriedFish"]
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)


# ----------------------------------------------------------------- routes and maps

def scenario_routes(keep):
    """PHASE2 'Maps and direct routes' (Aeaea-Aeolia filtered, Ismarus known but not adjacent to
    Aeolia, back to Scheria); GLOBAL 'Known island, no direct connection' (Ogygia from Aeaea)
    and 'Maps and navigation' (only Aeaea from Ithaca)."""
    islands = dict(GLOBAL_ISLANDS)
    islands["Aeaea"] = (1, ["Scheria", "Thrinacia", "Aeolia"])  # Aeolia: an invalid link
    cluster = Cluster(islands=islands)
    ships = []
    try:
        cluster.start()
        assert server_logged(cluster, "aeaea", "2 sea routes loaded"), \
            "SPHRAGIS did not filter Aeaea-Aeolia: %s" % cluster.log("aeaea")
        a = Ship(cluster, "Polyphemus", gold=1000)
        b = Ship(cluster, "Telemachus", gold=1000)
        ships = [a, b]
        a.send("CONNECT ITHACA")
        for island in ("Scheria", "Thrinacia", "Ogygia", "Aeolia"):
            assert "can't sail" in a.send("SAIL %s" % island), "Ithaca -> %s allowed" % island
        sail_route(a, "Aeaea", "Thrinacia", "Ogygia")
        assert "granted access" in a.send("SAIL Thrinacia")
        assert "granted access" in a.send("SAIL Aeaea")
        assert "can't sail" in a.send("SAIL Ogygia"), "sailed Aeaea -> Ogygia (no direct link)"
        assert "Current location: Aeaea" in status(a)[3]
        assert "can't sail" in a.send("SAIL Aeolia"), "the filtered Aeaea-Aeolia link was sent"
        assert "granted access" in a.send("SAIL Thrinacia")

        b.send("CONNECT ITHACA")
        sail_route(b, "Aeaea", "Scheria", "Aeolia")
        assert "can't sail" in b.send("SAIL Ismarus"), "sailed Aeolia -> Ismarus"
        assert "granted access" in b.send("SAIL Scheria")
        assert "granted access" in b.send("SAIL Ismarus")
        cluster.check_alive()
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)


# -------------------------------------------------------------- the Trident mission

def scenario_trident(keep):
    """PHASE3 rows 1-3, GLOBAL 'Complete mission and reward': TridentOfPoseidon -> Scheria
    [1500] with the real ship; final 1700 gold, 100 kg, no mission; CLAIM again pays nothing."""
    cluster = Cluster(islands=GLOBAL_ISLANDS)
    ship = None
    try:
        cluster.start()
        ship = Ship(cluster, "Polyphemus")
        ship.send("CONNECT ITHACA")
        vid = voyage_id(ship, "TridentOfPoseidon")
        out = ship.send("ACCEPT %d" % vid)
        assert "Object received correctly" in out, out
        assert md5_of(ship.file(".object")) == md5_of(
            os.path.join(cluster.dir, cluster.object_of("TridentOfPoseidon")))
        sail_route(ship, "Aeaea", "Scheria")
        _, _, _, out = status(ship)
        assert "TridentOfPoseidon -> Scheria" in out and "Scheria" in out, out
        out = ship.send("DELIVER")
        assert out.index("Object transferred correctly") < out.index("Stamped object received") \
            < out.index("Delivery completed"), out
        assert "Glyph" not in out
        out = ship.send("SAIL Ithaca")
        assert "Returning directly to Ithaca" in out, out
        assert "Current location: Ithaca" in status(ship)[3]
        assert "not connected" in ship.send("CLAIM"), "CLAIM before CONNECT ITHACA"
        out = ship.send("CONNECT ITHACA")
        assert "Connected to Ithaca" in out and "Leaving" not in out, out
        out = ship.send("CLAIM")
        assert "Reward received: 1500 gold" in out, out
        gold, prov, _, out = status(ship)
        assert gold == 1700 and prov == 100 and "Current voyage: None" in out, out
        out = ship.send("CLAIM")
        assert "Reward" not in out, "second CLAIM: %r" % out
        assert status(ship)[0] == 1700, "second CLAIM paid again"
        assert len(completed(cluster, 1)) == 1
        assert server_logged(cluster, "scheria", "sealed with the glyph of Scheria")
        no_zombies(cluster)
        cluster.check_alive()
    finally:
        if ship:
            ship.close()
        cluster.stop()
        cluster.cleanup(keep)


# ---------------------------------------------------------- death at sea / stranded

def scenario_deathmission(keep):
    """PHASE2 'Provisions and death at sea' (9 kg), PHASE3 / GLOBAL 'Death at sea': 2 kg at 5 s,
    0 at 10 s, the process exits, the object is lost, the queue slot is freed; on restart the
    ship has its initial resources and Ithaca keeps the assignment until LIST VOYAGES, which
    marks it failed."""
    cluster = Cluster(islands=GLOBAL_ISLANDS)
    ships = []
    try:
        cluster.start()
        holder = Ship(cluster, "Polyphemus")
        victim = Ship(cluster, "Eumaeus", cargo=[("Barley", 9)])
        watcher = Ship(cluster, "Telemachus")
        ships = [holder, victim, watcher]
        holder.send("CONNECT ITHACA")
        holder.send("SAIL Aeaea")
        victim.send("CONNECT ITHACA")
        vid = voyage_id(victim, "AeolusBagOfWinds")
        assert "Object received" in victim.send("ACCEPT %d" % vid)
        assert os.path.exists(victim.file(".object"))
        victim.ask("SAIL Aeaea", "harbor is full")
        victim.wait_ever("lost at sea", timeout=15)
        left = [int(x) for x in re.findall(r"Provisions remaining: (\d+) kg", victim.buf)]
        assert left == [2, 0], "provisions seen %s, expected [2, 0]" % left
        deadline = time.time() + 5
        while victim.alive() and time.time() < deadline:
            time.sleep(0.2)
        assert not victim.alive(), "the lost ship did not exit"
        assert not os.path.exists(victim.file(".object")), "the object survived the ship"
        # the queue no longer holds the dead ship: the next arrival docks when the berth frees
        watcher.send("CONNECT ITHACA")
        assert vid not in listed_ids(watcher), "the voyage was freed before the ship listed"
        watcher.ask("SAIL Aeaea", "harbor is full")
        holder.send("BUY MAP 1")
        holder.send("SAIL Scheria")
        watcher.wait_ever("granted access", timeout=5)
        watcher.wait_prompt()
        assert "Eumaeus" not in re.findall(r"DOCKED sent to (\S+);", "\n".join(cluster.log("aeaea")))

        victim.restart()
        gold, prov, cargo, out = status(victim)
        assert gold == 250 and prov == 9 and "Current voyage: None" in out, out
        victim.send("CONNECT ITHACA")
        assert vid not in listed_ids(victim), "the failed voyage is listed again"
        assert vid not in listed_ids(watcher), "the failed voyage became available"
        cluster.check_alive()
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_stranded(keep):
    """PHASE3 / GLOBAL 'stranded without resources': docked with no gold and nothing to sell,
    CTRL+C; restart with initial resources, object deleted, mission failed on LIST VOYAGES."""
    cluster = Cluster(islands=GLOBAL_ISLANDS)
    ship = None
    try:
        cluster.start()
        # 50 gold - the map = no gold and no cargo: the dead end of spec 6.8
        ship = Ship(cluster, "Polyphemus", gold=50, cargo=[])
        ship.send("CONNECT ITHACA")
        vid = voyage_id(ship, "AeolusBagOfWinds")
        assert "Object received" in ship.send("ACCEPT %d" % vid)
        assert "granted access" in ship.send("SAIL Aeaea")
        assert "Bought the map" in ship.send("BUY MAP 1")
        for line in ("BUY SeaSalt 1", "BUY MAP 1", "SELL Barley 1"):
            out = ship.send(line)
            assert "Bought" not in out and "Sold" not in out, "%r: %r" % (line, out)
        ship.close()
        assert ship.proc.returncode == 0
        assert not os.path.exists(ship.file(".object")), "object kept after CTRL+C"
        assert server_logged(cluster, "aeaea", "Polyphemus has left"), "berth not freed"
        ship.restart()
        gold, prov, _, out = status(ship)
        assert gold == 50 and prov == 0 and "Current voyage: None" in out, \
            "restart after being stranded: %r" % out
        ship.send("CONNECT ITHACA")
        assert vid not in listed_ids(ship), "the abandoned voyage is listed again"
        cluster.check_alive()
    finally:
        if ship:
            ship.close()
        cluster.stop()
        cluster.cleanup(keep)


# ---------------------------------------------------------------------- persistence

def scenario_persist_trade(keep):
    """PHASE3 'Odysseus persistence' (after trading, after claiming), GLOBAL 'Persistence
    across runs' (balance and cargo recovered, reward neither lost nor duplicated)."""
    cluster = Cluster(islands=GLOBAL_ISLANDS)
    ship = None
    try:
        cluster.start()
        ship = Ship(cluster, "Polyphemus")
        ship.send("CONNECT ITHACA")
        vid = voyage_id(ship, "AeolusBagOfWinds")
        ship.send("ACCEPT %d" % vid)
        sail_route(ship, "Aeaea", "Scheria")
        ship.send("BUY DriedFish 5")
        traded = status(ship)[:3]
        ship.restart()
        assert status(ship)[:3] == traded, "trade not recovered after restart"
        # the voyage is not part of this check: reconnect from Ithaca and claim with a new ship
        ship.close()
        assert ship.proc.returncode == 0
        ship2 = Ship(cluster, "Telemachus")
        ship = ship2
        ship.send("CONNECT ITHACA")
        vid = voyage_id(ship, "TridentOfPoseidon")
        ship.send("ACCEPT %d" % vid)
        sail_route(ship, "Aeaea", "Scheria")
        ship.send("DELIVER")
        ship.send("CONNECT ITHACA")
        assert "Reward received: 1500" in ship.send("CLAIM")
        paid = status(ship)[:3]
        ship.restart()
        assert status(ship)[:3] == paid, "claimed reward lost on restart"
        ship.send("CONNECT ITHACA")
        assert "Reward" not in ship.send("CLAIM")
        assert status(ship)[0] == paid[0], "reward paid twice after restart"
        assert len(completed(cluster, 1)) == 1
    finally:
        if ship:
            ship.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_persist_mission(keep):
    """PHASE3 'Odysseus persistence' (after receiving OBJECT, after receiving SIGNED) and GLOBAL
    'Persistence across runs' (exit after the receipt but before CLAIM, Ithaca kept running):
    mission and its file must be recovered and the CLAIM must still pay once. A restored
    mission is not failed by an accidental LIST VOYAGES, and a saved voyage whose files were
    deleted is dropped with a warning (gold and cargo are still restored)."""
    cluster = Cluster(islands=GLOBAL_ISLANDS)
    ship = None
    try:
        cluster.start()
        ship = Ship(cluster, "Polyphemus")
        ship.send("CONNECT ITHACA")
        vid = voyage_id(ship, "TridentOfPoseidon")
        ship.send("ACCEPT %d" % vid)
        obj_md5 = md5_of(ship.file(".object"))
        ship.restart()
        out = status(ship)[3]
        assert "TridentOfPoseidon" in out and "Object: on board" in out and \
            os.path.exists(ship.file(".object")), \
            "after OBJECT + restart: mission/object not recovered: %r" % out
        assert md5_of(ship.file(".object")) == obj_md5, "object changed across the restart"
        ship.send("CONNECT ITHACA")
        assert "active voyage" in ship.send("LIST VOYAGES"), "LIST allowed with a mission"
        sail_route(ship, "Aeaea", "Scheria")
        ship.send("BUY DriedFish 2")
        traded = status(ship)[:3]
        assert "mark of Scheria" in ship.send("DELIVER")
        ship.restart()
        gold, prov, cargo, out = status(ship)
        assert "TridentOfPoseidon" in out and "sealed by Scheria" in out and \
            os.path.exists(ship.file(".signed")), \
            "after SIGNED + restart: receipt not recovered: %r" % out
        assert (gold, prov, cargo) == traded, "cargo/gold not recovered with the mission"
        ship.send("CONNECT ITHACA")
        assert "Reward received: 1500" in ship.send("CLAIM"), "CLAIM after restart failed"
        ship.restart()
        out = status(ship)[3]
        assert "Current voyage: None" in out and status(ship)[0] == traded[0] + 1500, out
        assert not os.path.exists(ship.file(".object")) and not os.path.exists(
            ship.file(".signed")), "voyage files left after the reward"
        ship.send("CONNECT ITHACA")
        assert "Reward" not in ship.send("CLAIM"), "paid twice"
        assert len(completed(cluster, 1)) == 1

        # a saved voyage whose object was deleted between runs is dropped
        other = Ship(cluster, "Telemachus")
        try:
            other.send("CONNECT ITHACA")
            assert "Object received" in other.send(
                "ACCEPT %d" % voyage_id(other, "AeolusBagOfWinds"))
            other.close()
            os.remove(other.file(".object"))
            other._spawn()
            assert "files of the saved voyage are missing" in other.buf, other.buf
            gold, prov, _, out = status(other)
            assert "Current voyage: None" in out and gold == 250 and prov == 100, out
        finally:
            other.close()
    finally:
        if ship:
            ship.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_islandrestart(keep):
    """PHASE3 'Island persistence' (LIST MARKET after CTRL+C + restart, independent stocks),
    GLOBAL 'Ithaca or island crash' (clients report the error without hanging, servers restart
    and serve new connections, a restarted island restores its saved stock)."""
    cluster = Cluster(islands=GLOBAL_ISLANDS)
    ships = []
    try:
        cluster.start()
        trader = Ship(cluster, "Polyphemus")
        ships = [trader]
        trader.send("CONNECT ITHACA")
        trader.send("SAIL Aeaea")
        trader.send("BUY DriedFish 5")
        trader.send("SELL Barley 3")
        expected, _, _ = market(trader)
        scheria_before = read_stock_full(os.path.join(cluster.dir, "stocks", "Scheria.db"))

        assert cluster.stop_proc("aeaea") == 0, "island did not exit cleanly on CTRL+C"
        out = trader.send("LIST MARKET", timeout=10)
        assert "--- AEAEA MARKET ---" not in out or "ERROR" in out, \
            "no error from a dead island: %r" % out
        assert trader.alive(), "the ship died with the island"
        cluster.start_proc("aeaea")
        newcomer = Ship(cluster, "Telemachus")
        ships.append(newcomer)
        newcomer.send("CONNECT ITHACA")
        assert "granted access" in newcomer.send("SAIL Aeaea"), "restarted island refuses ships"
        assert market(newcomer)[0] == expected, "stock not restored after restart"
        assert read_stock_full(os.path.join(cluster.dir, "stocks", "Scheria.db")) == \
            scheria_before, "another island's stock changed"

        # Ithaca stopped while a ship is connected (SIGINT: a SIGKILL would leak its semaphores)
        talker = Ship(cluster, "Penelope")
        ships.append(talker)
        talker.send("CONNECT ITHACA")
        assert cluster.stop_proc("ithaca") == 0, "Ithaca did not exit cleanly with a client"
        out = talker.send("LIST VOYAGES", timeout=10)
        assert "AVAILABLE VOYAGES" not in out, "listing from a dead Ithaca: %r" % out
        assert talker.alive()
        cluster.start_proc("ithaca")
        out = talker.send("CONNECT ITHACA")
        if "Already connected" in out:
            out = talker.send("LIST VOYAGES")
        else:
            assert "Connected to Ithaca" in out, "reconnect after restart: %r" % out
            out = talker.send("LIST VOYAGES")
        assert "AVAILABLE VOYAGES" in out, "Ithaca unusable after restart: %r" % out
        cluster.check_alive()
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)


# --------------------------------------------------------------- transfer failures

def scenario_flipbyte(keep):
    """GLOBAL 'File integrity': one altered byte with the announced hash kept -> MD5_ERROR
    in DELIVERY and CLAIM, nothing sealed, no reward."""
    corrupt_scenario(keep, {"flip_byte": True}, "MD5_ERROR")


def scenario_cutoff(keep):
    """PHASE3 'Corrupt file or interrupted transfer': the connection closes during DELIVERY,
    SIGNED and CLAIM (OBJECT is peerdies); nothing is sealed or paid and the servers go on."""
    cluster = Cluster()
    conn = None
    try:
        cluster.set_object("AeolusBagOfWinds", 300_000)
        cluster.start()
        conn = ithaca_login(cluster, "Cutter")
        obj_name, obj_data = accept_voyage(conn, 1)

        # DELIVERY cut after 3 frames
        isl = Proto(cluster.ip, cluster.ports["scheria"])
        assert isl.request(MSG_ARRIVAL, "Cutter")[2] == b"DOCKED"
        isl.send(MSG_DELIVER, 0x00, "1&AeolusBagOfWinds")
        assert isl.send_transfer("DELIVERY", obj_name, obj_data, stall_after=3) == "stalled"
        isl.close()
        time.sleep(1)
        assert not server_logged(cluster, "scheria", "sealed", 1), "sealed a partial image"
        no_temp_files(cluster)
        assert records(cluster, "scheria", "Cutter") == [], "partial delivery kept"

        # SIGNED cut: the ship stops reading the sealed image after 3 frames
        isl = Proto(cluster.ip, cluster.ports["scheria"])
        assert isl.request(MSG_ARRIVAL, "Cutter")[2] == b"DOCKED"
        isl.send(MSG_DELIVER, 0x00, "1&AeolusBagOfWinds")
        assert isl.send_transfer("DELIVERY", obj_name, obj_data) == "ok"
        assert isl.receive_transfer("SIGNED", stop_after=3) is None
        isl.close()
        time.sleep(1)
        cluster.check_alive()
        no_temp_files(cluster)
        assert records(cluster, "scheria", "Cutter") == [], "unconfirmed delivery kept"

        # a complete delivery, then the CLAIM cut after 3 frames
        result, signed, final = island_deliver(cluster, "scheria", "Cutter", 1,
                                               "AeolusBagOfWinds", obj_name, obj_data)
        assert result == "ok" and is_ok(final)
        conn.send(MSG_CLAIM, 0x00, "1")
        assert conn.send_transfer("CLAIM", obj_name, signed[1], stall_after=3) == "stalled"
        conn.close()
        conn = None
        time.sleep(1)
        assert not completed(cluster), "voyage completed with a cut CLAIM"
        no_temp_files(cluster)
        assert records(cluster, "ithaca", "Cutter") == [], "partial claim kept"
        cluster.check_alive()
        # the ship comes back and claims properly (assignment kept: no LIST VOYAGES)
        conn = ithaca_login(cluster, "Cutter")
        result, final = claim_voyage(conn, 1, obj_name, signed[1])
        assert is_ok(final) and final[2] == b"OK&1150", "claim after a cut one: %s" % (final,)
        assert len(completed(cluster, 1)) == 1
    finally:
        if conn:
            conn.close()
        cluster.stop()
        cluster.cleanup(keep)


class FakeIthaca:
    """A listening socket that plays Ithaca for one real Odysseus (bad OBJECT transfers)."""

    def __init__(self, cluster):
        self.srv = socket.socket()
        self.srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.srv.bind((cluster.ip, 0))
        self.srv.listen(1)
        self.port = self.srv.getsockname()[1]
        self.conn = None

    def serve(self, fault):
        """Answers CONNECT, LIST and ACCEPT, then sends a faulty OBJECT. Returns the ACK."""
        sock, _ = self.srv.accept()
        self.conn = Proto.__new__(Proto)
        self.conn.sock = sock
        sock.settimeout(20)
        data = os.urandom(5000)
        while True:
            kind, _, _ = self.conn.recv()
            if kind == MSG_CONNECT:
                self.conn.send(MSG_CONNECT, FLAG_RESPONSE, "OK")
            elif kind == MSG_LIST:
                self.conn.send(MSG_LIST, FLAG_RESPONSE, "1&1&1&Fake&Scheria&100")
            elif kind == MSG_ACCEPT:
                self.conn.send(MSG_ACCEPT, FLAG_RESPONSE, "OK&1&Fake&Scheria&100")
                md5 = hashlib.md5(data).hexdigest()
                size = len(data)
                if fault == "md5":
                    md5 = "0" * 32
                self.conn.send(MSG_HEADER, 0x80, "OBJECT&fake.png&%d&%s" % (size, md5))
                ready = self.conn.recv()
                assert ready[0] == MSG_READY, ready
                chunks = [data[i:i + DATA_MAX] for i in range(0, len(data), DATA_MAX)]
                if fault == "cut":
                    for chunk in chunks[:3]:
                        self.conn.send(MSG_DATA, 0xA0, chunk)
                    sock.close()
                    return None
                if fault == "size":
                    chunks[-1] = chunks[-1][:-1]
                for chunk in chunks:
                    self.conn.send(MSG_DATA, 0xA0, chunk)
                return self.conn.recv()

    def close(self):
        if self.conn:
            self.conn.close()
        self.srv.close()


def scenario_badobject(keep):
    """PHASE3 'Corrupt file or interrupted transfer' on the Odysseus side: an OBJECT with a bad
    MD5, a bad size or a cut connection is refused (MD5_ERROR / SIZE_ERROR), no file is kept,
    no mission is taken, and the ship keeps working."""
    cluster = Cluster()
    try:
        for fault, ack in (("md5", "MD5_ERROR"), ("size", "SIZE_ERROR"), ("cut", None)):
            fake = FakeIthaca(cluster)
            ship = Ship(cluster, "Victim", ithaca_port=fake.port)
            result = {}
            t = threading.Thread(target=lambda: result.update(ack=fake.serve(fault)))
            t.start()
            try:
                ship.send("CONNECT ITHACA")
                ship.send("LIST VOYAGES")
                out = ship.send("ACCEPT 1", timeout=30)
                t.join(30)
                if ack:
                    got = result.get("ack")
                    assert got and got[0] == MSG_ACK and got[1] & FLAG_ERROR and \
                        got[2] == ack.encode(), "%s: ship ACKed %s" % (fault, got)
                assert "Object received correctly" not in out, "%s: %r" % (fault, out)
                assert not os.path.exists(ship.file(".object")), "%s: partial object kept" % fault
                assert "Current voyage: None" in status(ship)[3], "%s: mission kept" % fault
                assert ship.alive()
            finally:
                ship.close()
                fake.close()
                shutil.rmtree(ship.folder, ignore_errors=True)
    finally:
        cluster.cleanup(keep)


def scenario_fileerrors(keep):
    """PHASE3 'File errors and simultaneous transfers' (missing source file, unwritable
    destinations), PHASE2 / GLOBAL failed connections (IP/port with no server)."""
    cluster = Cluster()
    ships = []
    try:
        cluster.start()
        # missing object image at Ithaca
        os.remove(os.path.join(cluster.dir, cluster.object_of("LotusEaterLotus")))
        a = Ship(cluster, "Polyphemus")
        ships.append(a)
        a.send("CONNECT ITHACA")
        vid = voyage_id(a, "LotusEaterLotus")
        out = a.send("ACCEPT %d" % vid)
        assert "Object received correctly" not in out and "ERROR" in out, out
        assert "Current voyage: None" in status(a)[3]
        cluster.check_alive()
        # the ship folder is not writable: the OBJECT cannot be stored
        if os.geteuid() != 0:
            b = Ship(cluster, "Telemachus")
            ships.append(b)
            os.chmod(b.folder, 0o500)
            try:
                b.send("CONNECT ITHACA")
                out = b.send("ACCEPT %d" % voyage_id(b, "AeolusBagOfWinds"))
                assert "Object received correctly" not in out, out
                assert "Current voyage: None" in status(b)[3]
            finally:
                os.chmod(b.folder, 0o700)
            assert server_logged(cluster, "ithaca", "The voyage is free again")
            # the island folder is not writable: DELIVER fails cleanly
            c = Ship(cluster, "Penelope")
            ships.append(c)
            c.send("CONNECT ITHACA")
            assert "Object received" in c.send("ACCEPT %d" % voyage_id(c, "AeolusBagOfWinds"))
            sail_route(c, "Aeaea", "Scheria")
            folder = os.path.join(cluster.dir, "scheria_port")
            os.chmod(folder, 0o500)
            try:
                out = c.send("DELIVER")
            finally:
                os.chmod(folder, 0o700)
            assert "mark of Scheria" not in out and "ERROR" in out, out
            assert c.alive()
            # the island's final DELIVER error was consumed: the ship stays in step
            out = c.send("LIST MARKET")
            assert "Unexpected" not in out and "ERROR" not in out, out
            out = c.send("DELIVER")
            assert "mark of Scheria" in out, out
            cluster.check_alive()
        # nothing listening: CONNECT ITHACA and SAIL fail without killing the ship
        d = Ship(cluster, "Eumaeus", ithaca_port=free_port(), aeaea_port=free_port())
        ships.append(d)
        assert "ERROR" in d.send("CONNECT ITHACA")
        assert "granted access" not in d.send("SAIL Aeaea")
        assert d.alive()
        no_temp_files(cluster)
        cluster.check_alive()
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)


# --------------------------------------------------------------------- robustness

def scenario_slowclient(keep):
    """GLOBAL 'Resources and repeated runs': a client that sends half a frame does not block
    the server for others beyond FRAME_READ_TIMEOUT_SECONDS (5 s, code_audit.md §6)."""
    cluster = Cluster()
    slow = None
    ship = None
    try:
        cluster.start()
        for server in ("ithaca", "aeaea"):
            slow = socket.create_connection((cluster.ip, cluster.ports[server]))
            slow.sendall(bytes([MSG_CONNECT if server == "ithaca" else MSG_ARRIVAL, 0, 4]) + b"Sl")
            time.sleep(0.2)
            ship = Ship(cluster, "Patient")
            start = time.time()
            if server == "ithaca":
                out = ship.send("CONNECT ITHACA", timeout=10)
                assert "Connected" in out, out
            else:
                ship.send("CONNECT ITHACA")
                out = ship.send("SAIL Aeaea", timeout=10)
                assert "granted access" in out, out
            took = time.time() - start
            assert took < 7, "%s blocked by a half frame for %.1fs" % (server, took)
            print("      %s served another ship %.1fs after a half frame" % (server, took))
            ship.close()
            ship = None
            slow.close()
            slow = None
        cluster.check_alive()
    finally:
        if slow:
            slow.close()
        if ship:
            ship.close()
        cluster.stop()
        cluster.cleanup(keep)


def scenario_cycles(keep):
    """GLOBAL 'Resources and repeated runs' and 'Client disconnection': repeated missions,
    trades and CTRL+C / kill -9 of clients. Server descriptors do not grow, memory stays flat,
    no zombie md5sum children, no busy waiting (idle server and waiting ship use ~0 CPU)."""
    cluster = Cluster(aeaea_capacity=1)
    ships = []
    try:
        cluster.start()
        pids = {n: p.pid for n, p in cluster.procs.items()}
        samples = []
        for cycle in range(6):
            ship = Ship(cluster, "Cycle%d" % cycle)
            ships.append(ship)
            ship.send("CONNECT ITHACA")
            ship.send("LIST VOYAGES")
            assert "Object received" in ship.send("ACCEPT %d" % (cycle + 1))
            sail_route(ship, "Aeaea")
            ship.send("BUY DriedFish 1")
            if cycle % 2:
                ship.kill9()  # crash while docked, with a mission
            else:
                ship.close()
            time.sleep(0.5)
            no_zombies(cluster)
            samples.append({n: (fd_count(p), rss_kb(p)) for n, p in pids.items()})
        for name in pids:
            fds = [s[name][0] for s in samples]
            assert fds[-1] <= fds[1], "%s descriptors grow: %s" % (name, fds)
            rss = [s[name][1] for s in samples]
            assert rss[-1] - rss[1] < 1024, "%s memory grows: %s kB" % (name, rss)
        # busy waiting: an idle cluster and a ship waiting for a berth use ~no CPU
        holder = Ship(cluster, "Holder")
        waiter = Ship(cluster, "Waiter")
        ships += [holder, waiter]
        holder.send("CONNECT ITHACA")
        holder.send("SAIL Aeaea")
        waiter.send("CONNECT ITHACA")
        waiter.ask("SAIL Aeaea", "harbor is full")
        before = {n: cpu_ticks(p) for n, p in pids.items()}
        before["waiter"] = cpu_ticks(waiter.proc.pid)
        time.sleep(3)
        after = {n: cpu_ticks(p) for n, p in pids.items()}
        after["waiter"] = cpu_ticks(waiter.proc.pid)
        hz = os.sysconf("SC_CLK_TCK")
        for name in before:
            used = (after[name] - before[name]) / hz
            assert used < 0.3, "%s used %.2fs of CPU in 3 s idle (busy waiting?)" % (name, used)
        cluster.check_alive()
    finally:
        for s in ships:
            s.close()
        cluster.stop()
        cluster.cleanup(keep)


FORBIDDEN = re.compile(
    r"\b(?:f|s|sn|v|vf|vs|d)?printf\s*\(|\bv?f?scanf\s*\(|\bputs\s*\(|\bgets\s*\(|\bsystem\s*\("
    r"|\bpopen\s*\(|\b[lf]?stat\s*\(|\bgoto\b|\bcalloc\s*\(|\b_exit\s*\(|\babort\s*\("
    r"|\bvasprintf\b|=\s*\{\s*0?\s*\}")


def strip_c(text):
    """C source without comments and string/char literals (for the token checks)."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', '""', text)


def scenario_static(keep):
    """GLOBAL 'Final review' (the scriptable part): every source compiles with -Wall -Wextra
    -std=c99 without warnings, no forbidden calls or brace-zero init, MD5 via exec of the
    system md5sum (no own MD5), SPHRAGIS used for sealing, checking and route filtering."""
    sources = glob.glob(os.path.join(ROOT, "src", "*", "*.c"))
    for src in sources:
        res = subprocess.run(["gcc", "-Wall", "-Wextra", "-std=c99", "-fsyntax-only",
                              "-I" + os.path.join(ROOT, "include"),
                              "-I" + os.path.join(ROOT, "lib", "sphragis"), src],
                             capture_output=True, text=True)
        assert res.returncode == 0 and not res.stderr.strip(), "%s:\n%s" % (src, res.stderr)
    hits, code = [], ""
    for path in sources + glob.glob(os.path.join(ROOT, "include", "*", "*.h")):
        with open(path) as f:
            text = strip_c(f.read())
        code += text
        for n, line in enumerate(text.splitlines(), 1):
            if FORBIDDEN.search(line):
                hits.append("%s:%d: %s" % (os.path.relpath(path, ROOT), n, line.strip()))
    assert not hits, "forbidden constructs:\n" + "\n".join(hits)
    assert re.search(r"\bexec[lv]p?e?\s*\(", code) and re.search(r"\bfork\s*\(", code), \
        "md5sum is not run with fork + exec"
    with open(os.path.join(ROOT, "include", "utils", "utils_md5.h")) as f:
        assert '"md5sum"' in f.read(), "MD5_COMMAND is not md5sum"
    assert not re.search(r"0x67452301|0xefcdab89", code, re.I), "an own MD5 implementation"
    for fn in ("SPHRAGIS_sign_with_glyph", "SPHRAGIS_has_glyph",
               "SPHRAGIS_filter_island_configuration", "SPHRAGIS_display_map"):
        assert fn in code, "%s is never called" % fn


SCENARIOS = {
    "fullrun": scenario_fullrun,
    "badmd5": scenario_badmd5,
    "badsize": scenario_badsize,
    "wrongisland": scenario_wrongisland,
    "othership": scenario_othership,
    "otherobject": scenario_otherobject,
    "peerdies": scenario_peerdies,
    "objectrefused": scenario_objectrefused,
    "concurrent": scenario_concurrent,
    "sigint_claim": scenario_sigint_claim,
    "sigint_deliver": scenario_sigint_deliver,
    "sigint_object": scenario_sigint_object,
    "voyages": scenario_voyages,
    "berths": scenario_berths,
    "stock": scenario_stock,
    "lost": scenario_lost,
    "terminal": scenario_terminal,
    "frames": scenario_frames,
    "wrongstate": scenario_wrongstate,
    "waiting": scenario_waiting,
    "market": scenario_market,
    "buyrace": scenario_buyrace,
    "routes": scenario_routes,
    "trident": scenario_trident,
    "deathmission": scenario_deathmission,
    "stranded": scenario_stranded,
    "persist_trade": scenario_persist_trade,
    "persist_mission": scenario_persist_mission,
    "islandrestart": scenario_islandrestart,
    "flipbyte": scenario_flipbyte,
    "cutoff": scenario_cutoff,
    "badobject": scenario_badobject,
    "fileerrors": scenario_fileerrors,
    "slowclient": scenario_slowclient,
    "cycles": scenario_cycles,
    "static": scenario_static,
    "valgrind": scenario_valgrind,
    "valgrind_faults": scenario_valgrind_faults,  # slowest, keep last
}


def main():
    keep = "--keep" in sys.argv
    names = [a for a in sys.argv[1:] if not a.startswith("--")] or list(SCENARIOS)
    for build in ("odysseus", "ithaca", "island"):
        if not os.path.exists(os.path.join(ROOT, build)):
            sys.exit("missing binary %s, run make first" % build)
    failed = 0
    sems_before = semaphore_count()
    for name in names:
        start = time.time()
        try:
            SCENARIOS[name](keep)
            print("PASS %-8s (%.1fs)" % (name, time.time() - start))
        except Exception as e:  # noqa: BLE001
            failed += 1
            print("FAIL %-8s (%.1fs): %s" % (name, time.time() - start, e))
    sems_after = semaphore_count()
    if sems_after > sems_before:  # System V semaphores outlive a process that did not destroy them
        failed += 1
        print("FAIL ipcs    : %d semaphore arrays left behind" % (sems_after - sems_before))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
