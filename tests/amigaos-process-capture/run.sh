#!/usr/bin/env bash
set -euo pipefail

FIXTURE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_DIR="$(cd "$FIXTURE_DIR/../../.." && pwd)"
AMLC_JAR="${AMLC_JAR:-$WORKSPACE_DIR/amlc.jar}"
CAPTURE_STAGE="$(mktemp -d "${TMPDIR:-/tmp}/amlang-process-capture.XXXXXX")"
CAPTURE_PROFILE="${CAPTURE_PROFILE:-basic}"

cd "$FIXTURE_DIR"
rm -f builds/bin/amigaos/app
java -jar "$AMLC_JAR" build . -bt amigaos-docker
cp builds/bin/amigaos/app "$CAPTURE_STAGE/app"
docker run --rm \
    -v "$FIXTURE_DIR/child.c:/child.c:ro" \
    -v "$CAPTURE_STAGE:/stage" \
    amiga-gcc:13 m68k-amigaos-gcc -O2 -noixemul -Wall \
    -o /stage/child /child.c
cp "$CAPTURE_STAGE/child" "$CAPTURE_STAGE/child-other"
CAPTURE_LONG_DIR="$CAPTURE_STAGE/command-name-regression-level1/command-name-regression-level2/command-name-regression-level3"
mkdir -p "$CAPTURE_LONG_DIR"
cp "$CAPTURE_STAGE/child" "$CAPTURE_LONG_DIR/child-long"
mkdir "$CAPTURE_STAGE/data"
: > "$CAPTURE_STAGE/data/cwd-marker"
if [[ "$CAPTURE_PROFILE" == ade || "$CAPTURE_PROFILE" == ade-network ]]; then
    export IMAGE="${IMAGE:-amlang-amiberry-ade:latest}"
    # ixemul probes AmiTCP: during startup even for offline tools. Without
    # this temporary assign the headless fixture waits on a volume requester.
    CAPTURE_AMITCP_ASSIGN="SYS:"
    if [[ "$CAPTURE_PROFILE" == ade-network ]]; then
        # ixnet needs usergroup.library before selecting its AmiTCP backend.
        # Expose the image's existing library so its inetd startup check runs.
        docker run --rm --entrypoint sh "$IMAGE" -c \
            'test -f /amiga-sys/ADE/Sys/Libs/usergroup.library'
        CAPTURE_AMITCP_ASSIGN="ADE:Sys"
    fi
    printf 'Assign AmiTCP: %s\ncd DH1:\nDH1:app ade >DH1:output.log\n' "$CAPTURE_AMITCP_ASSIGN" > "$CAPTURE_STAGE/app_starter"
    printf 'all:\n\t@echo MAKE-STDOUT\n\t@echo MAKE-STDERR 1>&2\n' > "$CAPTURE_STAGE/makefile"
elif [[ "$CAPTURE_PROFILE" == basic ]]; then
    printf 'cd DH1:\nDH1:app >DH1:output.log\n' > "$CAPTURE_STAGE/app_starter"
else
    printf 'Unknown CAPTURE_PROFILE: %s (expected basic, ade, or ade-network)\n' "$CAPTURE_PROFILE" >&2
    exit 2
fi
printf 'Fixture staging and retained logs: %s\n' "$CAPTURE_STAGE"
"$WORKSPACE_DIR/amiberry-headless/run.sh" "$CAPTURE_STAGE" "${RUN_TIMEOUT:-120}"
if ! rg -q '^AMIGAOS-PROCESS-CAPTURE PASSED\r?$' "$CAPTURE_STAGE/output.log"; then
    printf 'Process capture regression failed; see %s/output.log\n' "$CAPTURE_STAGE" >&2
    exit 1
fi
if rg -q '^FAIL ' "$CAPTURE_STAGE/output.log"; then
    exit 1
fi
if [[ "$CAPTURE_PROFILE" == ade || "$CAPTURE_PROFILE" == ade-network ]]; then
    rg -q '^ADE-TOOLCHAIN-BEGIN\r?$' "$CAPTURE_STAGE/output.log"
fi
