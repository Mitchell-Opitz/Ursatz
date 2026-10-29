# Architecture

Ursatz is organized into layered modules, each living under `src/<module>/` with its tests mirrored under `tests/<module>/`. Theory Framework extension modules live outside the layers, under `frameworks/<name>/`. This page covers the layer model and the rules that hold it together. Per-module detail (what each module provides, what is deferred, what to know before building on it) lives under `docs/modules/`.

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
| L8 | `transform`, `analysis`, `generation` (three mutually-restricted siblings) |
| L9 | `io` (MIDI import, MIDI key-signature scan), `export` (MusicXML), `persistence` (SQLite), `corpus`, `corpus_stats`, `query` |
| Extensions | `frameworks/<name>/`: 17 Theory Frameworks built on L7 (see `frameworks/README.md`); `chain/`: the analysis chain (`ursatz_chain`), above the kernel and the frameworks |

## Dependency rule

A module in layer N may depend only on modules in layers 0 through N. It must never depend sideways within its own layer unless that specific dependency is documented (as `rules` on `theory` is), and never depend upward on a higher layer. No `src/` module includes a framework header. Frameworks never compile kernel sources (`architecture_framework_sources` checks every `frameworks/*/CMakeLists.txt` and `chain/CMakeLists.txt`); code that needs both a framework and the kernel lives in `chain/`. The former exception, `analysis`'s `ChordIdentification` and `KeyEstimation`, moved into the chain (R-010 finished, R-241).

The analysis chain (`chain/`, target `ursatz_chain`) is an extension above the numbered stack. It may include kernel L0 to L8 and the framework headers named in `chain/tests/test_architecture_chain.c`, and never L9 (`io`, `export`, `persistence`, `corpus`, `corpus_stats`, `query`). Headers are included as `ursatz_chain/<name>.h`. A framework test may link the chain; a framework library may not.

A framework may depend on L0 to L6, on `theory/framework.h`, `rules/rule.h` and `rules/constraint.h`, and on another framework only where that edge is recorded in `frameworks/README.md`.

Every module and every framework has an architecture test that allowlists the directories it may include (for example `tests/events/test_architecture_events.c`). A disallowed include fails that test. Framework architecture tests run from the root build like the module ones (since PR #99); see `docs/BUILDING.md`.

## Shared primitives

Small public helpers that every layer above reuses instead of writing its own copy.

- `core/string_copy.h`: `ursatz_string_copy`.
- `core/rational.h`: `Rational`, a signed exact rational on 64-bit integers, always in lowest terms. Every operation is checked and reports `RATIONAL_ERR_OVERFLOW`; nothing wraps. `time/time_rational.h` bridges it to `MusicalTime`, `Duration` and `PerformanceTime`. `time/exact_rational.h` is deprecated and kept until the Analyzer switches (P24).
- `time/meter_arithmetic.h`: exact measure and beat functions over a `MeterMap` (measure of a time, measure bounds, beat length and count, beat of a time, snap to beat, on beat), on `Rational`. The standard-form, standard-meter and standard-texture time helpers wrap it. `time_span_is_valid` in `time/time_span.h` is the shared span check.
- `pitch/pitch_arithmetic.h`: letter semitone and index, letter offset, pitch class, MIDI number, line-of-fifths position, spelling names and parsing. Theory-neutral; every framework's letter tables sit on it. Key, mode and degree functions are `frameworks/standard-tonality/tonal_key.h`.
- `io/midi_importer.h`: `midi_importer_import_complete` parses an SMF once and returns notes, meter, tempo, key signatures, and every track's name and program changes in one `MidiImport`. The three older import entry points are wrappers over it. `smf_reader.h` and `midi_event_decoder.h` are internal to `io`.
- `frameworks/standard-tonality/roman_numeral_grammar.h`: the Roman numeral grammar. `standard_tonality_numeral_parse` reads every numeral form (alteration, case, quality mark, figure, applied target), `standard_tonality_numeral_format` writes the canonical spelling, `standard_tonality_numeral_root_semitone` gives the root through an applied target. The older numeral entry points are wrappers on it. standard-harmony keeps its own label writer, checked by a test.
- `semantics/analysis_note.h`, `analysis_segment.h`, `analysis_key.h`, `voice_grid.h`: the neutral analysis input types (`AnalysisNote`, `AnalysisSegment`, `AnalysisKey`, `AnalysisVoiceGrid`) that framework adapters accept, so a caller no longer writes the converter. `semantics/` includes `interpretation/key.h` for `KeyStructureMode` (allowlisted in its architecture test), and `io/` may include `semantics/analysis_key.h`. `standard-counterpoint/slot_note.h`: the slot note and rule result the three counterpoint styles share. See `docs/modules/semantics.md`.

## Vocabulary enums

`src/semantics/` holds plain enums that name theory concepts frameworks share (`CadenceType`, `VoicePart`, `NonChordTone`, and others). Each carries no `EntityID`, no data, and no framework claim, only a name and small name-level predicates. What a name means, and which music it applies to, is always the framework's convention. New enums of this kind are admitted to the kernel only when at least two frameworks need them; a name only one framework uses belongs in that framework. Existing single-framework enums stay until their framework is next changed, then move into it.

## Analytic job owners

One owner per analytic job (R-224). Every other implementation of the job is retired or a thin wrapper, and its header names the owner. A framework may include kernel L0 to L6 plus L7 Framework, Rule and Constraint, never kernel L8 (`analysis/`, `transform/`, `generation/`), and the kernel depends on no framework (Law 11). A wrapper that joins an L8 analysis with a framework adapter therefore cannot live in either one: it lives in the `chain/` target above both (P22 B3). Where a framework needs an L8 owner and the chain does not apply, the caller injects it as a callback (the `StandardFormBoundaryDetector` pattern).

| Job | Owner | Other implementations: what happens |
|---|---|---|
| Sonority segmentation | `sonority_builder_build_adaptive` (`src/semantics/sonority_segmentation.c`) | `sonority_builder_build_from_notes`: kept as the fixed-grid primitive (a caller-given grid is a different job). Segmentation glue in `tests/analysis/weighted_pipeline.h` and `tests/accuracy/chorale_chain.c`: moves into the chain (P22 B4), deleted (P22 B5). |
| Chord identification | `common_practice_identify_weighted_sequence` and `common_practice_identify_weighted` (`frameworks/common-practice-minimal/weighted_chord_identification.c`) | `chain_subset_chord_analyze` (`chain/ursatz_chain/subset_chord_analyzer.c`) with `common_practice_identify_triad` / `common_practice_identify_seventh` (subset match, known duplicate-id gap): not the owner, kept only for the subset coverage reports; the analyzer lives in the chain. `standard_tonality_build_chord_from_label`: builds a chord from a label, a different job. |
| Key identification | `key_regions_estimate` (`frameworks/common-practice-minimal/key_regions.c`); the home key comes from its regions (P23 moves the Analyzer's home key onto it) | `chain_key_collection_analyze` (`chain/ursatz_chain/key_collection_analyzer.c`): not the owner, lives in the chain. `common_practice_identify_key` (one-directional containment, wrong tonics pass): deprecated until P27, kept while the GUI reaches it. Key signatures from MIDI or MusicXML are facts, not identification. |
| Cadence detection | `harmony_analysis_find_cadences` (`frameworks/standard-harmony/analysis/analysis_cadence.c`) | `cadence_detection_analyze`, `cadence_classification_analyze`: packaging of caller candidates into Cadence shells, kept as packaging. `fv1_add_boundary_cadences` (`tests/integration/framework_v1_chain.h`): moves into the chain (P22 B4). |
| Phrase segmentation | `standard_form_segment_phrases` (`frameworks/standard-form/analysis/analysis_phrase.c`), with `phrase_boundary_detection_analyze` as its injected detector through the chain bridge (P22 B3) | `phrase_segmentation_analyze`: an internal step of the kernel detector, kept. The bridge copies (Analyzer `pipeline/form_boundary.c`, Composer `src/form/form_boundary.c`, `tests/integration/form_kernel_bridge.h`): one chain bridge (P22 B3); the apps switch in P24 and P25. |
| Motive matching | `motive_variant_recognize` with the `motive_relation_apply` relations (R-147) | `motif_window_matching_find`: the exact contour and rhythm primitive, injected into standard-form through the chain bridge (P22 B3). `standard_form_motive_match_*` transposed, inverted, retrograde: switch to the kernel relations with the one motive value in P23. |
| Non-chord-tone classification | `motive_tone_roles` (`src/analysis/motive_tone_roles.c`; melody against a given harmony, full types and resolutions, R-196) | `common_practice_label_leftover_tones` and `standard_counterpoint_melody_nct_labels`: frameworks cannot call kernel L8, so they switch to the owner through an injected classifier in P23; until then each header names the owner. |
| Voice assignment | `voice_separate` (`src/analysis/voice_separation.c`); outer lines are their own job, owner `outer_voice_extract` | `standard_texture_consolidate_voices`: texture's merge step over assigned voices, not assignment, kept. `vl_separated` (`tests/integration/voice_leading_util.h`): moves into the chain (P22 B4). |
| Motion classification | `standard_counterpoint_classify_motions` (`frameworks/standard-counterpoint/analysis/analysis_motion.c`, `MotionType`) | `voice_leading_analysis_analyze`: packaging of caller-supplied motions (its docs say so), kept as packaging. |

## Open issues

- **Theory to Corpus** (Dependency Spec §7.2). The Registry lists `Tendency`, `StatisticalModel`, and `StyleModel` (L7) as depending on `Corpus` (L9). All three accept corpus-derived data only as an opaque, already-computed value; no L7 file includes a corpus header. Detail in `docs/modules/theory.md`.

## Resolved issues

- **Interpretation-family to Framework** (Dependency Spec §7.1, closed). `Interpretation`, `ScaleDegree`, `Chord`, and `Key` (L6) carry an optional `FrameworkReference` (`core`, L1) rather than a live `Framework` reference. `Chord`, `Key`, `ScaleDegree` and `Texture` require it at construction. Detail in `docs/modules/l6-notes.md`.
- **Reference-type Registry synchronization.** `VoiceReference`, `RelationshipReference`, and `ProvenanceReference` are authorized in the System Registry and implemented in `core`. See `docs/modules/core.md`.

## Module status

| Module | Layer | Status | Doc |
|---|---|---|---|
| identity | L0 | Implemented | `docs/modules/identity.md` |
| core | L1 | Implemented | `docs/modules/core.md` |
| time | L2 | Implemented | `docs/modules/time.md` |
| pitch | L2 | Implemented (partial, see doc) | `docs/modules/pitch.md` |
| events | L3 | Implemented | `docs/modules/events.md` |
| containers | L4 | Implemented (partial, see doc) | `docs/modules/containers.md` |
| structure | L5 | Implemented (partial, see doc) | `docs/modules/structure.md` |
| context | L6 | Implemented | `docs/modules/context.md` |
| semantics | L6 | Implemented | `docs/modules/semantics.md` |
| relationship | L6 | Implemented | `docs/modules/relationship.md` |
| interpretation | L6 | Implemented (partial, see doc) | `docs/modules/interpretation.md` |
| theory | L7 | Implemented (partial, see doc) | `docs/modules/theory.md` |
| rules | L7 | Implemented (partial, see doc) | `docs/modules/rules.md` |
| transform | L8 | Implemented | `docs/modules/transform.md` |
| analysis | L8 | Implemented | `docs/modules/analysis.md`, `docs/modules/motive-relations.md`, `docs/modules/motive-variant-recognition.md`, `docs/modules/motive-tone-roles.md` |
| generation | L8 | Implemented (partial, see doc) | `docs/modules/generation.md` |
| io | L9 | Implemented (partial, see doc) | `docs/modules/io.md` |
| export | L9 | Implemented (MusicXML from a MeterMap: parts, staves, key, tempo, dynamics; no MXL, repeats or articulations) | `src/export/musicxml_export.h` |
| persistence | L9 | Implemented (partial, see doc) | `docs/modules/persistence.md` |
| corpus | L9 | Implemented | `docs/modules/corpus.md` |
| corpus_stats | L9 | Implemented (partial, see doc) | `docs/modules/corpus_stats.md` |
| query | L9 | Implemented (partial, see doc) | `docs/modules/query.md` |

"Partial" means some types from the module's Registry row are implemented and some are explicitly deferred; the module's own doc lists which.

## Build and test

See `docs/BUILDING.md`.
