# Place amlc.jar in this folder or change value.
AMLC:=amlc.jar
CMD=java -jar $(AMLC)

build:
	$(CMD) build . -bt linux-x64 -ll5 -maxOneError

build-macos:
	$(CMD) build . -bt macos -ll5 -maxOneError

build-macos-arm:
	$(CMD) build . -bt macos-arm -ll5 -maxOneError

build-amigaos:
	$(CMD) build . -bt amigaos-docker -ll5

build-fld: # force load dependencies
	$(CMD) build . -fld -bt linux-x64 -ll5 -maxOneError

run:
	$(CMD) run . -bt linux-x64 -ll5 -maxOneError

clean:
	$(CMD) clean . -bt linux-x64
	$(CMD) clean . -bt macos
	$(CMD) clean . -bt macos-arm
	$(CMD) clean . -bt amigaos-docker
