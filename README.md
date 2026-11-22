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
| `workshop/` | The workshop: the one process that holds the data folder (projects, items and their versions, the Library, styles, links, settings and listening ratings), its transactions, the job queue and the events, and answers every query and command | [docs](workshop/docs/README.md) |
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
- CI (`.github/workflows/ci.yml`) installs `python3-jsonschema`, configures with `-DURSATZ_REQUIRE="jsonschema;node"` (so no schema or frontend test is silently missing), builds, runs the reach scan and the fast set (the docs and boundary checks are tests in it, and `accuracy_harness` runs in it and writes the report CI uploads). The nightly workflow also installs `valgrind`, requires all three tools, runs every test, and fails when no `valgrind` or `schema` test was registered.
- `ctest --test-dir build/<part>` runs one part's tests. Each part builds only from the root.
- Needs cmake 3.28+, and a C11 compiler.
- The build type defaults to Debug. Release builds work for tests too (the guard `test_asserts_on`), but the default stays Debug.

Tests, goldens, fixtures, test support and the checks are in [tests/README.md](tests/README.md).

Options (both ON by default):

- `URSATZ_TESTS`: build and register the tests of every part.
- `URSATZ_GUI`: build the GUI and its HTTP server.

## Knowledge

`knowledge/` holds the music-theory knowledge as data: entries of pitch material, chord types, relations, instruments and vocabulary, each with its citations, in JSON checked against the schemas in `knowledge/schema/` ([README](knowledge/README.md)). `tools/ci/check-knowledge.py` validates every entry; two tests run it (`knowledge_validate` on the real entries, `knowledge_validate_fixtures` on good and bad sets under `tests/fixtures/knowledge/`), both labelled `schema`.
