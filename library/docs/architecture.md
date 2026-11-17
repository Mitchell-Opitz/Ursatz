# Architecture

[Docs index](README.md): the Ursatz docs by topic.

Ursatz is organized into five groups under `library/`: `model/` (value types), `theory/` (rule libraries and vocabularies), `analysis/` (analyses and framework adapters), `chain/` (the analysis chain) and `io/` (import and export), each one CMake target, with one folder per area inside and its tests under `<group>/tests/<area>/`. This page explains the library; the groups and their rules are in [layers-and-dependency-rules.md](layers-and-dependency-rules.md). Per-module detail (what each module provides, what is deferred, what to know before building on it) lives under `docs/modules/`.

## Purpose

Ursatz is the base library of the platform: a theory-neutral C11 representation of musical material (events, structures, relationships, interpretations, contexts, provenance) with exact time and pitch.
Theory lives in 11 Theory Frameworks that produce interpretations under a named framework, and the analysis chain runs them over imported notes.
The library imports MIDI and MusicXML, analyzes and exports MusicXML.
It owns every analysis algorithm (R-217); the apps orchestrate and render.

## Where it sits

Ursatz depends on no other part of the repository. The Analyzer ([architecture](../../analyzer/docs/architecture.md)), the Composer ([architecture](../../composer/docs/architecture.md)) and the GUI ([architecture](../../gui/docs/architecture.md)) build on it, in the same tree. Details are in [Who calls it](#who-calls-it).

## Module map

| Directory | What it holds | Doc |
|---|---|---|
| `model/<area>/` | The value types, target `ursatz_model` | [Groups](layers-and-dependency-rules.md#groups) and [module-status.md](module-status.md) |
| `theory/<area>/` | Rule libraries, vocabularies and style models, target `ursatz_theory` | [theory/README.md](../theory/README.md) |
| `analysis/<area>/` | Analyses and the 11 frameworks' adapters, target `ursatz_analysis` | [analysis/README.md](../analysis/README.md) |
| `chain/` | The analysis chain, target `ursatz_chain` | [chain/README.md](../chain/README.md) |
| `io/` | MIDI and MusicXML import and export, target `ursatz_io` | [io](modules/io.md) |
| `<group>/tests/`, `tests/` | Tests beside the code they test; the accuracy harness, integration tests and test support in `tests/` | [testing](testing/accuracy.md) |
| `docs/modules/` | Per-module docs | [README](README.md) |
| `cmake/`, `scripts/` | Public header staging, docs check | [Public surface](public-surface.md) |

## Public API

The headers the apps include are the staged public headers (`build/library/public/manifest.txt`). A consumer sees headers by their path from `library/`: `model/<area>/`, `theory/<area>/`, `analysis/<area>/`, `io/` or `chain/`; each group stages only what something outside it includes ([Public surface](public-surface.md)).

| Directory | Header | Route or entry | What it does | Doc |
|---|---|---|---|---|
| model/identity | `entity_id.h`, `schema_version.h` | `EntityID` | Identity of every entity. | [identity](modules/identity.md) |
| model/core | `entity.h`, `framework_reference.h` and 7 more | `Value`, `Rational`, references | Typed values, exact rationals, provenance and voice references. | [core](modules/core.md) |
| model/time | `duration.h`, `meter_arithmetic.h` and 12 more | `MeterMap`, `TempoMap`, `MusicalTime` | Exact musical time, meter and tempo. | [time](modules/time.md) |
| model/pitch | `key_signature.h`, `midi_pitch.h` and 5 more | `PitchSpelling`, `pitch_arithmetic` | Pitch identity, spelling, MIDI numbers, key signatures. | [pitch](modules/pitch.md) |
| model/events | `dynamics.h`, `note_event.h` and 1 more | `NoteEvent` | The canonical note. | [events](modules/events.md) |
| model/containers | `section.h` | `Section` | The section container, grouping phrases. | [containers](modules/containers.md) |
| model/structure | `cadence.h`, `motif.h` and 3 more | `PhraseOccurrence`, `Structure` | Occurrences of structures. | [structure](modules/structure.md) |
| model/relationship | `relationship.h` | `Relationship` | Typed link between two entities. | [relationship](modules/relationship.md) |
| model/interpretation | `chord.h`, `claim_interpretation.h` and 4 more | `Interpretation`, `Chord`, `Uncertainty` | Claims under a named framework, with uncertainty. | [interpretation](modules/interpretation.md) |
| theory/framework | `framework.h`, `framework_singleton.h` | `Framework`, `framework_singleton_get` | The Theory Framework type and its singleton helper. | [theory](modules/theory.md) |
| theory/style | `style_features.h`, `style_model.h` and 2 more | `StyleModel` | Style models built from corpus statistics. | [theory](modules/theory.md) |
| theory/rules | `context.h`, `preference.h` | `Rule`, `Constraint`, `Preference`, `Context` | The rule objects and the opaque Context. | [rules](modules/rules.md), [context](modules/context.md) |
| theory/knowledge | `knowledge_snapshot.h`, `pitch_material.h` (unstaged until an application includes them) | `KnowledgeSnapshot`, `PitchMaterial` | The entries of `knowledge/` embedded in the build, as an immutable snapshot, and pitch material read from them. | [README](../theory/README.md#knowledge), [knowledge](../../knowledge/README.md) |
| theory/vocabulary | `applied_chord_kind.h`, `beat_strength.h` and 6 more | `CadenceType`, `VoicePart`, vocabulary enums | Shared theory names, plain enums with no framework claim. | [vocabulary enums](vocabulary-enums.md), [semantics](modules/semantics.md) |
| theory/tonality | `chord_vocabulary.h`, `chord_vocabulary_seventh.h` and 10 more | framework entry and rules | Roman numeral grammar, tonal keys, triad and seventh qualities, chord and key vocabulary. | [README](../theory/tonality/README.md), [chords](modules/standard-tonality-chords.md), [numerals and keys](modules/standard-tonality-numerals-and-keys.md) |
| theory/harmony | `harmonic_function.h`, `harmony_applied.h` and 20 more | framework entry and rules | Harmonic function, cadence, six-four and prolongation rules. | [README](../theory/harmony/README.md), [rules](modules/standard-harmony-rules.md), [prolongation and keys](modules/standard-harmony-prolongation-and-keys.md) |
| theory/voicing | `doubling_reading.h`, `framework.h` and 6 more | framework entry and rules | Range, spacing and voicing rules for one chord. | [README](../theory/voicing/README.md) |
| theory/counterpoint | `cantus_firmus_rules.h`, `consonance_rules.h` and 29 more | framework entry and rules | Species counterpoint rules. | [README](../theory/counterpoint/README.md), [species](modules/standard-counterpoint-species.md) |
| theory/form | `beginning_rules.h`, `combination_rules.h` and 20 more | framework entry and rules | Phrase, period, sentence and hybrid rules, expansion and length rules. | [README](../theory/form/README.md), [expansion](modules/standard-form-expansion.md) |
| theory/meter | `beat_grid.h`, `borrowed_rules.h` and 8 more | framework entry and rules | Meter, beat grid, syncopation, hemiola and hypermeter rules. | [README](../theory/meter/README.md), [metric position](modules/standard-meter-metric-position.md) |
| theory/galant | `galant_degree.h`, `galant_exemplar.h` and 11 more | framework entry and rules | Galant schema prototypes, rule of the octave and sequences. | [README](../theory/galant/README.md), [sequences](../theory/galant/sequences.md) |
| theory/markings | `dynamics_rules.h`, `framework.h` and 1 more | framework entry and rules | Dynamics and tempo rules. | [README](../theory/markings/README.md) |
| analysis/notes | `analysis_key.h`, `analysis_note.h` and 9 more | `AnalysisNote`, `CadenceType`, `sonority_segmentation` | The neutral analysis inputs and the segmentation. | [semantics](modules/semantics.md) |
| analysis/motive | `motif_window_matching.h`, `motive_link.h` and 4 more | `motive_sketch`, `motif_window_matching` | Motive sketches, relations, links and variant recognition. | [analysis](modules/analysis.md), [motive link](modules/motive-link.md) |
| analysis/voices | `outer_voice.h`, `voice_separation.h` | `outer_voice_extract`, `voice_separate` | Outer voices and voice separation. | [analysis voices](modules/analysis-voices.md) |
| analysis/harmony | `analysis_cadence.h`, `analysis_cadence_claims.h` and 18 more | adapters and perception | Chord segments to functions and cadences, weighted chord identification, key regions, non-chord tones, Roman numeral adapter. | [README](../analysis/harmony/README.md), [analysis detail](modules/common-practice-minimal-analysis.md), [harmony analysis](modules/standard-harmony-analysis.md), [tonality analysis](modules/standard-tonality-analysis.md) |
| analysis/form | `analysis_expansion.h`, `analysis_ideas.h` and 12 more | adapters and detection | Phrase, idea, repeat, motive and theme analysis; form structure and thematic returns. | [form analysis](modules/form-analysis.md) |
| analysis/meter | `meter_analysis_check.h`, `meter_analysis_density.h` and 5 more | adapters | Meter, syncopation, hemiola, hypermeter and density analysis. | [rhythm and meter analysis](modules/rhythm-meter-analysis.md) |
| analysis/texture | `framework.h`, `texture_analysis_classify.h` and 4 more | framework entry and adapters | Texture rules and classification. | [README](../analysis/texture/README.md), [texture analysis](modules/texture-analysis.md) |
| analysis/counterpoint | `analysis_grid.h`, `analysis_melody.h` and 5 more | adapters | Melody and motion analysis over species rules. | [melody analysis](modules/melody-analysis.md), [voice leading](modules/voice-leading-analysis.md) |
| analysis/galant | `galant_analysis_input.h`, `galant_analysis_match.h` and 1 more | adapters | Schema matching and sequence detection. | [schema and sequence analysis](modules/schema-sequence-analysis.md) |
| analysis/similarity | `similarity_model.h`, `similarity_scoring.h` | `similarity_model.h` | Similarity scoring over sequences. | [query](modules/query.md) |
| io | `midi_importer.h`, `midi_key_signature.h` and 5 more | `midi_importer_import_complete`, `MusicXMLImporter`, `musicxml_export_write_score` | Import of MIDI and MusicXML into notes, meter, tempo and keys; MusicXML and MIDI export. | [io](modules/io.md), [MIDI](modules/io-midi.md), [MusicXML](modules/io-musicxml.md), [export](modules/export.md) |
| chain | `analysis_chain.h`, `chain_inputs.h` | `analysis_chain_run` | The production chain from notes to every analysis. | [job owners](analytic-job-owners.md), [chain README](../chain/README.md) |

## Version constants and schemas

| Name | File | Current value | Doc |
|---|---|---|---|
| Framework ids (`<name>-v1`, one per framework) | `theory/<area>/` `*framework*.c` (the singleton init, one per former framework) | `-v1` for all 11 | [theory/README.md](../theory/README.md) |
| `SchemaVersion` (a value type for callers' schemas) | `model/identity/schema_version.h` | caller-supplied | [identity](modules/identity.md) |

The library defines no file format version. The versions of the document formats the apps write are in the apps' own docs.

## What it calls below

Nothing. Ursatz sits at the bottom of the stack; its only dependencies are POSIX threads (see [BUILDING.md](BUILDING.md)) and the ezxml parser used by `io`, vendored in the repository's `third_party/ezxml/`.

### Who calls it

| App | Headers used | Mostly from |
|---|---|---|
| Analyzer | 148 | analysis (harmony, form, counterpoint, meter), theory (form, galant) and model (time, pitch) |
| Composer | 114 | theory (counterpoint, harmony, form), model (time, pitch), analysis (harmony), theory vocabulary, io |
| GUI | 0 | none: the GUI reads the library through the Analyzer |

## Data flow

One file through the library:

1. A MIDI or MusicXML buffer enters `io/` (`midi_importer_import_complete` or `MusicXMLImporter`), which parses it once ([io-midi](modules/io-midi.md), [io-musicxml](modules/io-musicxml.md)).
2. The import holds kernel types: `NoteEvent`s with exact `MusicalTime` and `Duration`, a `MeterMap`, a `TempoMap` and key signature events (`events`, `time`, `pitch`).
3. The caller turns the import into neutral `AnalysisNote`s (`analysis/notes/analysis_note.h`); the library's tests use `tests/import_notes_test_util.h`.
4. `analysis_chain_run` (`chain/analysis_chain.h`) takes the notes and runs the stages in order: segments, key regions, weighted chords, Roman numerals, outer voices, cadences, phrase form and motives, meter, texture, voice leading, melody, schemas and sequences ([analytic-job-owners](analytic-job-owners.md)).
5. Each stage builds its input with `chain_inputs.h` and calls the owner of its job: an `analysis/<area>/` adapter or analysis.
6. Results are `Interpretation`s and framework result structs, owned by the `AnalysisChain` and read through its `analysis_chain_*` accessors.
7. The caller reads the results, or exports the notes with `export`.

## Finding things

Start at [docs/README.md](README.md), which lists every doc by topic.

- [Layers and dependency rules](layers-and-dependency-rules.md), with the open and resolved dependency issues.
- [Public surface](public-surface.md): which headers each target exposes.
- [Placement rule](placement-rule.md): kernel, framework, chain or app.
- [Vocabulary enums](vocabulary-enums.md): the shared theory names in `theory/vocabulary/`.
- [Shared primitives](shared-primitives.md): small public helpers every layer reuses.
- [Analytic job owners](analytic-job-owners.md): one owner per analytic job and the production chain.
- [Theory owners](theory-owners.md): the library owner of each piece of theory the apps copied.
- [Module status](module-status.md): status and doc of each module.
- [Build and test](BUILDING.md).
