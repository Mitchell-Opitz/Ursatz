# Ursatz GUI

A local web UI for the Ursatz platform. You upload a MIDI or MusicXML file,
the Analyzer library analyzes it, and the GUI shows the analysis document
section by section next to the score. Pieces can be grouped into
Collections. This repo is a host/shell around independent tool modules, not
a monolithic application.

## Building

```
git submodule update --init --recursive
cmake -B build
cmake --build build
ctest --test-dir build
```

Requires CMake 3.20 or newer, a C11 compiler, SQLite development headers and
pkg-config. Ursatz is a git submodule at `external/ursatz`, pinned at PR #195
(`d2b85ce`). The Analyzer (version 0.6.0) is a second submodule,
`external/ursatz-analyzer`, pinned at PR #67 (`ee8a901`). cJSON 1.7.18 is
vendored at `external/cjson` and added first, so the Ursatz Analyzer reuses it.
`scripts/check-pins.sh` fails when a pin is not on its remote's `main` or when
one repository is pinned at two commits; CI runs it after the submodule
checkout.

Two more checks keep the GUI a client and CI runs both (the build is
configured with `URSATZ_LEGACY_INCLUDE_DIRS` OFF, so only staged public headers
are visible):

- `scripts/check-library-includes.sh build` fails when `backend/src` or
  `backend/include` includes an Ursatz header other than the two storage edge
  headers above, an Analyzer header outside `ursatz_analyzer/`, a Composer header
  other than `ursatz_composer/composer_api.h`, or any `*_internal.h`.
- `scripts/check-frontend-theory.sh` fails when a file under `frontend/public`
  (not `vendor/`) defines `playbackFromMidi`, `randomSettings`, `defaultCadence`,
  `parseHarmonyToken`, `HARMONY_TOKEN`, `KEY_TONICS`, `RANDOM_METERS`,
  `chordProblems` or `numeralsOk`.

Done-when for the client boundary (P27), checked by hand: analyze a reference
piece (same claims and score as before the phase); make the "Knight's house"
theme with the same seeded plan; during a compose `ps` shows no
`ursatz-composer` process; opening a piece imports it once (the import counter);
all checks above pass.
`git submodule update --init --recursive` (or `git clone --recurse-submodules`
up front) is required before the first build. Ursatz's own tests are not
built (`URSATZ_BUILD_TESTS` is OFF). The Composer is a third submodule,
`external/ursatz-composer`, pinned at PR #95 (`10ea20b`, version 0.4.1, API 1.1). The backend links the Composer
library from the submodule (see Composer routes), without the command, and it reuses the cJSON, Ursatz and Analyzer
targets above. If the submodules are missing the build stops: run `git submodule update --init
--recursive` and build again.

## Out of scope

Native webview packaging, authentication, claim
editing, and accepting or disputing a claim (no promotion path exists). The
Composer has no blend, corpus seeding or explain.
