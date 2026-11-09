# Analysis chain

`ursatz_chain` is the library that joins the kernel (`src/`) and the Theory Frameworks (`frameworks/`) for analysis. A framework never compiles kernel sources (R-241), so anything that needs both lives here. Headers are included as `ursatz_chain/<name>.h`; only the six listed in `chain/CMakeLists.txt` are public, and the framework headers it takes are included by their public path (`standard-form/analysis/analysis_phrase.h`).

## Layer

An extension above the numbered stack. It may include kernel L0 to L8 and the framework headers listed in `chain/tests/test_architecture_chain.c`. It never includes L9 (`io`, `export`, `persistence`, `corpus`, `corpus_stats`, `query`). `architecture_chain` enforces this. The notes it reads (`semantics/analysis_note.h`) are made from an import by `io/import_notes.h`, which the caller includes.

## Contents

- `analysis_chain.h/.c`: `analysis_chain_run` and the opaque `AnalysisChain` that owns every result (below).
- `chain_inputs.h/.c`: the input builders, one per framework input, each owning what it allocates with a matching `_destroy`. A failed builder leaves its output untouched. `ChainInputStatus` names the failure.
- `form_bridge.h/.c`: `chain_form_boundary_detect` (standard-form's boundary detector, over the kernel's Phrase Boundary Detection) and `chain_form_match_windows` (standard-form's window matcher, over the kernel's Motif Window Matching).
- `subset_chord_analyzer.h/.c`: `chain_subset_chord_analyze` and `CHAIN_SUBSET_CHORD_ANALYZER`, the subset-match chord analyzer (not the chord identification owner; kept for the subset coverage reports).

## The run

```c
AnalysisChainConfig config = analysis_chain_config_default();
AnalysisChain *chain = NULL;
AnalysisChainStatus status = analysis_chain_run(&notes, key_events, key_event_count, &config, &chain);
/* status names the failing stage; analysis_chain_status_name(status) */
const CommonPracticeKeyRegions *keys = analysis_chain_key_regions(chain);
analysis_chain_destroy(chain);
```

`notes` is an `AnalysisNoteSet` (sorted by `analysis_note_compare`, every note with its `NoteEvent` and a known pitch, one `MeterMap`). The key events are `AnalysisKeyEvent`s (a key signature as a key at a time). Everything is borrowed and must outlive the chain.

Stages, in the order they run, with the owner each calls ([docs/analytic-job-owners.md](../docs/analytic-job-owners.md)):

| Stage (`ANALYSIS_CHAIN_STAGE_*`) | Builder | Owner |
|---|---|---|
| `SEGMENTS` | `chain_build_segments`, `chain_build_members`, `chain_attack_strength`; with `harmonic_spans` (and the chords stage) `chain_build_span_cells`, `chain_merge_segments` | `sonority_builder_build_adaptive`, then `harmonic_span_basses` and `harmonic_spans_group` (R-369 to R-373) |
| `KEYS` | `chain_build_key_segments`, `chain_estimate_key_regions` | `key_regions_estimate` |
| `CHORDS` | (in the run) | `common_practice_identify_weighted_sequence`, per key region, a minor region in the minor vocabulary; on spans the chord must hold the span's bass and a seventh is charged `ANALYSIS_CHAIN_SPAN_SEVENTH_PENALTY`; with `applied_chords` the key's applied chords are candidates too |
| `NUMERALS` | (in the run) | standard-tonality chord inputs and Roman numerals; with `applied_chords` an identified applied chord is read directly and the diatonic re-check is off |
| `OUTER_VOICES` | `chain_build_pitches` | `outer_voice_extract`, on the segmentation without the merge over unchanged harmony |
| `CADENCES` | `chain_build_harmony_segments`, `chain_add_boundary_cadences`, `chain_confirm_key_changes` | `harmony_analysis_find_cadences` |
| `FORM` | `chain_build_form_input`, `chain_build_grid_input`, `chain_build_motive_input` | the phrase grid and the cadence claims (final pass, below), `standard_form_segment_phrases` (kernel detector injected) or its grid form, ideas, themes, expansions, motive families (kernel window matcher injected) |
| `METER` | `chain_build_meter_input` | standard-meter: syncopations, hemiolas, hypermeter, density |
| `TEXTURE` | `chain_build_texture_input` | `voice_separate`, standard-texture |
| `VOICE_LEADING` | `chain_build_voice_grid` | `standard_counterpoint_classify_motions`, perfect intervals, harmony resolutions |
| `MELODY` | `chain_build_melody_line` | standard-counterpoint melody: intervals, contour, climax, leap recovery, non-chord tones (relabelled by the injected kernel classifier) |
| `SCHEMAS` | `chain_build_galant_segments` | `galant_schema_match`, `scale_sequence_detect` |

A stage pulls in the stages it needs (keys pull segments, chords keys, numerals chords, cadences numerals and outer voices, form cadences, meter form and chords and outer voices, voice leading cadences, melody chords and outer voices, schemas numerals and outer voices; texture reads only the notes). The accessors (`analysis_chain_chords`, `_key_regions`, `_numerals`, `_outer_voices`, `_cadences`, `_phrases`, `_phrase_grid`, `_ideas`, `_themes`, `_expansions`, `_motives`, `_meter`, `_texture`, `_voice_leading`, `_melody`, `_schemas`, `_sequences`) return the frameworks' own result structs and their Interpretations, or NULL for a stage that did not run.

The chain also keeps the inputs it builds for its stages, until `analysis_chain_destroy`, so a caller that maps results back to its own notes does not rebuild them: `analysis_chain_pitches`, `_segment_outer_voices`, `_harmony_inputs` and `_prepared_harmony` (of the final pass), `_form_input` (final pass), `_motive_input`, `_voice_grid` and `_galant_segments`. Each is NULL when the stage that builds it did not run.

## Config

`AnalysisChainConfig` has one field per stage config; `analysis_chain_config_default()` fills every one with the library's default. `stages` is the bitmask (default all). `first_measure` and `last_measure` bound the part of the piece analysed (the stages up to cadences, and voice leading, melody and schemas, run over the segments that start before the end of `last_measure`; form, meter and motives read the measures from `first_measure` to `last_measure`; `last_measure` 0 is the whole piece). `home_key_index` is the home key (a key index 0..23) or -1 for the key of the first key region; it is the key of the boundary cadences, the motive and melody degrees and the schema key degrees. `refine_keys` (default on, with the form stage) runs the second pass: the keys depend on the phrases, the phrases on the cadences and the cadences on the keys, so the first pass's chords become evidence for the keys and its phrase ends their boundaries, and every stage from the keys on runs once more. No further iteration. `phrase_grid` (default on, the switch for measuring the change, R-389 to R-391) finds the phrases first in the final pass (below); off, every pass runs as before. `harmonic_spans` (default on) reads the chords on harmonic spans (below). `applied_chords` (default on, the switch for measuring the change, R-380) reads applied chords (below). `refine_with_given_boundaries` (default off) runs that second pass with no form stage, on `key_regions.phrase_boundaries` as the caller gives them, for a caller that finds its own phrase ends (the accuracy harness).

## Phrases first

With `phrase_grid` the form stage of the final pass (the second pass of the key refinement, the only pass when there is none) finds the phrases before it trusts any cadence (R-389 to R-391; the rules are in `docs/modules/analysis-phrase-form.md` and the claims in `docs/modules/standard-harmony-analysis.md`). It builds the half readings (`harmony_analysis_half_readings`) over the pass's cadences, `chain_build_grid_input` (the note-level soprano of `chain_build_motive_input` and every cadence found or read, each over the window of its first arrival segment), finds the grid (`standard_form_find_phrase_grid`), lets the grid follow the written-out repeats (`standard_form_phrase_grid_follow_repeats`: where a repeat's earlier span is tiled by whole units, the units over its later span become copies of them, so Mozart 545's repeated development gets the units, cadences and phrases of its first statement), claims the cadences at the grid's points (`harmony_analysis_claim_cadences`; the claimed cadences replace the pass's), and then segments the phrases and the themes on the grid. The first pass keeps the cadence-ended phrases, so the key regions, chords and numerals do not move (P42 places keys on phrases). `analysis_chain_phrase_grid` returns the grid, NULL without one. The repeats and returns (`standard_form_find_repeats` over the notes in scope, R-400) are found once in `prepare`, whatever the passes do, and `analysis_chain_repeats` returns the list (empty, never NULL, when there are none); only repeats move the grid, returns are not followed.
With `keys_from_phrases` (default on; R-397, R-398) the final pass is followed by a third. It sets each key segment's chord from the second pass's chords, builds the phrases from the grid (each unit is one phrase, or two halves when it has a half measure; the first part of an elided unit also takes the elided measure), gives each its `cadence_keys` from every claimed cadence arriving in it (a PAC, IAC or unspecified authentic as authentic, a HC as half, over the arrival segment's chord; the alternative reading counts when it is a cadence), its `arrival_keys` when the next unit is elided at its last measure, and `closes` for a PAC at the unit's last measure, adds the exact repeats as `CommonPracticeKeyRepeat`, and runs everything from the keys on again with them in `key_regions` (the chain owns the arrays). The keys then change only where a phrase establishes the new key, and repeats read the same (R-400). It runs only with the form stage, `refine_keys` and `phrase_grid`; off, the chain is as before it. The third pass reads the chords after the pivot of an arrival change in the key they cadence into (R-399): the new region's numerals start at the segment after the pivot, when that segment starts before the region. On the grid the key changes are confirmed after the claim, by the cadences claimed, not before it. `tests/analysis/test_reference_keys.c` asserts Clementi 36's regions C 1-7, G 8-15, C minor 16-23, C 24-38 and Mozart 545's keys, with the repeats read the same and 124 of 146 measures as annotated (84.9%; 106, 72.6%, before).

## Applied chords

With `AnalysisChainConfig.applied_chords` (default on, with spans on or off, R-380) the chord identification also ranks the key's applied chords (V/x, V7/x, vii°/x, vii°7/x; `docs/modules/common-practice-minimal-weighted.md`) and `read_numerals` gives the Roman numerals `recheck_diatonic = false`: an identified applied chord is named directly (`standard_tonality_read_applied_chord`, linked to its resolution when it resolves) and no diatonic reading is relabelled afterwards. The switch reaches the chords stage only: the grouping, the key segments and the key regions keep the diatonic vocabulary, so the keys do not read an applied chord as a key. Off: diatonic chords and the re-check, as before. Known limits: borrowed chords, the Neapolitan and augmented sixths are not read; a major key's vii°7 of I reads as vii°7/vi.

## Harmonic spans and the bass

Segmentation cuts at every change of sounding notes, far finer than the rate the music changes chords. With `AnalysisChainConfig.harmonic_spans` (default on, the switch for measuring the change) and a stage that needs chords, `prepare` groups the segments into harmonic spans before any key region exists, so grouping takes no key (R-372): `chain_build_span_cells` reads each segment's measure and its metric level (standard-meter: downbeat BAR, the secondary beat HALF_BAR, another beat BEAT, anything else INNER), `harmonic_span_basses` reads each cell's bass (R-370), `harmonic_spans_group` partitions each measure (the framework README gives the rule and its two constants), and `chain_merge_segments` replaces the segments with one per span: the union of its cells' sounding notes, a note held when it started before the span, ids `seg-<n>` renumbered, the members rebuilt. The outer-voice lines are then built on the spans. Every stage after the chords sees one chord per span.

Each span's `CommonPracticeSegment` carries the bass the grouping read the span over (`has_bass`, `bass_pitch`, R-373: the lowest sounding pitch of its first cell, or the step an accented bass tone resolves to); it is never recomputed. The chord identification then requires the chord to hold that bass (`require_bass_chord_tone`) and charges a seventh `ANALYSIS_CHAIN_SPAN_SEVENTH_PENALTY` (0.15, R-376: a seventh outranks its own triad only when its seventh pitch class carries more than 15% of the weight), and the Roman numeral inputs read the inversion from the same bass. Grouping and the accent marking take the same seventh penalty (R-379). `harmonic_span_mark_resolved_accents` then flags, in each span, the accented tones that resolve by step over a held bass (R-382, needs voice ids, which the MIDI importer gives by track): the chords and leftover labels skip them, and the span's chords never read the next span's chord (`context_next` false), but the key segments count the tones (`weigh_resolved_accents`, R-386). The key regions keep the weighting config as given. With `harmonic_spans` off the chain is as before: one chord per segment.

## Ownership and order

`AnalysisChain` owns every result and every input it built; `analysis_chain_destroy` frees them. The notes, their events and the key events are the caller's. A builder's output borrows from its inputs (segments, lines, map) as its header says.

Known behavior, kept from the test glue the chain replaced (deleted in P22 B5; the chain's results on four reference pieces are fixed as expected values in `chain/tests/test_analysis_chain.c`):

- The notes are read in the order of `analysis_note_compare` (onset, pitch, id). The glue read them in the importer's order, which differs among simultaneous notes. Segments, keys, chords, numerals, outer lines, cadences, phrases and the rest come out equal; sums over a segment's members run in another order, so scores and uncertainties agree to within rounding (1e-9, below the library's own tie tolerance), and the few voice assignments that depend on the order of simultaneous notes can differ (texture).
- The outer lines are extracted on the segmentation that keeps every melody note (`analysis_chain_outer_voices`, one entry per voice segment). The melody and meter stages read them there (R-377): on harmonic spans a chord segment holds many melody notes, so a line sampled at each span's start keeps one note per span and loses the rest of the melody (and the soprano notes the meter's syncopation reads). The melody line takes each note's beat strength and non-chord tone from the chord segment that holds it (`chain_build_melody_line`). The harmony, galant and form builders index lines by chord segment, so the chain maps them with `chain_build_segment_lines`: entry i is the entry of the voice segment sounding at the start of chord segment i, absent when none does (`analysis_chain_segment_outer_voices`). The form input reads them too (P41 B2, R-377): `chain_build_form_input` takes the voice segments and the lines on them, so the soprano of the form input is the outer line on the voice segments, one note per soprano note; its segments and the cadences stay on the chord segments.

## Tests

The library's tests run analysis through the chain: `tests/analysis/weighted_pipeline.h` runs the segments and outer-voice stages, `tests/accuracy/chorale_chain.c` the keys, chords and numerals, `tests/integration/framework_v1_chain.h` the stages to the form, and `chain/tests/test_analysis_chain.c` every stage on Bach 114, Mozart 545, Clementi 36 and Field 1 against fixed expected results (a digest per stage). A test that feeds a stage an annotation's answer builds that stage's input with the builders in `chain_inputs.h`; `tests/integration/hand_cadences.h` turns annotated cadences into the cadences the builders take.

## Kernel owners (`ursatz_chain/kernel_owners.h`)

With `AnalysisChainConfig.inject_kernel_owners` (default on) the chain hands the kernel owners to the frameworks, which cannot call L8: `chain_classify_tones` (`motive_tone_roles_line`) is the `NonChordToneClassifier` of the weighted chord identifier and of the melody stage (`chain_relabel_melody_line`), and `chain_match_motive_relation` (`motive_variant_recognize` at depth 1: exact, transpose, invert, retrograde, augment 2:1, diminish 1:2, and a leading or trailing fragment) is the `StandardFormRelationMatchFn` (`relation_match`) of the motive stage. Turn it off to get the frameworks' own fallback rules. A classifier or matcher already set in `weighting` or `form` is kept.
