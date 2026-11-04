# Melody analysis

`frameworks/standard-counterpoint/analysis/` reads one melodic line and reports its intervals, contour, climax, leap recovery and non-chord tones as durable claims. There is no melody framework: the melodic rules (steps and leaps, leap recovery, a single climax) are standard-counterpoint's, so the adapter is here. standard-counterpoint includes no other framework; the chain from the outer voices to this adapter is `chain_build_melody_line` in `chain/`.

## Input: the melody line

One entry per position in time order, each a note or a rest (`analysis_melody.h`). A rest breaks the chain: no interval is formed across it. A note held across segments is one entry (the caller merges them); a repeated pitch is a second entry. A note has an id, its pitch (a whole-semitone `PitchNumericValue`), and optionally a `TimeSpan`, a `MetricalPosition`, a `BeatStrength`, a `PitchSpelling`, a scale degree (1 to 7 and a flat or sharp alteration), the note's non-chord-tone result with a reference, and a confidence. Build it with `standard_counterpoint_melody_line_init`, `_add_note`, `_add_rest`, `_destroy`.

## Intervals and contour

For each pair of consecutive notes the adapter computes the signed semitones, the direction (up, down, same) and the generic size. The kernel `Contour` and `Interval` take direction and size from the caller (a `PitchIdentity` is an opaque token with no tuning), so the adapter computes them from the numeric pitches and supplies them. Each interval is a note-id `Structure` shell, an `Interval` and a `Contour` value, and an `Interpretation` (claim `standard-counterpoint/melodic-interval`) targeting the shell.

The generic size comes from the spellings when both notes have one, else from the scale degrees (the letter distance in the direction of motion plus the octaves the semitones imply), else it is absent. With none, leap and step are read from the semitones (3 or more is a leap, 1 or 2 a step), the interval is marked approximate, and its uncertainty is raised by a penalty (default 0.25). An augmented second is then read as a leap.

Uncertainty = 1 − (1 − u)(1 − penalty), u = 1 − the lesser note confidence.

Per span, a contour summary (`standard-counterpoint/contour-summary`) counts the up, down and same intervals and the steps and leaps inside it, and gives the net direction and semitones, the range, and the lowest and highest notes (the first note at each pitch).

## Spans

Phrases do not exist yet. A span is a run of entries `[first, last]` the caller supplies; the default is the whole line as one span. When phrase segmentation exists its phrases are passed as the spans.

## Climax

The highest pitch of the span, the definition of `melodic_rules.h`. The claim (`standard-counterpoint/melodic-climax`) records the pitch, the first note at it, how many notes are at it and which (repeated top notes are listed, not collapsed), whether it is unique, its relative position among the span's notes (rank over notes − 1; by note order, not time), the metric strength of the first occurrence, whether it is approached by a leap, and whether it is the span's last note.

## Leap recovery and consecutive leaps

The existing cantus firmus rules. A leap is a third or larger. A leap of a fourth or larger must be followed by a step in the opposite direction. Status per leap (`standard-counterpoint/leap-recovery`): `recovered`, `unrecovered`, `not-required` (a third), or `not-judged` (a fourth or larger with nothing after it, which the rule does not judge). Consecutive leaps (`standard-counterpoint/consecutive-leaps`) are flagged for two in the same direction (unless two thirds outlining a perfect fifth) and for three in a row. Both are judged along the whole line, not per span.

## Non-chord tones

Attached, not derived: each note carries the result of the chord identification (chord tone, a `NonChordTone`, or unclassified) and a reference string naming its source. One claim per note (`standard-counterpoint/melodic-nct`) holds the status, type and reference. A note the identification did not cover is `unclassified`. Per span, a count claim (`standard-counterpoint/nct-counts`) gives the chord tones, unclassified notes and non-chord tones by type.

## Limits

- No phrases yet: spans are caller-supplied, and the fixture's are the two halves of the strain.
- The results depend on the outer-line extraction: a wrong soprano gives wrong intervals.
- The rule set is counterpoint-derived (cantus firmus rules), not a model of free melody: a leap of a fourth or larger left unrecovered is a fault of strict counterpoint, not of a minuet.
- Without spellings or degrees the leap/step split is approximate.
- The relative position of the climax is by note order, not by time.
- No motive detection, no rhythm analysis, no scale-degree tendency rules.
