# Architecture

[Docs index](README.md): the Ursatz docs by topic.

Ursatz is organized into layered modules, each living under `src/<module>/` with its tests mirrored under `tests/<module>/`. Theory Framework extension modules live outside the layers, under `frameworks/<name>/`. This page explains the library; the layer model and its rules are in [layers-and-dependency-rules.md](layers-and-dependency-rules.md). Per-module detail (what each module provides, what is deferred, what to know before building on it) lives under `docs/modules/`.

## Purpose

Ursatz is the base library of the platform: a theory-neutral C11 representation of musical material (events, structures, relationships, interpretations, contexts, provenance) with exact time and pitch.
Theory lives in 17 Theory Frameworks that produce interpretations under a named framework, and the analysis chain runs them over imported notes.
The library imports MIDI and MusicXML, analyzes, stores to SQLite and exports MusicXML.
It owns every analysis algorithm (R-217); the apps orchestrate and render.

## Where it sits

Ursatz depends on no other repo. The Analyzer, the Composer and the GUI pin it as the git submodule `external/ursatz` ([Ursatz-Analyzer](https://github.com/Mitchell-Opitz/Ursatz-Analyzer/blob/main/docs/architecture.md), [Ursatz-Composer](https://github.com/Mitchell-Opitz/Ursatz-Composer/blob/main/docs/architecture.md), [Ursatz-GUI](https://github.com/Mitchell-Opitz/Ursatz-GUI/blob/main/docs/architecture.md)), each at a commit on Ursatz `main` that its own `scripts/check-pins.sh` checks. Details are in [Who calls it](#who-calls-it).

## Module map

| Directory | What it holds | Doc |
|---|---|---|
| `src/<module>/` | Kernel modules L0 to L9, one folder each | [Layers](layers-and-dependency-rules.md#layers) and [module-status.md](module-status.md) |
| `frameworks/<name>/` | 17 Theory Frameworks built on L7 | [frameworks/README.md](../frameworks/README.md) |
| `chain/` | The analysis chain `ursatz_chain` | [chain/README.md](../chain/README.md) |
| `tests/` | Kernel tests mirrored by module, accuracy harness, integration tests | [testing](testing/accuracy.md) |
| `docs/modules/` | Per-module docs | [README](README.md) |
| `cmake/`, `scripts/` | Public header staging, app include report, docs check | [Public surface](public-surface.md) |
| `third_party/` | Vendored XML parser (ezxml), used only by `io` | [io-musicxml](modules/io-musicxml.md) |
| `examples/` | Usage examples | [README](../README.md) |

## Public API

The headers the apps include, from the union of `scripts/app_includes.sh` output over the three apps. A consumer sees headers by module path (kernel), folder name (framework) or `ursatz_chain/` (chain); see [Public surface](public-surface.md).

| Directory | Header | Route or entry | What it does | Doc |
|---|---|---|---|---|
| identity | `entity_id.h` | `EntityID` | Identity of every entity. | [identity](modules/identity.md) |
| core | `entity.h`, `provenance_reference.h` and 3 more | `Value`, `Rational`, references | Typed values, exact rationals, provenance and voice references. | [core](modules/core.md) |
| time | `duration.h`, `meter_arithmetic.h` and 11 more | `MeterMap`, `TempoMap`, `MusicalTime` | Exact musical time, meter and tempo. | [time](modules/time.md) |
| pitch | `key_signature.h`, `midi_pitch.h` and 5 more | `PitchSpelling`, `pitch_arithmetic` | Pitch identity, spelling, MIDI numbers, key signatures. | [pitch](modules/pitch.md) |
| events | `note_event.h` | `NoteEvent` | The canonical note. | [events](modules/events.md) |
| containers | `section.h` | `Section` | Containers for events and phrases. | [containers](modules/containers.md) |
| structure | `phrase_occurrence.h`, `structure.h` and 1 more | `PhraseOccurrence`, `Theme` | Occurrences of structures. | [structure](modules/structure.md) |
| semantics | `analysis_key.h`, `analysis_note.h` and 9 more | `CadenceType`, `sonority_segmentation`, vocabulary enums | Shared theory vocabulary and the neutral analysis inputs. | [semantics](modules/semantics.md) |
| relationship | `relationship.h` | `Relationship` | Typed link between two entities. | [relationship](modules/relationship.md) |
| interpretation | `chord.h`, `claim_interpretation.h` and 4 more | `Interpretation`, `Chord`, `Uncertainty` | Claims under a named framework, with uncertainty. | [interpretation](modules/interpretation.md) |
| theory | `framework.h`, `style_features.h` and 3 more | `Framework`, `StyleModel` | Theory Framework type and style models. | [theory](modules/theory.md) |
| analysis | `analyzer.h`, `form_structure_detection.h` and 9 more | `Analyzer`, `motive_sketch`, `outer_voice_extract`, `voice_separate` | Kernel analyses: motives, voices, progressions, form. | [analysis](modules/analysis.md), [motive link](modules/motive-link.md) |
| generation | `candidate.h`, `generation_record.h` and 1 more | `Generator`, `Candidate` | Candidate generation with a generation record. | [generation](modules/generation.md) |
| io | `midi_importer.h`, `midi_key_signature.h` and 2 more | `midi_importer_import_complete`, `MusicXMLImporter`, `midi_key_signature_scan` | Import of MIDI and MusicXML into notes, meter, tempo and keys. | [io](modules/io.md), [MIDI](modules/io-midi.md), [MusicXML](modules/io-musicxml.md) |
| export | `musicxml_export.h`, `score_quantize.h` and 1 more | `musicxml_export_write`, `score_quantize` | MusicXML export of a MeterMap and notes. | [export](modules/export.md) |
| persistence | `persistence.h` | `persistence.h` | SQLite store and load. | [persistence](modules/persistence.md) |
| corpus | `corpus.h`, `corpus_api.h` | `corpus.h`, `corpus_api.h` | Corpus storage. | [corpus](modules/corpus.md) |
| query | `similarity_model.h`, `similarity_scoring.h` | `similarity_model.h` | Similarity scoring over stored material. | [query](modules/query.md) |
| common-practice-minimal | `chord_vocabulary_seventh.h`, `framework.h` and 7 more | framework entry and rules | Weighted chord identification, key regions, non-chord tones. | [README](../frameworks/common-practice-minimal/README.md), [analysis detail](modules/common-practice-minimal-analysis.md) |
| galant-schemas | `analysis/galant_analysis_input.h`, `analysis/galant_analysis_match.h` and 4 more | framework entry and rules | Galant schema prototypes and matching. | [README](../frameworks/galant-schemas/README.md) |
| scale-harmonization | `analysis/scale_analysis_detect.h`, `scale_direction.h` and 3 more | framework entry and rules | Rule of the octave and sequence detection. | [README](../frameworks/scale-harmonization/README.md) |
| standard-counterpoint | `analysis/analysis_grid.h`, `analysis/analysis_melody.h` and 31 more | framework entry and rules | Species counterpoint rules and melody and motion analysis. | [README](../frameworks/standard-counterpoint/README.md), [species](modules/standard-counterpoint-species.md), [voice leading](modules/voice-leading-analysis.md) |
| standard-form | `analysis/analysis_expansion.h`, `analysis/analysis_ideas.h` and 18 more | framework entry and rules | Phrase, period, sentence and hybrid rules; phrase, motive and theme analysis. | [README](../frameworks/standard-form/README.md), [form analysis](modules/form-analysis.md) |
| standard-harmony | `analysis/analysis_cadence.h`, `analysis/analysis_function.h` and 23 more | framework entry and rules | Harmonic function, cadence, six-four and prolongation rules and analysis. | [README](../frameworks/standard-harmony/README.md), [rules](modules/standard-harmony-rules.md), [prolongation and keys](modules/standard-harmony-prolongation-and-keys.md), [analysis](modules/standard-harmony-analysis.md) |
| standard-markings | `dynamics_rules.h`, `framework.h` and 1 more | framework entry and rules | Dynamics and tempo rules. | [README](../frameworks/standard-markings/README.md) |
| standard-meter | `analysis/meter_analysis_check.h`, `analysis/meter_analysis_density.h` and 10 more | framework entry and rules | Meter, beat grid, syncopation, hemiola and hypermeter. | [README](../frameworks/standard-meter/README.md), [rhythm and meter analysis](modules/rhythm-meter-analysis.md) |
| standard-texture | `analysis/texture_analysis_classify.h`, `analysis/texture_analysis_input.h` and 2 more | framework entry and rules | Texture rules and classification. | [README](../frameworks/standard-texture/README.md), [texture analysis](modules/texture-analysis.md) |
| standard-tonality | `analysis/applied_chords.h`, `analysis/chord_input.h` and 3 more | framework entry and rules | Roman numeral grammar, tonal keys, chord input. | [README](../frameworks/standard-tonality/README.md), [tonality analysis](modules/standard-tonality-analysis.md) |
| standard-voicing | `doubling_reading.h`, `framework.h` and 5 more | framework entry and rules | Range, spacing and voicing rules for one chord. | [README](../frameworks/standard-voicing/README.md) |
| chain | `ursatz_chain/analysis_chain.h`, `ursatz_chain/chain_inputs.h` | `analysis_chain_run` | The production chain from notes to every analysis. | [job owners](analytic-job-owners.md), [chain README](../chain/README.md) |

## Version constants and schemas

| Name | File | Current value | Doc |
|---|---|---|---|
| `ursatz_version()` | `src/ursatz.h` | 0 | this page |
| Framework ids (`<name>-v1`, one per framework) | `frameworks/<name>/framework.c` (the singleton init) | `-v1` for all 17 | [frameworks/README.md](../frameworks/README.md) |
| SQLite schema (`PERSISTENCE_SCHEMA`, created with `IF NOT EXISTS`, no version number) | `src/persistence/persistence.c` | unversioned | [persistence](modules/persistence.md) |
| `SchemaVersion` (a value type for callers' schemas) | `src/identity/schema_version.h` | caller-supplied | [identity](modules/identity.md) |

The library defines no file format version. The versions of the document formats the apps write are in the apps' own docs.

## What it calls below

Nothing. Ursatz sits at the bottom of the stack; its only dependencies are SQLite and POSIX threads (see [BUILDING.md](BUILDING.md)) and the vendored ezxml parser used by `io`.

### Who calls it

| App | Headers used | Mostly from |
|---|---|---|
| Ursatz-Analyzer | 157 | frameworks (standard-form, standard-counterpoint, standard-harmony, standard-meter) and the kernel's semantics, time and pitch |
| Ursatz-Composer | 115 | standard-counterpoint, standard-harmony, standard-form, time, pitch, semantics, generation, io |
| Ursatz-GUI | 4 | analysis (`analyzer.h`), corpus, persistence |

Made with `scripts/app_includes.sh <app checkout>...`; the combined list is in `docs/public-headers-baseline.txt`.

## Data flow

One file through the library:

1. A MIDI or MusicXML buffer enters `io` (`midi_importer_import_complete` or `MusicXMLImporter`), which parses it once ([io-midi](modules/io-midi.md), [io-musicxml](modules/io-musicxml.md)).
2. The import holds kernel types: `NoteEvent`s with exact `MusicalTime` and `Duration`, a `MeterMap`, a `TempoMap` and key signature events (`events`, `time`, `pitch`).
3. `io/import_notes.h` turns the import into neutral `AnalysisNote`s (`semantics/analysis_note.h`).
4. `analysis_chain_run` (`ursatz_chain/analysis_chain.h`) takes the notes and runs the stages in order: segments, key regions, weighted chords, Roman numerals, outer voices, cadences, phrase form and motives, meter, texture, voice leading, melody, schemas and sequences ([analytic-job-owners](analytic-job-owners.md)).
5. Each stage builds its input with `chain_inputs.h` and calls the owner of its job: a framework (`frameworks/<name>/analysis/`) or the kernel's `analysis` module.
6. Results are `Interpretation`s and framework result structs, owned by the `AnalysisChain` and read through its `analysis_chain_*` accessors.
7. The caller reads the results, stores them with `persistence` or exports the notes with `export`.

## Finding things

Start at [docs/README.md](README.md), which lists every doc by topic.

- [Layers and dependency rules](layers-and-dependency-rules.md), with the open and resolved dependency issues.
- [Public surface](public-surface.md): which headers each target exposes.
- [Placement rule](placement-rule.md): kernel, framework, chain or app.
- [Vocabulary enums](vocabulary-enums.md): the shared theory names in `src/semantics/`.
- [Shared primitives](shared-primitives.md): small public helpers every layer reuses.
- [Analytic job owners](analytic-job-owners.md): one owner per analytic job and the production chain.
- [Theory owners](theory-owners.md): the library owner of each piece of theory the apps copied.
- [Module status](module-status.md): status and doc of each module.
- [Build and test](BUILDING.md).
