# Schema and sequence analysis (galant-schemas and scale-harmonization, L3.10)

[Docs index](../README.md): the Ursatz docs by topic.

Two adapters over one input. Schema matching is in `frameworks/galant-schemas/analysis/` (`galant_analysis_*.c`) and may include only `standard-counterpoint`. Sequence detection is in `frameworks/scale-harmonization/analysis/` (`scale_analysis_*.c`) and may include only `galant-schemas` and `standard-counterpoint`. Both architecture tests scan `analysis/`. No prototype, kind or rule is added: the adapters use the 26 prototypes of `galant_prototypes.h`, the patterns of `scale_patterns.h` and their rules.

Every result is a Relationship over its segments with an Interpretation (FrameworkReference of the framework, scalar confidence = 1 − uncertainty, author `<framework>-v1`) targeting the Relationship's id.

## Input

`GalantSegmentList` (`galant_analysis_input.h`): `galant_segments_init`, `galant_segments_add`, `galant_segments_destroy`. Ids are copied; `TimeSpan`s and `MetricalPosition`s are borrowed. Segments are in time order.

| Field | Meaning |
|---|---|
| id, span | the segment ref and its `TimeSpan` |
| position | the `MetricalPosition` of its start (optional) |
| strength | `BeatStrength`: STRONG (downbeat), MEDIUM (secondary beat), WEAK |
| beat | the length of a beat, in whole notes |
| key_id, key | the local key's ref and its tonic as a degree and alteration of the home key (`1 0` is the home key; in C major, A minor is `6 0`) |
| bass, soprano | degrees in the local key with the alteration against the major scale (a minor third is `3 -1`), or absent |
| numeral | root degree of the sounding root in the local key (an applied V/V has root 2), the caller's quality code, seventh flag, inversion 0–3, applied-to degree |
| uncertainty | 0 to 1 |

`tests/integration/schema_util.h` builds it from the outer voices (L2.6), the local key regions (L2.8), the Roman numerals (L3.1) and the meter; consecutive segments with the same key, degrees and numeral become one.

Degrees are letter-based: `galant_degree_plus` and `galant_degree_minus` add and subtract them exactly (the same convention as `galant_degree.h`), so a degree of a local key is read in the home key or in any other key by their tonics.

## Schema matching

`galant_schema_match(segments, config)`; config `max_gap_beats` (2) and `bass_only_allowance` (1).

- **Stages.** Each prototype stage takes a segment. The melody and bass degrees are checked with `galant_degree_allows` (accepted alterations count). A prototype with a defining bass requires it; a non-defining bass (the Meyer) may differ, at a credit of 0.75.
- **Stage meter.** A STRONG or MEDIUM segment is on a strong beat, a WEAK one on a weak beat. It must be the beat the prototype gives the stage.
- **Two-note stages** (the Monte): the next segment must keep the bass degree and have the second melody pitch.
- **Gaps.** Embellishing segments may lie between stages: the time from the end of one stage to the start of the next is at most `max_gap_beats` beats (counted in the beat of the earlier segment).
- **Metric fit.** 1 when each stage is at least as strong as every segment between it and the stage before, 0.8 otherwise.
- **Bass-only stage.** A segment with no soprano may take a stage on the bass alone, at most `bass_only_allowance` times per match, at credit 0.5.
- **Key-relative stages.** Stages are read in one frame key: the home key or the key of a segment within reach. A schema that moves through keys (the Fonte begins in the key of ii, the Monte climbs through IV and V) is read as its prototype has it, in the key it is written in. The frame is recorded in the claim.
- **Cross-check.** The stages are assembled as a `GalantExemplar` (on a C tonic) and kept only if `galant_matching_check` finds no hard violation.
- **Confidence** = pair share × metric fit × (1 − mean uncertainty of the segments used). Pair share is the mean credit of the stages (1, 0.75 or 0.5 as above).
- **Overlaps.** Matches that share a segment are resolved by confidence (more stages, then earlier, break ties). One boundary segment may be shared — the last stage of an earlier match and the first of a later one — when the earlier prototype's `SchemaFunction` does not come after the later's (opening gambit, continuation, pre-cadential, cadence, post-cadential).
- **Claim** `galant-schemas/schema`: name, function, the stage list (segment refs, degrees as read, credits), frame, and the confidence parts. The Relationship runs from the first stage's segment to the last, predicate `galant-schema`.

## Sequence detection

`scale_sequence_detect(segments, config)`; config `minimum_units` (3, at least 2).

- **Model and copy.** A sequence is a model of one or more chord segments repeated, transposed by a constant diatonic interval, at least `minimum_units` times. Chord segments are those with a bass; a segment with none ends a run. Degrees are read in the home key.
- **Kind.** The sequence is checked against each pattern of `scale_patterns.h` with `scale_sequence_check`, the soprano as the upper voice, the degrees spelled in a C frame (the bass to the nearest octave of its next degree, the soprano above it). A pattern whose hard rules hold over whole units gives the kind (parallel chords, 5-6, 7-6) and the claim is rule-backed. A run is cut to whole units.
- **Cycle of fifths.** scale-harmonization encodes no cycle pattern, so there is no rule to check. A model of two chords whose roots (from the numerals, applied chords included) each descend a fifth, with the same bass interval in each unit, is reported as the existing `SEQUENCE_KIND_CYCLE_OF_FIFTHS` with `rule_backed` false. Ascending fifths and other root motions have no kind and are not reported.
- **Comparing units.** Unit k against unit 0 by its root-motion pattern (when every chord has a numeral) and its bass-interval pattern; the unit share is the share that match. The soprano's steps repeating with the unit's period add 0.1.
- **Confidence** = unit share × (1 − mean uncertainty) + soprano bonus, at most 1.
- **Overlaps.** More units, then higher confidence, then rule-backed, then earlier. A run is cut at 32 stages.
- **Claim** `scale-harmonization/sequence`: kind, pattern or −1, rule-backed flag, direction, unit length and count, transposition in diatonic steps between unit starts, and the stage list (segment refs and bass, soprano and root degrees in the home key).

## Limits

- Everything depends on the outer-voice, key-region and numeral accuracy of the input. The whole chain on the fixture and the reference pieces is reported in the tests.
- The prototypes are the 26 of galant-schemas and the sequence patterns those of scale-harmonization; nothing else is found. The cycle of fifths is the one kind without a rule.
- A sequence is read from the bass and the soprano. A texture whose lowest upper voice is not the soprano may not match a 5-6 or 7-6 pattern.
- A sequence starts on the first stage of a pattern's unit.
- Minor-key roots of applied chords are approximated by the major-scale degrees.
- The soft TENTHS rule of the Prinner is not applied: outer-voice degrees give generic intervals only.
- Melodic-only sequences (motive transposition) are L3.7's. The rule of the octave is not checked here.
