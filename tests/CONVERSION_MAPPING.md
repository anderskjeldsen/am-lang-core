# Examples.aml to Test Files Mapping

This document shows how the example functions from `src/am-lang/Am/Examples/Examples.aml` have been converted into unit tests.

## Conversion Summary

### CollectionsTests.aml
- **main10** - HashSet operations → `testHashSetBasicOperations()`, `testHashSetNoDuplicates()`
- **main11** - HashSet with Strings → Covered in `testHashSetBasicOperations()`
- **main8** - HashMap operations → `testHashMapBasicOperations()`, `testHashMapContainsKey()`, `testHashMapContainsValue()`, `testHashMapRemove()`, `testHashMapClear()`
- **main28** - TreeSet examples → `testTreeSetBasicOperations()`, `testTreeSetSorting()`, `testTreeSetNoDuplicates()`, `testTreeSetRemove()`, `testTreeSetReverseOrder()`, `testTreeSetWithStrings()`

### StringTests.aml
- **main12** - StringBuilder → `testStringBuilder()`, `testStringBuilderMultipleAdds()`
- **main24** - String interpolation → `testStringInterpolation()`, `testStringInterpolationWithExpression()`

### ControlFlowTests.aml
- **main30** - Switch statements → `testSwitchWithInt()`, `testSwitchWithString()`
- **main27** - Continue in loops → `testForLoopWithContinue()`, `testWhileLoopWithContinue()`, `testEachLoopWithContinue()`
- **main23** - Break in loops → `testForLoopWithBreak()`
- **main22** - Pre-increment → `testPreIncrement()`
- **main16** - While loop with break → `testWhileLoop()` (covered)

### LangTests.aml
- **main21** - Array literals → `testArrayLiterals()`
- **main26** - Array iteration → `testArrayIteration()`
- **main18** - Int.maxValue → `testIntMaxValue()`
- **main14** - Char to int conversion → `testCharToInt()`
- **main17** - Boolean operations → `testBooleanOperations()`
- **main13** - ClassRef operations → `testClassRefForString()`, `testClassRefForInt()`
- **main25** - Static variable access → `testStaticFieldAccess()`

### GenericsTests.aml
- **main19** - List operations with filter/firstOr → `testListFilter()`, `testListFirstOr()`
- **main20** - Generic types → `testGenericListWithStrings()`, `testGenericListWithInts()`

### UtilTests.aml
- **main15** - TextParser → `testTextParserParseUntil()`, `testTextParserPeek()`, `testTextParserSkip()`

### ExceptionTests.aml
- **main13** (partial) - Exception handling → `testThrowAndCatch()`, `testExceptionMessage()`, `testListIndexOutOfBounds()`

## Examples NOT Converted (IO-related)

The following examples were **not** converted because they involve file I/O, networking, or threading:

- **main2** - Socket/Networking operations (uses Socket, SocketStream)
- **main6** - SSL Socket operations (uses SslSocketStream)
- **main13** (partial) - Thread operations (Thread.start())
- **main29** - Command line arguments (external input)

## Test Statistics

- **Total test files created**: 8 (7 test suites + 1 master runner)
- **Total test cases**: 60+
- **Lines of test code**: ~1,134 lines
- **Test framework code**: 60 lines

## Test Framework Features

The custom `Am.Testing.TestRunner` class provides:
- Test execution with automatic pass/fail tracking
- Assertion methods (assert, assertEquals, assertTrue, assertFalse)
- Exception handling within tests
- Summary reporting

## Running the Tests

To run all tests:
```aml
AllTests.main()
```

To run individual test suites:
```aml
CollectionsTests.main()
StringTests.main()
ControlFlowTests.main()
LangTests.main()
GenericsTests.main()
UtilTests.main()
ExceptionTests.main()
```
