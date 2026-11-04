#!/usr/bin/env bash
# Headless FGets-diagnostic runner. Same shape as run-clidrive.sh but
# bind-mounts User-Startup-fgets so the boot drops straight into the
# three-variant fgets test instead of bebbossh.

set -u
set -o pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
APP_DIR="$HERE/../builds/bin/amigaos"
CLIDRIVE="${CLIDRIVE:-/Users/anderskjeldsen/Projects/AmLang/CLIDrive}"
USER_STARTUP="$HERE/User-Startup-fgets"
KICK31="${KICK31:-$HOME/Documents/FS-UAE/Kickstarts/kick31.rom}"
RUN_TIMEOUT="${RUN_TIMEOUT:-120}"

if [[ ! -f "$APP_DIR/parent_fgets" ]]; then
    echo "No build at $APP_DIR/parent_fgets — run 'make build' first" >&2
    exit 1
fi
if [[ ! -f "$APP_DIR/child_fgets" ]]; then
    echo "No build at $APP_DIR/child_fgets — run 'make build' first" >&2
    exit 1
fi
if [[ ! -d "$CLIDRIVE" ]]; then
    echo "CLIDrive not found at $CLIDRIVE" >&2
    echo "Set CLIDRIVE=/path/to/CLIDrive if it lives elsewhere" >&2
    exit 1
fi
if [[ ! -f "$KICK31" ]]; then
    echo "Kickstart ROM not found at $KICK31" >&2
    exit 1
fi
if [[ -z "$(docker images -q amiberry:latest)" ]]; then
    echo "amiberry:latest image missing" >&2
    exit 1
fi

rm -f "$APP_DIR/output.log"

echo "Launching FGets diagnostic via Amiberry (timeout ${RUN_TIMEOUT}s)..."
docker run --rm \
    -e RUN_TIMEOUT="$RUN_TIMEOUT" \
    -v "$KICK31:/rom/kick31.rom:ro" \
    -v "$CLIDRIVE:/amiga-sys" \
    -v "$USER_STARTUP:/amiga-sys/S/User-Startup" \
    -v "$APP_DIR:/app" \
    amiberry:latest
