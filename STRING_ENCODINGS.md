# String encodings, embedded zero bytes, and Amiga compatibility

Status: implementation notes and proposed changes, checked 2026-09-29.
This document does not imply that the proposed fixes or encoding conversions
have been implemented.

## Current representation

AmLang strings use UTF-8 byte storage with an explicit byte length. A trailing
zero byte is also allocated for interoperability with C code. The trailing zero
is outside the recorded content length; it must not determine that length.

The native `string_holder` also records the UTF-16 code-unit count and has a
lazily allocated UTF-16 view:

- ASCII indexed access reads directly from the byte storage.
- Non-ASCII indexed access can build and reuse the UTF-16 cache.
- `characterAt()` returns a UTF-16 code unit, not an arbitrary UTF-8 byte.
- `getChars()` returns a new `UShort[]`, copying the cached UTF-16 view when
  available. It does not change the string's UTF-8 backing storage.
- Characters outside the Basic Multilingual Plane occupy two UTF-16 units
  (a surrogate pair). Code-unit counts are not necessarily counts of visible
  characters.

Relevant implementation: [core.h](src/native-c/libc/core.h),
[String.c](src/native-c/libc/Am/Lang/String.c), and
[String.aml](src/am-lang/Am/Lang/String.aml).

The `getChars()` documentation currently contains an outdated claim that
`characterAt()` indexes bytes. Its current native implementation uses UTF-16
unit indexing and the cache.

## What an embedded zero byte means

An embedded zero byte has numeric value `0` and is part of the content, rather
than just the terminator after it. For example, these three bytes represent
`A`, the null character U+0000, and `B` in UTF-8:

```text
Content bytes (hex): 41 00 42
Recorded byte length: 3
With trailing terminator: 41 00 42 00
```

Ordinary non-ASCII UTF-8 characters do not contain zero bytes. For example,
`—` (U+2014, em dash) is `E2 80 94`. A zero byte in valid UTF-8 encodes U+0000.

UTF-16 is different: zero bytes are common even in ordinary text. UTF-16LE
encodes `AB` as `41 00 42 00`. Those bytes require conversion before being
stored as internal UTF-8.

## C string functions and UTF-8

C strings can contain UTF-8, but null-terminated string functions do not treat
an embedded zero as content. They consider it the end of the string.

| Operation | Behaviour |
| --- | --- |
| `strlen()` | Counts bytes up to the first zero, not Unicode characters. |
| `strncpy(dst, src, n)` | Copies up to the first zero, then pads the remaining destination bytes with zeros. |
| `strcmp()` / `strstr()` | Operate on null-terminated byte strings; cannot inspect content after an embedded zero. |
| `memcpy(dst, src, n)` | Copies exactly the specified number of bytes, including zeros. |
| `memcmp(a, b, n)` | Compares the specified bytes, including zeros; it does not perform Unicode-aware comparison. |

Length-aware byte operations preserve UTF-8 data, but do not themselves decode
characters. Searching, slicing and character indexing must also respect their
specified byte or character semantics.

## Current `fromBytes()` and `toBytes()` limitations

The current shared native implementation ignores the `encoding` argument in
both methods, despite their public documentation describing named encodings.

`fromBytes()` allocates byte storage and uses `strncpy()` to copy the input. It
then calculates string metadata assuming UTF-8. This is not a general encoding
conversion. For input `41 00 42`, the stored bytes become `41 00 00`: the `B`
is lost even though the recorded length is still three bytes.

`toBytes()` uses `memcpy()` and the stored byte length. It returns the internal
bytes without converting to the requested encoding. It cannot recover content
already lost during construction.

The immediate copying fix is conceptually:

```c
memcpy(new_str, input_bytes, byte_length);
new_str[byte_length] = '\0';
```

Allocation sizes, input lengths, allocation failures and string metadata still
need their normal checks. Replacing this copy alone does not establish complete
embedded-null support: constructors, concatenation, hashing, equality, searching,
output and native API adapters must also be audited for terminator-based logic.

## Why appending bytes as characters corrupts text

`StringBuilder.addChar()` accepts a UTF-16 code unit and encodes it. It is not a
raw-byte append operation. Passing each byte of `E2 80 94` to `addChar()` treats
those values as three separate character codes, producing corrupted text.

The file-reading path should preserve the original bytes:

```text
File -> byte buffer -> UByte[] -> String.fromBytes(bytes, "UTF-8") -> parser
```

[TextStream.readString()](src/am-lang/Am/IO/TextStream.aml) follows this pattern.
Currently, `fromBytes()` assumes the bytes are UTF-8 rather than dispatching on
the encoding name. For UTF-8 input without embedded zeros, this avoids the
byte-as-character bug. The embedded-zero copying bug remains separate.

Use `getChars()` when a consumer actually needs an array of UTF-16 units.
Converting an already corrupted string to UTF-16 will not repair it. A parser
that accepts a `String` need not allocate an additional character array merely
to read a file correctly.

## Proposed encoding support

Keep internal storage as UTF-8 and convert at the byte-array boundaries:

```text
fromBytes(input, encoding): source encoding -> internal UTF-8
 toBytes(encoding):         internal UTF-8 -> destination encoding
```

A practical initial set is:

- UTF-8
- ASCII
- ISO-8859-1 (Latin-1)
- Windows-1252, which is distinct from Latin-1
- UTF-16LE and UTF-16BE

This is a contained conversion feature; it does not require replacing string
storage or the UTF-16 cache. A broad catalogue of encodings would need a larger
codec implementation or platform-dependent conversion libraries.

Before implementation, define:

- Accepted encoding names and aliases; reject unsupported names explicitly.
- Whether malformed input throws or is replaced with U+FFFD.
- Whether unrepresentable output characters throw or use a replacement byte.
- BOM handling, especially if a generic `UTF-16` name is supported.
- Behaviour for incomplete UTF-16 units and unpaired surrogates.

Encoding names must no longer be silently ignored. UTF-16 conversion should
read/write bytes with explicit endianness, rather than casting potentially
unaligned byte buffers to native `unsigned short*` pointers.

## Amiga compatibility and API boundaries

Explicit byte lengths and `memcpy()` work on Amiga as well as desktop systems.
Internal UTF-8 storage and a small set of conversion routines do not require
Unicode support from AmigaOS or an external conversion library.

Amiga-specific considerations remain:

- Account for memory use, especially when allocating conversion buffers and
  the optional UTF-16 cache.
- Handle UTF-16 byte order explicitly on big-endian 68k and PowerPC systems,
  just as on little-endian desktop systems.
- AmigaDOS and other APIs accepting null-terminated text cannot represent
  embedded U+0000. Reject such input where truncation would change meaning,
  such as filenames or commands; do not silently pass a shortened string.
- Correct internal storage does not make an OS text API UTF-8-aware. Convert
  to the encoding expected by that API, or use a renderer that understands
  the application's Unicode representation.

Binary data should normally remain in byte arrays. Embedded-null support in
strings is about preserving valid text content and predictable conversions,
not making every external text API binary-safe.

## Suggested verification

1. Preserve UTF-8 bytes through `fromBytes()` / `toBytes()`, including empty
   input, ASCII, accented letters, the em dash and supplementary characters.
2. Preserve leading, interior and trailing U+0000 within the recorded length.
3. Check `characterAt()` and `getChars()` before and after cache creation.
4. Test each encoding in both directions, including non-ASCII mappings and
   UTF-16 surrogate pairs with explicit byte order.
5. Exercise malformed input, unsupported encoding names, and characters that
   cannot be represented in a requested output encoding.
6. Test API adapters that must reject embedded zeros.
7. Run representative cases on Amiga and desktop targets.
