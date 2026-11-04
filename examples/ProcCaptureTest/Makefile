# Place amlc.jar in this folder or change value.
AMLC:=amlc.jar
CMD=java -jar $(AMLC)

build:
	$(CMD) build . -bt linux-x64 -ll5 -maxOneError

build-amigaos:
	$(CMD) build . -bt amigaos-docker -ll5

KICK31 ?= $(HOME)/Documents/FS-UAE/Kickstarts/kick31.rom

# Run the amigaos binary headlessly under Amiberry in Docker. The whole
# builds/bin/amigaos dir is mounted as DH1, so am-git + testrepo placed
# there are reachable as DH1:am-git / DH1:testrepo.
run-amigaos-amiberry:
	docker run --rm \
	    -v "$(KICK31):/rom/kick31.rom:ro" \
	    -v "$(CURDIR)/builds/bin/amigaos:/app" \
	    amiberry:latest
