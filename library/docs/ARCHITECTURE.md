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
| Sonority segmentation | `sonority_builder_build_adaptive` (`src/semantics/sonority_segmentation.c`) | `sonority_builder_build_from_notes`: kept as the fixed-grid primitive (a caller-given grid is a different job). Segmentation glue in `tests/analysis/weighted_pipeline.h` and `tests/accuracy/chorale_chain.c`: deleted (P22 B5); both run `analysis_chain_run`, whose segments stage is `chain_build_segments` and `chain_build_members`. |
| Chord identification | `common_practice_identify_weighted_sequence` and `common_practice_identify_weighted` (`frameworks/common-practice-minimal/weighted_chord_identification.c`) | `chain_subset_chord_analyze` (`chain/ursatz_chain/subset_chord_analyzer.c`) with `common_practice_identify_triad` / `common_practice_identify_seventh` (subset match, known duplicate-id gap): not the owner, kept only for the subset coverage reports; the analyzer lives in the chain. `standard_tonality_build_chord_from_label`: builds a chord from a label, a different job. |
| Key identification | `key_regions_estimate` (`frameworks/common-practice-minimal/key_regions.c`); the home key comes from its regions through `common_practice_home_key_region` (`key_regions.h`) | `chain_key_collection_analyze` (`chain/ursatz_chain/key_collection_analyzer.c`): not the owner, lives in the chain. `common_practice_identify_key` (one-directional containment, wrong tonics pass): deprecated until P27, kept while the GUI reaches it. Key signatures from MIDI or MusicXML are facts, not identification. |
| Cadence detection | `harmony_analysis_find_cadences` (`frameworks/standard-harmony/analysis/analysis_cadence.c`) | `cadence_detection_analyze`, `cadence_classification_analyze`: packaging of caller candidates into Cadence shells, kept as packaging. `fv1_add_boundary_cadences` (`tests/integration/framework_v1_chain.h`): deleted (P22 B5); `chain_add_boundary_cadences` runs in the chain. |
| Phrase segmentation | `standard_form_segment_phrases` (`frameworks/standard-form/analysis/analysis_phrase.c`), with `phrase_boundary_detection_analyze` as its injected detector through the chain bridge (P22 B3) | `phrase_segmentation_analyze`: an internal step of the kernel detector, kept. The bridge copies (Analyzer `pipeline/form_boundary.c`, Composer `src/form/form_boundary.c`, `tests/integration/form_kernel_bridge.h`, deleted in P22 B5): one chain bridge (P22 B3); the apps switch in P24 and P25. |
| Motive matching | `motive_variant_recognize` with the `motive_relation_apply` relations (R-147) | `motif_window_matching_find`: the exact contour and rhythm primitive, injected into standard-form through the chain bridge (P22 B3). `standard_form_motive_match_*` transposed, inverted, retrograde: switch to the kernel relations with the one motive value in P23. |
| Non-chord-tone classification | `motive_tone_roles` (`src/analysis/motive_tone_roles.c`; melody against a given harmony, full types and resolutions, R-196) | `common_practice_label_leftover_tones` and `standard_counterpoint_melody_nct_labels`: frameworks cannot call kernel L8, so they switch to the owner through an injected classifier in P23; until then each header names the owner. |
| Voice assignment | `voice_separate` (`src/analysis/voice_separation.c`); outer lines are their own job, owner `outer_voice_extract` | `standard_texture_consolidate_voices`: texture's merge step over assigned voices, not assignment, kept. `vl_separated` (`tests/integration/voice_leading_util.h`): kept, and it calls `voice_separate` itself; it separates the notes of a hand-labelled four-part fixture next to the fixture's true parts, an input `chain_build_voice_grid` (which separates the notes of a piece) does not take. |
| Motion classification | `standard_counterpoint_classify_motions` (`frameworks/standard-counterpoint/analysis/analysis_motion.c`, `MotionType`) | `voice_leading_analysis_analyze`: packaging of caller-supplied motions (its docs say so), kept as packaging. |

## Theory owners

The theory the apps used to copy now has one library owner each (R-217, R-223). The app switches at its bump (P24 Analyzer, P25 Composer) and deletes its copy then; until then each library header names the owner. Ports keep the app's rules, constants and tie-breaks; where the port differs the row says so.

| Theory | Owner | Port notes |
|---|---|---|
| Meter check (candidate meter scoring against harmonic rhythm and cadences) | `standard_meter_check_meter`, `standard_meter_check_changes` (`frameworks/standard-meter/analysis/meter_analysis_check.h`) | From Analyzer `meter_check_pipeline.c`. Exact `Rational` in place of `__int128`: an overflow is `STANDARD_METER_ANALYSIS_ERR_OVERFLOW`, never wrapped. Input is neutral evidence (times), not report entries. |
| Tuplets and duplets | `standard_meter_find_tuplets` (`frameworks/standard-meter/analysis/meter_analysis_tuplet.h`) | From Analyzer `rhythm_pipeline.c`. Input is an `AnalysisNoteSet` and an optional `in_line` mask; `meter` is a `MeterType` and `level` a `TupletLevel` (the caller renders the names). A time that overflows the 96-step grid arithmetic is `ERR_OVERFLOW`. |
| Home key (the key with the longest total duration over the regions) | `common_practice_home_key_region` (`frameworks/common-practice-minimal/key_regions.h`) | From Analyzer `key_region_pipeline.c` `home_key_region`. Sums on checked `Rational` (an overflow is false, never wrapped); a region's key is its `key_index`, so tonic and mode are both part of the key. Earliest key on a tie, as the Analyzer. |
| Chord quality name and inversion of a ranked chord | `common_practice_ranked_chord_quality_name`, `common_practice_ranked_chord_inversion` (`frameworks/common-practice-minimal/weighted_identification.h`) | From Analyzer `chord_pipeline.c` `triad_quality`, `inversion_of`. The bass pitch class must be 0..11 (the Analyzer reduced any integer mod 12); a set outside the vocabulary has no name (NULL), the caller renders "unknown". |
| Scale degree as numbers | `common_practice_scale_degree_of` (`frameworks/common-practice-minimal/outer_scale_degrees.h`) | Replaces parsing `common_practice_scale_degree_label` in the Analyzer (`motive_pipeline.c`, `melody_pipeline.c`). The label is built over it with equal output. |
| Triad and seventh quality as semitones | `standard_tonality_triad_quality_semitones`, `standard_tonality_seventh_quality_semitones` (`frameworks/standard-tonality/triad_rules.h`, `seventh_rules.h`) | From Analyzer `harmony_pipeline.c` `intervals_of`. A test checks each row against the framework's interval rules. |
| Chromatic degree label (a semitone distance from the tonic) | `standard_tonality_chromatic_degree_label` (`frameworks/standard-tonality/tonal_key.h`) | From Analyzer `style_labels.c` `style_label_degree` (also reached by Composer `style_check.c`). Any integer is reduced mod 12. |
| Tonic spelling of a key from its pitch class | `standard_tonality_key_tonic_of_pitch_class` (`frameworks/standard-tonality/tonal_key.h`) | From Analyzer `pitch_class_util.c` `tonic_prefers_flats`: the fewer accidentals in the signature, sharps on a tie. A minor key is judged by its relative major (D# minor, not Eb minor), where the Analyzer judged the tonic's own major key. |
| Accented beats of a bar | `standard_meter_accent_beats` (`frameworks/standard-meter/metric_position_rules.h`) | For Composer `harmony_rhythm.c` cells. Gives the beats whose `standard_meter_beat_strength` is strong or medium, so 2/4, 3/4, 6/8 and 9/8 give beat 0 alone and 4/4 and 12/8 give beats 0 and 2. The Composer's 6/8 has two cells (beat 4 medium); the library reads the second dotted beat as weak, so P25 declares that difference. |
| Melodic move legality and step size on semitones | `standard_counterpoint_melodic_semitones_allowed`, `standard_counterpoint_is_step` (`frameworks/standard-counterpoint/melodic_rules.h`) | From Composer `counterpoint_species.c` `move_ok` (equal for -24 to 24, tested) and `melody_nct.c` `is_step`. `leap_allowed` is not the rule: it allows the tritone and the library does not (P25 declares it). |
| Run of equal imperfect consonances | `standard_counterpoint_imperfect_run` (`frameworks/standard-counterpoint/harmonic_rules.h`) | From Composer `counterpoint_species.c` `parallel_cost` (`run` 3). A plain function, not a rule table entry, so no existing check changes. |
| Pitch classes of a mode, degree of a semitone | `standard_tonality_mode_pitch_classes`, `standard_tonality_degree_of_semitone` (`frameworks/standard-tonality/tonal_key.h`) | For Composer `melody_scale_mask`, `counterpoint_write.c` `scale_degree`, `melody_check.c` `pitch_degree`. A semitone not in the mode reads as the lower degree raised; the Composer's table reads some of them as the next degree (major 1, 3, 8, 10; natural minor 1, 6), recorded in `test_degree_of_semitone.c` for P25 to declare. |

## The production chain

`analysis_chain_run` (`chain/ursatz_chain/analysis_chain.h`) is the one production chain from neutral notes (`semantics/analysis_note.h`, made from an import by `io/import_notes.h`) to every analysis the libraries own. It runs the stages in the order of the dependency spec: segments, local key regions, weighted chords per region, Roman numerals, outer voices, cadences (with boundary cadences), phrase form (phrases, ideas, themes, expansions) and motive families, meter, texture, voice leading, melody, schemas and sequences. Each stage calls the owner of its job (the table above) through an input builder in `chain/ursatz_chain/chain_inputs.h`, which turns the notes and the earlier stages' results into the plain input the framework reads; no builder chooses a default. A stage subset (`AnalysisChainConfig.stages`) runs only the stages it names and the ones they need. The key regions depend on the phrases and the phrases on the keys, so the run goes through them twice (the first pass's chords are evidence and its phrase ends boundaries for the second); there is no further iteration. `AnalysisChain` owns every result and gives it back through accessors that return the frameworks' own result structs. No test glue runs analysis. The library's own tests run it through the chain: `tests/analysis/weighted_pipeline.h` (segments and outer lines), `tests/accuracy/chorale_chain.c` (the accuracy harness) and `tests/integration/framework_v1_chain.h` (the reference pieces) call `analysis_chain_run`, and the integration tests that give a stage an annotation's answer (a home key, hand cadences, a written soprano) build its input with the chain's builders. What stays in the tests is expected-value parsing and comparison. Detail in `chain/README.md` and `docs/testing/`.

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
