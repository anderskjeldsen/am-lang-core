# place amlc.jar in this folder or change value.
AMLC:=amlc.jar
CMD=java -jar $(AMLC)

build:
	$(CMD) build . -bt linux-x64 -ll5 -maxOneError

build-fld: # force load dependencies
	$(CMD) build . -fld -bt linux-x64 ll 5 -maxOneError

test:
	$(CMD) test . -bt linux-x64 -maxOneError
