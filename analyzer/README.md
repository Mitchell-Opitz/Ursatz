# Ursatz-Analyzer

A C11 library, run from [`ursatz analyze`](../cli/docs/analyze.md), that turns a MIDI or MusicXML file into a JSON analysis
document: the piece's time signature, measure count and pickup, its local keys over time (key
regions, with the home key, tonicizations, modulations and pivot chords), and the most likely chord
for each group of notes that start together, each located by measure and beat. It is a
thin consumer of the [Ursatz library](../library/README.md) in `library/` and its
`common-practice-minimal` framework.

## Build

The Analyzer builds from the repository root with the other parts ([README](../README.md#build-and-test)); it needs
CMake 3.28+ and a C11 compiler. cJSON 1.7.18 is vendored in `third_party/cjson`.

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build/analyzer --output-on-failure
```

Build details, the test list and the CI checks are in [docs/building.md](docs/building.md).

## Tests

Every test is in one of two sets. The fast set is every test without the `slow` label; pull request CI runs it in parallel:

```sh
ctest --test-dir build/analyzer --output-on-failure --no-tests=error -j$(nproc) -LE slow
```

The slow set is the tests labeled `slow` (the valgrind runs and the longest suites). Run it with
`ctest --test-dir build/analyzer -j$(nproc) -L slow`; the full run is `ctest --test-dir build/analyzer -j$(nproc)`. CI runs the slow set
nightly (and on demand) in the `Nightly` workflow, which runs the full suite.

A fast-set test that takes over 10 s is sped up or labeled `slow` (`tools/ci/slowest-tests.sh build/analyzer` from the repository root prints the ten slowest
tests and warns about fast-set tests over 10 s). Timeouts are set when each test is registered with `ursatz_test()`:
120 s for a test without the `slow` label, 1800 s for a test with it.

## Usage

The Analyzer's command line is part of `ursatz` ([cli docs](../cli/docs/README.md)):

- `ursatz analyze <input>` writes `<stem>.analysis.json`, the analysis document ([docs/analysis-document.md](docs/analysis-document.md), [schema](schema/analysis-document.schema.json)); `--view text` prints the measure-addressed text export instead ([docs/text-export.md](docs/text-export.md)). The notation readings (`--meter`, `--value-scale`, `--pickup`) and the input formats are in [docs/input.md](docs/input.md); the flags and exit codes are in [analyze.md](../cli/docs/analyze.md).
- `ursatz corpus <dir>` analyzes a folder into one document each and a corpus index ([docs/corpus.md](docs/corpus.md), [index schema](schema/corpus-index.v2.schema.json)); `ursatz style-profile <corpus output> --name <name>` counts a corpus into a style model ([docs/style-profile.md](docs/style-profile.md), [model schema](schema/style-model.v1.schema.json)). See [corpus.md](../cli/docs/corpus.md).
- The comparison of two style models, the similar-material search and the motive links are library functions that the workshop's API and the GUI call ([docs/similar-material.md](docs/similar-material.md), [docs/motive-links.md](docs/motive-links.md)); they have no command.

## Docs

All docs are indexed in [docs/README.md](docs/README.md). How the Analyzer is built and what it calls: [docs/architecture.md](docs/architecture.md).

- Build and test: [docs/building.md](docs/building.md)
- Input formats and notation readings: [docs/input.md](docs/input.md)
- Library headers and the source layout (`src/util`, `store`, `result`, `export`, `style`, `corpus`, `links`): [docs/library.md](docs/library.md), [docs/architecture.md](docs/architecture.md)
- What each analysis reads: [harmony](docs/analyses/harmony.md), [form and motive](docs/analyses/form-and-motive.md), [rhythm, texture and voice leading](docs/analyses/rhythm-texture-voice-leading.md)
- The analysis document: [docs/analysis-document.md](docs/analysis-document.md)
- Known limits: [docs/known-limits.md](docs/known-limits.md)
