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
| L8 | `analysis`, `generation` (two mutually-restricted siblings) |
| L9 | `io` (MIDI import, MIDI key-signature scan), `export` (MusicXML), `persistence` (SQLite), `corpus`, `corpus_stats`, `query` |
| Extensions | `frameworks/<name>/`: 17 Theory Frameworks built on L7 (see `frameworks/README.md`); `chain/`: the analysis chain (`ursatz_chain`), above the kernel and the frameworks |

## Dependency rule

A module in layer N may depend only on modules in layers 0 through N. It must never depend sideways within its own layer unless that specific dependency is documented, and never depend upward on a higher layer. The documented sideways edges are `rules` on `theory` (L7), `interpretation` on `relationship` through `claim_interpretation.h` (L6), and in L9 `export`, `corpus`, `corpus_stats` and `query` on `persistence` plus `corpus_stats` and `query` on `corpus`. No `src/` module includes a framework header. Frameworks never compile kernel sources (`architecture_framework_sources` checks every `frameworks/*/CMakeLists.txt` and `chain/CMakeLists.txt`); code that needs both a framework and the kernel lives in `chain/`. The former exception, `analysis`'s `ChordIdentification` and `KeyEstimation`, moved into the chain (R-010 finished, R-241).

The analysis chain (`chain/`, target `ursatz_chain`) is an extension above the numbered stack. It may include kernel L0 to L8 and the framework headers named in `chain/tests/test_architecture_chain.c`, and never L9 (`io`, `export`, `persistence`, `corpus`, `corpus_stats`, `query`). Headers are included as `ursatz_chain/<name>.h`. A framework test may link the chain; a framework library may not. The chain keeps the stage inputs it builds (pitches, harmony inputs, prepared harmony, form and motive inputs, voice grid, galant segments) and exposes them through `analysis_chain_*` accessors, and it reads the outer lines on the chord segments (`chain_build_segment_lines`, `analysis_chain_segment_outer_voices`) alongside the lines on the voice segments (`analysis_chain_outer_voices`).

A framework may depend on L0 to L6, on `theory/framework.h`, `theory/framework_singleton.h`, `rules/rule.h`, `rules/constraint.h`, `rules/preference.h` and `rules/rule_pair.h`, and on another framework only where that edge is recorded in `frameworks/README.md`.

Every module and every framework has an architecture test that allowlists the directories it may include (for example `tests/events/test_architecture_events.c`). A disallowed include fails that test. Framework architecture tests run from the root build like the module ones (since PR #99); see `docs/BUILDING.md`. `tests/test_architecture_tests.c` applies the same layer rules to the test sources (`tests/<module>/` and `frameworks/<name>/tests/`); the test-only edges it allows are listed in its `ALLOWED_TEST_EDGES`, one reason each.

## Shared primitives

Small public helpers that every layer above reuses instead of writing its own copy.

- `core/string_copy.h`: `ursatz_string_copy`.
- `core/rational.h`: `Rational`, a signed exact rational on 64-bit integers, always in lowest terms. Every operation is checked and reports `RATIONAL_ERR_OVERFLOW`; nothing wraps. `time/time_rational.h` bridges it to `MusicalTime`, `Duration` and `PerformanceTime`.
- `time/meter_arithmetic.h`: exact measure and beat functions over a `MeterMap` (measure of a time, measure bounds, beat length and count, beat of a time, snap to beat, on beat), on `Rational`. The standard-form, standard-meter and standard-texture time helpers wrap it. `time_span_is_valid` in `time/time_span.h` is the shared span check.
- `pitch/pitch_arithmetic.h`: letter semitone and index, letter offset, pitch class, MIDI number, line-of-fifths position, spelling names and parsing. Theory-neutral; every framework's letter tables sit on it. Key, mode and degree functions are `frameworks/standard-tonality/tonal_key.h`.
- `io/midi_importer.h`: `midi_importer_import_complete` parses an SMF once and returns notes, meter, tempo, key signatures, and every track's name and program changes in one `MidiImport`. The three older import entry points are wrappers over it. `smf_reader.h` and `midi_event_decoder.h` are internal to `io`.
- `frameworks/standard-tonality/roman_numeral_grammar.h`: the Roman numeral grammar. `standard_tonality_numeral_parse` reads every numeral form (alteration, case, quality mark, figure, applied target), `standard_tonality_numeral_format` writes the canonical spelling, `standard_tonality_numeral_root_semitone` gives the root through an applied target. The older numeral entry points are wrappers on it. standard-harmony keeps its own label writer, checked by a test.
- `semantics/analysis_note.h`, `analysis_segment.h`, `analysis_key.h`, `voice_grid.h`: the neutral analysis input types (`AnalysisNote`, `AnalysisSegment`, `AnalysisKey`, `AnalysisVoiceGrid`) that framework adapters accept, so a caller no longer writes the converter. `semantics/` includes `interpretation/key.h` for `KeyStructureMode` (allowlisted in its architecture test), and `io/` may include `semantics/analysis_key.h`. `standard-counterpoint/slot_note.h`: the slot note and rule result the three counterpoint styles share. See `docs/modules/semantics.md`.

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
