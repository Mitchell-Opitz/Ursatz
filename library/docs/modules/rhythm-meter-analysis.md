# Rhythm and meter analysis (standard-meter, L3.8)

[Docs index](../README.md): the Ursatz docs by topic.

Adapter in `frameworks/standard-meter/analysis/` (`meter_analysis_*.c`). It reads syncopation, hemiola, hypermeter and per-phrase rhythmic density from real music against the rules of `standard-meter`. The adapter includes no other framework and no kernel analysis; the architecture test scans `analysis/` as well as the framework.

Time is `Rational` and `score_time_*` (`time/score_time.h`). `meter_analysis_position.c` holds the pickup-aware beat grid, `standard_meter_level_at` and `standard_meter_beat_of` (a `double` beat), since they read the framework's own beat grid rather than the signature's unit.

Every result follows the durable claim pattern: an Interpretation with FrameworkReference standard-meter (author `standard-meter-v1`, scalar confidence 1 − uncertainty, status machine-generated) that targets a note, a Structure shell over notes, a Relationship, or a phrase.

## Input

`StandardMeterInput` (`meter_analysis_input.h`) is built with `standard_meter_input_init`, `_add_note`, `_add_phrase`, `_add_chord_change` and released with `_destroy`. Ids are copied; the `TimeSpan`s, `MusicalTime`s and the `MeterMap` are borrowed and must outlive the input and every result.

| Struct | Fields |
|---|---|
| note | id, `TimeSpan` (onset to release; a tie is one long note the caller merges), pitch (semitone number), `line` (any whole number the caller picks) |
| phrase | id, `TimeSpan`, cadence-arrival `TimeSpan` or none, expansion kind (none, internal, external, contraction) and measures, elided-with-next flag |
| chord change | id, `MusicalTime`; optional |

`chain_build_meter_input` (`ursatz_chain/chain_inputs.h`) builds it from the import, the L2 outer-voice lines (soprano 0, bass 1, other 2), the harmonic segments and the standard-form phrases.

`StandardMeterConfig`:

- `syncopation_all_notes` (default false: per line);
- `hemiola_min_evidence` (2);
- `onset_grid_denominator` (32), `onset_tolerance_denominator` (256), `release_tolerance_denominator` (64).

### Snap

An attack within 1/256 of a multiple of 1/32 is read as that multiple, and a release within 1/64 is. The wider release tolerance carries a performed file's legato overlap (a note run on into the next) and staccato lift to the written end. A time on another grid (a triplet) is not moved. Exact input is unchanged.

## Syncopation

Rule: `THROUGH_STRONGER` (`syncopation_rules.h`). A note is syncopated when it is attacked at a weaker metric level and sustained through a stronger level, and no note of its stream is attacked at that stronger position.

- Levels, strongest first: downbeat, secondary beat, beat, off-beat, finer (`metric_position_rules.h`). Positions examined are the beat starts and the beat's divisions (halves in simple meter, thirds in compound); a position must be strictly inside the note.
- Strength = attack level − sustained level, 1 to 4. An off-beat note held over a beat is 1; a beat note held over the barline is 2; an off-beat note held over the barline is 3.
- A note is reported once, at the strongest position it is held through.
- Stream: one line at a time, or all notes as one stream (`syncopation_all_notes`), where a position another line attacks does not count as sustained through.
- Claim `standard-meter/syncopation`: line, measure and beat of the attack, both levels, strength, the measure and beat held through. Uncertainty 0.1 when the onset moved in the snap, otherwise 0.

The REST and ACCENT forms of syncopation in `syncopation_rules.h` are not detected: the input has neither rests nor dynamics.

## Hemiola

Rule: `hemiola_rules.h`. Six pulses grouped 3 + 3 are heard 2 + 2 + 2.

- Meter: two measures of a simple triple meter (3/4, 3/2; the pulse is the beat) or one measure of 6/8 (the pulse is the eighth). 12/8, 4/4 and the irregular meters have no hemiola here. Pickups and short measures are not read.
- Evidence, over the window's pulses 0 to 5, all notes as one stream:
  - duration: attacks on pulses 2 and 4 and none on 1, 3, 5;
  - chord: chord changes on 2 and 4 and none on 1, 3, 5 (not available when the input has no chord changes);
  - accent: long notes (two pulses or more) begin on 2 and 4, and no note is attacked on 3.
- An attack that is not on a pulse is ignored by all three. It is a hemiola when at least `hemiola_min_evidence` of the available types agree and the number available is at least that minimum, so one type alone is never a hemiola.
- Windows do not overlap. The claim `standard-meter/hemiola` targets a Structure shell over the notes attacked on the window's pulses; it carries the first and last measure and the evidence. Uncertainty is 1 − agreeing/3.

## Hypermeter

Rules: `hypermeter_rules.h` (`LENGTH`, `EXPANSION`, `ELISION`, with the counting rules). Each phrase is read on its own, in measures counted from the MeterMap; a pickup measure is not counted.

- Length: 2 or 4 measures per hypermeasure. The score of a length is the mean of two fits, each 0 to 1: how much of the phrase whole hypermeasures cover, and whether the cadence arrival is in the last hypermeasure (an elided cadence scores 1 when the phrase is a whole number of hypermeasures, 0.5 when not; no cadence omits the fit). The best score wins, the longer length on a tie. The score is the confidence of every hypermeasure in the phrase (uncertainty 1 − score).
- Expansion: with an expansion from the form adapter the phrase is first read without its added measures. An internal expansion adds hyperbeats to the last hypermeasure (kind expanded), an external one adds a hypermeasure of its own after the cadence (extension). A contraction is read with the measures it lacks, and the last hypermeasure is that many shorter.
- A remainder (a phrase of 3 measures in 2s) is a short hypermeasure.
- Elision: a phrase with `elided_with_next` and a next phrase gets a reinterpretation: its last hyperbeat is the next phrase's first, "4 = 1" after a 4-measure hypermeasure. Elision is taken from that flag only.
- Output: a Relationship per hypermeasure from `standard-meter-measure-<first>` to `standard-meter-measure-<last>` (predicate `hypermeasure`) with an Interpretation `standard-meter/hypermeasure` (phrase, index, measures, nominal length, kind, extra hyperbeats, cadence inside, score). A reinterpretation is a Relationship between the two phrase ids (`hypermetric-reinterpretation`) with the Interpretation `standard-meter/hypermetric-reinterpretation`. The hyperdownbeats are the first measures of the hypermeasures; the per-phrase result gives the chosen length and score.

## Density profile

One profile per phrase for all its notes (line −1) and one per line that has a note in it. A note belongs to a phrase when its snapped onset is in the phrase's span. The claim `standard-meter/density` targets the phrase id with uncertainty 0.

- `notes`, and `attacks` (distinct onsets, a chord is one attack);
- attacks per beat: for each measure the phrase touches, the attacks in the phrase's part of the measure over the beats that part spans (the measure's reading, so a dotted quarter in 6/8); the mean over the measures, and the largest;
- mean inter-onset interval in whole notes (0 with fewer than two attacks);
- on-beat share and off-beat share of the attacks;
- duration histogram as exact rationals, ascending, counting notes. At most 16 distinct durations are kept, the shortest first, with `bins_truncated` set when some are left out.

## Limits

- Hypermeter depends on the accuracy of the phrases given. Detected phrases (L3.6) move hyperdownbeats with them; an elided cadence is not detected by the form adapter, so none is reported on its phrases.
- A hypermeasure is 2 or 4 measures (the irregular ones come only from expansions); 3-measure and longer hypermeasures are not chosen.
- Hemiola has no 12/8 or quadruple reading, and no chord-only reading.
- Syncopation reads durations as given: it cannot tell a performed long note from a tie, and it does not read rests or accents.
- No tuplet or borrowed-division detection yet (Analyzer A5.2 reports them from notated input); a triplet is read on the grid it falls on.
- Field 5 (`tests/fixtures/annotations/field5_rhythm.txt`) is a draft hand reading, unchecked against an edition.
