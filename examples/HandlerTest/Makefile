# HandlerTest — minimal dos.library handler test for AmigaOS.
#
# Two C programs:
#   parent.c  -> /app/app    (the handler + child spawner)
#   child.c   -> /app/child  (asks for a name, says hello)
#
# Build:    make build       (compiles via amiga-gcc docker)
# Run:      make run         (boots amiberry, redirects parent stdout to output.log)
# Clean:    make clean
#
# Output of interest is builds/bin/amigaos/output.log after `make run`.

CURDIR := $(shell pwd)
APP_DIR := $(CURDIR)/builds/bin/amigaos
KICK31 ?= $(HOME)/Documents/FS-UAE/Kickstarts/kick31.rom
RUN_TIMEOUT ?= 60

CFLAGS := -O2 -noixemul -Wall

build:
	mkdir -p $(APP_DIR)
	docker run --rm -v $(CURDIR):/host -w /host amiga-gcc:latest bash -c \
	    "/opt/amiga/bin/m68k-amigaos-gcc $(CFLAGS) -o builds/bin/amigaos/app    parent.c && \
	     /opt/amiga/bin/m68k-amigaos-gcc $(CFLAGS) -o builds/bin/amigaos/child  child.c  && \
	     /opt/amiga/bin/m68k-amigaos-gcc $(CFLAGS) -o builds/bin/amigaos/child2 child2.c && \
	     /opt/amiga/bin/m68k-amigaos-gcc $(CFLAGS) -o builds/bin/amigaos/child3 child3.c && \
	     /opt/amiga/bin/m68k-amigaos-gcc $(CFLAGS) -o builds/bin/amigaos/child4 child4.c && \
	     /opt/amiga/bin/m68k-amigaos-gcc $(CFLAGS) -o builds/bin/amigaos/child5 child5.c && \
	     /opt/amiga/bin/m68k-amigaos-gcc $(CFLAGS) -o builds/bin/amigaos/parent_fgets parent_fgets.c && \
	     /opt/amiga/bin/m68k-amigaos-gcc $(CFLAGS) -o builds/bin/amigaos/child_fgets  child_fgets.c"
	@echo "Built:"
	@ls -la $(APP_DIR)

run:
	@if [ ! -f $(APP_DIR)/app ]; then \
	    echo "No binary — run 'make build' first" >&2; exit 1; \
	fi
	@if [ ! -f "$(KICK31)" ]; then \
	    echo "Kickstart ROM not found at $(KICK31)" >&2; \
	    echo "Set KICK31=/path/to/kick31.rom" >&2; exit 1; \
	fi
	@rm -f $(APP_DIR)/output.log
	docker run --rm \
	    -e RUN_TIMEOUT=$(RUN_TIMEOUT) \
	    -v "$(KICK31):/rom/kick31.rom:ro" \
	    -v "$(APP_DIR):/app" \
	    amiberry:latest
	@echo "=== output.log ==="
	@cat $(APP_DIR)/output.log

run-clidrive:
	@if [ ! -f $(APP_DIR)/app ]; then \
	    echo "No binary — run 'make build' first" >&2; exit 1; \
	fi
	@KICK31="$(KICK31)" RUN_TIMEOUT=$(RUN_TIMEOUT) ./amiberry/run-clidrive.sh
	@echo "=== output.log ==="
	@cat $(APP_DIR)/output.log

# FGets line-termination diagnostic — runs three back-to-back parent_fgets
# invocations (LF, CR, CRLF) and tells us which terminator(s) libnix's
# stdio fgets() actually accepts as end-of-line through our custom DOS
# handler. Pure local test — no network, no SSH, no bebbossh involved.
run-fgets:
	@if [ ! -f $(APP_DIR)/parent_fgets ]; then \
	    echo "No fgets-test binary — run 'make build' first" >&2; exit 1; \
	fi
	@KICK31="$(KICK31)" RUN_TIMEOUT=$(RUN_TIMEOUT) ./amiberry/run-fgets.sh
	@echo "=== output.log ==="
	@cat $(APP_DIR)/output.log

clean:
	rm -rf $(APP_DIR)

.PHONY: build run run-clidrive run-fgets clean
