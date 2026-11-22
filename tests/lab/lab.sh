#!/usr/bin/env bash
# lab.sh - sets up and drives the Global Testing layout on the three lab machines.
#
#   Montserrat : Ithaca, Thrinacia (cap 1), Polyphemus, Eumaeus
#   Matagalls  : Aeaea (cap 1), Aeolia (cap 1), Telemachus
#   Puigpedros : Scheria (cap 2), Ogygia (cap 1), Ismarus (cap 1), Penelope
#
# Usage (from anywhere; the repo is found from this script's location):
#   tests/lab/lab.sh setup <montserrat_ip> <matagalls_ip> <puigpedros_ip>
#   tests/lab/lab.sh reset [process ...]     wipe and regenerate (all, or only those folders)
#   tests/lab/lab.sh run <process>           start a process in its own folder (VALGRIND=1 for valgrind)
#   tests/lab/lab.sh stock <process>         print the stock.db of an island folder
#   tests/lab/lab.sh files <process>         list the files of a process folder (+ md5sum)
#   tests/lab/lab.sh check                   semaphores, zombies, fds/memory/CPU of our processes
#   tests/lab/lab.sh ipcclean                remove this user's leftover System V semaphores
#   tests/lab/lab.sh list                    list processes, machines, IPs and ports
#
# Processes: ithaca thrinacia polyphemus polyphemus_broke eumaeus eumaeus_9kg
#            aeaea aeaea_filter aeolia telemachus
#            scheria scheria_race ogygia ismarus penelope
#   aeaea_filter = Aeaea with an extra (invalid) Aeolia route, for the SPHRAGIS filter row
#   scheria_race = Scheria with DriedFish stock = 5 kg, for the concurrent purchase row
#   polyphemus_broke = Polyphemus with 50 gold and no cargo (stranded / dead-end row)
#   eumaeus_9kg = Eumaeus with only 9 kg of Barley (death at sea row)
#
# Everything lives in $NOSTOS_LAB (default ~/nostos_lab), one folder per process.
# Ports can be overridden with PORT_<PROCESS>=n (use the same values on all machines).
set -u

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LAB="${NOSTOS_LAB:-$HOME/nostos_lab}"
ENV_FILE="$LAB/lab.env"

die() { echo "lab.sh: $*" >&2; exit 1; }

machine_of() {
    case "$1" in
        ithaca|thrinacia|polyphemus|polyphemus_broke|eumaeus|eumaeus_9kg) echo montserrat ;;
        aeaea|aeaea_filter|aeolia|telemachus) echo matagalls ;;
        scheria|scheria_race|ogygia|ismarus|penelope) echo puigpedros ;;
        *) return 1 ;;
    esac
}

# Folder a process runs in (variants of a ship share the ship's folder so the
# ship files are the same; island variants get their own folder and stock copy)
dir_of() {
    case "$1" in
        polyphemus_broke) echo "$LAB/montserrat/polyphemus" ;;
        eumaeus_9kg) echo "$LAB/montserrat/eumaeus" ;;
        *) echo "$LAB/$(machine_of "$1")/$1" ;;
    esac
}

load_env() {
    [ -f "$ENV_FILE" ] || die "run 'lab.sh setup <montserrat_ip> <matagalls_ip> <puigpedros_ip>' first"
    # shellcheck disable=SC1090
    . "$ENV_FILE"
    PORT_ITHACA="${PORT_ITHACA:-8625}"
    PORT_THRINACIA="${PORT_THRINACIA:-8626}"
    PORT_AEAEA="${PORT_AEAEA:-8625}"
    PORT_AEOLIA="${PORT_AEOLIA:-8626}"
    PORT_SCHERIA="${PORT_SCHERIA:-8625}"
    PORT_OGYGIA="${PORT_OGYGIA:-8626}"
    PORT_ISMARUS="${PORT_ISMARUS:-8627}"
}

ip_of_island() {
    case "$1" in
        Ithaca|Thrinacia) echo "$IP_MONTSERRAT" ;;
        Aeaea|Aeolia) echo "$IP_MATAGALLS" ;;
        Scheria|Ogygia|Ismarus) echo "$IP_PUIGPEDROS" ;;
    esac
}

port_of_island() {
    local v="PORT_$(echo "$1" | tr '[:lower:]' '[:upper:]')"
    echo "${!v}"
}

route() { echo "$1 $(ip_of_island "$1") $(port_of_island "$1")"; }

# write_island <dir> <Island> <capacity> <route...>
write_island() {
    local dir="$1" name="$2" cap="$3" lower
    shift 3
    lower="$(echo "$name" | tr '[:upper:]' '[:lower:]')"
    mkdir -p "$dir/stocks"
    {
        echo "$name"
        echo "./${lower}_files"
        echo "$(ip_of_island "$name") $(port_of_island "$name")"
        echo "$cap"
        echo "--- ROUTES ---"
        for r in "$@"; do route "$r"; done
    } > "$dir/$lower.dat"
    cp "$REPO/stocks/$name.db" "$dir/stocks/$name.db"
}

# write_ship <file> <Name> <folder> <gold> <cargo lines...>
write_ship() {
    local file="$1" name="$2" folder="$3" gold="$4"
    shift 4
    mkdir -p "$(dirname "$file")"
    {
        echo "$name"
        echo "$folder"
        echo "$IP_MONTSERRAT $PORT_ITHACA"
        route Aeaea
        echo "$gold"
        echo "$#"
        for c in "$@"; do echo "$c"; done
    } > "$file"
}

set_stock() { # set_stock <db> <product> <amount>
    command -v python3 >/dev/null || die "python3 is needed to edit $1"
    python3 - "$1" "$2" "$3" <<'EOF'
import struct, sys
rec = struct.Struct("100sii")
path, product, amount = sys.argv[1], sys.argv[2].encode(), int(sys.argv[3])
data = bytearray(open(path, "rb").read())
for off in range(0, len(data) - rec.size + 1, rec.size):
    name, qty, price = rec.unpack_from(data, off)
    if name.split(b"\0")[0] == product:
        rec.pack_into(data, off, name, amount, price)
open(path, "wb").write(data)
EOF
}

gen() {
    local p="$1" d
    d="$(dir_of "$p")"
    case "$p" in
        ithaca)
            rm -rf "$d"; mkdir -p "$d"
            printf 'Ithaca\n./objects\n%s %s\n' "$IP_MONTSERRAT" "$PORT_ITHACA" > "$d/ithaca.dat"
            cp "$REPO/voyages.dat" "$d/voyages.dat"
            cp -r "$REPO/objects" "$d/objects" ;;
        thrinacia) rm -rf "$d"; write_island "$d" Thrinacia 1 Aeaea Ogygia ;;
        aeaea) rm -rf "$d"; write_island "$d" Aeaea 1 Scheria Thrinacia ;;
        aeaea_filter) rm -rf "$d"; write_island "$d" Aeaea 1 Scheria Thrinacia Aeolia ;;
        aeolia) rm -rf "$d"; write_island "$d" Aeolia 1 Scheria ;;
        scheria) rm -rf "$d"; write_island "$d" Scheria 2 Aeaea Aeolia Ismarus ;;
        scheria_race)
            rm -rf "$d"; write_island "$d" Scheria 2 Aeaea Aeolia Ismarus
            set_stock "$d/stocks/Scheria.db" DriedFish 5 ;;
        ogygia) rm -rf "$d"; write_island "$d" Ogygia 1 Thrinacia ;;
        ismarus) rm -rf "$d"; write_island "$d" Ismarus 1 Scheria ;;
        polyphemus|polyphemus_broke)
            rm -rf "$d"
            write_ship "$d/polyphemus.dat" Polyphemus ./polyphemus_files 250 "Barley 40" "DriedFigs 60"
            write_ship "$d/polyphemus_broke.dat" Polyphemus ./polyphemus_files 50 ;;
        eumaeus|eumaeus_9kg)
            rm -rf "$d"
            write_ship "$d/eumaeus.dat" Eumaeus ./eumaeus_files 250 "Barley 40" "DriedFigs 60"
            write_ship "$d/eumaeus_9kg.dat" Eumaeus ./eumaeus_files 250 "Barley 9" ;;
        telemachus) rm -rf "$d"; write_ship "$d/telemachus.dat" Telemachus ./telemachus_files 250 "Barley 40" "DriedFigs 60" ;;
        penelope) rm -rf "$d"; write_ship "$d/penelope.dat" Penelope ./penelope_files 250 "Barley 40" "DriedFigs 60" ;;
        *) die "unknown process '$p'" ;;
    esac
}

ALL="ithaca thrinacia polyphemus eumaeus aeaea aeaea_filter aeolia telemachus scheria scheria_race ogygia ismarus penelope"

cmd_setup() {
    [ $# -eq 3 ] || die "usage: lab.sh setup <montserrat_ip> <matagalls_ip> <puigpedros_ip>"
    for ip in "$@"; do
        case "$ip" in localhost|127.*|0.0.0.0) die "use the machines' real IPs, not $ip" ;; esac
    done
    mkdir -p "$LAB"
    printf 'IP_MONTSERRAT=%s\nIP_MATAGALLS=%s\nIP_PUIGPEDROS=%s\n' "$1" "$2" "$3" > "$ENV_FILE"
    for v in ITHACA THRINACIA AEAEA AEOLIA SCHERIA OGYGIA ISMARUS; do
        eval "val=\${PORT_$v:-}"
        # shellcheck disable=SC2154
        [ -n "$val" ] && echo "PORT_$v=$val" >> "$ENV_FILE"
    done
    load_env
    for p in $ALL; do gen "$p"; done
    echo "Lab written to $LAB"
    cmd_list
}

cmd_reset() {
    load_env
    if [ $# -eq 0 ]; then set -- $ALL; fi
    for p in "$@"; do gen "$p"; echo "reset $p -> $(dir_of "$p")"; done
}

local_ips() { hostname -I 2>/dev/null || ip -4 -o addr show | awk '{print $4}' | cut -d/ -f1; }

cmd_run() {
    [ $# -eq 1 ] || die "usage: lab.sh run <process>"
    load_env
    local p="$1" m d ip vg=()
    m="$(machine_of "$p")" || die "unknown process '$p'"
    d="$(dir_of "$p")"
    [ -d "$d" ] || die "$d does not exist: run setup/reset"
    case "$m" in
        montserrat) ip="$IP_MONTSERRAT" ;; matagalls) ip="$IP_MATAGALLS" ;; *) ip="$IP_PUIGPEDROS" ;;
    esac
    if ! local_ips | tr ' ' '\n' | grep -qx "$ip"; then
        echo "WARNING: $p belongs on $m ($ip) but this machine is $(hostname) ($(local_ips))" >&2
    fi
    [ "${VALGRIND:-0}" = 1 ] && vg=(valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes)
    cd "$d" || exit 1
    case "$p" in
        ithaca) exec "${vg[@]}" "$REPO/ithaca" ithaca.dat voyages.dat ;;
        thrinacia|aeaea|aeolia|scheria|ogygia|ismarus|aeaea_filter|scheria_race)
            local isl="${p%%_*}" cap
            cap="$(echo "${isl:0:1}" | tr '[:lower:]' '[:upper:]')${isl:1}"
            exec "${vg[@]}" "$REPO/island" "$isl.dat" "stocks/$cap.db" ;;
        *) exec "${vg[@]}" "$REPO/odysseus" "$p.dat" ;;
    esac
}

cmd_stock() {
    [ $# -eq 1 ] || die "usage: lab.sh stock <island process>"
    local d f
    d="$(dir_of "$1")"
    f="$(ls "$d"/stocks/*.db 2>/dev/null | head -1)"
    [ -n "$f" ] || die "no stock.db in $d"
    python3 - "$f" <<'EOF'
import struct, sys
rec = struct.Struct("100sii")
data = open(sys.argv[1], "rb").read()
print(sys.argv[1], "(%d records)" % (len(data) // rec.size))
for off in range(0, len(data) - rec.size + 1, rec.size):
    n, q, p = rec.unpack_from(data, off)
    print("  %-16s %5d kg  %3d gold/kg" % (n.split(b"\0")[0].decode(), q, p))
EOF
}

cmd_files() {
    [ $# -eq 1 ] || die "usage: lab.sh files <process>"
    local d
    d="$(dir_of "$1")"
    # the original mission images (objects/<lowercase>.png) are skipped; kept claim images are shown
    (cd "$d" && find . -type f ! -regex '\./objects/[a-z_]*\.png' | sort | while read -r f; do
        printf '%-50s %8s  %s\n' "$f" "$(wc -c < "$f")" "$(md5sum "$f" | cut -d' ' -f1)"
    done)
    echo "(hidden or .tmp files below must be empty:)"
    find "$d" \( -name '*.tmp' -o -name '.*' \) -type f
}

cmd_check() {
    echo "== System V semaphores of $USER (must be empty when nothing runs)"
    ipcs -s | awk -v u="$USER" 'NR<=3 || $3==u'
    echo "== zombies (must be empty)"
    ps -eo pid,ppid,stat,comm | awk '$3 ~ /Z/'
    echo "== our processes: pid, state, RSS kB, %CPU, open fds, threads"
    for pid in $(pgrep -u "$USER" -x 'ithaca|island|odysseus|memcheck-amd64-' ); do
        printf '%7s %-10s %-3s %7s kB %5s%% %4s fds %3s thr  %s\n' "$pid" \
            "$(ps -o comm= -p "$pid")" "$(ps -o stat= -p "$pid")" "$(ps -o rss= -p "$pid")" \
            "$(ps -o %cpu= -p "$pid")" "$(ls /proc/"$pid"/fd 2>/dev/null | wc -l)" \
            "$(ps -o nlwp= -p "$pid")" "$(tr '\0' ' ' < /proc/"$pid"/cmdline)"
    done
}

cmd_ipcclean() {
    for id in $(ipcs -s | awk -v u="$USER" '$3==u {print $2}'); do
        ipcrm -s "$id" && echo "removed semaphore $id"
    done
}

cmd_list() {
    load_env
    echo "Montserrat $IP_MONTSERRAT : ithaca:$PORT_ITHACA thrinacia:$PORT_THRINACIA polyphemus eumaeus"
    echo "Matagalls  $IP_MATAGALLS : aeaea:$PORT_AEAEA aeolia:$PORT_AEOLIA telemachus"
    echo "Puigpedros $IP_PUIGPEDROS : scheria:$PORT_SCHERIA ogygia:$PORT_OGYGIA ismarus:$PORT_ISMARUS penelope"
}

[ $# -ge 1 ] || { sed -n '2,27p' "$0"; exit 1; }
c="$1"; shift
case "$c" in
    setup) cmd_setup "$@" ;;
    reset) cmd_reset "$@" ;;
    run) cmd_run "$@" ;;
    stock) cmd_stock "$@" ;;
    files) cmd_files "$@" ;;
    check) cmd_check ;;
    ipcclean) cmd_ipcclean ;;
    list) cmd_list ;;
    *) die "unknown command '$c'" ;;
esac
