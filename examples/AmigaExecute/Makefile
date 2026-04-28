# Place amlc.jar in this folder or change value.
AMLC:=amlc.jar
CMD=java -jar $(AMLC)

build:
	$(CMD) build . -bt linux-x64 -ll5 -maxOneError

build-amigaos:
	$(CMD) build . -bt amigaos-docker -ll5

build-fld: # force load dependencies
	$(CMD) build . -fld -bt linux-x64 -ll5 -maxOneError

clean:
	$(CMD) clean . -bt linux-x64
	$(CMD) clean . -bt amigaos-docker
