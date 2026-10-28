# Motive relations (analysis, L8)

Files: `src/analysis/motive_sketch.h/.c`, `src/analysis/motive_relation.h/.c`. Tests: `tests/analysis/test_motive_sketch.c`, `tests/analysis/test_motive_relation.c`. Tone labels for a sketch's notes: `docs/modules/motive-tone-roles.md`. Part of `analysis` (see `docs/modules/analysis.md`); includes only `time/exact_rational.h` and C headers.

A theory-neutral catalog of the ways a motive can be made into a variant of itself, one relation per operation of a composer's variant registry, with an `apply` over a plain value. The variant recognizer searches chains of these relations. A registry row maps to a relation by name, with the same parameters in the same order. Citations are Schoenberg, *Fundamentals of Musical Composition*.

## MotiveSketch

A motive as plain data, never allocated.

| Field | Meaning |
|---|---|
| `notes` (up to 64) | `pitch` (MIDI 0 to 127, or `MOTIVE_SKETCH_PITCH_UNKNOWN` for a rhythm sketch), exact `onset` and `duration` |
| `has_scale`, `tonic_pc`, `minor` | an optional scale: major, or natural minor |
| `bar`, `beat` | lengths of a bar and a beat, in whole notes |
| `pickup` | length before the first downbeat; 0 when the sketch starts on one, below `bar` otherwise |
| `length` | the end of the last note |
| `chords` (up to 8) | `onset`, `degree` 1 to 7, `alteration` -1/0/1, `inversion` 0 to 3, `seventh` |
| `features` (up to 8) | `label` 1 to 8 characters, `first`, `last` (0-based note indices, inclusive) |
| `accompaniment` | the sketch is an accompaniment, not a melody |

Times are `ExactRational`. An overflow is the bad value `{0, 0}`, which the `msk_` helpers pass on and `motive_sketch_validate` refuses. `motive_sketch_validate` also checks ascending, non-overlapping onsets, a pickup below the bar, a length not before the last note's end, chords ascending inside the length, and features inside the notes. `motive_sketch_equal` compares everything the counts say is in use.

Helpers:

- `motive_sketch_step` / `motive_sketch_pitch_at_step`: the scale step of a pitch (seven to an octave, counted from MIDI 0, so middle C is step 35 in C). A pitch outside the scale is on the step below it, raised. Without a scale the step is the pitch itself.
- `motive_sketch_chord_tones`: the pitch classes of a chord stacked in thirds from its root in the scale. An altered chord is a major triad (a dominant seventh) on the altered root. In minor, degree 5 has its third raised.
- `motive_sketch_expressible`: a duration a note list can write, w h q e s t with up to two dots. `motive_sketch_on_grid`: an onset on the 1/32 grid.
- `motive_sketch_strength`: 2 on the downbeat, 1 on another beat, 0 off the beat, from the pickup and the bar.

## MotiveRelation

`motive_relation_apply(in, step, out)` applies one `MotiveRelationStep` (a kind, up to 4 `value`s, and a `den` for each ratio). It never changes `in`, writes `out` only on `MOTIVE_RELATION_OK`, and makes the result as long as its notes. A relation other than `exact` whose result is the input is `UNCHANGED`.

Results: `OK`, `NOT_APPLICABLE` (nothing to do, or a rule refuses), `UNCHANGED`, `RANGE` (a pitch outside MIDI 0 to 127), `NOT_EXPRESSIBLE` (a duration or onset that cannot be written), `NO_SCALE`, `NO_HARMONY`, `NO_FEATURE`, `INVALID` (the sketch or the step is malformed).

Cost (used for a chain's distance): exact 0; transpose, augment, diminish 1; change-meter 3; every other relation 2. A relation that needs pitches (transpose, invert, interval-change, add-interval, ancillary-fill, the harmony relations and the frame relations) is `NOT_APPLICABLE` on a rhythm sketch.

### The nine development relations

They match the Composer's `motive_develop` at `466b38c`. A fragment or liquidation shorter than its minimum is `NOT_APPLICABLE`.

| Relation | Parameters | Rule, refusals | Citation | Cost |
|---|---|---|---|---|
| `exact` | none | the source again | ch. III, Ex. 14 | 0 |
| `transpose` | tonal, by, degree | semitones, or scale steps when tonal; a degree moves the first note to that scale degree by the nearest way (up to three steps); `NO_SCALE` for tonal | ch. III, Exs. 14, 28 | 1 |
| `invert` | axis (-1 the first note), tonal | mirrored about the axis | ch. III, Exs. 14, 19 | 2 |
| `retrograde` | none | each note's span mirrored in time, notes reversed | ch. III, Exs. 14, 19 | 2 |
| `interval-change` | interval (0 every one), amount, keep-contour | the interval widened or narrowed, later notes carried along; a repeated note is left alone; keep-contour refuses a changed direction | ch. III, Ex. 21 | 2 |
| `fragment` | position (1 leading, 2 trailing, 3 middle), length, min | a run of notes, moved to start at 0 | ch. III, Ex. 21; ch. VIII, Ex. 52 | 2 |
| `liquidate` | drop, keep-rhythm, min | the weakest notes go first (off the beat, then beats, then downbeats; the later of equals); kept notes keep their onsets, or follow one another from 0 | ch. VIII, Ex. 52 | 2 |
| `augment`, `diminish` | ratio (0:0 for 2:1 and 1:2) | onsets and durations scaled; every duration must be writable and every onset on the grid, and a source of whole beats stays whole beats | ch. III, Exs. 14, 17 | 1 |

### Rhythm (ch. III)

| Relation | Parameters | Rule, refusals | Citation | Cost |
|---|---|---|---|---|
| `note-repeat` | note, count 2 to 4 | one note split into equal repeated notes; `NOT_EXPRESSIBLE` when a part cannot be written | Exs. 17, 18 | 2 |
| `rhythm-repeat` | first, length, target | the durations of notes first.. copied onto the notes from target, laid end to end; the notes after keep their distance | Exs. 22 to 24 | 2 |
| `rhythm-shift` | offset | the motive starts later in the bar: the pickup becomes (pickup - offset) mod bar, the notes stay; a multiple of a bar is `NOT_APPLICABLE` | Exs. 22 to 24 | 2 |
| `add-upbeat` | count 1 to 3, approach +1 below / -1 above | half-beat notes stepping into the first note; the pickup grows by their length, and `NOT_APPLICABLE` when it reaches a bar; the first chord keeps covering them | Ex. 22 | 2 |
| `change-meter` | num, den | each old bar becomes one new bar, times scaled by new bar / old bar; `NOT_EXPRESSIBLE` when a value cannot be written | Ex. 23 | 3 |

### Intervals (ch. III and VIII)

| Relation | Parameters | Rule, refusals | Citation | Cost |
|---|---|---|---|---|
| `add-interval` | interval, steps | a note after the note before the interval, steps from it, taking the second half of its duration | Exs. 18 to 23 | 2 |
| `omit-interval` | interval | the note after the interval goes, the note before it lengthened to hold its time; needs 3 notes | Exs. 18 to 23 | 2 |
| `ancillary-fill` | interval, kind (0 passing, 1 neighbor) | passing notes fill a leap by steps, or one neighbor (upper, lower when descending) is added; the added notes share the second half of the first note | Exs. 18 to 23 | 2 |
| `condense` | first, last, ratio below 1 | notes first..last compressed, later notes moved earlier | ch. VIII, Ex. 52 | 2 |
| `repeat-feature` | feature, times 1 or 2, steps | the feature's notes repeated right after it, each repeat moved by steps (a sequence), later notes moved later; `NO_FEATURE` for a feature that does not exist | Exs. 18 to 23 | 2 |
| `shift-feature` | feature, offset | the feature moved later, the note before it lengthened and its last note shortened; `NOT_APPLICABLE` when the last note is not longer than the offset | Exs. 18 to 23 | 2 |

### Harmony and melody adaptation (ch. V, Exs. 25 to 29)

A harmony relation needs a scale (`NO_SCALE`). A sketch without chords is given its best implied chord at 0 first (the triad holding the most notes, then the most time, the first of I V IV vi ii iii vii on a tie), so the result has chords. Then the melody is adapted to the chords that changed, through the tone roles of the melody before the change (`docs/modules/motive-tone-roles.md`):

- A note that was a chord tone and is not one of its new chord moves to the nearest tone of the new chord within an octave. Of two equally near, the one in the direction from the previous note comes first (up for the first note). The first of the nearest that keeps the contour's directions is taken, and if none within six semitones does, the nearest.
- A non-chord tone whose resolution target moved moves by the same scale steps, so it still resolves by step to the moved target (a pair figure moves both notes). A non-chord tone whose figure no longer fits after that is left where it is and is unclassified in the result.
- A note that was unclassified is left. A chord that already holds the notes changes only the chords.

`motive_relation_harmony_changes_notes(in, step)` tells whether a step succeeds and changes at least one pitch; the Composer's draws use it to prefer harmony steps a reader can hear.

`harmony-inversion` also moves the bass: when the lowest note that starts inside the chord was the old bass chord tone, it moves to the nearest pitch of the new bass chord tone (the lower when two are equally near).

| Relation | Parameters | Rule, refusals | Cost |
|---|---|---|---|
| `harmony-inversion` | chord, inversion 0 to 3 | the chord's inversion; 3 needs a seventh | 2 |
| `harmony-append` | degree, seventh | a chord at the last note's onset; `NOT_APPLICABLE` when the last chord is already there | 2 |
| `harmony-insert` | chord, degree | the chord's span split at the note onset nearest its middle | 2 |
| `harmony-substitute` | chord, degree, then (0 none) | the chord replaced by degree, or by degree then a second degree split at the same point | 2 |
| `passing-harmony` | chord | a chord a step between two chords' roots (the dominant of the second when the roots are a step apart) on the weak note before the change | 2 |
| `counter-line` | none | for an accompaniment: repeated notes inside one chord step toward the nearest tone of the next chord (of its own, in the last) | 2 |
| `dominant-form` | none | I becomes V; I-V, I-IV, I-II and any other harmony starting on I become V-I; I-V-I becomes V-I-V (ch. V's table). The melody moves a tonal fourth down, or a fifth up when that is out of range, keeping rhythm and contour, and is then adapted | 2 |

### Liquidation by features (ch. VIII, R-144)

`liquidate-features` (steps 1 to 3, shorten): removes the steps most characteristic features one by one. A feature's weight is its leaps (steps of 3 or more, semitones without a scale) plus its notes whose duration occurs only once in the motive. The heaviest goes first, the later of equals. A removed feature is replaced by one note of its span on the nearest chord tone of its chord (the implied chord when there are none). Only a feature of two notes or more can go. Without labelled features each beat group is a feature, and a sketch that starts with labelled features keeps to them. With shorten the result is diminished 1:2. Cost 2.

### Frame relations (R-197)

Five relations that work from the tone roles (`motive-tone-roles.md`): they need a scale and pitches (`NO_SCALE`, `NOT_APPLICABLE` otherwise), and a sketch without chords is read against the tonic triad of its scale. Cost 2 each.

| Relation | Parameters | Effect |
|---|---|---|
| `reduce-to-frame` | none | Every note that is not a chord tone (non-chord or unclassified) goes. Its time goes to the kept note before it; a leading one's time goes to the next kept note, which then starts earlier. A feature keeps its kept notes and goes when it has none. `NOT_APPLICABLE` when no chord tone is left, `UNCHANGED` when every note is one, `NOT_EXPRESSIBLE` when a kept note's grown duration is not a written value. |
| `add-non-chord-tone` | after, type | `after` is the 0-based index of a chord tone with a note after it; `type` is a `NonChordTone` value. The figure goes between that note and the next, taking half of the earlier note's duration (a double neighbor or changing tone is two notes sharing that half). `INVALID` for an index or type out of range, `NOT_APPLICABLE` for a last note, a note that is not a chord tone, or a type that cannot be formed there, `NOT_EXPRESSIBLE` when the shared time is not a written value. |
| `remove-non-chord-tone` | note | The note goes, its time as in `reduce-to-frame`. `NOT_APPLICABLE` for a chord tone. A note that is unclassified can go too. Removing one of a pair leaves the other. |
| `change-non-chord-tone` | note, type | Re-pitches the note (a pair type: the pair, so the note and its neighbour) to form the new type between the same chord tones. `UNCHANGED` for the type it already has, `NOT_APPLICABLE` for a chord tone, a first or last note, or a type that cannot be formed. Changing a double neighbor or changing tone to a single type re-pitches the note asked for and leaves its partner as it is. |
| `reharmonize-frame` | degree, seventh | Every chord is replaced by the one chord at 0. The melody is adapted as for a harmony relation: chord tones move to the nearest tone of the new chord, a non-chord tone follows the target it resolves to, one whose figure no longer fits is left and is unclassified. |

A figure is formed from the notes either side of it: the pitch is the first of a fixed list of candidates (scale steps next to the note before and the note after, then those notes' own pitches, then two steps away; for a pair, the scale steps above and below the note it embellishes in either order, and for a double neighbor the semitones above and below) for which the tone roles of the result label the note with that type. The rules of `motive-tone-roles.md` decide, so a type they can only tell by context is formed only where that context is: a suspension or retardation needs a chord change at the figure's onset, an anticipation one where the next note starts, a double neighbor (a pair less than a third apart) uses chromatic neighbours, and a pedal is never formed by one note.

## Notes

- Spelling: `motive_sketch_step` gives a pitch on a scale degree that degree unraised (F in C major is step 3, alteration 0). Only a pitch on no degree is the degree below it raised (F# is step 3, alteration 1). Exact degree first, then the lowest degree a half step below, as the Composer's `degree_of`; natural minor.
- Pickup: `augment` and `diminish` scale the pickup with the notes (`pickup x factor`) and refuse a result whose pickup reaches the bar.
- `motive_sketch_contour_normalize` writes a sketch as its contour ranks (see `motive-variant-recognition.md`).

- The relation order of `transpose` follows the Composer's registry row (tonal, by, degree).
- A fragment or liquidation does not change the pickup: the result is a sketch that starts at its first note, as the Composer's motive does.
- An applied dominant is a diatonic chord on the degree a fifth above the second root: a chord is a degree, an alteration, an inversion and a seventh, with no quality.
