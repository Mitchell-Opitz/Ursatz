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
ctest --test-dir build --output-on-failure
```

If you cloned without `--recurse-submodules`, run `git submodule update --init --recursive`.

## Usage

```sh
ursatz-composer --version
ursatz-composer --help
```

The CLI prints the version and usage only. Planning, I/O, rendering and filling are not
implemented yet.
