# Ursatz GUI

A local web UI for the Ursatz platform. You upload a MIDI or MusicXML file,
the Analyzer library analyzes it, and the GUI shows the analysis document
section by section next to the score. Pieces can be grouped into
Collections. This repo is a host/shell around independent tool modules, not
a monolithic application.

## Views

The Library, Tags panel, Collections, Subjects, Motifs, Themes, Pieces, Motive links and Analysis views are described in [docs/views/](docs/views/library.md):

- [Library and navigation](docs/views/library.md): upload, the table, filters, the Tags panel.
- [Motifs](docs/views/motifs.md): the Materials views and the Motif editor.
- [Themes and pieces](docs/views/themes-and-pieces.md): the Themes view, the Pieces view, item references.
- [Analysis and score](docs/views/analysis-and-score.md): Subjects, Motive links, the Analysis view, the Score and the Inspector.
- [Playback](docs/views/playback.md): playing pieces and motifs.

## Documentation

[docs/README.md](docs/README.md) lists every doc, starting with [docs/architecture.md](docs/architecture.md).

## Test

The tests are split into a fast set and a slow set. The `slow` label is new: a test carries it when it takes over 10 s.

- Fast set (every test without the `slow` label), what a pull request runs:

  ```
  ctest --test-dir build/gui --output-on-failure --no-tests=error -j$(nproc) -LE slow
  ```

- Slow set (`analyzer_route`, `piece_route`, `styles_route`, `theme_route`):

  ```
  ctest --test-dir build/gui -j$(nproc) -L slow
  ```

- Full run, both sets:

  ```
  ctest --test-dir build/gui -j$(nproc)
  ```

A fast-set test that takes over 10 s is sped up or labeled `slow`. `tools/ci/slowest-tests.sh build/gui` (from the repository root) prints the ten slowest tests of the last run and warns about any fast-set test over 10 s.

CMake sets the timeouts (`cmake/TestTimeouts.cmake`): 30 s for a test without the `slow` label, 1800 s for a slow one. The `Nightly` workflow runs the full suite, so the slow set runs nightly in CI.

The tests and checks are in [docs/testing.md](docs/testing.md).

## Building

The GUI builds from the repository root with the other parts ([README](../README.md#build-and-test)):

```
cmake -S . -B build
cmake --build build -j
ctest --test-dir build/gui
```

Requires CMake 3.28 or newer, a C11 compiler, SQLite development headers and pkg-config. The GUI links the targets
the root adds before it: cJSON, civetweb (both in `third_party/`), the library, the Analyzer and the Composer library
(without the command; see [Composer routes](docs/api/composer-routes.md#composer-routes)). `-DURSATZ_GUI=OFF` skips the
GUI; `-DURSATZ_TESTS=OFF` skips every part's tests. The library's own tests are built with the rest.

Boundary checks keep the GUI a client; CI runs them (the build is configured with `URSATZ_LEGACY_INCLUDE_DIRS` OFF, so
only staged public headers are visible). Run them from anywhere in the repository after the build:

- `tools/ci/check-library-includes-gui.sh build` fails when `backend/src`
  includes an Ursatz header, an Analyzer header outside `ursatz_analyzer/`, a
  Composer header other than `ursatz_composer/composer_api.h`, or any `*_internal.h`.
- `tools/ci/check-docs.sh gui` fails when a README is over 10 KB, another doc over 20 KB, a relative link or anchor does not resolve, or a file under `docs/` is missing from `docs/README.md`.
- `tools/ci/check-frontend-theory.sh` fails when a file under `frontend/public`
  (not `vendor/`) defines `playbackFromMidi`, `randomSettings`, `defaultCadence`,
  `parseHarmonyToken`, `HARMONY_TOKEN`, `KEY_TONICS`, `RANDOM_METERS`,
  `chordProblems` or `numeralsOk`.

Done-when for the client boundary, checked by hand: analyze a reference
piece (same claims and score as before the phase); make the "Knight's house"
theme with the same seeded plan; during a compose `ps` shows no
`ursatz-composer` process; opening a piece imports it once (the import counter);
all checks above pass.

## Out of scope

Native webview packaging, authentication, claim
editing, and accepting or disputing a claim (no promotion path exists). The
Composer has no blend, corpus seeding or explain.
