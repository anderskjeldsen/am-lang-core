# Array literals: the v1-only cases (#193)

Nested array literals (`[[1, 2], [3]]`, `Int[][]`, `new Int[][n]`) and arrays of
nullable primitives (`Int?[]`) work with the v1 compiler but not yet with v2, which
has neither arrays of arrays nor `Int?[]`. They live in this separate package rather than under `tests/`, so
am-lang-core's suite still runs under v2. The rest of #193 is in
`tests/ArrayLiteralTests.aml`.

Run with the development compiler (prints `array-literals-v1: all passed`, or
throws on the first failure):

```sh
java -jar ../../../amlc.jar run . -bt linux-x64 -lpp ../../../
```
