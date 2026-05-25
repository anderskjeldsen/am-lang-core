# Place amlc.jar in this folder or change value.
AMLC:=amlc.jar
CMD=java -jar $(AMLC)

build:
	$(CMD) build . -bt linux-x64 -ll5 -maxOneError

build-amigaos:
	$(CMD) build . -bt amigaos-docker -ll5

build-fld: # force load dependencies
	$(CMD) build . -fld -bt linux-x64 -ll5 -maxOneError

# Run the amigaos binary under VAMOS (amitools m68k emulator) via Docker.
# Requires the `amitools:latest` image — see ../../../am-lang-compiler-code/amitools-docker/.
# Note: VAMOS does NOT emulate AmigaOS multitasking (CreateNewProc returns
# NULL), so the thread starts fail and join_0 busy-waits forever. Useful
# only as a sanity check that the binary loads. Use run-amigaos-fsuae for
# actual threading.
run-amigaos-vamos:
	docker run --rm -v "$$HOME:/host" amitools:latest \
		vamos -C 68020 /host/$(patsubst $(HOME)/%,%,$(CURDIR))/builds/bin/amigaos/app

# Run the amigaos binary under FS-UAE headlessly. Boots from a small
# directory hard drive at fs-uae/boot whose Startup-Sequence runs the
# app and pipes stdout to output.log (visible on the host). The launcher
# polls for a sentinel line and kills FS-UAE once it appears.
run-amigaos-fsuae:
	./fs-uae/run-headless.sh

# Run the amigaos binary headlessly inside Docker via Amiberry (arm64
# JIT — finishes ThreadTest in ~5s on Apple Silicon, vs fs-uae-in-docker
# which never makes visible boot progress). The image is baked by
# ../../../am-lang-compiler-code/amiberry-docker/ and built on demand if
# the local tag is missing. The kickstart 3.1 ROM is not redistributable,
# so the caller must point KICK31 at their own copy:
#   make run-amigaos-amiberry KICK31=/path/to/kick31.rom
AMIBERRY_DOCKER_DIR := ../../../am-lang-compiler-code/amiberry-docker
run-amigaos-amiberry:
	@if [ -z "$(KICK31)" ]; then \
		echo "KICK31 not set — point it at a kickstart 3.1 ROM:"; \
		echo "  make run-amigaos-amiberry KICK31=/path/to/kick31.rom"; \
		exit 1; \
	fi
	@if [ ! -f "$(KICK31)" ]; then \
		echo "KICK31 file not found: $(KICK31)"; \
		exit 1; \
	fi
	@if [ -z "$$(docker images -q amiberry:latest)" ]; then \
		echo "amiberry:latest not found — building..."; \
		$(AMIBERRY_DOCKER_DIR)/build.sh; \
	else \
		echo "Using cached amiberry:latest image."; \
	fi
	docker run --rm \
		-v "$(KICK31):/rom/kick31.rom:ro" \
		-v "$(CURDIR)/builds/bin/amigaos:/app" \
		amiberry:latest

clean:
	$(CMD) clean . -bt linux-x64
	$(CMD) clean . -bt amigaos-docker
