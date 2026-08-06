# Prefer local amlc.jar, otherwise use the one in am-lang-core.
AMLC:=../../amlc.jar
ifneq ("$(wildcard amlc.jar)","")
AMLC:=amlc.jar
endif
CMD=java -jar $(AMLC)
DOLPHIN?=$(shell command -v dolphin-emu 2>/dev/null || command -v dolphin 2>/dev/null || echo /Applications/Dolphin.app/Contents/MacOS/Dolphin)
GAMECUBE_APP:=builds/bin/nintendo-ppc/app
GAMECUBE_ELF:=builds/bin/nintendo-ppc/app.elf
GAMECUBE_DOL:=builds/bin/nintendo-ppc/app.dol
# Launch Dolphin with a self-contained profile (examples/gameCubeTest/dolphin/)
# instead of your global GUI config, so the run can't be broken by whatever the
# GUI last saved. That profile pins the two settings that matter on Apple Silicon:
#   * CPUCore = 0 (Interpreter) — the JIT miscompiles this homebrew (black/hang).
#   * GFXBackend = Software Renderer — driver-independent, always presents;
#     OGL on modern macOS is flaky. Vulkan also works if you prefer GPU speed.
# -v pins the video backend at launch — Dolphin rewrites the profile ini on
# every exit, so a backend set only in the ini gets clobbered by the last run.
DOLPHIN_FLAGS?=-u "$(CURDIR)/dolphin" -v Vulkan

build:
	$(CMD) build . -bt linux-x64 -ll5 -maxOneError

build-gamecube:
	$(CMD) build . -bt gamecube-docker -ll5

package-gamecube-dol: build-gamecube
	@if [ ! -f "$(GAMECUBE_APP)" ]; then echo "GameCube ELF missing at $(GAMECUBE_APP)"; exit 1; fi
	docker run --rm -v "$(CURDIR)/builds:/src" --entrypoint /bin/sh -i webhdx/devkitppc-libogc2 -lc 'cd /src/bin/nintendo-ppc && /opt/devkitpro/tools/bin/elf2dol app app.dol'

run-gamecube-dolphin: package-gamecube-dol
	@if [ ! -x "$(DOLPHIN)" ]; then echo "Dolphin executable not found. Set DOLPHIN=/path/to/Dolphin"; exit 1; fi
	@if [ ! -f "$(GAMECUBE_DOL)" ]; then echo "GameCube DOL missing at $(GAMECUBE_DOL)"; exit 1; fi
	"$(DOLPHIN)" $(DOLPHIN_FLAGS) -e "$(GAMECUBE_DOL)"

run-gamecube-dolphin-no-build:
	@if [ ! -x "$(DOLPHIN)" ]; then echo "Dolphin executable not found. Set DOLPHIN=/path/to/Dolphin"; exit 1; fi
	@if [ ! -f "$(GAMECUBE_DOL)" ]; then echo "GameCube DOL missing at $(GAMECUBE_DOL). Run make package-gamecube-dol first."; exit 1; fi
	"$(DOLPHIN)" $(DOLPHIN_FLAGS) -e "$(GAMECUBE_DOL)"

run-gamecube-dolphin-elf: build-gamecube
	@if [ ! -x "$(DOLPHIN)" ]; then echo "Dolphin executable not found. Set DOLPHIN=/path/to/Dolphin"; exit 1; fi
	@if [ ! -f "$(GAMECUBE_APP)" ]; then echo "GameCube ELF missing at $(GAMECUBE_APP)"; exit 1; fi
	cp "$(GAMECUBE_APP)" "$(GAMECUBE_ELF)"
	"$(DOLPHIN)" $(DOLPHIN_FLAGS) -e "$(GAMECUBE_ELF)"

build-fld: # force load dependencies
	$(CMD) build . -fld -bt linux-x64 -ll5 -maxOneError

run:
	$(CMD) run . -bt linux-x64 -ll5 -maxOneError

clean:
	$(CMD) clean . -bt linux-x64
	$(CMD) clean . -bt gamecube-docker
