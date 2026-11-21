#!/usr/bin/env bash
# Join and exit races of short-lived threads (Redmine #459). See
# src/am-lang/ThreadExitRace/Program.aml for the modes.
#   ./run.sh            build and run every mode on linux-x64
#   ./run.sh --amiga    build for AmigaOS (amiga-gcc docker image) and run it
#                       under the amlang-amiberry image: join and exit 20
#                       times each, drop 3 times, nested 10 times, in one
#                       session; then Avail, Dir and one more run, to check
#                       nothing was corrupted. Every run must exit 0, and
#                       after a join no worker task may still be bound to
#                       its isolate (it would still be in the program's code)
#   ./run.sh --morphos  build the MorphOS variant only (no emulator here)
# AMLC_JAR overrides the compiler jar (default: the workspace's amlc.jar);
# AMLC overrides the whole command. N overrides the 20 repetitions.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(cd "$HERE/../../.." && pwd)"
AMLC_JAR="${AMLC_JAR:-$WORKSPACE/amlc.jar}"
AMLC="${AMLC:-java -jar $AMLC_JAR}"
N="${N:-20}"
cd "$HERE"

fail=0
pass() { echo "PASS $1"; }
flunk() { echo "FAIL $1"; fail=1; }

# A join must leave no worker bound (bound after join 0).
check_join_line() {   # label file
    if grep -q "join: 120 threads joined; alive after join [0-9]*, bound after join 0" "$2"; then pass "$1"; else flunk "$1"; sed 's/^/    | /' "$2" | head -20; fi
}

if [[ "${1:-}" == "--morphos" ]]; then
    $AMLC build . -bt morphos-ppc-docker > build-morphos.log 2>&1
    grep -q "Build complete" build-morphos.log && { echo "MORPHOS BUILD PASSED"; exit 0; }
    tail -20 build-morphos.log; echo "MORPHOS BUILD FAILED"; exit 1
elif [[ "${1:-}" == "--amiga" ]]; then
    $AMLC build . -bt amigaos-docker > build-amigaos.log 2>&1
    grep -q "Build complete" build-amigaos.log || { tail -20 build-amigaos.log; exit 1; }
    RUN="$HERE/amiga-run"
    rm -rf "$RUN"; mkdir -p "$RUN"
    cp builds/amigaos-docker/bin/app "$RUN/app"
    {
        echo "cd DH1:"
        echo "Stack 200000"
        echo "FailAt 1000"
        echo "Avail >DH1:avail-before.log"
        echo "Echo \"start\" >DH1:rc.log"
        for m in join exit; do
            for i in $(seq 1 "$N"); do
                echo "DH1:app $m >DH1:out-$m-$i.log"
                echo "Echo \"$m-$i \$RC\" >>DH1:rc.log"
            done
        done
        for i in 1 2 3; do
            echo "DH1:app drop >DH1:out-drop-$i.log"
            echo "Echo \"drop-$i \$RC\" >>DH1:rc.log"
        done
        for i in $(seq 1 10); do
            echo "DH1:app nested >DH1:out-nested-$i.log"
            echo "Echo \"nested-$i \$RC\" >>DH1:rc.log"
        done
        echo "Avail >DH1:avail-after.log"
        echo "Dir DH1: >DH1:dir.log"
        echo "DH1:app join >DH1:out-final.log"
        echo "Echo \"final \$RC\" >>DH1:rc.log"
        echo "Type DH1:rc.log >DH1:output.log"
        echo "Echo \"=== TEST RUN COMPLETE ===\" >>DH1:output.log"
    } > "$RUN/app_starter"
    docker run --rm -e RUN_TIMEOUT="${RUN_TIMEOUT:-900}" -v "$RUN:/dh1" "${IMAGE:-amlang-amiberry:latest}" > "$RUN/emulator.log" 2>&1
    runs=0; bad=0
    while read -r label rc; do
        [[ "$label" == start ]] && continue
        runs=$((runs + 1))
        rc=$(echo "$rc" | tr -d '\r')
        [[ "$rc" == 0 ]] || { echo "  $label: RC $rc"; bad=1; }
    done < <(tr -d '\r' < "$RUN/rc.log" 2>/dev/null)
    want=$((2 * N + 3 + 10 + 1))
    if (( runs == want && !bad )); then pass "$runs runs exit 0"; else flunk "$runs of $want runs exit 0"; fi
    for i in $(seq 1 "$N"); do check_join_line "join-$i" "$RUN/out-join-$i.log"; done
    check_join_line "final join" "$RUN/out-final.log"
    ok=1
    for i in $(seq 1 "$N"); do grep -q "exit: done" "$RUN/out-exit-$i.log" || { echo "  exit-$i printed no 'exit: done'"; ok=0; }; done
    for i in $(seq 1 10); do grep -q "nested: done" "$RUN/out-nested-$i.log" || { echo "  nested-$i printed no 'nested: done'"; ok=0; }; done
    if (( ok )); then pass "exit and nested runs"; else flunk "exit and nested runs"; fi
    for i in 1 2 3; do
        if grep -q "drop: 80 threads.*tasks left 0" "$RUN/out-drop-$i.log"; then pass "drop-$i"; else flunk "drop-$i"; sed 's/^/    | /' "$RUN/out-drop-$i.log"; fi
        tr -d '\r' < "$RUN/out-drop-$i.log" | grep "^drop:" | sed 's/^/  /'
    done
    [[ -s "$RUN/avail-after.log" && -s "$RUN/dir.log" ]] && pass "Avail and Dir afterwards" || flunk "Avail and Dir afterwards"
    grep -qi "guru\|software failure" "$RUN/emulator.log" && flunk "emulator log mentions a guru"
    echo "  Avail before:"; tr -d '\r' < "$RUN/avail-before.log" | sed 's/^/    | /'
    echo "  Avail after:";  tr -d '\r' < "$RUN/avail-after.log" | sed 's/^/    | /'
    echo "  alive/bound after join (join runs):"
    tr -d '\r' < <(cat "$RUN"/out-join-*.log) | grep "^join: 120" | sed 's/.*alive after join/    alive/' | sort | uniq -c
else
    $AMLC build . -bt linux-x64 > build-linux-x64.log 2>&1
    grep -q "Build complete" build-linux-x64.log || { tail -20 build-linux-x64.log; exit 1; }
    RUN="$HERE/linux-run"
    rm -rf "$RUN"; mkdir -p "$RUN"
    for m in join exit drop nested; do
        for i in $(seq 1 "$N"); do
            ( cd "$RUN" && timeout 20 "$HERE/builds/linux-x64/bin/app" "$m" > "out-$m-$i.log" 2>&1 ); rc=$?
            if [[ $rc != 0 ]] || ! grep -q "$m: done" "$RUN/out-$m-$i.log"; then flunk "$m-$i (rc $rc)"; sed 's/^/    | /' "$RUN/out-$m-$i.log"; fi
        done
        echo "ran $m $N times"
    done
    for i in $(seq 1 "$N"); do grep -q "bound after join 0" "$RUN/out-join-$i.log" || flunk "join-$i bound"; done
fi
if (( fail )); then echo "THREAD EXIT RACE FAILED"; exit 1; fi
echo "THREAD EXIT RACE PASSED"
