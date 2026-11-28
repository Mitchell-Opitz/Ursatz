# Harmonic tension (R-764)

`theory/harmony/chord_value.h` gives the value of a sonority after Hindemith (The Craft of Musical Composition, Book I chapter III sections 2 to 12 and chapter IV section 3): the chord group Ia to VI it belongs to, ranked 1 (least tension) to 12 (most). `DIMENSION_TENSION` ([dimension.md](dimension.md)) measures a span with it.

## The tables

Two C tables are the identity of two K4 vocabulary subtypes in `knowledge/vocabulary/hindemith.json`; the test `vocabulary_entries` pins each row to its entry and each entry to its row.

- `CHORD_VALUE_SERIES_TWO[12]` (`interval-class`): Series 2 from the octave to the tritone, each with its semitones, rank and root (the lower tone of the fifth, major third, minor third and minor and major seventh; the upper tone of the fourth, minor and major sixth, and the major and minor second; none for the tritone).
- `CHORD_VALUE_GROUPS[12]` (`chord-group`): Ia, Ib, IIa, IIb1, IIb2, IIb3, IIIa, IIIb, IVa, IVb, V, VI, ranked 1 to 12, each with its group number, the tritones it holds (none, one, several, any), its sharp intervals (none, mild, sharp, any) and where its root lies (bass, above, none, any: IIb3 has a root in either place).

## Classifying a sonority

`chord_value_classify(pitches, count, &value)` takes MIDI pitches, the lowest the bass, and gives `group` (the code), `rank`, `root_pitch_class` (-1 for none) and `root_in_bass`. A pitch class sounding twice counts once, by its lowest pitch; fewer than three pitch classes is refused.

- Intervals are read as sounding, from the lower tone to the upper, reduced into the octave. A tritone is counted; a minor second or major seventh makes the chord sharp, a major second or minor seventh mild.
- The root is the root of the best interval by Series 2 (the lower one when the best occurs twice).
- An augmented triad, or three tones that are two superposed fourths as sounding (C F Bb, not C D G), is group V; a diminished triad or seventh is group VI; neither has a root.
- No tritone: a major or minor triad with no other tone is group I, any other chord group III. With a minor second or major seventh and a tritone: group IV. With one tritone and only mild intervals: group II, IIb2 when the root is above the bass, IIa when it is in the bass and the only mild intervals are minor sevenths, else IIb1; with several tritones IIb3. Groups I, III and IV are a with the root in the bass, b above it.

## The measure

`measure_tension` cuts a span's notes at every onset and release, classifies each sonority of three or more pitch classes, and gives the mean of the ranks weighted by the length of each sonority as a Rational: a block I-IV-V-I in root position is 1, a bar of V7 is 3, a bar half C E G and half B D F is 13/2. A span with no such sonority is `DIMENSION_ERR_INVALID_INPUT`. `dimension_contrast` does not use it (tension is not a contrast aspect, as brightness is not).
