# Am Lang Core Tests

This directory contains unit tests for the Am Lang Core library.

## Test Files

- **CollectionsTests.aml** - Tests for List, HashMap, HashSet, and TreeSet
- **StringTests.aml** - Tests for String operations and StringBuilder
- **ControlFlowTests.aml** - Tests for control flow (if/else, switch, loops, break, continue)
- **LangTests.aml** - Tests for primitive types, arrays, and basic language features
- **GenericsTests.aml** - Tests for generic types and functions
- **UtilTests.aml** - Tests for utility classes like TextParser
- **ExceptionTests.aml** - Tests for exception handling
- **AllTests.aml** - Master test runner that executes all test suites

## Test Framework

The tests use a simple test framework (`Am.Testing.TestRunner`) that provides:
- `test(name, testFunc)` - Run a test with a name and test function
- `assert(condition, message)` - Assert a condition is true
- `assertEquals(expected, actual, message)` - Assert two values are equal
- `assertTrue(condition, message)` - Assert a condition is true
- `assertFalse(condition, message)` - Assert a condition is false
- `printSummary()` - Print test results summary

## Running Tests

To run a specific test file, change the `main()` function reference in the package to point to the test class. For example:

```aml
namespace Am.Tests {
    class CoreStartup {
        static fun main() {
            CollectionsTests.main()
        }
    }
}
```

Or run all tests:

```aml
namespace Am.Tests {
    class CoreStartup {
        static fun main() {
            AllTests.main()
        }
    }
}
```

Then build and run using the AmLang compiler (amlc):

```bash
java -jar amlc-1.jar . -acmd -bt <build-target>
```

## Test Coverage

These tests cover the main functionality from Examples.aml, focusing on:
- Non-native classes (List, HashMap, HashSet, TreeSet, StringBuilder)
- Core language features (control flow, generics, primitives)
- Utility classes (TextParser)
- Exception handling

Tests that involve IO operations (File, FileStream, Socket, Thread) are not included as per the requirements.
