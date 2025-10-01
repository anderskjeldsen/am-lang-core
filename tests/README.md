# Am Lang Core Tests

This directory contains unit tests for the Am Lang Core library.

## Test Files

- **CollectionsTests.aml** - Tests for List, HashMap, HashSet, and TreeSet
- **StringTests.aml** - Tests for String operations and StringBuilder
- **ControlFlowTests.aml** - Tests for control flow (switch, loops, break, continue)
- **LangTests.aml** - Tests for primitive types, arrays, and basic language features
- **GenericsTests.aml** - Tests for generic types and functions
- **UtilTests.aml** - Tests for utility classes like TextParser
- **ExceptionTests.aml** - Tests for exception handling

## Test Framework

The tests use the Assert class from the `@anderskjeldsen/am-tests` package. Each test class contains multiple `test` functions (not `fun`) with no parameters. Tests throw an AssertException if they fail.

## Test Structure

Each test file is a class containing static test functions:

```aml
namespace Am.Tests {
    class MyTests {
        import Am.Lang
        import Am.Testing
        
        static test myFirstTest() {
            var value = 5
            Assert.assertEquals(5, value)
        }
        
        static test mySecondTest() {
            var list = List.newList<Int>()
            list.add(1)
            Assert.assertEquals(1, list.getSize())
        }
    }
}
```

## Running Tests

The compiler automatically discovers and runs tests. No main functions are needed.

## Test Coverage

These tests cover the main functionality from Examples.aml, focusing on:
- Non-native classes (List, HashMap, HashSet, TreeSet, StringBuilder)
- Core language features (control flow, generics, primitives)
- Utility classes (TextParser)
- Exception handling

Tests that involve IO operations (File, FileStream, Socket, Thread) are not included as per the requirements.
