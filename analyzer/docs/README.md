# Analyzer docs index

Every doc in the Ursatz-Analyzer repo, one line each. Back to the [repo README](../README.md).

## Start here

- [architecture.md](architecture.md): purpose, module map, public API, versions, what it calls and the data flow of one file.

## Build and test

- [building.md](building.md): build requirements, the test list, the include, file scope and pin checks, the theory taken from the library.

## Using it

- [corpus.md](corpus.md): analyzing a folder into documents and a corpus index.
- [style-profile.md](style-profile.md): style profiles, the model file and comparing styles.
- [similar-material.md](similar-material.md): searching a corpus by motive or progression.
- [motive-links.md](motive-links.md): linking motive families across pieces.
- [input.md](input.md): input formats, notation readings, meter and measure numbers.

## API reference

- [library.md](library.md): the public headers, CMake targets and pipeline stages.
- [gui-api.md](gui-api.md): the entries the GUI calls.
- [composer-api.md](composer-api.md): the entries the Composer calls.
- [analysis-result.md](analysis-result.md): the `AnalyzerResult`.

## Formats and schemas

- [analysis-document.md](analysis-document.md): the analysis document top level, metadata, meter, sources and claim index.
- [analysis-document/keys.md](analysis-document/keys.md): key, tonicization and modulation claims.
- [analysis-document/chords.md](analysis-document/chords.md): note, chord, vocabulary and non-chord-tone claims.
- [analysis-document/harmony-structure.md](analysis-document/harmony-structure.md): outer voice, cadence, prolongation, progression and harmonic rhythm claims.
- [analysis-document/form.md](analysis-document/form.md): phrase, theme and form plan claims.
- [analysis-document/motive-and-melody.md](analysis-document/motive-and-melody.md): motive and melody claims.
- [analysis-document/rhythm-and-texture.md](analysis-document/rhythm-and-texture.md): rhythm, meter and texture claims.
- [analysis-document/voice-leading.md](analysis-document/voice-leading.md): voice leading, schema, sequence, marking and counterpoint check claims.
- [analysis-document/versions.md](analysis-document/versions.md): every schema version and what changed.
- [text-export.md](text-export.md): the measure-addressed text export of a piece, for pasting into a conversation.

## How it works inside

- [analyses/harmony.md](analyses/harmony.md): how chords, numerals, outer voices, keys, cadences, functions and progressions are read.
- [analyses/form-and-motive.md](analyses/form-and-motive.md): how phrases, themes, the form plan, motives and melody are read.
- [analyses/rhythm-texture-voice-leading.md](analyses/rhythm-texture-voice-leading.md): how rhythm, texture, voice leading, schemas and sequences are read.

## Limits and changes

- [known-limits.md](known-limits.md): where the Analyzer reads a piece differently from its annotation.
