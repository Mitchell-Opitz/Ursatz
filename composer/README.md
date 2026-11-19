# Ursatz-Composer

A seeded, reproducible composing partner. Every result carries its provenance: the seed, the
inputs and the generators that produced it. A piece is a plan that can be filled top-down,
bottom-up or middle-out.

It is a C11 static library (`ursatz_composer`) plus a CLI (`ursatz-composer`), built on the library in
[`library/`](../library/README.md) and the Analyzer in [`analyzer/`](../analyzer/README.md), in the same repository.
Style-dependent generators live here; the Generator descriptor, Candidate and GenerationRecord types they use live here too.

Intents are structured (CLI flags or a JSON request), never natural language.

The Composer knows its music through a knowledge snapshot ([`knowledge/`](../knowledge/README.md)): every API entry that composes or
reads takes a call context holding one, and the keys it writes are of the pitch materials in it (the major and the natural minor).
It holds no scale table of its own ([the call context](docs/api.md#the-call-context)). The command line builds one snapshot per process.

## Build

The Composer builds from the repository root with the other parts ([README](../README.md#build-and-test)); it needs
CMake 3.28+, and a C11 compiler.

```sh
cmake -S . -B build
cmake --build build -j
```

The command is `build/composer/cli/ursatz-composer`. The library layout is in [docs/building.md](docs/building.md); the
source directories are in [docs/structure.md](docs/structure.md).

## Tests

Tests are split by the `slow` label so a pull request run stays short.

- The fast set is every test without the `slow` label. It is what a pull request runs in CI:
  `ctest --test-dir build/composer --output-on-failure --no-tests=error -j$(nproc) -LE slow`
- The slow set is the tests labeled `slow`: `ctest --test-dir build/composer -j$(nproc) -L slow`. It runs nightly in CI.
- The full run is both sets: `ctest --test-dir build/composer -j$(nproc)`. Run it before a push that changes behavior.
- A fast-set test over 10 s is sped up or labeled `slow`. After a run, `tools/ci/slowest-tests.sh build/composer` (from the repository root) prints the ten
  slowest tests and warns about any fast-set test over 10 s.
- Timeouts are set when a test is registered with `ursatz_test()`: 120 s for a test in the fast set (a hang guard: the one-tree fast set runs on a loaded 4 core CI runner, where tests of 20 s alone take over 30 s) and 1800 s for a slow one, unless the test sets its own. The default is the same for every part.

## Usage

```sh
ursatz-composer grow theme.txt --into sentence --key "D minor" --meter 4/4 --seed 7 --out theme.json --midi theme.mid
```

Every command, with an example each, is in [docs/cli.md](docs/cli.md).

## Composer API

An application asks the Composer for everything through a versioned in-process C API, `include/ursatz_composer/composer_api.h`:
JSON text requests, a `ComposerResponse` for every output, a `UrsatzJob` for progress and cancel.

See [docs/api.md](docs/api.md) for every entry, the job rules and a minimal C client.

## Documentation

All docs are indexed in [docs/README.md](docs/README.md). Start with [docs/architecture.md](docs/architecture.md), then:

- [Plan format](../library/docs/modules/plan-format.md), [fill loop](docs/fill-loop.md), [melody](docs/melody-fill.md), [accompaniment](docs/accompaniment.md), [voicing](docs/voicing.md), [counterpoint](docs/counterpoint.md)
- [Growing a theme](docs/grow.md), [pieces from forms](docs/piece.md), [forms as documents](docs/forms.md), [hearing the form](docs/form-hear.md)
- [Changelog](docs/changelog.md)
