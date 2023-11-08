# am-lang-core

Quick start (for AmigaOS3): \n

There is no "main" function in this library, but there are some examples in src/Am/Examples/Examples.aml - just rename for example main12() to main() and build.

build for AmigaOS (3.x): \
java -jar amlc-1.jar . -bt amigaos_docker_unix-x64

add more runtime logging: \
java -jar amlc-1.jar . -bt amigaos_docker_unix-x64 -rl

add more comments in the code: \
java -jar amlc-1.jar . -bt amigaos_docker_unix-x64 -rdc

Binary will end up in builds/bin/amigaos/ as a file called "app". Copy the "app" file to an Amiga environment and execute it. 

