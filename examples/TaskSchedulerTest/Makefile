AMLC:=amlc.jar
CMD=java -jar $(AMLC)

build:
	$(CMD) build . -bt macos-arm -ll5 -maxOneError

build-linux:
	$(CMD) build . -bt linux-x64 -ll5 -maxOneError

build-amigaos:
	$(CMD) build . -bt amigaos-docker -ll5

clean:
	$(CMD) clean . -bt macos-arm
	$(CMD) clean . -bt linux-x64
	$(CMD) clean . -bt amigaos-docker
