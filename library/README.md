# Ursatz

Ursatz is a general-purpose, theory-neutral computational music platform, written as a C11
static library (`libursatz`). It provides a canonical representation of musical material that
strictly separates events, structures, relationships, observations, interpretations, contexts,
and provenance. Interpretations (chord, key, cadence, form) are only ever produced under an
explicit, named Theory Framework.

- `src/`: the kernel, layers L0 through L9. L0 to L8 are implemented. L9 is partial: MIDI
  import, MusicXML export, SQLite persistence, and corpus storage.
- `frameworks/`: 17 Theory Framework extension modules (tonality, harmony, counterpoint,
  form, meter, notation, and more). See [frameworks/README.md](frameworks/README.md).
- `chain/`: the analysis chain `ursatz_chain`. See [chain/README.md](chain/README.md).

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure -LE slow
bash scripts/check-docs.sh
```

Requirements, the full test run and CI are in [docs/BUILDING.md](docs/BUILDING.md).

## Using the library

Link `ursatz` (and the frameworks and `ursatz_chain` you need) from a submodule build. A consumer sees only the
public headers: kernel headers by their module path (`time/meter_map.h`), framework headers by their folder
name (`standard-form/analysis/analysis_phrase.h`), the chain as `ursatz_chain/analysis_chain.h`. Details in
[docs/public-surface.md](docs/public-surface.md).

## Docs

- [docs/README.md](docs/README.md): every doc by topic.
- [docs/architecture.md](docs/architecture.md): what the library is for, its module map, public API and data flow.
