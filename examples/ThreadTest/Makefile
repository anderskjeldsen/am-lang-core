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

clean:
	$(CMD) clean . -bt linux-x64
	$(CMD) clean . -bt amigaos-docker
