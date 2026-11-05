# Architecture

Ursatz is organized into layered modules, each living under `src/<module>/` with its tests mirrored under `tests/<module>/`. Theory Framework extension modules live outside the layers, under `frameworks/<name>/`. This page explains the library, then covers the layer model and the rules that hold it together. Per-module detail (what each module provides, what is deferred, what to know before building on it) lives under `docs/modules/`.

## Purpose

Ursatz is the base library of the platform: a theory-neutral C11 representation of musical material (events, structures, relationships, interpretations, contexts, provenance) with exact time and pitch.
Theory lives in 17 Theory Frameworks that produce interpretations under a named framework, and the analysis chain runs them over imported notes.
The library imports MIDI and MusicXML, analyzes, stores to SQLite and exports MusicXML.
It owns every analysis algorithm (R-217); the apps orchestrate and render.

## Where it sits

Ursatz depends on no other repo. The Analyzer, the Composer and the GUI pin it as the git submodule `external/ursatz` ([Ursatz-Analyzer](https://github.com/Mitchell-Opitz/Ursatz-Analyzer), [Ursatz-Composer](https://github.com/Mitchell-Opitz/Ursatz-Composer), [Ursatz-GUI](https://github.com/Mitchell-Opitz/Ursatz-GUI)), each at a commit on Ursatz `main` that its own `scripts/check-pins.sh` checks. Details are in [Who calls it](#who-calls-it).

## Module map

| Directory | What it holds | Doc |
|---|---|---|
| `src/<module>/` | Kernel modules L0 to L9, one folder each | [Layers](#layers) and [module-status.md](module-status.md) |
| `frameworks/<name>/` | 17 Theory Frameworks built on L7 | [frameworks/README.md](../frameworks/README.md) |
| `chain/` | The analysis chain `ursatz_chain` | [chain/README.md](../chain/README.md) |
| `tests/` | Kernel tests mirrored by module, accuracy harness, integration tests | [testing](testing/accuracy.md) |
| `docs/modules/` | Per-module docs | [README](README.md) |
| `cmake/`, `scripts/` | Public header staging, app include report, docs check | [Public surface](#public-surface) |
| `third_party/` | Vendored XML parser (ezxml), used only by `io` | [io-musicxml](modules/io-musicxml.md) |
| `examples/` | Usage examples | [README](../README.md) |

## Public API

The headers the apps include, from the union of `scripts/app_includes.sh` output over the three apps. A consumer sees headers by module path (kernel), folder name (framework) or `ursatz_chain/` (chain); see [Public surface](#public-surface).

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

## Layers

| Layer | Modules |
|---|---|
| L0 | `identity` |
| L1 | `core` |
| L2 | `time`, `pitch` (siblings, do not depend on each other) |
| L3 | `events` |
| L4 | `containers` |
| L5 | `structure` |
| L6 | `context`, `semantics`, `relationship`, `interpretation` (four mutually-restricted siblings) |
| L7 | `theory`, `rules` (`rules` depends on `theory`; `theory` never depends back) |
| L8 | `analysis`, `generation` (two mutually-restricted siblings) |
| L9 | `io` (MIDI import, MIDI key-signature scan), `export` (MusicXML), `persistence` (SQLite), `corpus`, `corpus_stats`, `query` |
| Extensions | `frameworks/<name>/`: 17 Theory Frameworks built on L7 (see `frameworks/README.md`); `chain/`: the analysis chain (`ursatz_chain`), above the kernel and the frameworks |

## Dependency rule

A module in layer N may depend only on modules in layers 0 through N. It must never depend sideways within its own layer unless that specific dependency is documented, and never depend upward on a higher layer. The documented sideways edges are `rules` on `theory` (L7), `interpretation` on `relationship` through `claim_interpretation.h` (L6), and in L9 `export`, `corpus`, `corpus_stats` and `query` on `persistence` plus `corpus_stats` and `query` on `corpus`. No `src/` module includes a framework header. Frameworks never compile kernel sources (`architecture_framework_sources` checks every `frameworks/*/CMakeLists.txt` and `chain/CMakeLists.txt`); code that needs both a framework and the kernel lives in `chain/`. The former exception, `analysis`'s `ChordIdentification` and `KeyEstimation`, moved into the chain (R-010 finished, R-241).

The analysis chain (`chain/`, target `ursatz_chain`) is an extension above the numbered stack. It may include kernel L0 to L8 and the framework headers named in `chain/tests/test_architecture_chain.c`, and never L9 (`io`, `export`, `persistence`, `corpus`, `corpus_stats`, `query`). Headers are included as `ursatz_chain/<name>.h`. A framework test may link the chain; a framework library may not. The chain keeps the stage inputs it builds (pitches, harmony inputs, prepared harmony, form and motive inputs, voice grid, galant segments) and exposes them through `analysis_chain_*` accessors, and it reads the outer lines on the chord segments (`chain_build_segment_lines`, `analysis_chain_segment_outer_voices`) alongside the lines on the voice segments (`analysis_chain_outer_voices`).

A framework may depend on L0 to L6, on `theory/framework.h`, `theory/framework_singleton.h`, `rules/rule.h`, `rules/constraint.h`, `rules/preference.h` and `rules/rule_pair.h`, and on another framework only where that edge is recorded in `frameworks/README.md`.

Every module and every framework has an architecture test that allowlists the directories it may include (for example `tests/events/test_architecture_events.c`). A disallowed include fails that test. Framework architecture tests run from the root build like the module ones (since PR #99); see `docs/BUILDING.md`. `tests/test_architecture_tests.c` applies the same layer rules to the test sources (`tests/<module>/` and `frameworks/<name>/tests/`); the test-only edges it allows are listed in its `ALLOWED_TEST_EDGES`, one reason each. Two of them reach kernel L8 from a framework test: `common-practice-minimal` tests include `analysis/outer_voice.h` and `standard-harmony` tests include `analysis/period_detection.h`; framework sources never include L8.

## Shared primitives

Small public helpers that every layer above reuses are listed in [shared-primitives.md](shared-primitives.md).

## Vocabulary enums

`src/semantics/` holds plain enums that name theory concepts frameworks share (`CadenceType`, `VoicePart`, `NonChordTone`, and others). Each carries no `EntityID`, no data, and no framework claim, only a name and small name-level predicates. What a name means, and which music it applies to, is always the framework's convention. New enums of this kind are admitted to the kernel only when at least two frameworks need them; a name only one framework uses belongs in that framework. Existing single-framework enums stay until their framework is next changed, then move into it.

## Analytic job owners

One owner per analytic job (R-224). Every other implementation of the job is retired or a thin wrapper, and its header names the owner. The owner table and the production chain are in [analytic-job-owners.md](analytic-job-owners.md).

## Public surface

A library target exposes only the headers it names (R-223). `ursatz_public_headers` (`cmake/UrsatzPublicHeaders.cmake`) copies each public header at configure time to `build/public/<target>/<prefix>/<path>` and makes that folder the target's public include directory. The target's own source directories are private. Re-running configure stages an edited header again.

| Target | Included as | Example |
|---|---|---|
| `ursatz` (kernel) | its module folder, no prefix | `time/meter_map.h`, `analysis/motive_sketch.h` |
| a framework | its folder name, then the path in the framework | `standard-form/analysis/analysis_motive.h`, `common-practice-minimal/key_regions.h` |
| `ursatz_chain` | `ursatz_chain/` | `ursatz_chain/analysis_chain.h` |

A framework or chain source that includes another framework's header uses that path, never the bare name, so the eight names that exist in more than one framework (`analysis_claim.h`, `analysis_input.h`, `analysis_phrase.h`, `framework.h`, `hierarchy_rules.h`, `key_signature_rules.h`, `rule_pair.h`, `stem_rules.h`) mean one header each. A sibling or a header of the same framework stays bare. `rule_pair.h` stays private in each framework.

The public set of a target is: every header the apps include (`docs/public-headers-baseline.txt`, made by `scripts/app_includes.sh <app checkout>...`), the owners named on this page, every header a framework or the chain takes from another target, every header those include, and nothing named `*_internal.h`. `internal_expansion_technique.h` is a plain enum and is public. A framework with nothing in the set stages nothing.

A test sees the internals of what it tests (`ursatz_test_includes()`); an app does not.

Three tests guard this: `architecture_public_headers` (a public header includes only public headers of its own or a linked target), `public_headers_compile` (a generated file per target includes every public header and compiles with only the staged folders of the target and its links), `architecture_header_paths` (no bare include of another framework's header).

## Placement rule

Every analysis algorithm is library code (R-217). The kernel owns what needs no theory; a framework owns what one theory defines; the chain owns what joins them. The Analyzer orchestrates, combines, caches and exports: it calls owners, places their results in its report, stores them and renders documents. It holds no copy of an algorithm a library owns. What stays in the Analyzer is the names of its own style statistics (`style_label_interval`, `_contour`, `_duration`, `_tempo`, `_density`, `_join`, `_nct_figure`, `_progression` and the style dimension and category tables), the window search and scoring of similar material over stored documents, motive link caching and scope reading, report placement, and document rendering. The Composer and the GUI follow the same rule: a theory rule either app needs is added here first and the app calls it.

## Theory owners

The theory the apps used to copy has one library owner each (R-217, R-223). The table is in [theory-owners.md](theory-owners.md).

## Open issues

- **Theory to Corpus** (Dependency Spec §7.2). The Registry lists `Tendency`, `StatisticalModel`, and `StyleModel` (L7) as depending on `Corpus` (L9). All three accept corpus-derived data only as an opaque, already-computed value; no L7 file includes a corpus header. Detail in `docs/modules/theory.md`.

## Resolved issues

- **Interpretation-family to Framework** (Dependency Spec §7.1, closed). `Interpretation`, `ScaleDegree`, `Chord`, and `Key` (L6) carry an optional `FrameworkReference` (`core`, L1) rather than a live `Framework` reference. `Chord`, `Key`, `ScaleDegree` and `Texture` require it at construction. Detail in `docs/modules/l6-notes.md`.
- **Reference-type Registry synchronization.** `VoiceReference`, `RelationshipReference`, and `ProvenanceReference` are authorized in the System Registry and implemented in `core`. See `docs/modules/core.md`.

## Module status

The status and doc of each module are in [module-status.md](module-status.md).

## Build and test

See `docs/BUILDING.md`.

