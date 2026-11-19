#!/usr/bin/env bash
# Out-of-memory shutdown (Redmine #446) as real programs. See
# src/am-lang/OomShutdown/Program.aml for the modes.
#   ./run.sh           build and check on linux-x64 (releaseHeapOnExit off),
#                      then with AddressSanitizer + TRACKOBJECTS builds with
#                      the feature on and off (ASAN=0 skips them)
#   ./run.sh --amiga   build for AmigaOS (amiga-gcc docker image) and check
#                      under the amlang-amiberry image
# The *-file modes (#447) also check that the files left open in the skipped
# frames were flushed and closed: their data is on disk, the worker's can be
# read back by an exit hook, and on AmigaOS (releaseHeapOnExit always on) they
# can be deleted after exit. On linux-x64 FileStream flushes every write and
# the exit closes the files, so these pass with the feature off as well; the
# ASan build's object count is what shows the release.
# AMLC_JAR overrides the compiler jar (default: the workspace's amlc.jar);
# AMLC overrides the whole command (e.g. a v2 build of amlc).
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(cd "$HERE/../../.." && pwd)"
AMLC_JAR="${AMLC_JAR:-$WORKSPACE/amlc.jar}"
AMLC="${AMLC:-java -jar $AMLC_JAR}"
cd "$HERE"

fail=0
check() {   # mode rc expected-rc first(yes/no) second(yes/no) dir out [must-contain] [must-not-contain]
    local mode=$1 rc=$2 want=$3 first=$4 second=$5 dir=$6 out=$7 has=${8:-} hasnot=${9:-}
    local ok=1
    [[ "$rc" == "$want" ]] || { echo "  exit code $rc, expected $want"; ok=0; }
    [[ -f "$dir/oom-first-$mode.txt" ]] && f=yes || f=no
    [[ -f "$dir/oom-second-$mode.txt" ]] && s=yes || s=no
    [[ "$f" == "$first" ]] || { echo "  first exit hook ran: $f, expected $first"; ok=0; }
    [[ "$s" == "$second" ]] || { echo "  second exit hook ran: $s, expected $second"; ok=0; }
    if [[ -n "$has" ]] && ! grep -q -- "$has" "$out"; then echo "  output lacks: $has"; ok=0; fi
    if [[ -n "$hasnot" ]] && grep -q -- "$hasnot" "$out"; then echo "  output has: $hasnot"; ok=0; fi
    if (( ok )); then echo "PASS $mode"; else echo "FAIL $mode"; sed 's/^/    | /' "$out"; fail=1; fi
}

# mode expected-first expected-second must-contain must-not-contain
CASES=(
    "normal yes yes|normal run|Out of memory"
    "main yes yes|Out of memory allocating|main: not reached"
    "main-sticky no no|Out of memory during shutdown|main: not reached"
    "shutdown yes no|Out of memory during shutdown: skipping|not reached"
    "worker yes yes|on thread 'oomer'|not reached"
    "worker-main-receive yes yes|on thread 'oomer'|not reached"
    "worker-main-busy yes yes|on thread 'oomer'|not reached"
    "send yes yes|Out of memory|not reached"
    "late yes yes|slow: finished|not reached"
    "main-file yes yes|Out of memory allocating|files: not reached"
    "main-sticky-file no no|Out of memory during shutdown|files: not reached"
    "worker-file yes yes|read back 2: cycle worker-file|not reached"
)
FILE_MODES="main-file main-sticky-file worker-file"

# The data each *-file mode wrote without closing must be in its files.
check_files() {   # mode dir
    local mode=$1 dir=$2 ok=1 n want got
    for n in 1 2; do
        [[ $n == 1 ]] && want="nested $mode" || want="cycle $mode"
        got=$(tr -d '\r' < "$dir/oom-data-$mode-$n.txt" 2>/dev/null)
        [[ "$got" == "$want" ]] || { echo "  oom-data-$mode-$n.txt holds '$got', expected '$want'"; ok=0; }
    done
    if [[ $mode == worker-file ]] && ! grep -q "read back 1: nested worker-file" "$dir/out-$mode.log"; then
        echo "  the exit hook could not read the worker's first file back"; ok=0
    fi
    if (( ok )); then echo "PASS $mode files"; else echo "FAIL $mode files"; fail=1; fi
}

if [[ "${1:-}" == "--amiga" ]]; then
    $AMLC build . -bt amigaos-docker > build-amigaos.log 2>&1
    grep -q "Build complete" build-amigaos.log || { tail -20 build-amigaos.log; exit 1; }
    # Plain docker volumes: the docker daemon may not see a private /tmp.
    RUN="$HERE/amiga-run"
    rm -rf "$RUN"; mkdir -p "$RUN"
    cp builds/amigaos-docker/bin/app "$RUN/app"
    {
        echo "cd DH1:"
        echo "Stack 200000"
        echo "FailAt 1000"
        echo "Echo \"start\" >DH1:rc.log"
        for c in "${CASES[@]}"; do
            m=${c%% *}
            echo "DH1:app $m >DH1:out-$m.log"
            echo "Echo \"$m \$RC\" >>DH1:rc.log"
        done
        # A file still locked by the exited program cannot be deleted (copy
        # first: the host checks the data).
        for m in $FILE_MODES; do
            for n in 1 2; do
                echo "Copy DH1:oom-data-$m-$n.txt DH1:copy-$m-$n.txt"
                echo "Delete DH1:oom-data-$m-$n.txt QUIET"
                echo "Echo \"delete-$m-$n \$RC\" >>DH1:rc.log"
            done
        done
        echo "Type DH1:rc.log >DH1:output.log"
        echo "Echo \"=== TEST RUN COMPLETE ===\" >>DH1:output.log"
    } > "$RUN/app_starter"
    docker run --rm -e RUN_TIMEOUT="${RUN_TIMEOUT:-300}" -v "$RUN:/dh1" "${IMAGE:-amlang-amiberry:latest}" > "$RUN/emulator.log" 2>&1
    for c in "${CASES[@]}"; do
        spec=${c%%|*}; rest=${c#*|}; has=${rest%%|*}; hasnot=${rest#*|}
        read -r m first second <<< "$spec"
        rc=$(grep "^$m " "$RUN/rc.log" | head -1 | awk '{print $2}' | tr -d '\r')
        want=20; [[ "$m" == normal || "$m" == late ]] && want=0
        check "$m" "${rc:-none}" "$want" "$first" "$second" "$RUN" "$RUN/out-$m.log" "$has" "$hasnot"
    done
    for m in $FILE_MODES; do
        ok=1
        for n in 1 2; do
            drc=$(grep "^delete-$m-$n " "$RUN/rc.log" | head -1 | awk '{print $2}' | tr -d '\r')
            [[ "$drc" == 0 ]] || { echo "  Delete oom-data-$m-$n.txt: RC ${drc:-none} (still locked?)"; ok=0; }
            [[ -f "$RUN/copy-$m-$n.txt" ]] && cp "$RUN/copy-$m-$n.txt" "$RUN/oom-data-$m-$n.txt"
        done
        if (( ok )); then echo "PASS $m unlocked"; else echo "FAIL $m unlocked"; fail=1; fi
        check_files "$m" "$RUN"
    done
else
    $AMLC build . -bt linux-x64 > build-linux-x64.log 2>&1
    grep -q "Build complete" build-linux-x64.log || { tail -20 build-linux-x64.log; exit 1; }
    RUN="$HERE/linux-run"
    rm -rf "$RUN"; mkdir -p "$RUN"
    CASES+=("busy yes yes|did not stop within|not reached")
    for c in "${CASES[@]}"; do
        spec=${c%%|*}; rest=${c#*|}; has=${rest%%|*}; hasnot=${rest#*|}
        read -r m first second <<< "$spec"
        ( cd "$RUN" && timeout 20 "$HERE/builds/linux-x64/bin/app" "$m" > "out-$m.log" 2>&1 ); rc=$?
        want=1; [[ "$m" == normal || "$m" == late ]] && want=0
        check "$m" "$rc" "$want" "$first" "$second" "$RUN" "$RUN/out-$m.log" "$has" "$hasnot"
    done
    for m in $FILE_MODES; do check_files "$m" "$RUN"; done

    if [[ "${ASAN:-1}" != 0 ]]; then
        # AddressSanitizer + TRACKOBJECTS, with and without releaseHeapOnExit
        # (#447). With it (linux-x64-asan), the end-of-heap release frees
        # everything a skipped frame held without a use-after-free or a leak:
        # "Allocated objects 2" is what is left, the immortal out-of-memory
        # exception and its message. The emergency modes skip the statics'
        # dereference, so what the statics hold stays. Without it
        # (linux-x64-asan-off, the default off the Amiga family), what skipped
        # frames held stays allocated until the process exits (a higher count,
        # leak checking off); only the absence of a sanitizer report is checked.
        for target in linux-x64-asan linux-x64-asan-off; do
            $AMLC build . -bt $target > build-$target.log 2>&1
            grep -q "Build complete" build-$target.log || { tail -20 build-$target.log; exit 1; }
            RUN="$HERE/run-$target"
            rm -rf "$RUN"; mkdir -p "$RUN"
            leaks=1; [[ $target == linux-x64-asan-off ]] && leaks=0
            for c in "${CASES[@]}"; do
                spec=${c%%|*}
                read -r m first second <<< "$spec"
                ( cd "$RUN" && ASAN_OPTIONS=detect_leaks=$leaks timeout 60 "$HERE/builds/$target/bin/app" "$m" > "out-$m.log" 2>&1 ); rc=$?
                want=1; [[ "$m" == normal || "$m" == late ]] && want=0
                ok=1
                [[ "$rc" == "$want" ]] || { echo "  exit code $rc, expected $want"; ok=0; }
                if grep -q "Sanitizer" "$RUN/out-$m.log"; then echo "  AddressSanitizer report"; ok=0; fi
                if grep -q "could not be released" "$RUN/out-$m.log"; then echo "  objects left unreleased"; ok=0; fi
                if (( leaks )); then
                    case "$m" in
                        main-sticky|shutdown|main-sticky-file) ;;
                        *) grep -q "^Allocated objects 2$" "$RUN/out-$m.log" || { echo "  not every object was released"; ok=0; } ;;
                    esac
                fi
                if (( ok )); then echo "PASS $target $m"; else echo "FAIL $target $m"; grep -v "^Dereference static" "$RUN/out-$m.log" | sed 's/^/    | /' | head -60; fail=1; fi
            done
            for m in $FILE_MODES; do check_files "$m" "$RUN"; done
        done
    fi
fi
if (( fail )); then echo "OOM SHUTDOWN FAILED"; exit 1; fi
echo "OOM SHUTDOWN PASSED"
