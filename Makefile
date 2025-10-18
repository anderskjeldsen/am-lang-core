# place amlc.jar in this folder or change value.
AMLC:=amlc.jar
CMD=java -jar $(AMLC)

build:
	$(CMD) build . -bt linux-x64 -ll5 -maxOneError

build-force-deps:
	$(CMD) build . -fld -bt linux-x64 ll 4

test:
	$(CMD) test . -bt linux-x64
