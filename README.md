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
- suspendable functions
- lambda expressions
- native c support, 
- exceptions (try,catch,throw) 
- and a lot more. 

There are still some major features missing, like for example for-loops. I have to use while loops until I prioritize for-loops 🙂 Currently it compiles only on Mac/Linux/Windows (using Docker), but one of the ultimate goals is to re-write the compiler in its own language. The compiler doesn't generate any machine code on its own, it writes C-code and lets GCC make the machine code. Performance-wise it's not as efficient as C (obviously?), but one can easily let c/asm do the heavy lifting and use this for orchestration. 

# Requirements
- Java 11+
- Docker
- Windows, MacOS, Linux

# Quick start (for AmigaOS3):

There is no "main" function in this library, but there are some examples in src/Am/Examples/Examples.aml - just rename for example main12() to main() and build.

build for AmigaOS (3.x): \
java -jar amlc-1.jar . -bt amigaos_docker_unix-x64

add more runtime logging: \
java -jar amlc-1.jar . -bt amigaos_docker_unix-x64 -rl

add more comments in the code: \
java -jar amlc-1.jar . -bt amigaos_docker_unix-x64 -rdc

Binary will end up in builds/bin/amigaos/ as a file called "app". Copy the "app" file to an Amiga environment and execute it. 

# Code example

The following code fills up a HashSet2 (will be renamed to HashSet) and times it.

namespace Am.Examples {    

    class CoreStartup {
        import Am.Lang
        import Am.Lang.Diagnostics
        import Am.IO
        import Am.IO.Networking
        import Am.Collections

        static fun main() {
            var set = new HashSet2<Int>()
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