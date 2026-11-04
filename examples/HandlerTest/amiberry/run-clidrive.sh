#!/usr/bin/env bash
# Headless HandlerTest runner using the user's CLIDrive Workbench
# image as DH0 instead of the amiberry image's stripped /amiga-sys.
# CLIDrive has the real Libs, Devs, and Apps (including bebbossh)
# the test will eventually need to spawn for the actual SSH password
# diagnosis.
#
# Bind mounts:
#   $KICK31         -> /rom/kick31.rom    (kickstart)
#   $CLIDRIVE       -> /amiga-sys         (DH0 — overrides baked SYS)
#   $USER_STARTUP   -> /amiga-sys/S/User-Startup  (chains to our test)
#   $APP_DIR        -> /app               (DH1 — parent + child + child2)
#   default template stays at /conf/threadtest.uae.template — its
#   __ROM__ + __APP_DIR__ substitution still works.

set -u
set -o pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
APP_DIR="$HERE/../builds/bin/amigaos"
CLIDRIVE="${CLIDRIVE:-/Users/anderskjeldsen/Projects/AmLang/CLIDrive}"
USER_STARTUP="$HERE/User-Startup"
KICK31="${KICK31:-$HOME/Documents/FS-UAE/Kickstarts/kick31.rom}"
RUN_TIMEOUT="${RUN_TIMEOUT:-180}"

if [[ ! -f "$APP_DIR/app" ]]; then
    echo "No build at $APP_DIR/app — run 'make build' first" >&2
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

# Clear any prior log so we don't grep against a stale sentinel.
rm -f "$APP_DIR/output.log"

echo "Launching CLIDrive boot via Amiberry (timeout ${RUN_TIMEOUT}s)..."
docker run --rm \
    -e RUN_TIMEOUT="$RUN_TIMEOUT" \
    -v "$KICK31:/rom/kick31.rom:ro" \
    -v "$CLIDRIVE:/amiga-sys" \
    -v "$USER_STARTUP:/amiga-sys/S/User-Startup" \
    -v "$APP_DIR:/app" \
    amiberry:latest
