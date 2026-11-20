# Ursatz

Ursatz is a computational music platform in C11: a theory-neutral library, an analyzer that reads MIDI and MusicXML into an analysis, a seeded composer, and a local web GUI. One repository, one CMake build.

## Parts

Each part builds on the library, and every part builds on `common/`. The Analyzer and the Composer are siblings: neither includes or links the other (the Composer hears through the library chain). The workshop sits above both and holds the data folder; the GUI sits on the workshop only.

| Folder | Part | Docs |
|---|---|---|
| `common/` | The helpers every part shares: one status set, files, strings, JSON, the hash, the job handle and the event bus | [docs](common/docs/README.md) |
| `library/` | The music theory library (`libursatz`): a canonical representation of musical material | [README](library/README.md), [docs](library/docs/README.md) |
| `analyzer/` | The Analyzer: a library that writes the JSON analysis document and its text export (run by `ursatz analyze`) | [README](analyzer/README.md), [docs](analyzer/docs/README.md) |
| `composer/` | The Composer: a seeded, reproducible composing partner (run by `ursatz`) | [README](composer/README.md), [docs](composer/docs/README.md) |
| `workshop/` | The workshop: the one process that holds the data folder (projects, items and their versions, the Library, styles, links), its transactions, the job queue and the events, and answers every query and command | [docs](workshop/docs/README.md) |
| `cli/` | `ursatz`: the one command line, for Claude sessions and scripts, over the workshop: it composes, analyzes and renders, and reads and changes a data folder | [README](cli/README.md), [docs](cli/docs/README.md) |
| `gui/` | The GUI: a local web UI that is a client of the workshop (the `ursatz-gui-backend` server and its frontend) | [README](gui/README.md), [docs](gui/docs/README.md) |

`third_party/` holds the vendored cJSON, ezxml and civetweb. Nothing is fetched at configure time.

## Running the GUI

```
build/gui/backend/ursatz-gui-backend --data <folder> [--port <n>]
```

`--data` names the data folder (`URSATZ_DATA`, or `data_dir` of `<folder>/ursatz.json`; the default is the current directory) and the port defaults to 8765. The folder is created when it is absent; a second process on one folder is refused. A folder from before the workshop (or in layout 1) is refused with what to run:

```
build/cli/ursatz --data <folder> migrate --dry-run   # writes nothing, prints every planned move
build/cli/ursatz --data <folder> migrate [--backup <dir>]
```

A migration copies the whole folder to the backup first (beside it unless `--backup` names a folder) and checks the copy. See [the data folder](workshop/docs/data-folder.md).

## The three layers

What the platform holds about a piece is in three layers, each a JSON file with its own schema (R-625 to R-636, P62):

| Layer | What it is | Format | Schema |
|---|---|---|---|
| Realization | the music as written: lines, performer parts and their notes, the meters and the tempo; one per piece | `ursatz-realization` 1 | [realization.v1.schema.json](library/realization/schema/realization.v1.schema.json) |
| Reading | what one reader (the Analyzer, the Composer or a person) says about one realization, as claims naming its notes; the realization is named by its digest | `ursatz-reading` 1 | [reading.v1.schema.json](library/reading/schema/reading.v1.schema.json) |
| Plan | the piece as a tree of sections with its lanes: what the Composer composes from | `ursatz-plan` 6 | [plan.v6.schema.json](library/plan/schema/plan.v6.schema.json) |

`ursatz analyze <input> --layers <dir>` writes the realization and the Analyzer's reading; `--model-plan <file>` writes the plan a reading gives (`reading_to_plan`). `ursatz theme`, `piece` and `fill` take `--layers <dir>` and write the plan, the realization and the Composer's reading of it. See [library/docs/modules/reading.md](library/docs/modules/reading.md).

## Ursatz Text

Ursatz Text is the compact, lossless text form of a plan, a reading, a realization, a knowledge file and the commands that change them: `parse(print(x))` equals `x`. `ursatz show` prints a piece or a file in text, in views that leave out what a reader does not need (summary, structure, harmony, one line, one part, reading), and `ursatz apply` takes text commands, so a change to a piece is one line:

```
$ ursatz show item:pce-2c8d54834a9c499f --view summary
ursatz-text 1 view summary fill
fill  form binary  home C major  4/4  bars 1-37
  A  bars 1-8  letter A  harmony I viio6 | I6 | I viio6 | I6 | I | IV | V | I
  B  bars 9-16  letter B  harmony I | - | I | - | I | IV | ii V | I
  material  bars 17-37  role material
# leaves out: notes, constraints, ids, spans (--view full)
# expand: ursatz show <address>/<section> --view structure
$ echo "section.insert item:pce-2c8d54834a9c499f@v2 after A id T bars 4 role body letter T" | ursatz apply
```

The grammar is in [library/text/README.md](library/text/README.md), every command with its params in [workshop/docs/command-reference.md](workshop/docs/command-reference.md), and `show`, `apply` and `restore` in [the data folder page](cli/docs/data-folder.md).

## Build and test

From the repository root:

```
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure -LE slow
```

- `-LE slow` runs the fast set (what CI runs). `-L slow` runs the slow set; the nightly workflow runs every test.
- CI (`.github/workflows/ci.yml`) installs `python3-jsonschema`, configures with `-DURSATZ_REQUIRE="jsonschema;node"` (so no schema or frontend test is silently missing), builds, runs the boundary checks and the fast set (`accuracy_harness` runs in it and writes the report CI uploads). The nightly workflow also installs `valgrind`, requires all three tools, runs every test, and fails when no `valgrind` or `schema` test was registered.
- `ctest --test-dir build/<part>` runs one part's tests. Each part builds only from the root.
- Needs cmake 3.28+, and a C11 compiler.
- The build type defaults to Debug. Release builds work for tests too (the guard `test_asserts_on`), but the default stays Debug.

Tests are registered with `ursatz_test()` (`cmake/UrsatzTest.cmake`; `ursatz_test_executable()` and `ursatz_test_library()` build shared helpers). It compiles every test executable with `-UNDEBUG`, so asserts hold in any build type, defines `URSATZ_FIXTURES_DIR` (the root `tests/fixtures`), and sets the timeout (120 s, or 1800 s for a `slow` test). Labels: `slow` (nightly only), `valgrind` (always with `slow`), `schema`, `accuracy`, `golden`. Some tests need valgrind, python3 with jsonschema or node (`cmake/UrsatzTools.cmake`); a test whose tool is missing is not registered, and `-DURSATZ_REQUIRE="valgrind;jsonschema;node"` makes the configure fail when a named tool is missing.

Goldens: the tests labelled `golden` compare their output with checked-in files (`analyzer/tests/snapshots`, `composer/tests/snapshots`, `tests/fixtures/forms`, `gui/backend/tests/fixtures/pieces`). One switch updates all of them: `cmake --build build --target update-goldens` (build the tree first) runs `ctest -L golden` with `URSATZ_UPDATE_GOLDENS=1`, which makes every golden test write its files, then runs `ctest -L golden` again without the switch and fails when that run does. The update run fails on purpose: a test run with `URSATZ_UPDATE_GOLDENS` set exits 2 and prints `<test>: goldens updated (URSATZ_UPDATE_GOLDENS)`, so CI can never pass in update mode. Review the changed files with `git diff` and commit them in their own commit. The chain digests (inline in `test_analysis_chain.c`, printed by `URSATZ_CHAIN_DUMP=1`) and the accuracy baseline (`accuracy_harness --update-baseline`) are not goldens in this sense.

Options (both ON by default):

- `URSATZ_TESTS`: build and register the tests of every part.
- `URSATZ_GUI`: build the GUI and its HTTP server.

## Knowledge

`knowledge/` holds the music-theory knowledge as data: entries of pitch material, chord types and relations, each with its citations, in JSON checked against the schemas in `knowledge/schema/` ([README](knowledge/README.md)). `tools/ci/check-knowledge.py` validates every entry; two tests run it (`knowledge_validate` on the real entries, `knowledge_validate_fixtures` on good and bad sets under `tests/fixtures/knowledge/`), both labelled `schema`.

## Test fixtures

`tests/fixtures/` holds the reference pieces and annotations every part's tests read, once ([README](tests/fixtures/README.md)). Tests find it through `URSATZ_FIXTURES_DIR`.

## Test support

`tests/support/` holds what every part's tests share: `ursatz_test.h` (library `ursatz_test_support`: `CHECK`, `CHECKF`, `test_finish`, file reading and writing, temp paths, JSON helpers) and `test_http.h` (library `ursatz_test_http`: a plain HTTP client, linked only by the GUI backend tests). Helpers that belong to one part (the Analyzer's MIDI builders, the Composer's oracles, the GUI's `gui_check.h`) stay in that part's `tests/support` or `tests/` folder. The frontend tests share `gui/frontend/tests/support/` (`check.js`, the fixtures and the fake DOM).

## Checks

Every include follows the layer table, `cmake/layer-table.txt` (test `layer_table`, in the fast set): the module of every C file, what each module may include, and a few rules that are not include edges.

`tools/ci/` holds the boundary, docs and test-timing scripts that CI runs, each runnable from anywhere in the repository after the build: `check-docs.sh <part>`, `check-statics.sh <part>...`, `check-hearing.sh`, `check-primitives.sh`, `check-frontend-theory.sh`, `check-reach.sh [<scan build dir>]` and `slowest-tests.sh build`. `check-primitives.sh` (test `primitive_owners`, in the fast set) refuses a copy of a shared primitive outside its owner (R-622): a gcd function, `__int128`, a type named `*Rational`, `*Fraction` or `*Ratio`, a table of the twelve pitch names, a scale-step table or a table of Roman numeral names in `common/`, `library/`, `analyzer/`, `composer/` or `gui/backend/src`; the owners are `library/model/core/rational.{c,h}`, `library/model/pitch/`, `library/theory/tonality/` and `knowledge/`, and `tools/ci/primitive-allow.txt` lists what stays with a reason and the phase that removes it. `check-reach.sh` builds its own scan tree (`build-reach` by default, tests off) and lists every function in the repository's own C code that no application main (`ursatz`, `ursatz-gui-backend`) reaches, in `<scan build dir>/reach/unreached.txt`. CI runs it with `tools/ci/reach-allow.txt`: a function no application reaches fails CI unless it is listed there with a reason, and a listed one that is reached or gone fails too. Delete such a function, or list it.
