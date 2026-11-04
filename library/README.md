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

See [docs/BUILDING.md](docs/BUILDING.md) for build and test instructions, and
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the layer model and current module status.
Per-module detail lives under `docs/modules/`.

## Using the library

Link `ursatz` (and the frameworks and `ursatz_chain` you need) from a submodule build. A consumer sees only the
public headers: kernel headers by their module path (`time/meter_map.h`), framework headers by their folder
name (`standard-form/analysis/analysis_phrase.h`), the chain as `ursatz_chain/analysis_chain.h`. Details in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), Public surface.
