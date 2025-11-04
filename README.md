# am-lang-core

# What is am-lang-core?

"AmLang" is a new programming language mainly targetted at niche platforms like AmigaOS (Also playing with MorphOS, Nintendo Wii Homebrew, Raspberry Pi Pico, Atari TOS). Read more about it below.

am-lang-core is the core library that one should use when programming AmLang. It has classes like Int, String, File, Stream, FileStream, HashMap, HashSet, List and so on. 

# Am Lang Features
AmLang is inspired by other programming languages like Kotlin, Java, C#, TypeScript, Swift etc. 

- ARC (automatic reference counting)
- classes
- interfaces
- namespaces
- for-loops
- switch statements
- suspendable functions
- lambda expressions
- native c support
- exceptions (try,catch,throw)
- and a lot more. 

Currently it compiles only on Mac/Linux/Windows (using Docker), but one of the ultimate goals is to re-write the compiler in its own language. The compiler doesn't generate any machine code on its own, it writes C-code and lets GCC make the machine code. Performance-wise it's not as efficient as C (obviously?), but one can easily let c/asm do the heavy lifting and use this for orchestration. 

# Requirements
- Java 11+
- Docker
- Windows, MacOS, Linux

# Quick start (for AmigaOS3):

There is no "main" function in this library, but there are some examples in src/Am/Examples/Examples.aml - just rename for example main12() to main() and build.

build for AmigaOS (3.x): \
java -jar amlc-1.jar . -acmd -bt amigaos_docker_unix-x64

add more runtime logging: \
java -jar amlc-1.jar . -acmd -bt amigaos_docker_unix-x64 -rl

add more comments in the code: \
java -jar amlc-1.jar . -acmd -bt amigaos_docker_unix-x64 -rdc

Binary will end up in builds/bin/amigaos/ as a file called "app". Copy the "app" file to an Amiga environment and execute it. 

The option "-acmd" means that you allow the compiler to run commands from the package.yml file. 

The option "-bt" specifies which build target (from package.yml) you want to use (which platform to build for and how to build)

# Code example

The following code fills up a HashSet and times it.

    namespace Am.Examples {    

        class CoreStartup {
            import Am.Lang
            import Am.Lang.Diagnostics
            import Am.IO
            import Am.IO.Networking
            import Am.Collections

            static fun main() {
                var set = new HashSet<Int>()
                var startDate = Date.now()
                var i = 1
                var max = 1000000
                ("Adding " + max.toString() + " key-value pairs to a HashSet on an emulated 020").println()
                while(i <= max) {                
                    set.add(i)
                    i++
                }

                var endDate = Date.now()

                ("Time: " + (endDate.getValue() - startDate.getValue()).toString() + "ms").println()

                var testVal = 4
                var iset = set as Set<Int>
                var hasValue = iset.has(testVal)

                if (hasValue) {
                    "found".println()
                } else {
                    "not found".println()
                }
            }
        }
    }


# Build System Notes

## Testing
- **All tests are compiled regardless of which you want to run** - the build system compiles everything in the tests/ directory
- **You cannot build or test a single .aml file - it's all or nothing**
- The entire project (src/ and tests/) is compiled together as one unit
- Tests are built into `builds/test-bin/linux-x64/test_app` (or similar for other platforms)
- Use `make test` to run all tests
- Use `make gdb-test` for automatic crash analysis with GDB
- Use `make gdb-test-interactive` for manual debugging with GDB

## Debugging
- The build system includes comprehensive GDB integration
- `make gdb-test` - Runs tests under GDB and automatically analyzes crashes
- `make gdb-test-interactive` - Drops into interactive GDB session for manual debugging
- `make test-verbose` - Runs tests with high logging output

## Compiler Integration
- Tests use the same compiler as the main build
- The Java compiler (`amlc-0.6.4.jar`) translates AmLang to C code
- GCC then compiles the generated C code
- Native C implementations can override AmLang implementations if present
- Generated C code can be found in `builds/` directory for inspection

# Try AmLang for yourself
We've made a web-based playground (IDE) that you can try here: https://www.kelson.no/tools/amlangide

