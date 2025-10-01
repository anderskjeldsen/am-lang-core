# Examples.aml to Test Files Mapping

This document shows how the example functions from `src/am-lang/Am/Examples/Examples.aml` have been converted into unit tests.

## Conversion Summary

### CollectionsTests.aml
- **main10** - HashSet operations → `hashSetAddAndHas()`, `hashSetNoDuplicates()`
- **main11** - HashSet with Strings → Covered in `hashSetAddAndHas()`
- **main8** - HashMap operations → `hashMapSetAndGet()`, `hashMapContainsKey()`, `hashMapContainsValue()`, `hashMapRemove()`, `hashMapClear()`
- **main28** - TreeSet examples → `treeSetAddAndHas()`, `treeSetSorting()`, `treeSetNoDuplicates()`, `treeSetRemove()`, `treeSetReverseOrder()`, `treeSetWithStrings()`

### StringTests.aml
- **main12** - StringBuilder → `stringBuilder()`, `stringBuilderMultipleAdds()`
- **main24** - String interpolation → `stringInterpolation()`, `stringInterpolationWithExpression()`

### ControlFlowTests.aml
- **main30** - Switch statements → `switchWithInt()`, `switchWithString()`
- **main27** - Continue in loops → `forLoopWithContinue()`, `whileLoopWithContinue()`, `eachLoopWithContinue()`
- **main23** - Break in loops → `forLoopWithBreak()`
- **main22** - Pre-increment → `preIncrement()`
- **main16** - While loop with break → `whileLoop()` (covered)

### LangTests.aml
- **main21** - Array literals → `arrayLiterals()`
- **main26** - Array iteration → `arrayIteration()`
- **main18** - Int.maxValue → `intMaxValue()`
- **main14** - Char to int conversion → `charToInt()`
- **main17** - Boolean operations → `booleanOperations()`
- **main13** - ClassRef operations → `classRefForString()`, `classRefForInt()`
- **main25** - Static variable access → `staticFieldAccess()`

### GenericsTests.aml
- **main19** - List operations with filter/firstOr → `listFilter()`, `listFirstOr()`
- **main20** - Generic types → `genericListWithStrings()`, `genericListWithInts()`

### UtilTests.aml
- **main15** - TextParser → `textParserParseUntil()`, `textParserPeek()`, `textParserSkip()`

### ExceptionTests.aml
- **main13** (partial) - Exception handling → `throwAndCatch()`, `exceptionMessage()`, `listIndexOutOfBounds()`

## Examples NOT Converted (IO-related)

The following examples were **not** converted because they involve file I/O, networking, or threading:

- **main2** - Socket/Networking operations (uses Socket, SocketStream)
- **main6** - SSL Socket operations (uses SslSocketStream)
- **main13** (partial) - Thread operations (Thread.start())
- **main29** - Command line arguments (external input)

## Test Statistics

- **Total test files created**: 7 test classes
- **Total test cases**: 50
- **Lines of test code**: ~800 lines

## Test Framework

Tests use the `Assert` class from `@anderskjeldsen/am-tests`:
- `Assert.assertEquals(expected, actual)` - Assert two values are equal
- `Assert.assertTrue(condition)` - Assert a condition is true
- `Assert.assertFalse(condition)` - Assert a condition is false

Tests are declared with the `test` keyword (not `fun`) and have no parameters. The compiler automatically discovers and runs all tests.

## Running the Tests

The compiler automatically runs all tests during unit testing. No main functions or manual test runners are needed.
