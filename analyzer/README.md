# Ursatz-Analyzer

A small C11 command-line tool that turns a MIDI or MusicXML file into a Markdown report and a JSON analysis
document: the piece's time signature, measure count and pickup, its local keys over time (key
regions, with the home key, tonicizations, modulations and pivot chords), and the most likely chord
for each group of notes that start together, each located by measure and beat. It is a
thin consumer of the [Ursatz](https://github.com/Mitchell-Opitz/Ursatz) library and its
`common-practice-minimal` framework.

## Build

Requires CMake 3.20+, a C11 compiler, and SQLite3 (for Ursatz). Ursatz is a git submodule at
`external/ursatz`, pinned to Ursatz main at `d2b85ce`. cJSON 1.7.18 is vendored at `external/cjson`; it is added only when no
parent project already provides a `cjson` target.

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Build details, the test list and the CI checks are in [docs/building.md](docs/building.md).

## Tests

Every test is in one of two sets. The fast set is every test without the `slow` label; pull request CI runs it in parallel:

```sh
ctest --test-dir build --output-on-failure --no-tests=error -j$(nproc) -LE slow
```

The slow set is the tests labeled `slow` (the valgrind runs and the longest suites). Run it with
`ctest --test-dir build -j$(nproc) -L slow`; the full run is `ctest --test-dir build -j$(nproc)`. CI runs the slow set
nightly (and on demand) in the `Nightly` workflow, which runs the full suite.

A fast-set test that takes over 10 s is sped up or labeled `slow` (`scripts/slowest-tests.sh build` prints the ten slowest
tests and warns about fast-set tests over 10 s). Timeouts come from `cmake/TestTimeouts.cmake`, so local runs match CI:
30 s for a test without the `slow` label, 1800 s for a test with it.

## Usage

```sh
ursatz-analyzer [--json] [--markdown] [--input-format midi|musicxml] [--check-counterpoint <1-5> [--cantus upper|lower]] [--meter <n/d>] [--value-scale <n or n/d>] [--pickup <n/d>] <input-file-path>
```

Writes two files next to the input file:

- `<stem>.report.md`: the Markdown report.
- `<stem>.analysis.json`: the analysis document. See
  [docs/analysis-document.md](docs/analysis-document.md) and the
  [schema](schema/analysis-document.v9.schema.json).

`--markdown` writes only the report, `--json` writes only the analysis document. No flag, or both,
writes both. `--input-format` forces the input format (see [Input formats](docs/input.md#input-formats)). Flags can come before or
after the path. Exits 1 with a message on an unknown flag, a
missing path, bad input, allocation failure, or write failure.

`--check-counterpoint <1-5>` also checks two-voice input against that species of Ursatz's
standard-counterpoint (see `counterpoint_check` in
[docs/analysis-document/voice-leading.md](docs/analysis-document/voice-leading.md#counterpoint-check)). `--cantus upper|lower` says which voice is
the cantus firmus (default lower) and needs `--check-counterpoint`. The two voices are the input's
two streams (MIDI tracks or MusicXML parts), or the two voices the kernel finds in a single stream.
For any other input the rest of the analysis is written and the CLI exits 2 with "counterpoint
check needs two voices".

`--meter <n/d>`, `--value-scale <n or n/d>` and `--pickup <n/d>` read the notes as other notation
than the file's before they are analyzed (see [Notation readings](docs/input.md#notation-readings)). They apply to one input file,
not to corpus, style or similar-material runs.

```sh
ursatz-analyzer --corpus <dir> [--out <dir>] [--resume] [--markdown]
```

Corpus mode analyzes every MIDI and MusicXML file under a folder and writes one analysis document each, plus a
corpus index. See [docs/corpus.md](docs/corpus.md) and the [index schema](schema/corpus-index.v1.schema.json).
It cannot be combined with an input file, `--json`, `--check-counterpoint`, `--cantus`, `--input-format`, `--meter`, `--value-scale` or `--pickup`.

```sh
ursatz-analyzer --style-profile <corpus output dir> --name <style name> [--out <file>]
ursatz-analyzer --compare-styles <a.json> <b.json> [--without <category>]... [--format text|json]
```

`--style-profile` counts the harmony, rhythm, melody and form labels of a corpus run's documents and writes them as a
style model file, with four categories for motive degrees, intervals, rhythms and non-chord-tone figures;
`--compare-styles` gives the distance between two such files, leaving out the categories named by `--without`. See
[docs/style-profile.md](docs/style-profile.md) and the [model schema](schema/style-model.v1.schema.json).

```sh
ursatz-analyzer --similar <corpus output dir> (--motive <file> | --progression "<numerals>") [--min <n/d>] [--max <n>] [--format text|json]
```

`--similar` finds passages of a corpus run's documents that are similar to a motive (the soprano of a MIDI or MusicXML
file) or a progression (Roman numerals). See [docs/similar-material.md](docs/similar-material.md) and the
[output schema](schema/similar-material.v1.schema.json).

```sh
ursatz-analyzer --link-motives <corpus output dir> [--cache <file>] [--depth <1-6>] [--candidates <n>] [--format text|json]
```

`--link-motives` finds the motive families of a corpus run's documents that are variants of one another (a chain of
motive relations from one to the other), groups them, and keeps what it compared in a cache so a later run compares only
what is new. See [docs/motive-links.md](docs/motive-links.md) and the [output schema](schema/motive-links.v1.schema.json).

## Docs

All docs are indexed in [docs/README.md](docs/README.md). How the Analyzer is built and what it calls: [docs/architecture.md](docs/architecture.md).

- Build and test: [docs/building.md](docs/building.md)
- Input formats and notation readings: [docs/input.md](docs/input.md)
- Library headers and pipeline: [docs/library.md](docs/library.md)
- What each analysis reads: [harmony](docs/analyses/harmony.md), [form and motive](docs/analyses/form-and-motive.md), [rhythm, texture and voice leading](docs/analyses/rhythm-texture-voice-leading.md)
- The analysis document: [docs/analysis-document.md](docs/analysis-document.md)
- Known limits: [docs/known-limits.md](docs/known-limits.md)
