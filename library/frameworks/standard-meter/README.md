# standard-meter

Theory Framework instance `standard-meter-v1`: how Western staff
notation organizes rhythm into meter. Path `frameworks/standard-meter/`,
tests in `frameworks/standard-meter/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/standard-rhythm/` and `frameworks/standard-notation/`. It
depends on `time/` types (`NoteValue`, `Duration`, `TimeSignature`), not
on standard-rhythm.

## Scope

Simple and compound meter, built on `TimeSignature`
(`src/time/time_signature.h`: a top number and an undotted bottom unit --
the two numbers and nothing more; this framework reads them):

- **Meter class** (`meter_class_rules.h`): top 2, 3, 4 is simple and 6, 9,
  12 is compound, each duple, triple, or quadruple (six categories). Any
  other top number is unclassified, not invalid. A simple top counts
  beats; a compound top counts divisions, three to a beat, so the beat is
  a dotted note (a dotted quarter over 8, a dotted half over 4, a dotted
  eighth over 16). A simple beat divides into 2 then 4, a compound beat
  into 3 then 6. The usual compound bottom numbers are 4, 8, 16.
- **Beat grid** (`beat_grid.h`): where beats fall in a measure, from the
  meter class, so beaming, ties, and counting work for both.

- **Beats and grouping** (`grouping_rules.h`): the beat divides into
  two, then four; 2, 3, 4 beats per group are duple, triple, quadruple;
  beat 1 is the downbeat and the last beat the upbeat.
- **Time signatures** (`time_signature_rules.h`): a simple meter's top
  number is 2, 3, or 4; the bottom number is usually 2, 4, or 8 (1 and 16
  also occur); common time is 4/4 and cut time is 2/2; a time signature
  is not a fraction (2/4 and 4/8 hold the same length but differ).
- **Measures** (`measure_rules.h`): a measure is one beat group; a
  multi-measure rest of n measures lasts n times the measure.
- **Anacrusis** (`anacrusis_rules.h`): a pickup is shorter than a
  measure, is counted as the end of an imaginary measure, and shortens
  the final measure by its length.
- **Counting** (`counting_rules.h`): American traditional counting by
  position in the beat -- simple "1 e + a" with "ta" one level finer,
  compound "1 ta la ta li ta" -- and parentheses for beats not said
  aloud. A dot parenthesizes a beat in simple meter but not in compound,
  where the dotted note is the beat.
- **Beaming** (`beaming_rules.h`): only eighth notes and shorter are
  beamed; how many of a value make a beat depends on the time signature;
  a beam group stays inside one beat, so sixteenths beam in sixes in 6/8
  and in threes in 6/16.
- **Borrowed divisions** (`borrowed_rules.h`): a triplet in a simple
  meter and a duplet in a compound meter divide the beat the other way,
  "borrowed" from the other class. A tuplet's metric level (beat,
  subdivision, multi-beat) comes from its span against the beat; triplets
  are typical at any level and duplets at the beat or a subdivision, as a
  description, not a prohibition. Counting borrows the other class's
  syllables: a triplet is "1 la li", a duplet "1 +".
- **Hypermeter** (`hypermeter_rules.h`): groups of measures counted like
  beats (duple, triple, quadruple hypermeter); the first is accented, and
  in quadruple the third also is. Pure counting on a measure index. It also
  states how a phrase is heard in hypermeasures: a length of 2 or 4 measures
  scored by how whole hypermeasures fill the phrase and hold the cadence's
  arrival in the last one (`LENGTH`), how an expansion changes a hypermeasure
  (`EXPANSION`), and how an elided cadence counts twice, "4 = 1" (`ELISION`).
- **Syncopation** (`syncopation_rules.h`): off-beat accents, made by a
  note sustained from off the beat across the next beat line (ties and
  dots), a rest on the beat with notes only off it, or an accent on an
  off-beat note. Inputs are plain positions, lengths, and an accent flag.
  `THROUGH_STRONGER` is the general form on metric levels: a note attacked at
  a weaker level and held through a stronger one, its strength the difference
  of the two levels.
- **Hemiola** (`hemiola_rules.h`): six pulses grouped 3 + 3 and heard 2 + 2
  + 2. It stands in a simple triple meter (two measures of 3/4 or 3/2) and in
  6/8 (one measure), and is read from three kinds of evidence, the attacks,
  the chord changes and the long-note accents, of which a minimum must agree.
- **Metric position** (`metric_position_rules.h`): see [standard-meter-metric-position.md](../../docs/modules/standard-meter-metric-position.md).
- **Segmentation beat grid** (`segmentation_beat_grid.h`): the standard
  `BeatGridProvider` for adaptive sonority segmentation
  (`semantics/sonority_segmentation.h`, which cannot include a framework, so
  the beat grid is injected). It returns the beat starts of a measure from
  the meter class's beat unit, in the signature the kernel says the measure
  is read in: 6/8 gives two dotted-quarter beats, 3/4 three quarter-note
  beats, 12/8 four dotted-quarter beats, 5/8 five eighth-note pulses. A short
  measure's last beat is cut at the barline, and a pickup's beats fall where
  a full measure's would, so a pickup starting partway through a beat begins
  with a partial beat.
- **Beat ties** (`beat_tie_rules.h`): a note starting off the beat that
  runs past the next beat is split and tied there to show the beat.

Stem and flag direction for beamed and flagged notes are in
`frameworks/standard-notation/`.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.

## Accented beats

`standard_meter_accent_beats` (`metric_position_rules.h`) lists the beats of a bar that read strong or medium by `standard_meter_beat_strength`: beat 0 in every meter, and beat 2 in a quadruple meter (4/4, 12/8). It works for irregular meters too, where every bottom-unit pulse is a beat. Owner for the Composer's accented harmony cells.

## Not covered

Irregular meter groupings (5/8, 7/8 are read as bottom-unit pulses; a 2+3 or
3+2 grouping is not defined), partial beams for
mixed groupings (including the common partial-beam variations), the
vocal-music beaming convention, conducting patterns (the same for simple
and compound),
and engraving craft. That simple quadruple can feel like two groups of two
is a perceptual ambiguity and is not asserted as a rule. Counting is
American only; the syllables are a table so another system can be added.

## Analysis of real music

`analysis/` reads real music against these rules: syncopation per line, hemiola, hypermeter inferred from phrase and cadence placement, and a rhythmic density profile per phrase. The input is neutral structs the caller fills; standard-meter includes no other framework. See `docs/modules/rhythm-meter-analysis.md`.

Two adapters carry the rules the Analyzer used to hold (R-217, P23 B1):

- `analysis/meter_analysis_check.h`: the meter check. Input: `StandardMeterCheckEvidence` (harmonic-change times, cadence-arrival times and every note onset, in whole notes on `Rational`) and the `MeterMap`. The effective reading is the map's first full measure; the candidates are 2/4, 3/4, 4/4, 3/8, 6/8, 9/8 and 12/8 at scale 1/2, 1 and 2 with each beat as a pickup, scored from the share of changes on downbeats (harmonic mean with the share of downbeats carrying a change), on beats, and of arrivals on downbeats; a secondary downbeat of a four-beat meter counts half. The best candidate with different barlines is offered when it beats the effective reading by `STANDARD_METER_CHECK_MIN_MARGIN` (0.15); on equal score and barlines the one nearest an eighth by the common onset gap is shown, then the quarter-beat signature. `standard_meter_check_changes` finds the change times from labels and regions. An exact time that does not fit 64 bits is `STANDARD_METER_ANALYSIS_ERR_OVERFLOW`. Claim type `standard-meter/meter-check`, read back with `standard_meter_check_claim_read`.
- `analysis/meter_analysis_tuplet.h`: tuplets and duplets from an `AnalysisNoteSet`. Onsets are read on a 96-step grid within a quarter step (rubato is left out); a triplet is an attack or distance off the 1/32 grid, a duplet is two halves of a compound beat; runs are per ratio, a single attack is dropped, and the order is smaller `actual`, measure, beat. `in_line` selects the notes to read. Claim type `standard-meter/tuplet`, read back with `standard_meter_tuplet_claim_read`.

## Neutral analysis input

`standard_meter_input_add_analysis_note` (`analysis/meter_analysis_input.h`) takes an `AnalysisNote` (`semantics/analysis_note.h`) field for field: `line` is the note's voice (0 when it has none) and `pitch` is its `midi`. A note with an unknown pitch is rejected. The caller no longer writes the converter.
