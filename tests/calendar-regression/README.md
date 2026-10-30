# Signed calendar regression

Run with the isolate compiler and local-package path, for example:

```sh
MAKEFLAGS=-j4 java -Xmx768m -jar ../../../amlc.jar build . -bt macos-arm -lpp ../../../
./builds/bin/macos-arm/app
```

Targets: `macos-arm` exercises OS named zones; `calendar-utc` forces the portable
UTC backend on macOS; `amigaos_docker` cross-compiles with `amiga-gcc:latest`.
The two macOS targets enable address and undefined-behavior sanitizers.
The Amiga executable needs a separate runtime test on hardware or an emulator.

The new static `DateTime.calendarParts` and `calendarMillis` APIs accept signed
millisecond timestamps independently of the existing unsigned instance API.
They normalize calendar fields and cover +/-8.64e15 milliseconds using constant-time
Gregorian arithmetic, without depending on the width of `time_t` or changing `TZ`.
Parts use 1-based months and offsets east of UTC. Invalid timestamps/unsupported
zones produce an empty parts array; composition/parsing failures produce NaN.
Composition currently bounds each input field to +/-1e9 before normalization.

`resolveTimeZone`, `timeZoneAbbreviation` and `availableTimeZones` use the Apple
system database on macOS. Other targets currently offer UTC (and accept GMT as an
alias); empty zone selects UTC there. There is no bundled timezone database.
Amiga's existing `Date.now()` clock backend is reused and assumes its clock is UTC.
DST gap/overlap disambiguation follows CoreFoundation and is not yet guaranteed to
match every JavaScript case. ISO parsing is shared; macOS also preserves the prior
browser's local slash-date and RFC-like forms. Locale-sensitive formatting remains
in the browser adapter and is incomplete. The old ULong DateTime API is unchanged.

Tests cover pre-epoch milliseconds, Gregorian eras, leap years via browser cases,
normalization, expanded years, range limits, invalid ISO suffixes, timezones and DST.
Browser companion: `tucanium/tests/calendar_test.py` and `calendar.html`.
The ISO/TimeClip boundary reference is
[ECMAScript Date](https://tc39.es/ecma262/multipage/numbers-and-dates.html#sec-date-objects).
