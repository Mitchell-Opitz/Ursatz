# Ursatz-Composer

A seeded, reproducible composing partner. Every result carries its provenance: the seed, the
inputs and the generators that produced it. A piece is a plan that can be filled top-down,
bottom-up or middle-out.

It is a C11 static library (`ursatz_composer`) plus a CLI (`ursatz-composer`), built on
[Ursatz](https://github.com/Mitchell-Opitz/Ursatz) and
[Ursatz-Analyzer](https://github.com/Mitchell-Opitz/Ursatz-Analyzer). Both are git submodules
under `external/`. Style-dependent generators live in this repo; Ursatz keeps only the
theory-neutral Generator interface.

Intents are structured (CLI flags or a JSON request), never natural language.

## Build

Requires CMake 3.20+, a C11 compiler, pkg-config and SQLite3 (for Ursatz).

```sh
git clone --recurse-submodules https://github.com/Mitchell-Opitz/Ursatz-Composer.git
cd Ursatz-Composer
cmake -S . -B build
cmake --build build
```

If you cloned without `--recurse-submodules`, run `git submodule update --init --recursive`. The submodule pins and the
library layout are in [docs/building.md](docs/building.md); the source directories are in [docs/structure.md](docs/structure.md).

## Tests

Tests are split by the `slow` label so a pull request run stays short.

- The fast set is every test without the `slow` label. It is what a pull request runs in CI:
  `ctest --test-dir build --output-on-failure --no-tests=error -j$(nproc) -LE slow`
- The slow set is the tests labeled `slow`: `ctest --test-dir build -j$(nproc) -L slow`. It runs nightly in CI.
- The full run is both sets: `ctest --test-dir build -j$(nproc)`. Run it before a push that changes behavior.
- A fast-set test over 10 s is sped up or labeled `slow`. After a run, `scripts/slowest-tests.sh build` prints the ten
  slowest tests and warns about any fast-set test over 10 s.
- Timeouts come from `cmake/TestTimeouts.cmake`: 30 s for a test in the fast set and 1800 s for a slow one, unless the test sets its own.

## Usage

```sh
ursatz-composer grow theme.txt --into sentence --key "D minor" --meter 4/4 --seed 7 --out theme.json --midi theme.mid
```

Every command, with an example each, is in [docs/cli.md](docs/cli.md).

## Composer API

An application asks the Composer for everything through a versioned in-process C API, `include/ursatz_composer/composer_api.h`:
JSON text requests, a `ComposerResponse` for every output, a `ComposerJob` for progress and cancel.

See [docs/api.md](docs/api.md) for every entry, the job rules and a minimal C client.

## Documentation

All docs are indexed in [docs/README.md](docs/README.md). Start with [docs/architecture.md](docs/architecture.md), then:

- [Plan format](docs/plan-format.md), [fill loop](docs/fill-loop.md), [melody](docs/melody-fill.md), [accompaniment](docs/accompaniment.md), [voicing](docs/voicing.md), [counterpoint](docs/counterpoint.md)
- [Growing a theme](docs/grow.md), [pieces from forms](docs/piece.md), [forms as documents](docs/forms.md), [hearing the form](docs/form-hear.md)
- [Changelog](docs/changelog.md)
