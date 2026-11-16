# Ursatz

Ursatz is a general-purpose, theory-neutral computational music platform, written as a C11
static library (`libursatz`). It provides a canonical representation of musical material that
strictly separates events, structures, relationships, interpretations, contexts,
and provenance. Interpretations (chord, key, cadence, form) are only ever produced under an
explicit, named Theory Framework.

- `src/`: the kernel, layers L0 through L9. L0 to L8 are implemented. L9 is partial: MIDI
  import, MusicXML export and similarity scoring.
- `frameworks/`: 11 Theory Framework extension modules (tonality, harmony, counterpoint,
  form, meter, texture, and more). See [frameworks/README.md](frameworks/README.md).
- `chain/`: the analysis chain `ursatz_chain`. See [chain/README.md](chain/README.md).

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
- A library test is registered with `ursatz_test(<name> LIBS ursatz ...)` (`cmake/UrsatzTest.cmake` at the repository root): it builds `test_<name>` from `test_<name>.c`, compiles it with `-UNDEBUG`, and sets the timeout (120 s, 1800 s for `slow`). `VALGRIND` adds `<name>_valgrind`.
- Labels: `slow` (nightly only), `valgrind` (every memcheck test, always with `slow`), `accuracy` (the accuracy harness, gate and corpus tests).
- The slow set, with every memcheck test, runs nightly in CI (`.github/workflows/nightly.yml`).

Requirements, the full test run and CI are in [docs/BUILDING.md](docs/BUILDING.md).

## Using the library

Link `ursatz` (and the frameworks and `ursatz_chain` you need) from the repository's build. A consumer sees only the
public headers: kernel headers by their module path (`time/meter_map.h`), framework headers by their folder
name (`standard-form/analysis/analysis_phrase.h`), the chain as `ursatz_chain/analysis_chain.h`. Details in
[docs/public-surface.md](docs/public-surface.md).

## Docs

- [docs/README.md](docs/README.md): every doc by topic.
- [docs/architecture.md](docs/architecture.md): what the library is for, its module map, public API and data flow.
