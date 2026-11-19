# Ursatz

Ursatz is a general-purpose, theory-neutral computational music platform, written as a C11
static library (`libursatz`). It provides a canonical representation of musical material that
strictly separates events, structures, relationships, interpretations, contexts,
and provenance. Interpretations (chord, key, cadence, form) are only ever produced under an
explicit, named Theory Framework.

- `model/`: the value types (identity, core, time, pitch, events, containers, structure, relationship,
  interpretation), target `ursatz_model`.
- `theory/`: the rule libraries, vocabularies and style models of the 11 Theory Frameworks (tonality, harmony,
  counterpoint, form, meter, and more), target `ursatz_theory`. See [theory/README.md](theory/README.md).
- `analysis/`: the analyses and the frameworks' adapters, target `ursatz_analysis`. See
  [analysis/README.md](analysis/README.md).
- `chain/`: the analysis chain, target `ursatz_chain`: the run on an imported score, its collected result, the voice faults, the style distance of a heard piece and the library's producer key (P61; both apps hear through it). See [chain/README.md](chain/README.md).
- `io/`: MIDI and MusicXML import and export, target `ursatz_io`: the imported score and its import (`imported_score.h`) and the score layout the MusicXML writer prints (`score_layout.h`), both P61 moves from the Analyzer and the writer. See [docs/modules/io.md](docs/modules/io.md).

## Build and test

From the repository root ([README](../README.md#build-and-test)):

```sh
cmake -S . -B build
cmake --build build -j
bash tools/ci/check-docs.sh library
```

- Fast set (every test without the `slow` label), what pull request CI runs:
  `ctest --test-dir build/library --output-on-failure --no-tests=error -j$(nproc) -LE slow`
- Slow set: `ctest --test-dir build/library -j$(nproc) -L slow`
- Full run, both sets: `ctest --test-dir build/library -j$(nproc) --output-on-failure`
- The 10 s rule: a fast-set test that takes over 10 s is sped up or labeled `slow`. CI prints the ten slowest tests and
  warns about each fast-set test over 10 s.
- A library test is registered with `ursatz_test(<name> LIBS ursatz_theory ...)` (`cmake/UrsatzTest.cmake` at the repository root): it builds `test_<name>` from `test_<name>.c`, compiles it with `-UNDEBUG`, and sets the timeout (120 s, 1800 s for `slow`). `VALGRIND` adds `<name>_valgrind`.
- Fixtures are in the root `tests/fixtures/` (`URSATZ_FIXTURES_DIR`).
- Labels: `slow` (nightly only), `valgrind` (every memcheck test, always with `slow`), `accuracy` (the accuracy harness, gate and corpus tests).
- The slow set, with every memcheck test, runs nightly in CI (`.github/workflows/nightly.yml`).

Requirements, the full test run and CI are in [docs/BUILDING.md](docs/BUILDING.md).

## Using the library

Link the groups you need (`ursatz_model`, `ursatz_theory`, `ursatz_analysis`, `ursatz_chain`, `ursatz_io`) from the
repository's build. A consumer sees only the staged public headers, by their path from `library/`
(`model/time/meter_map.h`, `analysis/form/analysis_phrase.h`, `chain/analysis_chain.h`). Details in
[docs/public-surface.md](docs/public-surface.md).

## Docs

- [docs/README.md](docs/README.md): every doc by topic.
- [docs/architecture.md](docs/architecture.md): what the library is for, its module map, public API and data flow.
