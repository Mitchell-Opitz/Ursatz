# Ursatz

Ursatz is a computational music platform in C11: a theory-neutral library, an analyzer that reads MIDI and MusicXML into an analysis, a seeded composer, and a local web GUI. One repository, one CMake build.

## Parts

Each part builds on the ones before it, in this order.

| Folder | Part | Docs |
|---|---|---|
| `library/` | The music theory library (`libursatz`): a canonical representation of musical material | [README](library/README.md), [docs](library/docs/README.md) |
| `analyzer/` | The Analyzer: a library and the `ursatz-analyzer` command that write the JSON analysis document and its text export | [README](analyzer/README.md), [docs](analyzer/docs/README.md) |
| `composer/` | The Composer: a seeded, reproducible composing partner and the `ursatz-composer` command | [README](composer/README.md), [docs](composer/docs/README.md) |
| `gui/` | The GUI: a local web UI over the other parts (the `ursatz-gui-backend` server and its frontend) | [README](gui/README.md), [docs](gui/docs/README.md) |

`third_party/` holds the vendored cJSON, ezxml and civetweb. Nothing is fetched at configure time.

## Build and test

From the repository root:

```
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure -LE slow
```

- `-LE slow` runs the fast set (what CI runs). `-L slow` runs the slow set; the nightly workflow runs every test.
- `ctest --test-dir build/<part>` runs one part's tests. Each part builds only from the root.
- Needs cmake 3.28+, and a C11 compiler.
- The build type defaults to Debug. Release builds work for tests too (the guard `test_asserts_on`), but the default stays Debug.

Tests are registered with `ursatz_test()` (`cmake/UrsatzTest.cmake`; `ursatz_test_executable()` and `ursatz_test_library()` build shared helpers). It compiles every test executable with `-UNDEBUG`, so asserts hold in any build type, defines `URSATZ_FIXTURES_DIR` (the root `tests/fixtures`), and sets the timeout (120 s, or 1800 s for a `slow` test). Labels: `slow` (nightly only), `valgrind` (always with `slow`), `schema`, `accuracy`, `golden`. Some tests need valgrind, python3 with jsonschema or node (`cmake/UrsatzTools.cmake`); a test whose tool is missing is not registered, and `-DURSATZ_REQUIRE="valgrind;jsonschema;node"` makes the configure fail when a named tool is missing.

Options (both ON by default):

- `URSATZ_TESTS`: build and register the tests of every part.
- `URSATZ_GUI`: build the GUI and its HTTP server.

## Checks

`tools/ci/` holds the boundary, docs and test-timing scripts that CI runs, each runnable from anywhere in the repository after the build: `check-docs.sh <part>`, `check-library-includes-{analyzer,composer,gui}.sh build`, `check-chain-builds.sh`, `check-cli-api.sh`, `check-statics.sh <part>...`, `check-layers.sh`, `check-hearing.sh`, `check-frontend-theory.sh`, `check-reach.sh [<scan build dir>]` and `slowest-tests.sh build`. `check-reach.sh` builds its own scan tree (`build-reach` by default, tests off) and lists every function in the repository's own C code that no application main (`ursatz-analyzer`, `ursatz-composer`, `ursatz-gui-backend`) reaches, in `<scan build dir>/reach/unreached.txt`. CI runs it with `tools/ci/reach-allow.txt`: a function no application reaches fails CI unless it is listed there with a reason, and a listed one that is reached or gone fails too. Delete such a function, or list it.
