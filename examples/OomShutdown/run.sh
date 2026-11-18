#!/usr/bin/env bash
# Out-of-memory shutdown (Redmine #446) as real programs. See
# src/am-lang/OomShutdown/Program.aml for the modes.
#   ./run.sh           build and check on linux-x64
#   ./run.sh --amiga   build for AmigaOS (amiga-gcc docker image) and check
#                      under the amlang-amiberry image
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
)

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
fi
if (( fail )); then echo "OOM SHUTDOWN FAILED"; exit 1; fi
echo "OOM SHUTDOWN PASSED"
