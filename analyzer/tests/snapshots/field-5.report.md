# Analysis Report: Field 5.mid

## Summary

- **File:** Field 5.mid, 1083 notes.
- **Input:** MIDI file. Meter: embedded in the file. Key signature: embedded in the file. Pitch spelling: by rule.
- **Key:** Bb major, the key with the most time across 6 key regions.
- **Chords:** 82 chord events; 82 matched a chord (100%), 0 did not.
- **Most frequent chords:** I (30), dominant7 (8), diminished (6).

## Meter

- **Time signature:** 12/8
- **Measures:** 43 measures.
- **Pickup:** None.

## Key regions

Positions are measure and beat. Uncertainty is the library's, from 0 (clearly this key) to 1 (another key fits as well or better). The home key is the key with the most time across its regions.

- **Home key:** Bb major.
- **Opening key:** Bb major.
- **Closing key:** Bb major.
- **Notated key signatures:** Bb major or G minor at m. 1, beat 1.

| Position | Key | Uncertainty | Set by |
| --- | --- | --- | --- |
| m. 1, beat 1 to m. 6, beat 1 | Bb major | 0.92 | key signature |
| m. 6, beat 1 to m. 8, beat 1 | D minor | 1.00 | profile |
| m. 8, beat 1 to m. 27, beat 1 | Bb major | 0.67 | profile |
| m. 27, beat 1 to m. 29, beat 1 | D minor | 0.59 | profile |
| m. 29, beat 1 to m. 32, beat 1 | F major | 0.81 | profile |
| m. 32, beat 1 to m. 44, beat 1 | Bb major | 0.60 | profile |

## Tonicizations

2 brief excursions of another key that return to the key before. Span is the stretch in the target key. The library tells a tonicization from a modulation by how long the new key lasts, whether it returns, and whether a cadence arrives in it; without a cadence it is a tonicization however long.

| Span | In | Target |
| --- | --- | --- |
| m. 6, beat 1 to m. 8, beat 1 | Bb major | D minor |
| m. 29, beat 1 to m. 32, beat 1 | D minor | F major |

## Modulations

A modulation is a lasting new key. Position is where the new key arrives. Type is pivot when the library found a pivot chord and direct when it did not. A return goes back to the key before an excursion. The library gives the change's extent from the length of the new key, whether it returns, and whether a cadence arrives in it.

| Position | From | To | Extent | Type | Return |
| --- | --- | --- | --- | --- | --- |
| m. 32, beat 1 | F major | Bb major | modulation | pivot | no |

## Pivot chords

The chord at a modulation that is diatonic in both the old and the new key, named by root and quality, with no Roman numeral. Modulation is its row in the Modulations table, counting from 1.

| Position | Chord | Bass | Inversion | From | To | Modulation |
| --- | --- | --- | --- | --- | --- | --- |
| m. 31, beat 4 to m. 32, beat 1 | D minor | F | first | F major | Bb major | 1 |

## Chords

Positions are measure and beat. Numeral is the Roman numeral in the local key region, which is named where it changes. Function is the library's harmonic function of the chord: an applied chord is a dominant toward its target degree, and a predominant is strong (IV, ii) or weak (iii, vi). Six-four is the library's type for a chord in second inversion. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Numeral | Function | Six-four | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 3 | I | I (in Bb major) | tonic | - | Bb | major | Bb | root position | 2 | 1.00 | - |
| m. 1, beat 3 to m. 2, beat 1 | diminished | vii°6/4/iii | dominant of 3 | - | Db | diminished | G | second | 2 | 0.00 | diminished (E diminished, score 0.91) |
| m. 2, beat 1 to m. 3, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 4 | 0.14 | - |
| m. 3, beat 1 to m. 3, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 2 | 1.00 | - |
| m. 3, beat 3 to m. 3, beat 4 | diminished7 | vii°7/vi | dominant of 6 | - | Gb | diminished7 | Gb | root position | 1 | 0.56 | - |
| m. 3, beat 4 to m. 4, beat 1 | vii° | vii° | dominant | - | A | diminished | A | root position | 1 | 1.00 | - |
| m. 4, beat 1 to m. 5, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 4 | 0.50 | - |
| m. 5, beat 1 to m. 5, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 2 | 1.00 | - |
| m. 5, beat 3 to m. 6, beat 1 | major | V/iii | dominant of 3 | - | A | major | A | root position | 2 | 0.77 | - |
| m. 6, beat 1 to m. 6, beat 3 | minor | i (in D minor) | tonic | - | D | minor | D | root position | 2 | 1.00 | - |
| m. 6, beat 3 to m. 6, beat 4 | major | V6/4/V | dominant of 5 | - | E | major | B | second | 1 | 0.49 | - |
| m. 6, beat 4 to m. 7, beat 1 | diminished7 | viio4/2/VII | dominant of 7 | - | B | diminished7 | Ab | third | 1 | 1.00 | - |
| m. 7, beat 1 to m. 7, beat 4 | major | VII | predominant (weak) | - | C | major | C | root position | 3 | 0.33 | - |
| m. 7, beat 4 to m. 8, beat 1 | diminished | iio6 | predominant (strong) | - | E | diminished | G | first | 1 | 0.61 | - |
| m. 8, beat 1 to m. 8, beat 3 | V | V (in Bb major) | dominant | - | F | major | Eb | - | 2 | 0.47 | - |
| m. 8, beat 3 to m. 8, beat 4 | I | I6 | tonic | - | Bb | major | D | first | 1 | 0.33 | - |
| m. 8, beat 4 to m. 9, beat 1 | dominant7 | V4/3 | dominant | - | F | dominant7 | C | second | 1 | 0.05 | vii° (A diminished, score 0.82) |
| m. 9, beat 1 to m. 9, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 2 | 1.00 | - |
| m. 9, beat 3 to m. 10, beat 1 | diminished | vii°6/V | dominant of 5 | - | E | diminished | G | first | 2 | 0.28 | - |
| m. 10, beat 1 to m. 11, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 4 | 0.14 | - |
| m. 11, beat 1 to m. 11, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 2 | 1.00 | - |
| m. 11, beat 3 to m. 11, beat 4 | diminished7 | vii°7/vi | dominant of 6 | - | Gb | diminished7 | Gb | root position | 1 | 0.56 | - |
| m. 11, beat 4 to m. 12, beat 1 | vii° | vii° | dominant | - | A | diminished | A | root position | 1 | 1.00 | - |
| m. 12, beat 1 to m. 13, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 4 | 0.50 | - |
| m. 13, beat 1 to m. 13, beat 3 | IV | IV6 | predominant (strong) | - | Eb | major | G | first | 2 | 0.71 | - |
| m. 13, beat 3 to m. 14, beat 1 | I | I6/4 | tonic | - | Bb | major | F | second | 2 | 0.12 | - |
| m. 14, beat 1 to m. 14, beat 4 | major | V6/V | dominant of 5 | - | C | major | E | first | 3 | 0.30 | - |
| m. 14, beat 4 to m. 15, beat 1 | vii° | vii°6/4 | dominant | - | A | diminished | Eb | second | 1 | 0.01 | dominant7 (F dominant7, score 0.83) |
| m. 15, beat 1 to m. 15, beat 2 | I | I6 | tonic | - | Bb | major | D | first | 1 | 0.08 | iii (D minor, score 0.96) |
| m. 15, beat 2 to m. 15, beat 3 | ii | ii6 | predominant (strong) | - | C | minor | Eb | first | 1 | 0.30 | - |
| m. 15, beat 3 to m. 15, beat 4 | I | I6/4 | dominant | cadential | Bb | major | F | second | 1 | 0.14 | - |
| m. 15, beat 4 to m. 16, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 1 | 0.89 | - |
| m. 16, beat 1 to m. 16, beat 4 | diminished | vii°6/iii | dominant of 3 | - | Db | diminished | E | first | 3 | 0.06 | diminished7 (E diminished7, score 0.90) |
| m. 16, beat 4 to m. 17, beat 1 | diminished7 | vii°4/2/vi | dominant of 6 | - | Gb | diminished7 | Eb | third | 1 | 0.01 | vii° (A diminished, score 0.84) |
| m. 17, beat 1 to m. 18, beat 1 | I | I6 | tonic | - | Bb | major | D | first | 4 | 0.16 | - |
| m. 18, beat 1 to m. 19, beat 1 | I | I6/4 | tonic | - | Bb | major | Bb | root position | 4 | 0.15 | - |
| m. 19, beat 1 to m. 20, beat 1 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 4 | 0.24 | - |
| m. 20, beat 1 to m. 21, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 4 | 0.15 | - |
| m. 21, beat 1 to m. 22, beat 1 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 4 | 0.24 | - |
| m. 22, beat 1 to m. 23, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 4 | 1.00 | - |
| m. 23, beat 1 to m. 23, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 2 | 1.00 | - |
| m. 23, beat 3 to m. 24, beat 1 | diminished | vii°6/4/iii | dominant of 3 | - | Db | diminished | G | second | 2 | 0.00 | diminished (E diminished, score 0.91) |
| m. 24, beat 1 to m. 25, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 4 | 0.14 | - |
| m. 25, beat 1 to m. 25, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 2 | 0.33 | - |
| m. 25, beat 3 to m. 25, beat 4 | diminished7 | vii°7/vi | dominant of 6 | - | Gb | diminished7 | Gb | root position | 1 | 0.57 | - |
| m. 25, beat 4 to m. 26, beat 1 | vii° | vii° | dominant | - | A | diminished | A | root position | 1 | 1.00 | - |
| m. 26, beat 1 to m. 27, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 4 | 0.52 | - |
| m. 27, beat 1 to m. 27, beat 3 | major | VI (in D minor) | predominant (weak) | - | Bb | major | Bb | root position | 2 | 1.00 | - |
| m. 27, beat 3 to m. 28, beat 1 | major | V | dominant | - | A | major | A | root position | 2 | 0.62 | - |
| m. 28, beat 1 to m. 28, beat 3 | minor | i | tonic | - | D | minor | D | root position | 2 | 1.00 | - |
| m. 28, beat 3 to m. 29, beat 1 | minor7 | iv6/5 | predominant (strong) | - | G | minor7 | Bb | first | 2 | 0.06 | half-diminished7 (E half-diminished7, score 0.65) |
| m. 29, beat 1 to m. 29, beat 4 | minor7 | vi4/2 (in F major) | predominant (weak) | - | D | minor7 | C | third | 3 | 0.01 | I (F major, score 0.81) |
| m. 29, beat 4 to m. 30, beat 1 | dominant7 | V4/3 | dominant | - | C | dominant7 | G | second | 1 | 0.02 | vii° (E diminished, score 0.87) |
| m. 30, beat 1 to m. 30, beat 3 | I | I | tonic | - | F | major | Eb | - | 2 | 1.00 | - |
| m. 30, beat 3 to m. 30, beat 4 | IV | IV6 | predominant (strong) | - | Bb | major | D | first | 1 | 0.33 | - |
| m. 30, beat 4 to m. 31, beat 1 | dominant7 | V4/3/IV | dominant of 4 | - | F | dominant7 | C | second | 1 | 0.76 | - |
| m. 31, beat 1 to m. 31, beat 3 | dominant7 | V6/5/V | dominant of 5 | - | G | dominant7 | B | first | 2 | 0.15 | - |
| m. 31, beat 3 to m. 31, beat 4 | dominant7 | V4/3/IV | dominant of 4 | - | F | dominant7 | C | second | 1 | 0.65 | - |
| m. 31, beat 4 to m. 32, beat 1 | vi | vi6 | predominant (weak) | - | D | minor | F | first | 1 | 0.34 | - |
| m. 32, beat 1 to m. 32, beat 2 | ii | ii6/4 (in Bb major) | predominant (strong) | - | C | minor | G | second | 1 | 0.58 | - |
| m. 32, beat 2 to m. 32, beat 3 | dominant7 | V7/ii | dominant of 2 | - | G | dominant7 | G | root position | 1 | 0.35 | - |
| m. 32, beat 3 to m. 32, beat 4 | ii | ii | predominant (strong) | - | C | minor | C | root position | 1 | 1.00 | - |
| m. 32, beat 4 to m. 33, beat 1 | V | V6 | dominant | - | F | major | A | first | 1 | 0.47 | - |
| m. 33, beat 1 to m. 33, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 2 | 1.00 | - |
| m. 33, beat 3 to m. 34, beat 1 | ii | ii6 | predominant (strong) | - | C | minor | Eb | first | 2 | 0.41 | - |
| m. 34, beat 1 to m. 34, beat 3 | I | I6/4 | dominant | cadential | Bb | major | F | second | 2 | 0.71 | - |
| m. 34, beat 3 to m. 35, beat 1 | V | V | dominant | - | F | major | F | root position | 2 | 0.38 | - |
| m. 35, beat 1 to m. 35, beat 3 | IV | IV6 | predominant (strong) | - | Eb | major | Gb | - | 2 | 0.74 | - |
| m. 35, beat 3 to m. 36, beat 1 | I | I6/4 | tonic | - | Bb | major | F | second | 2 | 0.16 | - |
| m. 36, beat 1 to m. 36, beat 4 | dominant7 | V6/5/V | dominant of 5 | - | C | dominant7 | E | first | 3 | 0.14 | - |
| m. 36, beat 4 to m. 37, beat 1 | vii° | vii°6/4 | dominant | - | A | diminished | Eb | second | 1 | 0.01 | dominant7 (F dominant7, score 0.83) |
| m. 37, beat 1 to m. 37, beat 2 | diminished | vii°6/ii | dominant of 2 | - | B | diminished | D | first | 1 | 0.00 | diminished (D diminished, score 0.96) |
| m. 37, beat 2 to m. 37, beat 3 | ii | ii6 | predominant (strong) | - | C | minor | Eb | first | 1 | 0.26 | - |
| m. 37, beat 3 to m. 37, beat 4 | I | I6/4 | tonic | - | Bb | major | F | second | 1 | 0.14 | - |
| m. 37, beat 4 to m. 38, beat 1 | minor7 | iii6/5 | predominant (weak) | - | D | minor7 | F | first | 1 | 0.20 | - |
| m. 38, beat 1 to m. 39, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 4 | 0.15 | - |
| m. 39, beat 1 to m. 40, beat 1 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 4 | 0.24 | - |
| m. 40, beat 1 to m. 41, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 4 | 0.15 | - |
| m. 41, beat 1 to m. 42, beat 1 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 4 | 0.24 | - |
| m. 42, beat 1 to m. 42, beat 4 | I | I | tonic | - | Bb | major | Bb | root position | 3 | 0.14 | - |
| m. 42, beat 4 to m. 43, beat 1 | V | V | dominant | - | F | major | F | root position | 1 | 0.00 | iii (D minor, score 0.73) |
| m. 43, beat 1 to m. 44, beat 1 | I | I6/4 | tonic | - | Bb | major | Bb | root position | 4 | 0.14 | - |

## Applied chords

Chords the library reads as applied (secondary) chords: the chord as it reads in the tonicized key, the chord it tonicizes and that chord's tonic. The last column checks each against the tonicization claims above for the same span; where they disagree both are shown and neither is changed.

| Position | Applied chord | Target | Resolution | Tonicization |
| --- | --- | --- | --- | --- |
| m. 1, beat 3 to m. 2, beat 1 | vii°6/4 (secondary-leading-tone) | iii (D) | no resolution | DISAGREES: no tonicization is reported here |
| m. 3, beat 3 to m. 3, beat 4 | vii°7 (secondary-leading-tone) | vi (G) | no resolution | DISAGREES: no tonicization is reported here |
| m. 5, beat 3 to m. 6, beat 1 | V (secondary-dominant) | iii (D) | to the target | agrees: tonicization of D minor |
| m. 6, beat 3 to m. 6, beat 4 | V6/4 (secondary-dominant) | V (A) | no resolution | DISAGREES: the tonicization is of D minor |
| m. 6, beat 4 to m. 7, beat 1 | viio4/2 (secondary-leading-tone) | VII (C) | to the target | DISAGREES: the tonicization is of D minor |
| m. 9, beat 3 to m. 10, beat 1 | vii°6 (secondary-leading-tone) | V (F) | no resolution | DISAGREES: no tonicization is reported here |
| m. 11, beat 3 to m. 11, beat 4 | vii°7 (secondary-leading-tone) | vi (G) | no resolution | DISAGREES: no tonicization is reported here |
| m. 14, beat 1 to m. 14, beat 4 | V6 (secondary-dominant) | V (F) | no resolution | DISAGREES: no tonicization is reported here |
| m. 16, beat 1 to m. 16, beat 4 | vii°6 (secondary-leading-tone) | iii (D) | no resolution | DISAGREES: no tonicization is reported here |
| m. 16, beat 4 to m. 17, beat 1 | vii°4/2 (secondary-leading-tone) | vi (G) | no resolution | DISAGREES: no tonicization is reported here |
| m. 23, beat 3 to m. 24, beat 1 | vii°6/4 (secondary-leading-tone) | iii (D) | no resolution | DISAGREES: no tonicization is reported here |
| m. 25, beat 3 to m. 25, beat 4 | vii°7 (secondary-leading-tone) | vi (G) | no resolution | DISAGREES: no tonicization is reported here |
| m. 30, beat 4 to m. 31, beat 1 | V4/3 (secondary-dominant) | IV (Bb) | deceptive | DISAGREES: the tonicization is of F major |
| m. 31, beat 1 to m. 31, beat 3 | V6/5 (secondary-dominant) | V (C) | no resolution | DISAGREES: the tonicization is of F major |
| m. 31, beat 3 to m. 31, beat 4 | V4/3 (secondary-dominant) | IV (Bb) | no resolution | DISAGREES: the tonicization is of F major |
| m. 32, beat 2 to m. 32, beat 3 | V7 (secondary-dominant) | ii (C) | to the target | DISAGREES: no tonicization is reported here |
| m. 36, beat 1 to m. 36, beat 4 | V6/5 (secondary-dominant) | V (F) | no resolution | DISAGREES: no tonicization is reported here |
| m. 37, beat 1 to m. 37, beat 2 | vii°6 (secondary-leading-tone) | ii (C) | to the target | DISAGREES: no tonicization is reported here |
## Outer voices

The highest (soprano) and lowest (bass) sounding note of each chord's span, with its scale degree in that chord's local key. A flat or sharp sign marks a chromatic alteration. A degree is "-" where the span lies in no key region. A rest means the library found no note for that line in the span, a bridged one a short gap inside the line, and a register guard a note the library dropped rather than adopt an accompaniment note. Degrees carry the key region's uncertainty in the analysis document.

| Position | Key | Soprano | Degree | Bass | Degree |
| --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 3 | Bb major | F5 | 5 | Bb2 | 1 |
| m. 1, beat 3 to m. 2, beat 1 | Bb major | F5 | 5 | G3 | 6 |
| m. 2, beat 1 to m. 3, beat 1 | Bb major | G5 | 6 | Bb2 | 1 |
| m. 3, beat 1 to m. 3, beat 3 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 3, beat 3 to m. 3, beat 4 | Bb major | Eb5 | 4 | Gb3 | b6 |
| m. 3, beat 4 to m. 4, beat 1 | Bb major | Eb5 | 4 | A3 | 7 |
| m. 4, beat 1 to m. 5, beat 1 | Bb major | C5 | 2 | Bb2 | 1 |
| m. 5, beat 1 to m. 5, beat 3 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 5, beat 3 to m. 6, beat 1 | Bb major | E5 | #4 | A2 | 7 |
| m. 6, beat 1 to m. 6, beat 3 | D minor | F5 | 3 | D3 | 1 |
| m. 6, beat 3 to m. 6, beat 4 | D minor | E5 | 2 | B2 | #6 |
| m. 6, beat 4 to m. 7, beat 1 | D minor | D5 | 1 | Ab3 | #4 |
| m. 7, beat 1 to m. 7, beat 4 | D minor | C5 | 7 | C3 | 7 |
| m. 7, beat 4 to m. 8, beat 1 | D minor | E5 | 2 | G3 | 4 |
| m. 8, beat 1 to m. 8, beat 3 | Bb major | F5 | 5 | F3 | 5 |
| m. 8, beat 3 to m. 8, beat 4 | Bb major | Bb5 | 1 | D3 | 3 |
| m. 8, beat 4 to m. 9, beat 1 | Bb major | Eb5 | 4 | C3 | 2 |
| m. 9, beat 1 to m. 9, beat 3 | Bb major | F5 | 5 | Bb2 | 1 |
| m. 9, beat 3 to m. 10, beat 1 | Bb major | F5 | 5 | G3 | 6 |
| m. 10, beat 1 to m. 11, beat 1 | Bb major | G5 | 6 | Bb2 | 1 |
| m. 11, beat 1 to m. 11, beat 3 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 11, beat 3 to m. 11, beat 4 | Bb major | Eb5 | 4 | Gb3 | b6 |
| m. 11, beat 4 to m. 12, beat 1 | Bb major | Eb5 | 4 | A3 | 7 |
| m. 12, beat 1 to m. 13, beat 1 | Bb major | C5 | 2 | Bb2 | 1 |
| m. 13, beat 1 to m. 13, beat 3 | Bb major | Eb5 | 4 | G3 | 6 |
| m. 13, beat 3 to m. 14, beat 1 | Bb major | Eb5 | 4 | F3 | 5 |
| m. 14, beat 1 to m. 14, beat 4 | Bb major | Eb5 | 4 | E3 | #4 |
| m. 14, beat 4 to m. 15, beat 1 | Bb major | D5 | 3 | Eb3 | 4 |
| m. 15, beat 1 to m. 15, beat 2 | Bb major | F5 | 5 | D3 | 3 |
| m. 15, beat 2 to m. 15, beat 3 | Bb major | F5 | 5 | Eb3 | 4 |
| m. 15, beat 3 to m. 15, beat 4 | Bb major | C5 | 2 | F3 | 5 |
| m. 15, beat 4 to m. 16, beat 1 | Bb major | C5 | 2 | F3 | 5 |
| m. 16, beat 1 to m. 16, beat 4 | Bb major | Db5 | b3 | E3 | #4 |
| m. 16, beat 4 to m. 17, beat 1 | Bb major | Db5 | b3 | Eb3 | 4 |
| m. 17, beat 1 to m. 18, beat 1 | Bb major | F5 | 5 | D3 | 3 |
| m. 18, beat 1 to m. 19, beat 1 | Bb major | C5 | 2 | F3 | 5 |
| m. 19, beat 1 to m. 20, beat 1 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 20, beat 1 to m. 21, beat 1 | Bb major | Bb4 | 1 | Bb2 | 1 |
| m. 21, beat 1 to m. 22, beat 1 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 22, beat 1 to m. 23, beat 1 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 23, beat 1 to m. 23, beat 3 | Bb major | F5 | 5 | Bb2 | 1 |
| m. 23, beat 3 to m. 24, beat 1 | Bb major | F5 | 5 | G3 | 6 |
| m. 24, beat 1 to m. 25, beat 1 | Bb major | G5 | 6 | Bb2 | 1 |
| m. 25, beat 1 to m. 25, beat 3 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 25, beat 3 to m. 25, beat 4 | Bb major | Eb5 | 4 | Gb3 | b6 |
| m. 25, beat 4 to m. 26, beat 1 | Bb major | Eb5 | 4 | A3 | 7 |
| m. 26, beat 1 to m. 27, beat 1 | Bb major | Bb4 | 1 | Bb2 | 1 |
| m. 27, beat 1 to m. 27, beat 3 | D minor | D5 | 1 | Bb2 | 6 |
| m. 27, beat 3 to m. 28, beat 1 | D minor | E5 | 2 | A2 | 5 |
| m. 28, beat 1 to m. 28, beat 3 | D minor | F5 | 3 | D3 | 1 |
| m. 28, beat 3 to m. 29, beat 1 | D minor | Bb5 | 6 | Bb2 | 6 |
| m. 29, beat 1 to m. 29, beat 4 | F major | D5 | 6 | C3 | 5 |
| m. 29, beat 4 to m. 30, beat 1 | F major | E5 | 7 | G3 | 2 |
| m. 30, beat 1 to m. 30, beat 3 | F major | F5 | 1 | F3 | 1 |
| m. 30, beat 3 to m. 30, beat 4 | F major | F5 | 1 | D3 | 6 |
| m. 30, beat 4 to m. 31, beat 1 | F major | F5 | 1 | C3 | 5 |
| m. 31, beat 1 to m. 31, beat 3 | F major | F5 | 1 | B2 | #4 |
| m. 31, beat 3 to m. 31, beat 4 | F major | F5 | 1 | C3 | 5 |
| m. 31, beat 4 to m. 32, beat 1 | F major | Eb5 | b7 | F3 | 1 |
| m. 32, beat 1 to m. 32, beat 2 | Bb major | D5 | 3 | G3 | 6 |
| m. 32, beat 2 to m. 32, beat 3 | Bb major | D5 | 3 | G3 | 6 |
| m. 32, beat 3 to m. 32, beat 4 | Bb major | Eb5 | 4 | C4 | 2 |
| m. 32, beat 4 to m. 33, beat 1 | Bb major | F5 | 5 | A3 | 7 |
| m. 33, beat 1 to m. 33, beat 3 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 33, beat 3 to m. 34, beat 1 | Bb major | C5 | 2 | Eb3 | 4 |
| m. 34, beat 1 to m. 34, beat 3 | Bb major | Bb4 | 1 | F3 | 5 |
| m. 34, beat 3 to m. 35, beat 1 | Bb major | Bb4 | 1 | F3 | 5 |
| m. 35, beat 1 to m. 35, beat 3 | Bb major | E5 | #4 | G3 | 6 |
| m. 35, beat 3 to m. 36, beat 1 | Bb major | Eb5 | 4 | F3 | 5 |
| m. 36, beat 1 to m. 36, beat 4 | Bb major | Bb5 | 1 | E3 | #4 |
| m. 36, beat 4 to m. 37, beat 1 | Bb major | rest (register guard) | - | Eb3 | 4 |
| m. 37, beat 1 to m. 37, beat 2 | Bb major | F5 | 5 | D3 | 3 |
| m. 37, beat 2 to m. 37, beat 3 | Bb major | F5 | 5 | Eb3 | 4 |
| m. 37, beat 3 to m. 37, beat 4 | Bb major | C5 | 2 | F3 | 5 |
| m. 37, beat 4 to m. 38, beat 1 | Bb major | D5 | 3 | F3 | 5 |
| m. 38, beat 1 to m. 39, beat 1 | Bb major | C5 | 2 | Bb2 | 1 |
| m. 39, beat 1 to m. 40, beat 1 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 40, beat 1 to m. 41, beat 1 | Bb major | Bb4 | 1 | Bb2 | 1 |
| m. 41, beat 1 to m. 42, beat 1 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 42, beat 1 to m. 42, beat 4 | Bb major | Bb4 | 1 | Bb2 | 1 |
| m. 42, beat 4 to m. 43, beat 1 | Bb major | D5 | 3 | F2 | 5 |
| m. 43, beat 1 to m. 44, beat 1 | Bb major | Bb4 | 1 | F2 | 5 |

## Chord vocabulary

Each distinct Roman numeral of the chords the library numbered, for the whole piece and for each key region, sorted by count, then total length, then first appearance. Count is the number of chord rows with that numeral (every row in the Chords table is one). Runs counts a stretch of adjacent identical numerals in one key region once. Beats is the total length, Share its fraction of the scope's numbered chord length, and First where it first appears. Applied chords are listed on their own. Chords without a numeral are not counted.

### Overall

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 20 | 15 | 61 | 35.5% | m. 1, beat 1 |
| I6/4 | 7 | 7 | 16 | 9.3% | m. 13, beat 3 |
| I7 | 4 | 4 | 16 | 9.3% | m. 19, beat 1 |
| V | 4 | 4 | 7 | 4.1% | m. 8, beat 1 |
| I6 | 3 | 3 | 6 | 3.5% | m. 8, beat 3 |
| IV6 | 3 | 3 | 5 | 2.9% | m. 13, beat 1 |
| ii6 | 3 | 3 | 4 | 2.3% | m. 15, beat 2 |
| vii° | 3 | 3 | 3 | 1.7% | m. 3, beat 4 |
| i | 2 | 2 | 4 | 2.3% | m. 6, beat 1 |
| V4/3 | 2 | 2 | 2 | 1.2% | m. 8, beat 4 |
| vii°6/4 | 2 | 2 | 2 | 1.2% | m. 14, beat 4 |
| VII | 1 | 1 | 3 | 1.7% | m. 7, beat 1 |
| vi4/2 | 1 | 1 | 3 | 1.7% | m. 29, beat 1 |
| VI | 1 | 1 | 2 | 1.2% | m. 27, beat 1 |
| iv6/5 | 1 | 1 | 2 | 1.2% | m. 28, beat 3 |
| iio6 | 1 | 1 | 1 | 0.6% | m. 7, beat 4 |
| V7 | 1 | 1 | 1 | 0.6% | m. 15, beat 4 |
| vi6 | 1 | 1 | 1 | 0.6% | m. 31, beat 4 |
| ii6/4 | 1 | 1 | 1 | 0.6% | m. 32, beat 1 |
| ii | 1 | 1 | 1 | 0.6% | m. 32, beat 3 |
| V6 | 1 | 1 | 1 | 0.6% | m. 32, beat 4 |
| iii6/5 | 1 | 1 | 1 | 0.6% | m. 37, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 30 | 25 | 83 | 48.3% | m. 1, beat 1 |
| V | 5 | 5 | 8 | 4.7% | m. 8, beat 1 |
| ii | 5 | 5 | 6 | 3.5% | m. 15, beat 2 |
| vii° | 5 | 5 | 5 | 2.9% | m. 3, beat 4 |
| I7 | 4 | 4 | 16 | 9.3% | m. 19, beat 1 |
| IV | 3 | 3 | 5 | 2.9% | m. 13, beat 1 |
| V7 | 3 | 3 | 3 | 1.7% | m. 8, beat 4 |
| i | 2 | 2 | 4 | 2.3% | m. 6, beat 1 |
| VII | 1 | 1 | 3 | 1.7% | m. 7, beat 1 |
| vi7 | 1 | 1 | 3 | 1.7% | m. 29, beat 1 |
| VI | 1 | 1 | 2 | 1.2% | m. 27, beat 1 |
| iv7 | 1 | 1 | 2 | 1.2% | m. 28, beat 3 |
| iio | 1 | 1 | 1 | 0.6% | m. 7, beat 4 |
| vi | 1 | 1 | 1 | 0.6% | m. 31, beat 4 |
| iii7 | 1 | 1 | 1 | 0.6% | m. 37, beat 4 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°7/vi | 3 | 3 | 3 | 1.7% | m. 3, beat 3 |
| V6/5/V | 2 | 2 | 5 | 2.9% | m. 31, beat 1 |
| vii°6/4/iii | 2 | 2 | 4 | 2.3% | m. 1, beat 3 |
| V4/3/IV | 2 | 2 | 2 | 1.2% | m. 30, beat 4 |
| V6/V | 1 | 1 | 3 | 1.7% | m. 14, beat 1 |
| vii°6/iii | 1 | 1 | 3 | 1.7% | m. 16, beat 1 |
| V/iii | 1 | 1 | 2 | 1.2% | m. 5, beat 3 |
| vii°6/V | 1 | 1 | 2 | 1.2% | m. 9, beat 3 |
| V6/4/V | 1 | 1 | 1 | 0.6% | m. 6, beat 3 |
| viio4/2/VII | 1 | 1 | 1 | 0.6% | m. 6, beat 4 |
| vii°4/2/vi | 1 | 1 | 1 | 0.6% | m. 16, beat 4 |
| V7/ii | 1 | 1 | 1 | 0.6% | m. 32, beat 2 |
| vii°6/ii | 1 | 1 | 1 | 0.6% | m. 37, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°7/vi | 4 | 4 | 4 | 2.3% | m. 3, beat 3 |
| vii°/iii | 3 | 3 | 7 | 4.1% | m. 1, beat 3 |
| V7/V | 2 | 2 | 5 | 2.9% | m. 31, beat 1 |
| V/V | 2 | 2 | 4 | 2.3% | m. 6, beat 3 |
| V7/IV | 2 | 2 | 2 | 1.2% | m. 30, beat 4 |
| V/iii | 1 | 1 | 2 | 1.2% | m. 5, beat 3 |
| vii°/V | 1 | 1 | 2 | 1.2% | m. 9, beat 3 |
| viio7/VII | 1 | 1 | 1 | 0.6% | m. 6, beat 4 |
| V7/ii | 1 | 1 | 1 | 0.6% | m. 32, beat 2 |
| vii°/ii | 1 | 1 | 1 | 0.6% | m. 37, beat 1 |

### Bb major (region 0)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 5 | 3 | 14 | 70.0% | m. 1, beat 1 |
| vii° | 1 | 1 | 1 | 5.0% | m. 3, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 5 | 3 | 14 | 70.0% | m. 1, beat 1 |
| vii° | 1 | 1 | 1 | 5.0% | m. 3, beat 4 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°6/4/iii | 1 | 1 | 2 | 10.0% | m. 1, beat 3 |
| V/iii | 1 | 1 | 2 | 10.0% | m. 5, beat 3 |
| vii°7/vi | 1 | 1 | 1 | 5.0% | m. 3, beat 3 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°/iii | 1 | 1 | 2 | 10.0% | m. 1, beat 3 |
| V/iii | 1 | 1 | 2 | 10.0% | m. 5, beat 3 |
| vii°7/vi | 1 | 1 | 1 | 5.0% | m. 3, beat 3 |

### D minor (region 1)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| VII | 1 | 1 | 3 | 37.5% | m. 7, beat 1 |
| i | 1 | 1 | 2 | 25.0% | m. 6, beat 1 |
| iio6 | 1 | 1 | 1 | 12.5% | m. 7, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| VII | 1 | 1 | 3 | 37.5% | m. 7, beat 1 |
| i | 1 | 1 | 2 | 25.0% | m. 6, beat 1 |
| iio | 1 | 1 | 1 | 12.5% | m. 7, beat 4 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V6/4/V | 1 | 1 | 1 | 12.5% | m. 6, beat 3 |
| viio4/2/VII | 1 | 1 | 1 | 12.5% | m. 6, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V/V | 1 | 1 | 1 | 12.5% | m. 6, beat 3 |
| viio7/VII | 1 | 1 | 1 | 12.5% | m. 6, beat 4 |

### Bb major (region 2)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 10 | 7 | 32 | 42.1% | m. 9, beat 1 |
| I6/4 | 3 | 3 | 7 | 9.2% | m. 13, beat 3 |
| I6 | 3 | 3 | 6 | 7.9% | m. 8, beat 3 |
| I7 | 2 | 2 | 8 | 10.5% | m. 19, beat 1 |
| vii° | 2 | 2 | 2 | 2.6% | m. 11, beat 4 |
| V | 1 | 1 | 2 | 2.6% | m. 8, beat 1 |
| IV6 | 1 | 1 | 2 | 2.6% | m. 13, beat 1 |
| V4/3 | 1 | 1 | 1 | 1.3% | m. 8, beat 4 |
| vii°6/4 | 1 | 1 | 1 | 1.3% | m. 14, beat 4 |
| ii6 | 1 | 1 | 1 | 1.3% | m. 15, beat 2 |
| V7 | 1 | 1 | 1 | 1.3% | m. 15, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 16 | 13 | 45 | 59.2% | m. 8, beat 3 |
| vii° | 3 | 3 | 3 | 3.9% | m. 11, beat 4 |
| I7 | 2 | 2 | 8 | 10.5% | m. 19, beat 1 |
| V7 | 2 | 2 | 2 | 2.6% | m. 8, beat 4 |
| V | 1 | 1 | 2 | 2.6% | m. 8, beat 1 |
| IV | 1 | 1 | 2 | 2.6% | m. 13, beat 1 |
| ii | 1 | 1 | 1 | 1.3% | m. 15, beat 2 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°7/vi | 2 | 2 | 2 | 2.6% | m. 11, beat 3 |
| V6/V | 1 | 1 | 3 | 3.9% | m. 14, beat 1 |
| vii°6/iii | 1 | 1 | 3 | 3.9% | m. 16, beat 1 |
| vii°6/V | 1 | 1 | 2 | 2.6% | m. 9, beat 3 |
| vii°6/4/iii | 1 | 1 | 2 | 2.6% | m. 23, beat 3 |
| vii°4/2/vi | 1 | 1 | 1 | 1.3% | m. 16, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°7/vi | 3 | 3 | 3 | 3.9% | m. 11, beat 3 |
| vii°/iii | 2 | 2 | 5 | 6.6% | m. 16, beat 1 |
| V/V | 1 | 1 | 3 | 3.9% | m. 14, beat 1 |
| vii°/V | 1 | 1 | 2 | 2.6% | m. 9, beat 3 |

### D minor (region 3)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| VI | 1 | 1 | 2 | 25.0% | m. 27, beat 1 |
| V | 1 | 1 | 2 | 25.0% | m. 27, beat 3 |
| i | 1 | 1 | 2 | 25.0% | m. 28, beat 1 |
| iv6/5 | 1 | 1 | 2 | 25.0% | m. 28, beat 3 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| VI | 1 | 1 | 2 | 25.0% | m. 27, beat 1 |
| V | 1 | 1 | 2 | 25.0% | m. 27, beat 3 |
| i | 1 | 1 | 2 | 25.0% | m. 28, beat 1 |
| iv7 | 1 | 1 | 2 | 25.0% | m. 28, beat 3 |

### F major (region 4)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vi4/2 | 1 | 1 | 3 | 25.0% | m. 29, beat 1 |
| I | 1 | 1 | 2 | 16.7% | m. 30, beat 1 |
| V4/3 | 1 | 1 | 1 | 8.3% | m. 29, beat 4 |
| IV6 | 1 | 1 | 1 | 8.3% | m. 30, beat 3 |
| vi6 | 1 | 1 | 1 | 8.3% | m. 31, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vi7 | 1 | 1 | 3 | 25.0% | m. 29, beat 1 |
| I | 1 | 1 | 2 | 16.7% | m. 30, beat 1 |
| V7 | 1 | 1 | 1 | 8.3% | m. 29, beat 4 |
| IV | 1 | 1 | 1 | 8.3% | m. 30, beat 3 |
| vi | 1 | 1 | 1 | 8.3% | m. 31, beat 4 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V4/3/IV | 2 | 2 | 2 | 16.7% | m. 30, beat 4 |
| V6/5/V | 1 | 1 | 2 | 16.7% | m. 31, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/IV | 2 | 2 | 2 | 16.7% | m. 30, beat 4 |
| V7/V | 1 | 1 | 2 | 16.7% | m. 31, beat 1 |

### Bb major (region 5)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 4 | 4 | 13 | 27.1% | m. 33, beat 1 |
| I6/4 | 4 | 4 | 9 | 18.8% | m. 34, beat 1 |
| I7 | 2 | 2 | 8 | 16.7% | m. 39, beat 1 |
| ii6 | 2 | 2 | 3 | 6.2% | m. 33, beat 3 |
| V | 2 | 2 | 3 | 6.2% | m. 34, beat 3 |
| IV6 | 1 | 1 | 2 | 4.2% | m. 35, beat 1 |
| ii6/4 | 1 | 1 | 1 | 2.1% | m. 32, beat 1 |
| ii | 1 | 1 | 1 | 2.1% | m. 32, beat 3 |
| V6 | 1 | 1 | 1 | 2.1% | m. 32, beat 4 |
| vii°6/4 | 1 | 1 | 1 | 2.1% | m. 36, beat 4 |
| iii6/5 | 1 | 1 | 1 | 2.1% | m. 37, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 8 | 8 | 22 | 45.8% | m. 33, beat 1 |
| ii | 4 | 4 | 5 | 10.4% | m. 32, beat 1 |
| V | 3 | 3 | 4 | 8.3% | m. 32, beat 4 |
| I7 | 2 | 2 | 8 | 16.7% | m. 39, beat 1 |
| IV | 1 | 1 | 2 | 4.2% | m. 35, beat 1 |
| vii° | 1 | 1 | 1 | 2.1% | m. 36, beat 4 |
| iii7 | 1 | 1 | 1 | 2.1% | m. 37, beat 4 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V6/5/V | 1 | 1 | 3 | 6.2% | m. 36, beat 1 |
| V7/ii | 1 | 1 | 1 | 2.1% | m. 32, beat 2 |
| vii°6/ii | 1 | 1 | 1 | 2.1% | m. 37, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/V | 1 | 1 | 3 | 6.2% | m. 36, beat 1 |
| V7/ii | 1 | 1 | 1 | 2.1% | m. 32, beat 2 |
| vii°/ii | 1 | 1 | 1 | 2.1% | m. 37, beat 1 |

## Non-chord tones

271 notes sound against a chord without being one of its tones; 187 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 1, beat 3 to m. 2, beat 1 | 0 | unclassified | diminished |
| m. 1, beat 3 to m. 2, beat 1 | 6 | unclassified | diminished |
| m. 1, beat 3 to m. 2, beat 1 | 10 | unclassified | diminished |
| m. 1, beat 3 to m. 2, beat 1 | 14 | unclassified | diminished |
| m. 2, beat 1 to m. 3, beat 1 | 11 | unclassified | I |
| m. 3, beat 3 to m. 3, beat 4 | 30 | unclassified | diminished7 |
| m. 3, beat 3 to m. 3, beat 4 | 36 | suspension | diminished7 |
| m. 4, beat 1 to m. 5, beat 1 | 41 | unclassified | I |
| m. 4, beat 1 to m. 5, beat 1 | 53 | escape-tone | I |
| m. 5, beat 3 to m. 6, beat 1 | 67 | unclassified | major |
| m. 5, beat 3 to m. 6, beat 1 | 73 | unclassified | major |
| m. 6, beat 3 to m. 6, beat 4 | 85 | unclassified | major |
| m. 7, beat 1 to m. 7, beat 4 | 93 | unclassified | major |
| m. 7, beat 1 to m. 7, beat 4 | 96 | unclassified | major |
| m. 7, beat 1 to m. 7, beat 4 | 98 | unclassified | major |
| m. 7, beat 1 to m. 7, beat 4 | 100 | unclassified | major |
| m. 7, beat 4 to m. 8, beat 1 | 98 | unclassified | diminished |
| m. 8, beat 1 to m. 8, beat 3 | 102 | unclassified | V |
| m. 8, beat 1 to m. 8, beat 3 | 105 | unclassified | V |
| m. 8, beat 1 to m. 8, beat 3 | 110 | unclassified | V |
| m. 8, beat 3 to m. 8, beat 4 | 114 | suspension | I |
| m. 8, beat 4 to m. 9, beat 1 | 119 | unclassified | dominant7 |
| m. 8, beat 4 to m. 9, beat 1 | 125 | anticipation | dominant7 |
| m. 9, beat 3 to m. 10, beat 1 | 126 | unclassified | diminished |
| m. 9, beat 3 to m. 10, beat 1 | 136 | unclassified | diminished |
| m. 9, beat 3 to m. 10, beat 1 | 141 | unclassified | diminished |
| m. 10, beat 1 to m. 11, beat 1 | 139 | unclassified | I |
| m. 11, beat 3 to m. 11, beat 4 | 159 | unclassified | diminished7 |
| m. 11, beat 3 to m. 11, beat 4 | 165 | suspension | diminished7 |
| m. 12, beat 1 to m. 13, beat 1 | 170 | unclassified | I |
| m. 12, beat 1 to m. 13, beat 1 | 182 | escape-tone | I |
| m. 13, beat 3 to m. 14, beat 1 | 190 | unclassified | I |
| m. 13, beat 3 to m. 14, beat 1 | 203 | unclassified | I |
| m. 14, beat 1 to m. 14, beat 4 | 203 | unclassified | major |
| m. 14, beat 1 to m. 14, beat 4 | 205 | unclassified | major |
| m. 14, beat 1 to m. 14, beat 4 | 208 | unclassified | major |
| m. 14, beat 1 to m. 14, beat 4 | 212 | unclassified | major |
| m. 14, beat 1 to m. 14, beat 4 | 215 | unclassified | major |
| m. 14, beat 4 to m. 15, beat 1 | 215 | unclassified | vii° |
| m. 14, beat 4 to m. 15, beat 1 | 218 | unclassified | vii° |
| m. 14, beat 4 to m. 15, beat 1 | 219 | unclassified | vii° |
| m. 14, beat 4 to m. 15, beat 1 | 222 | neighbor | vii° |
| m. 15, beat 1 to m. 15, beat 2 | 223 | unclassified | I |
| m. 15, beat 2 to m. 15, beat 3 | 224 | unclassified | ii |
| m. 15, beat 2 to m. 15, beat 3 | 227 | unclassified | ii |
| m. 15, beat 3 to m. 15, beat 4 | 232 | suspension | I |
| m. 15, beat 3 to m. 15, beat 4 | 231 | unclassified | I |
| m. 15, beat 4 to m. 16, beat 1 | 233 | unclassified | dominant7 |
| m. 15, beat 4 to m. 16, beat 1 | 236 | suspension | dominant7 |
| m. 16, beat 1 to m. 16, beat 4 | 237 | unclassified | diminished |
| m. 16, beat 1 to m. 16, beat 4 | 244 | unclassified | diminished |
| m. 16, beat 1 to m. 16, beat 4 | 247 | unclassified | diminished |
| m. 16, beat 1 to m. 16, beat 4 | 250 | unclassified | diminished |
| m. 16, beat 4 to m. 17, beat 1 | 241 | unclassified | diminished7 |
| m. 16, beat 4 to m. 17, beat 1 | 250 | suspension | diminished7 |
| m. 17, beat 1 to m. 18, beat 1 | 260 | unclassified | I |
| m. 17, beat 1 to m. 18, beat 1 | 259 | unclassified | I |
| m. 17, beat 1 to m. 18, beat 1 | 261 | unclassified | I |
| m. 17, beat 1 to m. 18, beat 1 | 263 | unclassified | I |
| m. 17, beat 1 to m. 18, beat 1 | 262 | unclassified | I |
| m. 17, beat 1 to m. 18, beat 1 | 270 | unclassified | I |
| m. 17, beat 1 to m. 18, beat 1 | 272 | unclassified | I |
| m. 17, beat 1 to m. 18, beat 1 | 271 | unclassified | I |
| m. 18, beat 1 to m. 19, beat 1 | 272 | unclassified | I |
| m. 18, beat 1 to m. 19, beat 1 | 271 | unclassified | I |
| m. 18, beat 1 to m. 19, beat 1 | 276 | appoggiatura | I |
| m. 18, beat 1 to m. 19, beat 1 | 286 | passing | I |
| m. 18, beat 1 to m. 19, beat 1 | 291 | passing | I |
| m. 18, beat 1 to m. 19, beat 1 | 296 | passing | I |
| m. 18, beat 1 to m. 19, beat 1 | 301 | passing | I |
| m. 18, beat 1 to m. 19, beat 1 | 311 | passing | I |
| m. 18, beat 1 to m. 19, beat 1 | 316 | passing | I |
| m. 18, beat 1 to m. 19, beat 1 | 321 | passing | I |
| m. 19, beat 1 to m. 20, beat 1 | 332 | unclassified | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 331 | neighbor | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 337 | unclassified | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 342 | unclassified | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 341 | passing | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 347 | unclassified | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 346 | neighbor | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 352 | unclassified | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 351 | passing | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 357 | unclassified | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 362 | unclassified | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 361 | neighbor | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 367 | unclassified | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 372 | unclassified | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 371 | passing | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 377 | unclassified | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 376 | neighbor | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 382 | unclassified | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 381 | unclassified | major7 |
| m. 19, beat 1 to m. 20, beat 1 | 387 | unclassified | major7 |
| m. 20, beat 1 to m. 21, beat 1 | 395 | appoggiatura | I |
| m. 20, beat 1 to m. 21, beat 1 | 405 | passing | I |
| m. 20, beat 1 to m. 21, beat 1 | 410 | passing | I |
| m. 20, beat 1 to m. 21, beat 1 | 415 | passing | I |
| m. 20, beat 1 to m. 21, beat 1 | 420 | passing | I |
| m. 20, beat 1 to m. 21, beat 1 | 430 | passing | I |
| m. 20, beat 1 to m. 21, beat 1 | 435 | passing | I |
| m. 20, beat 1 to m. 21, beat 1 | 440 | passing | I |
| m. 21, beat 1 to m. 22, beat 1 | 451 | unclassified | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 450 | neighbor | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 456 | unclassified | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 461 | unclassified | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 460 | passing | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 466 | unclassified | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 465 | neighbor | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 471 | unclassified | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 470 | passing | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 476 | unclassified | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 481 | unclassified | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 480 | neighbor | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 486 | unclassified | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 491 | unclassified | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 490 | passing | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 496 | unclassified | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 495 | neighbor | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 501 | unclassified | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 500 | unclassified | major7 |
| m. 21, beat 1 to m. 22, beat 1 | 506 | unclassified | major7 |
| m. 23, beat 3 to m. 24, beat 1 | 527 | unclassified | diminished |
| m. 23, beat 3 to m. 24, beat 1 | 533 | unclassified | diminished |
| m. 23, beat 3 to m. 24, beat 1 | 537 | unclassified | diminished |
| m. 23, beat 3 to m. 24, beat 1 | 541 | unclassified | diminished |
| m. 24, beat 1 to m. 25, beat 1 | 538 | unclassified | I |
| m. 24, beat 1 to m. 25, beat 1 | 554 | unclassified | I |
| m. 24, beat 1 to m. 25, beat 1 | 558 | unclassified | I |
| m. 24, beat 1 to m. 25, beat 1 | 559 | unclassified | I |
| m. 24, beat 1 to m. 25, beat 1 | 562 | unclassified | I |
| m. 24, beat 1 to m. 25, beat 1 | 564 | unclassified | I |
| m. 25, beat 1 to m. 25, beat 3 | 564 | unclassified | I |
| m. 25, beat 3 to m. 25, beat 4 | 565 | unclassified | diminished7 |
| m. 25, beat 3 to m. 25, beat 4 | 571 | retardation | diminished7 |
| m. 26, beat 1 to m. 27, beat 1 | 585 | unclassified | I |
| m. 26, beat 1 to m. 27, beat 1 | 590 | escape-tone | I |
| m. 27, beat 3 to m. 28, beat 1 | 598 | unclassified | major |
| m. 27, beat 3 to m. 28, beat 1 | 610 | unclassified | major |
| m. 28, beat 3 to m. 29, beat 1 | 620 | unclassified | minor7 |
| m. 28, beat 3 to m. 29, beat 1 | 626 | unclassified | minor7 |
| m. 28, beat 3 to m. 29, beat 1 | 631 | unclassified | minor7 |
| m. 29, beat 1 to m. 29, beat 4 | 642 | unclassified | minor7 |
| m. 29, beat 1 to m. 29, beat 4 | 644 | unclassified | minor7 |
| m. 29, beat 4 to m. 30, beat 1 | 641 | unclassified | dominant7 |
| m. 29, beat 4 to m. 30, beat 1 | 644 | retardation | dominant7 |
| m. 30, beat 1 to m. 30, beat 3 | 653 | unclassified | I |
| m. 30, beat 3 to m. 30, beat 4 | 657 | suspension | IV |
| m. 30, beat 4 to m. 31, beat 1 | 662 | unclassified | dominant7 |
| m. 30, beat 4 to m. 31, beat 1 | 666 | unclassified | dominant7 |
| m. 30, beat 4 to m. 31, beat 1 | 668 | anticipation | dominant7 |
| m. 31, beat 1 to m. 31, beat 3 | 675 | unclassified | dominant7 |
| m. 31, beat 3 to m. 31, beat 4 | 677 | suspension | dominant7 |
| m. 31, beat 3 to m. 31, beat 4 | 682 | unclassified | dominant7 |
| m. 31, beat 4 to m. 32, beat 1 | 679 | unclassified | vi |
| m. 31, beat 4 to m. 32, beat 1 | 682 | unclassified | vi |
| m. 31, beat 4 to m. 32, beat 1 | 686 | unclassified | vi |
| m. 32, beat 1 to m. 32, beat 2 | 683 | unclassified | ii |
| m. 32, beat 1 to m. 32, beat 2 | 686 | unclassified | ii |
| m. 32, beat 2 to m. 32, beat 3 | 687 | unclassified | dominant7 |
| m. 32, beat 2 to m. 32, beat 3 | 690 | unclassified | dominant7 |
| m. 32, beat 3 to m. 32, beat 4 | 691 | unclassified | ii |
| m. 32, beat 4 to m. 33, beat 1 | 698 | retardation | V |
| m. 32, beat 4 to m. 33, beat 1 | 703 | unclassified | V |
| m. 33, beat 3 to m. 34, beat 1 | 710 | retardation | ii |
| m. 33, beat 3 to m. 34, beat 1 | 714 | unclassified | ii |
| m. 33, beat 3 to m. 34, beat 1 | 716 | unclassified | ii |
| m. 33, beat 3 to m. 34, beat 1 | 718 | unclassified | ii |
| m. 34, beat 3 to m. 35, beat 1 | 719 | unclassified | V |
| m. 34, beat 3 to m. 35, beat 1 | 725 | suspension | V |
| m. 34, beat 3 to m. 35, beat 1 | 732 | unclassified | V |
| m. 35, beat 1 to m. 35, beat 3 | 732 | unclassified | IV |
| m. 35, beat 1 to m. 35, beat 3 | 737 | unclassified | IV |
| m. 35, beat 1 to m. 35, beat 3 | 738 | unclassified | IV |
| m. 35, beat 3 to m. 36, beat 1 | 733 | unclassified | I |
| m. 35, beat 3 to m. 36, beat 1 | 745 | unclassified | I |
| m. 35, beat 3 to m. 36, beat 1 | 747 | unclassified | I |
| m. 36, beat 1 to m. 36, beat 4 | 745 | unclassified | dominant7 |
| m. 36, beat 1 to m. 36, beat 4 | 751 | unclassified | dominant7 |
| m. 36, beat 1 to m. 36, beat 4 | 756 | unclassified | dominant7 |
| m. 36, beat 1 to m. 36, beat 4 | 759 | unclassified | dominant7 |
| m. 36, beat 1 to m. 36, beat 4 | 764 | unclassified | dominant7 |
| m. 36, beat 4 to m. 37, beat 1 | 768 | unclassified | vii° |
| m. 36, beat 4 to m. 37, beat 1 | 771 | unclassified | vii° |
| m. 36, beat 4 to m. 37, beat 1 | 772 | unclassified | vii° |
| m. 36, beat 4 to m. 37, beat 1 | 775 | neighbor | vii° |
| m. 37, beat 1 to m. 37, beat 2 | 773 | unclassified | diminished |
| m. 37, beat 1 to m. 37, beat 2 | 780 | unclassified | diminished |
| m. 37, beat 2 to m. 37, beat 3 | 777 | unclassified | ii |
| m. 37, beat 2 to m. 37, beat 3 | 780 | unclassified | ii |
| m. 37, beat 3 to m. 37, beat 4 | 785 | unclassified | I |
| m. 37, beat 3 to m. 37, beat 4 | 784 | unclassified | I |
| m. 37, beat 4 to m. 38, beat 1 | 786 | retardation | minor7 |
| m. 37, beat 4 to m. 38, beat 1 | 789 | unclassified | minor7 |
| m. 37, beat 4 to m. 38, beat 1 | 792 | unclassified | minor7 |
| m. 38, beat 1 to m. 39, beat 1 | 793 | unclassified | I |
| m. 38, beat 1 to m. 39, beat 1 | 798 | appoggiatura | I |
| m. 38, beat 1 to m. 39, beat 1 | 808 | passing | I |
| m. 38, beat 1 to m. 39, beat 1 | 813 | passing | I |
| m. 38, beat 1 to m. 39, beat 1 | 818 | passing | I |
| m. 38, beat 1 to m. 39, beat 1 | 823 | passing | I |
| m. 38, beat 1 to m. 39, beat 1 | 833 | passing | I |
| m. 38, beat 1 to m. 39, beat 1 | 838 | passing | I |
| m. 38, beat 1 to m. 39, beat 1 | 843 | passing | I |
| m. 39, beat 1 to m. 40, beat 1 | 854 | unclassified | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 853 | neighbor | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 859 | unclassified | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 864 | unclassified | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 863 | passing | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 869 | unclassified | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 868 | neighbor | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 874 | unclassified | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 873 | passing | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 879 | unclassified | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 884 | unclassified | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 883 | neighbor | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 889 | unclassified | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 894 | unclassified | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 893 | passing | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 899 | unclassified | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 898 | neighbor | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 904 | unclassified | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 903 | unclassified | major7 |
| m. 39, beat 1 to m. 40, beat 1 | 909 | unclassified | major7 |
| m. 40, beat 1 to m. 41, beat 1 | 917 | appoggiatura | I |
| m. 40, beat 1 to m. 41, beat 1 | 927 | passing | I |
| m. 40, beat 1 to m. 41, beat 1 | 932 | passing | I |
| m. 40, beat 1 to m. 41, beat 1 | 937 | passing | I |
| m. 40, beat 1 to m. 41, beat 1 | 942 | passing | I |
| m. 40, beat 1 to m. 41, beat 1 | 952 | passing | I |
| m. 40, beat 1 to m. 41, beat 1 | 957 | passing | I |
| m. 40, beat 1 to m. 41, beat 1 | 962 | passing | I |
| m. 41, beat 1 to m. 42, beat 1 | 973 | unclassified | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 972 | neighbor | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 978 | unclassified | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 983 | unclassified | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 982 | passing | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 988 | unclassified | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 987 | neighbor | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 993 | unclassified | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 992 | passing | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 998 | unclassified | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 1003 | unclassified | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 1002 | neighbor | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 1008 | unclassified | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 1013 | unclassified | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 1012 | passing | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 1018 | unclassified | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 1017 | neighbor | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 1023 | unclassified | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 1022 | unclassified | major7 |
| m. 41, beat 1 to m. 42, beat 1 | 1028 | unclassified | major7 |
| m. 42, beat 1 to m. 42, beat 4 | 1037 | unclassified | I |
| m. 42, beat 1 to m. 42, beat 4 | 1042 | appoggiatura | I |
| m. 42, beat 1 to m. 42, beat 4 | 1040 | neighbor | I |
| m. 42, beat 1 to m. 42, beat 4 | 1041 | unclassified | I |
| m. 42, beat 1 to m. 42, beat 4 | 1043 | unclassified | I |
| m. 42, beat 1 to m. 42, beat 4 | 1044 | unclassified | I |
| m. 42, beat 1 to m. 42, beat 4 | 1050 | unclassified | I |
| m. 42, beat 4 to m. 43, beat 1 | 1050 | unclassified | V |
| m. 42, beat 4 to m. 43, beat 1 | 1051 | unclassified | V |
| m. 42, beat 4 to m. 43, beat 1 | 1057 | unclassified | V |
| m. 42, beat 4 to m. 43, beat 1 | 1056 | escape-tone | V |
| m. 42, beat 4 to m. 43, beat 1 | 1059 | unclassified | V |
| m. 43, beat 1 to m. 44, beat 1 | 1060 | unclassified | I |
| m. 43, beat 1 to m. 44, beat 1 | 1059 | unclassified | I |
| m. 43, beat 1 to m. 44, beat 1 | 1066 | unclassified | I |
| m. 43, beat 1 to m. 44, beat 1 | 1069 | unclassified | I |
| m. 43, beat 1 to m. 44, beat 1 | 1071 | unclassified | I |
| m. 43, beat 1 to m. 44, beat 1 | 1073 | unclassified | I |
| m. 43, beat 1 to m. 44, beat 1 | 1074 | unclassified | I |
| m. 43, beat 1 to m. 44, beat 1 | 1076 | unclassified | I |

## Cadences

The library's standard-harmony analysis found 3 cadences from the chords, the meter and the soprano. Position is where the arrival begins. Key is the key region of the arrival. Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added before a section start.

| Position | Type | Key | Soprano | Beat strength | Boundary |
| --- | --- | --- | --- | --- | --- |
| m. 8, beat 3 | evaded | Bb major | 1 | medium | no |
| m. 28, beat 1 | authentic-unspecified | D minor | 3 | strong | no |
| m. 35, beat 1 | deceptive | Bb major | #4 | strong | no |

### Competing readings

A cadence the evidence does not settle has two readings, the likelier first, each with its share of the evidence.

- m8 b3 evaded cadence (50%), or tonic prolongation (50%)

### Cadence plan

1. m. 8, beat 3: evaded in Bb major
2. m. 28, beat 1: authentic-unspecified in D minor
3. m. 35, beat 1: deceptive in Bb major

**Counts:** evaded 1, authentic-unspecified 1, deceptive 1.

## Phrases

The library's standard-form analysis found 2 phrases from the soprano, the meter and the cadences. Length counts the measures of the phrase proper; Ends is what closed it: its cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in the piece; Expansion is a lengthening or shortening of the phrase's ending, or an extension after its cadence, with the measures it adds or removes.

| Phrase | Measures | Length | Cadence | Ends | Role | Expansion |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 28 | 28 | authentic-unspecified (m. 28, beat 1) | cadence | beginning | - |
| 2 | mm. 29 to 43 | 15 | none | none | ending | - |

- **Phrase 1:** ideas basic-idea (mm. 1 to 2), contrasting-idea (mm. 3 to 4), contrasting-idea (mm. 5 to 6), contrasting-idea (mm. 7 to 8), basic-idea (mm. 9 to 10), contrasting-idea (mm. 11 to 12), contrasting-idea (mm. 13 to 14), contrasting-idea (mm. 15 to 16), contrasting-idea (mm. 17 to 18), contrasting-idea (mm. 19 to 20), contrasting-idea (mm. 21 to 22), basic-idea (mm. 23 to 24), contrasting-idea (mm. 25 to 26), contrasting-idea (mm. 27 to 28); subphrases mm. 1 to 4, mm. 5 to 28
- **Phrase 2:** ideas basic-idea (mm. 29 to 30), contrasting-idea (mm. 31 to 32), contrasting-idea (mm. 33 to 34), contrasting-idea (mm. 35 to 36), contrasting-idea (mm. 37 to 38), contrasting-idea (mm. 39 to 40), contrasting-idea (mm. 41 to 43); subphrases mm. 29 to 32, mm. 33 to 43

## Form plan

The kernel's form structure detection grouped the phrases into 1 section, closing a section at each phrase that ends on a perfect or imperfect authentic cadence; the phrases after the last such cadence form the last section. Sections are lettered by first appearance, and a section whose opening theme returns an earlier section's material takes that letter with a prime. Key is the key the section opens in.

| Section | Measures | Key | Phrases | Closing cadence | Returns |
| --- | --- | --- | --- | --- | --- |
| A | mm. 1 to 43 | Bb major | 1 to 2 | none | - |

### Outline

- form (mm. 1 to 43)
  - section A (mm. 1 to 43, Bb major)
    - phrase 1 (mm. 1 to 28)
      - subphrase 1 (mm. 1 to 4)
        - idea basic-idea (mm. 1 to 2)
        - idea contrasting-idea (mm. 3 to 4)
      - subphrase 2 (mm. 5 to 28)
        - idea contrasting-idea (mm. 5 to 6)
        - idea contrasting-idea (mm. 7 to 8)
        - idea basic-idea (mm. 9 to 10)
        - idea contrasting-idea (mm. 11 to 12)
        - idea contrasting-idea (mm. 13 to 14)
        - idea contrasting-idea (mm. 15 to 16)
        - idea contrasting-idea (mm. 17 to 18)
        - idea contrasting-idea (mm. 19 to 20)
        - idea contrasting-idea (mm. 21 to 22)
        - idea basic-idea (mm. 23 to 24)
        - idea contrasting-idea (mm. 25 to 26)
        - idea contrasting-idea (mm. 27 to 28)
    - phrase 2 (mm. 29 to 43)
      - subphrase 1 (mm. 29 to 32)
        - idea basic-idea (mm. 29 to 30)
        - idea contrasting-idea (mm. 31 to 32)
      - subphrase 2 (mm. 33 to 43)
        - idea contrasting-idea (mm. 33 to 34)
        - idea contrasting-idea (mm. 35 to 36)
        - idea contrasting-idea (mm. 37 to 38)
        - idea contrasting-idea (mm. 39 to 40)
        - idea contrasting-idea (mm. 41 to 43)

## Motives

The library's motive analysis found 11 motive families in the soprano, the highest sounding note at each attack. A family is a short figure that recurs; Notes is the length of the figure, Pattern whether its occurrences match by scale steps or by semitones, Measures the measures the occurrences start in, Phrases the phrases they lie in (an occurrence across a phrase boundary lies in none), and Transformations how many occurrences are of each kind the library names against the first occurrence (exact, real-transposed, tonal-transposed, inverted, retrograde, augmented, diminished, fragmented, varied).

| Family | Notes | Degrees | Pattern | Occurrences | Measures | Phrases | Transformations |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 5 | 2 3 2 2 5 | scale steps | 2 | 14, 36 | 1, 2 | exact 2 |
| 2 | 8 | #4 5 b6 6 b7 7 1 b2 | scale steps | 4 | 18, 20, 38, 40 | 1, 2 | exact 2, fragmented 2 |
| 3 | 3 | #4 5 b6 | scale steps | 8 | 18, 20, 38, 40, 42, 43 | 1, 2 | exact 2, real-transposed 4, tonal-transposed 2 |
| 4 | 8 | b6 6 b7 7 1 b2 2 b3 | scale steps | 4 | 18, 20, 38, 40 | 1, 2 | exact 4 |
| 5 | 8 | 7 1 b2 2 b3 3 4 3 | scale steps | 4 | 18, 20, 38, 40 | 1, 2 | exact 4 |
| 6 | 8 | 2 b3 3 4 3 2 b2 2 | scale steps | 4 | 18, 20, 38, 40 | 1, 2 | exact 4 |
| 7 | 8 | 4 3 2 b2 2 3 4 3 | scale steps | 4 | 19, 21, 39, 41 | 1, 2 | exact 4 |
| 8 | 8 | b2 2 3 4 3 2 b2 2 | scale steps | 4 | 19, 21, 39, 41 | 1, 2 | exact 4 |
| 9 | 8 | 4 3 2 b2 2 5 1 #4 | scale steps | 4 | 19, 21, 39, 41 | 1, 2 | exact 2, fragmented 2 |
| 10 | 8 | b2 2 5 1 #4 5 b6 6 | scale steps | 2 | 19, 39 | 1, 2 | exact 2 |
| 11 | 8 | 1 #4 5 b6 6 b7 7 1 | scale steps | 2 | 20, 40 | 1, 2 | exact 2 |

## Melody

The library's melody adapter read the soprano of each phrase as a line of notes: the soprano notes that start in the phrase. Range is the lowest and highest note and the semitones between them; Climax is the first note at the highest pitch, with how many notes sound it when it is not one; Contour counts the intervals between consecutive notes that go up, go down and repeat, and how many of them are steps and leaps.

| Phrase | Measures | Notes | Range | Climax | Up | Down | Same | Steps | Leaps |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 28 | 81 | F4 to F6 (24) | F6, m. 8, beat 2.33 | 41 | 34 | 5 | 43 | 32 |
| 2 | mm. 29 to 43 | 53 | Bb3 to Bb6 (36) | Bb6, m. 36, beat 2.67 | 24 | 20 | 5 | 28 | 15 |

### Leaps

A leap is an interval of a third or larger. The library's rule asks a leap of a fourth or larger to be followed by a step in the opposite direction: such a leap is Recovered when one follows, Unrecovered when a step the same way, a repeated note or another leap does, and Not judged when nothing follows it; a third asks nothing (Not required). Consecutive counts the pairs of leaps in a row the library's rule finds fault with.

| Phrase | Leaps | Recovered | Unrecovered | Not required | Not judged | Consecutive |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 32 | 4 | 18 | 10 | 0 | 4 |
| 2 | 15 | 3 | 6 | 6 | 0 | 5 |

### Non-chord tones

Each melody note is set against the chord of the segment it starts in, as the library's chord identification reads it: a Chord tone is one of the chord's tones, a Non-chord tone is one it left over and named, and Unclassified notes are in a segment with no chord or were left unnamed. By type counts the non-chord tones under the library's names.

| Phrase | Notes | Chord tones | Non-chord tones | Unclassified | By type |
| --- | --- | --- | --- | --- | --- |
| 1 | 81 | 59 | 12 | 10 | passing 4, neighbor 2, anticipation 1, appoggiatura 4, escape-tone 1 |
| 2 | 53 | 36 | 9 | 8 | passing 4, neighbor 2, anticipation 1, appoggiatura 1, escape-tone 1 |

### Tendencies

The library's melodic tendencies are soft preferences a well-formed line usually shows: Pitch proximity (steps outnumber leaps, small leaps outnumber large ones), Step declination (descending steps outnumber ascending ones), Step inertia (the line continues in its direction at least as often as it reverses), Melodic regression (extreme notes turn back toward the middle) and Melodic arch (the line rises to a climax in its middle half and ends below it); a phrase either shows a tendency or does not. The seventh degree column counts the notes the library labels the seventh degree that another note follows, judged by its leading tone rule: a leading tone goes up to the tonic by step, and a seventh a whole step below the tonic does not. Followed is the count that did, Broken the count that did not, with where each broken one stands.

| Phrase | Pitch proximity | Step declination | Step inertia | Melodic regression | Melodic arch | Seventh degree followed | Seventh degree broken |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | does not hold | does not hold | does not hold | does not hold | holds | 2 | 1 (m. 5, beat 4) |
| 2 | holds | does not hold | holds | holds | holds | 3 | 0 |

### Development

Each family's occurrences in time order, as the measure they start in and the kind the library names against the family's prototype, which is the original.

- **Family 1:** m14 original > m36 exact
- **Family 2:** m18 original > m20 fragmented > m38 exact > m40 fragmented
- **Family 3:** m18 original > m18 real-transposed > m20 real-transposed > m38 exact > m38 real-transposed > m40 real-transposed > m42 tonal-transposed > m43 tonal-transposed
- **Family 4:** m18 original > m20 exact > m38 exact > m40 exact
- **Family 5:** m18 original > m20 exact > m38 exact > m40 exact
- **Family 6:** m18 original > m20 exact > m38 exact > m40 exact
- **Family 7:** m19 original > m21 exact > m39 exact > m41 exact
- **Family 8:** m19 original > m21 exact > m39 exact > m41 exact
- **Family 9:** m19 original > m21 fragmented > m39 exact > m41 fragmented
- **Family 10:** m19 original > m39 exact
- **Family 11:** m20 original > m40 exact

## Progression patterns

13 recurring progressions of two to six chords, counted over the library's Roman numerals: a held chord counts once, and a progression does not cross an unlabelled chord or a change of key. Ranked by count, then length, then first occurrence; the top 10 are shown. Measures are where each occurrence begins and ends.

| Rank | Progression | Count | Measures |
| --- | --- | --- | --- |
| 1 | I > I7 > I | 4 | mm. 17 to 20, mm. 20 to 23, mm. 38 to 40, mm. 40 to 42 |
| 2 | vii° > I | 4 | mm. 3 to 5, mm. 11 to 12, mm. 14 to 15, mm. 25 to 26 |
| 3 | I > vii°7/vi > vii° > I | 3 | mm. 2 to 5, mm. 10 to 12, mm. 24 to 26 |
| 4 | V > I | 3 | m. 8, mm. 32 to 33, mm. 42 to 43 |
| 5 | ii > I | 3 | m. 15, mm. 33 to 34, m. 37 |
| 6 | I > vii°/iii > I > vii°7/vi > vii° > I | 2 | mm. 1 to 5, mm. 22 to 26 |
| 7 | I > I7 > I > I7 > I | 2 | mm. 17 to 23, mm. 38 to 42 |
| 8 | I > ii > I | 2 | m. 15, mm. 33 to 34 |
| 9 | I > V7 | 2 | m. 8, m. 15 |
| 10 | V7 > I | 2 | mm. 8 to 9, mm. 29 to 30 |

## Harmonic rhythm

How often the harmony changes. A change is a chord whose base Roman numeral or key differs from the chord before it, counted in the measure it starts in. Changes lists the measures of each row in order; Mean is changes per measure over the row.

| Measures | Changes | Mean |
| --- | --- | --- |
| mm. 1 to 4 | 2, 1, 2, 1 | 1.5 |
| mm. 5 to 8 | 1, 3, 2, 3 | 2.25 |
| mm. 9 to 12 | 2, 1, 2, 1 | 1.5 |
| mm. 13 to 16 | 2, 2, 4, 2 | 2.5 |
| mm. 17 to 20 | 1, 0, 1, 1 | 0.75 |
| mm. 21 to 24 | 1, 1, 1, 1 | 1 |
| mm. 25 to 28 | 2, 1, 2, 2 | 1.75 |
| mm. 29 to 32 | 2, 3, 3, 4 | 3 |
| mm. 33 to 36 | 2, 2, 2, 2 | 2 |
| mm. 37 to 40 | 4, 1, 1, 1 | 1.75 |
| mm. 41 to 43 | 1, 2, 1 | 1.33 |

**Mean:** 1.77 changes per measure over 43 measures.

### Approach to cadences

Changes per measure over the two measures before each cadence's arrival (Approach), against the two measures before those (Before). Faster is yes when the approach changes harmony more often.

| Cadence | Arrival | Before | Approach | Faster |
| --- | --- | --- | --- | --- |
| 1 | m. 8 | 1 | 2.5 | yes |
| 2 | m. 28 | 1.5 | 1.5 | no |
| 3 | m. 35 | 3.5 | 2 | no |

## Rhythm and meter

**Meter fit:** 12/8, scale 1, no pickup (fit 0.82); no better reading found

### Density

The library's density profile of each phrase, for all its notes and for each line that has a note in it (the soprano and bass of the outer voices, and Other for the rest). A note is in a phrase when its onset is; a chord is one attack. Attacks per beat is read measure by measure over the beats the phrase spans in it, as a mean and the largest; Mean gap is the mean distance between attacks in whole notes; On the beat is the share of attacks exactly on a beat. Durations lists the notes' lengths in whole notes, shortest first, with how many notes have each. The patterns that recur are the motive families' rhythms (see Motives).

| Phrase | Measures | Line | Notes | Attacks | Per beat | Max per beat | Mean gap | On the beat | Durations |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 28 | all | 620 | 339 | 3.08 | 4.25 | 0.12 | 32.4% | 1/32 (3), 1/16 (14), 35/512 (1), 3/32 (3), 1/8 (528), 1/4 (3), 5/16 (1), 11/32 (6), 3/8 (38), 1/2 (1), 19/32 (1), 5/8 (5), 23/32 (1), 3/4 (12), 787/1024 (1), 17/16 (1), ... |
| 1 | mm. 1 to 28 | soprano | 81 | 81 | 0.73 | 1 | 0.51 | 64.2% | 1/32 (1), 1/16 (7), 1/8 (32), 1/4 (1), 11/32 (2), 3/8 (20), 1/2 (1), 19/32 (1), 5/8 (2), 23/32 (1), 3/4 (12), 37/32 (1) |
| 1 | mm. 1 to 28 | bass | 103 | 103 | 0.94 | 1 | 0.4 | 51.5% | 1/8 (99), 3/8 (4) |
| 1 | mm. 1 to 28 | other | 436 | 261 | 2.37 | 3.25 | 0.16 | 26.4% | 1/32 (2), 1/16 (7), 35/512 (1), 3/32 (3), 1/8 (397), 1/4 (2), 5/16 (1), 11/32 (4), 3/8 (14), 5/8 (3), 787/1024 (1), 17/16 (1) |
| 2 | mm. 29 to 43 | all | 449 | 188 | 3.13 | 4.75 | 0.12 | 31.4% | 1/32 (3), 1/16 (11), 3/32 (2), 1/8 (401), 3/16 (1), 1/4 (1), 5/16 (1), 11/32 (3), 3/8 (13), 201/512 (1), 7/16 (1), 59/128 (1), 123/256 (2), 1/2 (3), 9/16 (1), 3/4 (4) |
| 2 | mm. 29 to 43 | soprano | 52 | 52 | 0.87 | 1.25 | 0.43 | 51.9% | 1/16 (5), 1/8 (28), 1/4 (1), 11/32 (2), 3/8 (9), 7/16 (1), 1/2 (1), 9/16 (1), 3/4 (4) |
| 2 | mm. 29 to 43 | bass | 59 | 59 | 0.98 | 1 | 0.38 | 55.9% | 1/8 (57), 3/8 (1), 1/2 (1) |
| 2 | mm. 29 to 43 | other | 338 | 151 | 2.52 | 4.25 | 0.14 | 24.5% | 1/32 (3), 1/16 (6), 3/32 (2), 1/8 (316), 3/16 (1), 5/16 (1), 11/32 (1), 3/8 (3), 201/512 (1), 59/128 (1), 123/256 (2), 1/2 (1) |

### Syncopation

A note is syncopated when it is attacked at a weaker metric level and held through a stronger one that no note of its line attacks. Each line (the soprano and bass of the outer voices, and Other for the rest) is read on its own. Strength is the difference of the two levels, 1 to 4: an off-beat note held over the beat is 1, a note on a beat held over the barline is 2, an off-beat note held over the barline is 3.

| Line | Measure | Beat | Attacked at | Held through | Strength |
| --- | --- | --- | --- | --- | --- |
| soprano | 14 | 2 | beat | secondary-beat (m. 14, beat 3) | 1 |
| soprano | 24 | 3.5 | finer | offbeat (m. 24, beat 3.67) | 1 |
| soprano | 24 | 3.83 | finer | beat (m. 24, beat 4) | 2 |
| soprano | 28 | 3.5 | finer | offbeat (m. 28, beat 3.67) | 1 |
| soprano | 43 | 3.17 | finer | beat (m. 43, beat 4) | 2 |
| other | 8 | 4.95 | finer | downbeat (m. 9, beat 1) | 4 |
| other | 28 | 4.5 | finer | offbeat (m. 28, beat 4.67) | 1 |
| other | 30 | 4.95 | finer | downbeat (m. 31, beat 1) | 4 |
| other | 43 | 3.05 | finer | beat (m. 43, beat 4) | 2 |
| other | 43 | 3.05 | finer | beat (m. 43, beat 4) | 2 |
| other | 43 | 3.1 | finer | beat (m. 43, beat 4) | 2 |

### Hemiola

The library found no hemiola.

### Hypermeter

The library reads each phrase in hypermeasures of two or four measures: the length that fills the phrase and holds the cadence's arrival in the last one. The first measure of each hypermeasure is a hyperdownbeat. A phrase that does not fill its hypermeasures ends in a short one, an internal expansion makes the last one longer, an external expansion adds one of its own and a contraction shortens the last. Score is the library's confidence in the reading. The form adapter does not report elided cadences, so no phrase is read as elided.

| Phrase | Measures | Length | Hyperdownbeats | Hypermeasures | Cadence arrives | Score |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 28 | 4 | 1, 5, 9, 13, 17, 21, 25 | 7 regular | m. 28 | 1 |
| 2 | mm. 29 to 43 | 2 | 29, 31, 33, 35, 37, 39, 41, 43 | 7 regular, 1 short | none | 0.93 |

### Tuplets and borrowed divisions

A meter divides its beat two ways (simple) or three (compound). A tuplet divides it the other way: a triplet in a simple meter or a duplet in a compound one is a borrowed division. The library names those two and says at which level a group occurs (the whole Beat, a Subdivision of it, or several beats) and whether that is typical. Only triplets (sextuplets included) and duplets are looked for, read from the notes' times alone, rounded to a grid of 96 to the whole note. A run is two or more consecutive attacks of one ratio.

| Ratio | Name | Runs | Attacks | Borrowed |
| --- | --- | --- | --- | --- |
| 3:2 | triplet | 2 | 5 | no |

| Measure | Beat | Ratio | Name | Meter | Borrowed | Level | Attacks |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 8 | 4.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 43 | 3 | 3:2 | triplet | compound | no | subdivision | 3 |

## Texture

The library's texture adapter read every measure of the piece from the voices the kernel separated in each track or part, classified it, and merged consecutive measures of the same texture into passages. Category is the main texture (homorhythm and melody and accompaniment are homophony). Voices is the most sounding at once, Synchrony the share of attack times where the voices attack together, Pattern the score of a repeated cell (0 to 1: evidence for an accompaniment figure, not a name for it). Confidence is the texture's margin from its thresholds times the voice separation's.

| Measures | Texture | Category | Voices | Synchrony | Pattern | Confidence |
| --- | --- | --- | --- | --- | --- | --- |
| mm. 1 to 2 | melody and accompaniment | homophony | 2 | 0.25 | 0.2 | 0.73 |
| m. 3 | polyphony | polyphony | 2 | 0.25 | 0.17 | 0.52 |
| m. 4 | melody and accompaniment | homophony | 2 | 0.23 | 0.37 | 0.41 |
| m. 5 | polyphony | polyphony | 2 | 0.25 | 0.14 | 0.43 |
| mm. 6 to 9 | melody and accompaniment | homophony | 2 | 0.32 | 0.19 | 0.44 |
| m. 10 | polyphony | polyphony | 3 | 0.25 | 0.11 | 0.44 |
| mm. 11 to 12 | melody and accompaniment | homophony | 2 | 0.24 | 0.4 | 0.79 |
| mm. 13 to 15 | polyphony | polyphony | 3 | 0.31 | 0.08 | 0.61 |
| mm. 16 to 17 | melody and accompaniment | homophony | 3 | 0.33 | 0.06 | 0.45 |
| mm. 18 to 21 | homorhythm | homophony | 4 | 1 | 0.86 | 1 |
| mm. 22 to 25 | melody and accompaniment | homophony | 4 | 0.31 | 0.24 | 0.74 |
| m. 26 | polyphony | polyphony | 2 | 0.29 | 0.31 | 0.5 |
| m. 27 | melody and accompaniment | homophony | 2 | 0.25 | 0.12 | 0.35 |
| mm. 28 to 29 | polyphony | polyphony | 2 | 0.21 | 0.23 | 0.61 |
| mm. 30 to 33 | melody and accompaniment | homophony | 3 | 0.35 | 0.16 | 0.5 |
| m. 34 | polyphony | polyphony | 2 | 0.17 | 0 | 0.54 |
| mm. 35 to 37 | melody and accompaniment | homophony | 2 | 0.44 | 0.2 | 0.89 |
| mm. 38 to 42 | homorhythm | homophony | 4 | 0.93 | 0.7 | 1 |
| m. 43 | melody and accompaniment | homophony | 4 | 0.6 | 0 | 0.52 |

### Changes

| Measure | From | To | Category changes |
| --- | --- | --- | --- |
| 3 | melody and accompaniment | polyphony | yes |
| 4 | polyphony | melody and accompaniment | yes |
| 5 | melody and accompaniment | polyphony | yes |
| 6 | polyphony | melody and accompaniment | yes |
| 10 | melody and accompaniment | polyphony | yes |
| 11 | polyphony | melody and accompaniment | yes |
| 13 | melody and accompaniment | polyphony | yes |
| 16 | polyphony | melody and accompaniment | yes |
| 18 | melody and accompaniment | homorhythm | no |
| 22 | homorhythm | melody and accompaniment | no |
| 26 | melody and accompaniment | polyphony | yes |
| 27 | polyphony | melody and accompaniment | yes |
| 28 | melody and accompaniment | polyphony | yes |
| 30 | polyphony | melody and accompaniment | yes |
| 34 | melody and accompaniment | polyphony | yes |
| 35 | polyphony | melody and accompaniment | yes |
| 38 | melody and accompaniment | homorhythm | no |
| 43 | homorhythm | melody and accompaniment | no |

## Voice leading

The kernel separated the notes of the whole piece into voices (voice 0 is the top) and the library's counterpoint adapter classified how each pair of voices moves from one chord segment to the next. Parallel motion keeps the interval type, similar motion changes it; a voice that rests in either segment gives no motion. Faults are the library's: parallel fifths and octaves between any pair of voices, direct fifths and octaves in the outer pair, and a seventh or leading tone left unresolved when its chord changes (voice 0 is the top).

4 voices over 82 chord segments, 237 motions between consecutive segments: 68 contrary, 49 similar, 3 parallel, 103 oblique, 14 static.

| Voices | Contrary | Similar | Parallel | Oblique | Static |
| --- | --- | --- | --- | --- | --- |
| 0 and 1 | 8 | 9 | 1 | 13 | 5 |
| 0 and 2 | 15 | 7 | 2 | 24 | 0 |
| 0 and 3 | 7 | 9 | 0 | 21 | 1 |
| 1 and 2 | 10 | 9 | 0 | 20 | 0 |
| 1 and 3 | 11 | 5 | 0 | 17 | 2 |
| 2 and 3 | 17 | 10 | 0 | 8 | 6 |

0 parallel fifths, 0 parallel octaves, 1 direct fifths or octaves in the outer voices, 3 unresolved sevenths, 1 unresolved leading tone.

| Location | Fault | Voices |
| --- | --- | --- |
| m. 37, beat 2 to m. 37, beat 4 | direct-perfects | 0 and 3 |
| m. 11, beat 4 to m. 13, beat 1 | leading-tone-resolves | 3 |
| m. 29, beat 1 to m. 30, beat 1 | seventh-resolves-down | 2 |
| m. 29, beat 1 to m. 30, beat 1 | seventh-resolves-down | 3 |
| m. 37, beat 4 to m. 39, beat 1 | seventh-resolves-down | 2 |

64 chords with three or more voices sounding: the root is doubled in 13, the third in 1, the fifth in 4, the seventh in 2, no member doubled in 44. Doubling is counted, not judged. Spacing is judged by the library for the 41 chords with four voices all sounding: 25 too wide.

| Location | Chord | Gap too wide |
| --- | --- | --- |
| m. 1, beat 3 to m. 2, beat 1 | 1 | voices 1 and 2 |
| m. 3, beat 3 to m. 3, beat 4 | 4 | voices 1 and 2 |
| m. 7, beat 4 to m. 8, beat 1 | 13 | voices 1 and 2 |
| m. 8, beat 1 to m. 8, beat 3 | 14 | voices 1 and 2 |
| m. 9, beat 1 to m. 9, beat 3 | 17 | voices 1 and 2 |
| m. 9, beat 3 to m. 10, beat 1 | 18 | voices 1 and 2 |
| m. 10, beat 1 to m. 11, beat 1 | 19 | voices 0 and 1 |
| m. 11, beat 3 to m. 11, beat 4 | 21 | voices 1 and 2 |
| m. 13, beat 3 to m. 14, beat 1 | 25 | voices 1 and 2 |
| m. 15, beat 2 to m. 15, beat 3 | 29 | voices 0 and 1 |
| m. 15, beat 3 to m. 15, beat 4 | 30 | voices 1 and 2 |
| m. 15, beat 4 to m. 16, beat 1 | 31 | voices 1 and 2 |
| m. 16, beat 4 to m. 17, beat 1 | 33 | voices 1 and 2 |
| m. 23, beat 3 to m. 24, beat 1 | 41 | voices 1 and 2 |
| m. 25, beat 3 to m. 25, beat 4 | 44 | voices 0 and 1 |
| m. 29, beat 4 to m. 30, beat 1 | 52 | voices 1 and 2 |
| m. 31, beat 1 to m. 31, beat 3 | 56 | voices 1 and 2 |
| m. 31, beat 3 to m. 31, beat 4 | 57 | voices 1 and 2 |
| m. 31, beat 4 to m. 32, beat 1 | 58 | voices 1 and 2 |
| m. 32, beat 1 to m. 32, beat 2 | 59 | voices 1 and 2 |
| m. 32, beat 4 to m. 33, beat 1 | 62 | voices 0 and 1 |
| m. 35, beat 3 to m. 36, beat 1 | 68 | voices 0 and 1 |
| m. 37, beat 3 to m. 37, beat 4 | 73 | voices 1 and 2 |
| m. 37, beat 4 to m. 38, beat 1 | 74 | voices 1 and 2 |
| m. 42, beat 4 to m. 43, beat 1 | 80 | voices 0 and 1 |

## Markings

A MIDI file carries no tempo or dynamics markings.
