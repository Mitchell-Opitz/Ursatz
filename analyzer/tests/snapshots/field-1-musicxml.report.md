# Analysis Report: field-1.musicxml

## Summary

- **File:** field-1.musicxml, 1191 notes.
- **Input:** MusicXML score, 1 part merged into one texture. Meter: notated. Key signature: inferred. Pitch spelling: notated.
- **Key:** Eb major, the key with the most time across 4 key regions.
- **Chords:** 107 chord events; 106 matched a chord (99%), 1 did not.
- **Most frequent chords:** I (36), dominant7 (21), IV (9).

## Meter

- **Time signature:** 12/8 (notated)
- **Measures:** 67 measures.
- **Pickup:** None.

## Key regions

Positions are measure and beat. Uncertainty is the library's, from 0 (clearly this key) to 1 (another key fits as well or better). The home key is the key with the most time across its regions.

- **Home key:** Eb major.
- **Opening key:** Eb major.
- **Closing key:** Eb major.

| Position | Key | Uncertainty | Set by |
| --- | --- | --- | --- |
| m. 1, beat 1 to m. 20, beat 1 | Eb major | 0.73 | profile |
| m. 20, beat 1 to m. 28, beat 1 | Bb major | 0.88 | profile |
| m. 28, beat 1 to m. 32, beat 1 | F minor | 0.77 | profile |
| m. 32, beat 1 to m. 68, beat 1 | Eb major | 0.74 | profile |

## Tonicizations

1 brief excursion of another key that returns to the key before. Span is the stretch in the target key. The library tells a tonicization from a modulation by how long the new key lasts, whether it returns, and whether a cadence arrives in it; without a cadence it is a tonicization however long.

| Span | In | Target |
| --- | --- | --- |
| m. 28, beat 1 to m. 32, beat 1 | Bb major | F minor |

## Modulations

A modulation is a lasting new key. Position is where the new key arrives. Type is pivot when the library found a pivot chord and direct when it did not. A return goes back to the key before an excursion. The library gives the change's extent from the length of the new key, whether it returns, and whether a cadence arrives in it.

| Position | From | To | Extent | Type | Return |
| --- | --- | --- | --- | --- | --- |
| m. 20, beat 1 | Eb major | Bb major | modulation | pivot | no |
| m. 32, beat 1 | F minor | Eb major | modulation | pivot | no |

## Pivot chords

The chord at a modulation that is diatonic in both the old and the new key, named by root and quality, with no Roman numeral. Modulation is its row in the Modulations table, counting from 1.

| Position | Chord | Bass | Inversion | From | To | Modulation |
| --- | --- | --- | --- | --- | --- | --- |
| m. 18, beat 1 to m. 19, beat 1 | D# major7 | A# | second | Eb major | Bb major | 1 |
| m. 31, beat 1 to m. 32, beat 1 | F minor | F | root position | F minor | Eb major | 2 |

## Chords

Positions are measure and beat. Numeral is the Roman numeral in the local key region, which is named where it changes. Function is the library's harmonic function of the chord: an applied chord is a dominant toward its target degree, and a predominant is strong (IV, ii) or weak (iii, vi). Six-four is the library's type for a chord in second inversion. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Numeral | Function | Six-four | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 4 to m. 2, beat 1 | V | V (in Eb major) | dominant | - | A# | major | A# | root position | 1 | 1.00 | - |
| m. 2, beat 1 to m. 3, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.69 | - |
| m. 3, beat 1 to m. 3, beat 2 | V | V | dominant | - | A# | major | A# | root position | 1 | 0.27 | - |
| m. 3, beat 2 to m. 3, beat 4 | half-diminished7 | vii∅4/3 | dominant | - | D | half-diminished7 | F | first | 2 | 0.15 | - |
| m. 3, beat 4 to m. 4, beat 1 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 1 | 0.71 | - |
| m. 4, beat 1 to m. 5, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 5, beat 1 to m. 6, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 6, beat 1 to m. 7, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.58 | - |
| m. 7, beat 1 to m. 8, beat 1 | V | V | dominant | - | A# | major | A# | root position | 4 | 0.81 | - |
| m. 8, beat 1 to m. 9, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 9, beat 1 to m. 10, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 10, beat 1 to m. 11, beat 1 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 4 | 0.31 | - |
| m. 11, beat 1 to m. 11, beat 3 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 2 | 0.57 | - |
| m. 11, beat 3 to m. 11, beat 4 | vii° | vii°6 | dominant | - | D | diminished | F | first | 1 | 0.11 | - |
| m. 11, beat 4 to m. 12, beat 1 | iii | iii6 | predominant (weak) | - | G | minor | A# | first | 1 | 0.49 | - |
| m. 12, beat 1 to m. 13, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.69 | - |
| m. 13, beat 1 to m. 14, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.80 | - |
| m. 14, beat 1 to m. 15, beat 1 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 4 | 0.29 | - |
| m. 15, beat 1 to m. 15, beat 2 | V | V | dominant | - | A# | major | A# | root position | 1 | 0.44 | - |
| m. 15, beat 2 to m. 15, beat 4 | half-diminished7 | vii∅4/3 | dominant | - | D | half-diminished7 | F | first | 2 | 0.08 | ii (F minor, score 0.73) |
| m. 15, beat 4 to m. 16, beat 1 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 1 | 0.16 | - |
| m. 16, beat 1 to m. 17, beat 1 | I | I | tonic | - | D# | major | A# | second | 4 | 0.46 | - |
| m. 17, beat 1 to m. 17, beat 2 | vii° | vii°6 | dominant | - | D | diminished | F | first | 1 | 0.22 | - |
| m. 17, beat 2 to m. 18, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 3 | 0.19 | - |
| m. 18, beat 1 to m. 19, beat 1 | I | I | tonic | - | D# | major | A# | second | 4 | 0.56 | - |
| m. 19, beat 1 to m. 19, beat 2 | vii° | vii°6 | dominant | - | D | diminished | F | first | 1 | 0.36 | - |
| m. 19, beat 2 to m. 20, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 3 | 0.20 | - |
| m. 20, beat 1 to m. 21, beat 1 | IV | IV (in Bb major) | predominant (strong) | - | D# | major | D# | root position | 4 | 0.47 | - |
| m. 21, beat 1 to m. 22, beat 1 | I | I | tonic | - | A# | major | A# | root position | 4 | 0.70 | - |
| m. 22, beat 1 to m. 22, beat 4 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 3 | 0.33 | - |
| m. 22, beat 4 to m. 23, beat 1 | iii | iii6 | predominant (weak) | - | D | minor | F | first | 1 | 0.55 | - |
| m. 23, beat 1 to m. 24, beat 1 | I | I | tonic | - | A# | major | A# | root position | 4 | 0.10 | vi (G minor, score 0.75) |
| m. 24, beat 1 to m. 24, beat 2 | diminished | vii°6/4/V | dominant of 5 | - | E | diminished | A# | second | 1 | 0.45 | - |
| m. 24, beat 2 to m. 25, beat 1 | I | I6/4 | tonic | - | A# | major | D | first | 3 | 0.11 | - |
| m. 25, beat 1 to m. 25, beat 2 | vi | vi6 | predominant (weak) | - | G | minor | A# | first | 1 | 0.14 | - |
| m. 25, beat 2 to m. 26, beat 1 | I | I6/4 | tonic | - | A# | major | D | first | 3 | 0.12 | - |
| m. 26, beat 1 to m. 27, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 4 | 0.72 | - |
| m. 27, beat 1 to m. 28, beat 1 | I | I | tonic | - | A# | major | A# | root position | 4 | 0.66 | - |
| m. 28, beat 1 to m. 28, beat 3 | major | IV (in F minor) | predominant (strong) | - | A# | major | A# | root position | 2 | 1.00 | - |
| m. 28, beat 3 to m. 28, beat 4 | minor | i6 | tonic | - | F | minor | G# | first | 1 | 0.58 | - |
| m. 28, beat 4 to m. 29, beat 1 | diminished | viio6 | dominant | - | E | diminished | G | first | 1 | 0.20 | - |
| m. 29, beat 1 to m. 30, beat 1 | minor | i | tonic | - | F | minor | F | root position | 4 | 1.00 | - |
| m. 30, beat 1 to m. 31, beat 1 | major | V | dominant | - | C | major | C | root position | 4 | 0.14 | - |
| m. 31, beat 1 to m. 32, beat 1 | minor | i | tonic | - | F | minor | F | root position | 4 | 0.46 | - |
| m. 32, beat 1 to m. 33, beat 1 | iii | iii6 (in Eb major) | predominant (weak) | - | G | minor | A# | first | 4 | 0.18 | - |
| m. 33, beat 1 to m. 33, beat 3 | I | I | tonic | - | D# | major | D# | root position | 2 | 0.60 | - |
| m. 33, beat 3 to m. 34, beat 1 | minor7 | ii6/5 | predominant (strong) | - | F | minor7 | G# | first | 2 | 0.01 | IV (G# major, score 0.89) |
| m. 34, beat 1 to m. 35, beat 1 | V | V | dominant | - | A# | major | A# | root position | 4 | 0.02 | iii (G minor, score 0.73) |
| m. 35, beat 1 to m. 36, beat 1 | I | I | tonic | - | D# | major | A# | second | 4 | 0.02 | vi (C minor, score 0.98) |
| m. 36, beat 1 to m. 36, beat 3 | dominant7 | V4/3 | dominant | - | A# | dominant7 | A# | root position | 2 | 0.01 | vii° (D diminished, score 0.88) |
| m. 36, beat 3 to m. 36, beat 4 | vii° | vii°6 | dominant | - | D | diminished | F | first | 1 | 0.10 | - |
| m. 36, beat 4 to m. 37, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.29 | - |
| m. 37, beat 1 to m. 38, beat 1 | I | I | tonic | - | D# | major | A# | second | 4 | 0.04 | vi (C minor, score 0.95) |
| m. 38, beat 1 to m. 39, beat 1 | dominant7 | V4/3 | dominant | - | A# | dominant7 | A# | root position | 4 | 0.03 | vii° (D diminished, score 0.73) |
| m. 39, beat 1 to m. 40, beat 1 | I | I | tonic | - | D# | major | A# | second | 4 | 0.55 | - |
| m. 40, beat 1 to m. 41, beat 1 | major | V6/V | dominant of 5 | - | F | major | A | first | 4 | 0.53 | - |
| m. 41, beat 1 to m. 41, beat 2 | vi | vi6/4 | predominant (weak) | - | C | minor | G | second | 1 | 0.07 | I (D# major, score 0.88) |
| m. 41, beat 2 to m. 41, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 41, beat 3 to m. 41, beat 4 | vii° | vii°6 | dominant | - | D | diminished | F | first | 1 | 0.01 | diminished (B diminished, score 0.69) |
| m. 41, beat 4 to m. 42, beat 1 | IV | IV6 | predominant (strong) | - | G# | major | G# | root position | 1 | 0.03 | minor7 (F minor7, score 0.78) |
| m. 42, beat 1 to m. 42, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 2 | 0.10 | minor7 (C minor7, score 0.84) |
| m. 42, beat 3 to m. 42, beat 4 | iii | iii6 | predominant (weak) | - | G | minor | A# | first | 1 | 0.14 | - |
| m. 42, beat 4 to m. 43, beat 1 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 1 | 0.04 | dominant7 (A# dominant7, score 0.74) |
| m. 43, beat 1 to m. 44, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 44, beat 1 to m. 44, beat 3 | I | I | tonic | - | D# | major | D# | root position | 2 | 1.00 | - |
| m. 44, beat 3 to m. 44, beat 4 | diminished | vii°6/V | dominant of 5 | - | A | diminished | C | first | 1 | 0.10 | - |
| m. 44, beat 4 to m. 45, beat 1 | dominant7 | V7/V | dominant of 5 | - | F | dominant7 | F | root position | 1 | 0.11 | vii° (D diminished, score 0.78) |
| m. 45, beat 1 to m. 45, beat 2 | V | V | dominant | - | A# | major | A# | root position | 1 | 0.87 | - |
| m. 45, beat 2 to m. 45, beat 4 | half-diminished7 | vii∅4/3 | dominant | - | D | half-diminished7 | F | first | 2 | 0.16 | - |
| m. 45, beat 4 to m. 46, beat 1 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 1 | 0.70 | - |
| m. 46, beat 1 to m. 47, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 47, beat 1 to m. 48, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.68 | - |
| m. 48, beat 1 to m. 49, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.12 | - |
| m. 49, beat 1 to m. 49, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 2 | 0.12 | dominant7 (A# dominant7, score 0.55) |
| m. 49, beat 3 to m. 49, beat 4 | vii° | vii°6 | dominant | - | D | diminished | F | first | 1 | 0.09 | dominant7 (A# dominant7, score 0.86) |
| m. 49, beat 4 to m. 50, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.08 | iii (G minor, score 0.67) |
| m. 50, beat 1 to m. 51, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.77 | - |
| m. 51, beat 1 to m. 51, beat 3 | dominant7 | V7/vi | dominant of 6 | - | G | dominant7 | G | root position | 2 | 1.00 | - |
| m. 51, beat 3 to m. 51, beat 4 | diminished | vii°6/vi | dominant of 6 | - | B | diminished | D | first | 1 | 0.19 | - |
| m. 51, beat 4 to m. 52, beat 1 | dominant7 | V7/vi | dominant of 6 | - | G | dominant7 | G | root position | 1 | 1.00 | - |
| m. 52, beat 1 to m. 52, beat 3 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 2 | 0.35 | - |
| m. 52, beat 3 to m. 52, beat 4 | IV | IV6/4 | predominant (strong) | - | Ab | major | D# | second | 1 | 0.35 | - |
| m. 52, beat 4 to m. 53, beat 1 | diminished7 | vii°4/2/vi | dominant of 6 | - | B | diminished7 | G# | third | 1 | 0.31 | - |
| m. 53, beat 1 to m. 54, beat 1 | iii | iii6 | predominant (weak) | - | G | minor | A# | first | 4 | 0.02 | dominant7 (A# dominant7, score 0.77) |
| m. 54, beat 1 to m. 55, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.77 | - |
| m. 55, beat 1 to m. 55, beat 3 | dominant7 | V7/vi | dominant of 6 | - | G | dominant7 | G | root position | 2 | 1.00 | - |
| m. 55, beat 3 to m. 55, beat 4 | diminished | vii°6/vi | dominant of 6 | - | B | diminished | D | first | 1 | 0.09 | vii° (D diminished, score 0.94) |
| m. 55, beat 4 to m. 56, beat 1 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.24 | - |
| m. 56, beat 1 to m. 57, beat 1 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 4 | 0.14 | - |
| m. 57, beat 1 to m. 57, beat 2 | V | V | dominant | - | A# | major | A# | root position | 1 | 0.36 | - |
| m. 57, beat 2 to m. 57, beat 4 | IV | IV | predominant (strong) | - | G# | major | A# | - | 2 | 0.16 | half-diminished7 (D half-diminished7, score 0.50) |
| m. 57, beat 4 to m. 58, beat 1 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 1 | 0.18 | - |
| m. 58, beat 1 to m. 59, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 59, beat 1 to m. 60, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 4 | 0.88 | - |
| m. 60, beat 1 to m. 61, beat 1 | I | I6 | tonic | - | D# | major | G | first | 4 | 0.15 | - |
| m. 61, beat 1 to m. 62, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 4 | 0.85 | - |
| m. 62, beat 1 to m. 63, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.90 | - |
| m. 63, beat 1 to m. 63, beat 4 | major | V6/V | dominant of 5 | - | F | major | G# | - | 3 | 0.29 | - |
| m. 63, beat 4 to m. 64, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 1.00 | - |
| m. 64, beat 1 to m. 64, beat 2 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.18 | - |
| m. 64, beat 2 to m. 64, beat 3 | diminished7 | vii°6/5/ii | dominant of 2 | - | E | diminished7 | G | first | 1 | 0.09 | diminished (G diminished, score 0.82) |
| m. 64, beat 3 to m. 65, beat 1 | ii | ii6 | predominant (strong) | - | F | minor | G# | first | 2 | 0.29 | - |
| m. 65, beat 1 to m. 65, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 2 | 0.11 | - |
| m. 65, beat 3 to m. 65, beat 4 | iii | iii6 | predominant (weak) | - | G | minor | A# | first | 1 | 0.07 | V (A# major, score 0.81) |
| m. 65, beat 4 to m. 66, beat 1 | dominant7 | V4/3 | dominant | - | A# | dominant7 | F | second | 1 | 0.13 | ii (F minor, score 0.64) |
| m. 66, beat 1 to m. 67, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 67, beat 1 to m. 67, beat 4 | I | I | tonic | - | D# | major | D# | root position | 3 | 1.00 | - |

## Applied chords

Chords the library reads as applied (secondary) chords: the chord as it reads in the tonicized key, the chord it tonicizes and that chord's tonic. The last column checks each against the tonicization claims above for the same span; where they disagree both are shown and neither is changed.

| Position | Applied chord | Target | Resolution | Tonicization |
| --- | --- | --- | --- | --- |
| m. 24, beat 1 to m. 24, beat 2 | vii°6/4 (secondary-leading-tone) | V (F) | no resolution | DISAGREES: no tonicization is reported here |
| m. 40, beat 1 to m. 41, beat 1 | V6 (secondary-dominant) | V (Bb) | no resolution | DISAGREES: no tonicization is reported here |
| m. 44, beat 3 to m. 44, beat 4 | vii°6 (secondary-leading-tone) | V (Bb) | no resolution | DISAGREES: no tonicization is reported here |
| m. 44, beat 4 to m. 45, beat 1 | V7 (secondary-dominant) | V (Bb) | to the target | DISAGREES: no tonicization is reported here |
| m. 51, beat 1 to m. 51, beat 3 | V7 (secondary-dominant) | vi (C) | no resolution | DISAGREES: no tonicization is reported here |
| m. 51, beat 3 to m. 51, beat 4 | vii°6 (secondary-leading-tone) | vi (C) | no resolution | DISAGREES: no tonicization is reported here |
| m. 51, beat 4 to m. 52, beat 1 | V7 (secondary-dominant) | vi (C) | no resolution | DISAGREES: no tonicization is reported here |
| m. 52, beat 4 to m. 53, beat 1 | vii°4/2 (secondary-leading-tone) | vi (C) | no resolution | DISAGREES: no tonicization is reported here |
| m. 55, beat 1 to m. 55, beat 3 | V7 (secondary-dominant) | vi (C) | no resolution | DISAGREES: no tonicization is reported here |
| m. 55, beat 3 to m. 55, beat 4 | vii°6 (secondary-leading-tone) | vi (C) | no resolution | DISAGREES: no tonicization is reported here |
| m. 63, beat 1 to m. 63, beat 4 | V6 (secondary-dominant) | V (Bb) | to the target | DISAGREES: no tonicization is reported here |
| m. 64, beat 2 to m. 64, beat 3 | vii°6/5 (secondary-leading-tone) | ii (F) | to the target | DISAGREES: no tonicization is reported here |
## Outer voices

The highest (soprano) and lowest (bass) sounding note of each chord's span, with its scale degree in that chord's local key. A flat or sharp sign marks a chromatic alteration. A degree is "-" where the span lies in no key region. A rest means the library found no note for that line in the span, a bridged one a short gap inside the line, and a register guard a note the library dropped rather than adopt an accompaniment note. Degrees carry the key region's uncertainty in the analysis document.

| Position | Key | Soprano | Degree | Bass | Degree |
| --- | --- | --- | --- | --- | --- |
| m. 1, beat 4 to m. 2, beat 1 | Eb major | A#4 | 5 | A#4 | 5 |
| m. 2, beat 1 to m. 3, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 3, beat 1 to m. 3, beat 2 | Eb major | D#5 | 1 | A#2 | 5 |
| m. 3, beat 2 to m. 3, beat 4 | Eb major | D5 | 7 | G#3 | 4 |
| m. 3, beat 4 to m. 4, beat 1 | Eb major | D5 | 7 | G#3 | 4 |
| m. 4, beat 1 to m. 5, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 5, beat 1 to m. 6, beat 1 | Eb major | A#5 | 5 | D#3 | 1 |
| m. 6, beat 1 to m. 7, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 7, beat 1 to m. 8, beat 1 | Eb major | C6 | 6 | A#2 | 5 |
| m. 8, beat 1 to m. 9, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 9, beat 1 to m. 10, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 10, beat 1 to m. 11, beat 1 | Eb major | D5 | 7 | G#2 | 4 |
| m. 11, beat 1 to m. 11, beat 3 | Eb major | C5 | 6 | A#2 | 5 |
| m. 11, beat 3 to m. 11, beat 4 | Eb major | G5 | 3 | F3 | 2 |
| m. 11, beat 4 to m. 12, beat 1 | Eb major | G5 | 3 | A#3 | 5 |
| m. 12, beat 1 to m. 13, beat 1 | Eb major | G5 | 3 | D#3 | 1 |
| m. 13, beat 1 to m. 14, beat 1 | Eb major | G5 | 3 | D#3 | 1 |
| m. 14, beat 1 to m. 15, beat 1 | Eb major | D5 | 7 | G#2 | 4 |
| m. 15, beat 1 to m. 15, beat 2 | Eb major | C5 | 6 | A#2 | 5 |
| m. 15, beat 2 to m. 15, beat 4 | Eb major | C6 | 6 | G#3 | 4 |
| m. 15, beat 4 to m. 16, beat 1 | Eb major | C6 | 6 | G#3 | 4 |
| m. 16, beat 1 to m. 17, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 17, beat 1 to m. 17, beat 2 | Eb major | C6 | 6 | F3 | 2 |
| m. 17, beat 2 to m. 18, beat 1 | Eb major | C6 | 6 | A#2 | 5 |
| m. 18, beat 1 to m. 19, beat 1 | Eb major | F5 | 2 | D#3 | 1 |
| m. 19, beat 1 to m. 19, beat 2 | Eb major | D#5 | 1 | F3 | 2 |
| m. 19, beat 2 to m. 20, beat 1 | Eb major | C6 | 6 | A#2 | 5 |
| m. 20, beat 1 to m. 21, beat 1 | Bb major | F5 | 5 | D#3 | 4 |
| m. 21, beat 1 to m. 22, beat 1 | Bb major | F5 | 5 | A#2 | 1 |
| m. 22, beat 1 to m. 22, beat 4 | Bb major | G5 | 6 | F2 | 5 |
| m. 22, beat 4 to m. 23, beat 1 | Bb major | D6 | 3 | F3 | 5 |
| m. 23, beat 1 to m. 24, beat 1 | Bb major | D6 | 3 | A#2 | 1 |
| m. 24, beat 1 to m. 24, beat 2 | Bb major | G5 | 6 | A#2 | 1 |
| m. 24, beat 2 to m. 25, beat 1 | Bb major | F5 | 5 | F3 | 5 |
| m. 25, beat 1 to m. 25, beat 2 | Bb major | G5 | 6 | A#2 | 1 |
| m. 25, beat 2 to m. 26, beat 1 | Bb major | G5 | 6 | F3 | 5 |
| m. 26, beat 1 to m. 27, beat 1 | Bb major | F#5 | b6 | F2 | 5 |
| m. 27, beat 1 to m. 28, beat 1 | Bb major | C6 | 2 | A#2 | 1 |
| m. 28, beat 1 to m. 28, beat 3 | F minor | F5 | 1 | A#2 | 4 |
| m. 28, beat 3 to m. 28, beat 4 | F minor | F5 | 1 | G#2 | 3 |
| m. 28, beat 4 to m. 29, beat 1 | F minor | G5 | 2 | G2 | 2 |
| m. 29, beat 1 to m. 30, beat 1 | F minor | G#5 | 3 | F2 | 1 |
| m. 30, beat 1 to m. 31, beat 1 | F minor | G5 | 2 | C3 | 5 |
| m. 31, beat 1 to m. 32, beat 1 | F minor | G#5 | 3 | F2 | 1 |
| m. 32, beat 1 to m. 33, beat 1 | Eb major | D4 | 7 | A#2 | 5 |
| m. 33, beat 1 to m. 33, beat 3 | Eb major | F5 | 2 | D#3 | 1 |
| m. 33, beat 3 to m. 34, beat 1 | Eb major | D#5 | 1 | G#3 | 4 |
| m. 34, beat 1 to m. 35, beat 1 | Eb major | D5 | 7 | A#2 | 5 |
| m. 35, beat 1 to m. 36, beat 1 | Eb major | G5 | 3 | D#3 | 1 |
| m. 36, beat 1 to m. 36, beat 3 | Eb major | G#5 | 4 | F3 | 2 |
| m. 36, beat 3 to m. 36, beat 4 | Eb major | G#5 | 4 | F3 | 2 |
| m. 36, beat 4 to m. 37, beat 1 | Eb major | G#5 | 4 | A#2 | 5 |
| m. 37, beat 1 to m. 38, beat 1 | Eb major | D#6 | 1 | D#3 | 1 |
| m. 38, beat 1 to m. 39, beat 1 | Eb major | G#6 | 4 | F3 | 2 |
| m. 39, beat 1 to m. 40, beat 1 | Eb major | F5 | 2 | D#3 | 1 |
| m. 40, beat 1 to m. 41, beat 1 | Eb major | G5 | 3 | A2 | #4 |
| m. 41, beat 1 to m. 41, beat 2 | Eb major | C6 | 6 | G3 | 3 |
| m. 41, beat 2 to m. 41, beat 3 | Eb major | A#5 | 5 | F#3 | b3 |
| m. 41, beat 3 to m. 41, beat 4 | Eb major | A#5 | 5 | F3 | 2 |
| m. 41, beat 4 to m. 42, beat 1 | Eb major | G#5 | 4 | C4 | 6 |
| m. 42, beat 1 to m. 42, beat 3 | Eb major | D#6 | 1 | A#2 | 5 |
| m. 42, beat 3 to m. 42, beat 4 | Eb major | C6 | 6 | A#2 | 5 |
| m. 42, beat 4 to m. 43, beat 1 | Eb major | G#5 | 4 | G#3 | 4 |
| m. 43, beat 1 to m. 44, beat 1 | Eb major | G6 | 3 | D#3 | 1 |
| m. 44, beat 1 to m. 44, beat 3 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 44, beat 3 to m. 44, beat 4 | Eb major | F5 | 2 | C3 | 6 |
| m. 44, beat 4 to m. 45, beat 1 | Eb major | F5 | 2 | F3 | 2 |
| m. 45, beat 1 to m. 45, beat 2 | Eb major | D5 | 7 | A#2 | 5 |
| m. 45, beat 2 to m. 45, beat 4 | Eb major | C5 | 6 | G#3 | 4 |
| m. 45, beat 4 to m. 46, beat 1 | Eb major | D5 | 7 | G#3 | 4 |
| m. 46, beat 1 to m. 47, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 47, beat 1 to m. 48, beat 1 | Eb major | A#5 | 5 | D#3 | 1 |
| m. 48, beat 1 to m. 49, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 49, beat 1 to m. 49, beat 3 | Eb major | D#5 | 1 | A#2 | 5 |
| m. 49, beat 3 to m. 49, beat 4 | Eb major | D6 | 7 | F3 | 2 |
| m. 49, beat 4 to m. 50, beat 1 | Eb major | A#5 | 5 | A#3 | 5 |
| m. 50, beat 1 to m. 51, beat 1 | Eb major | G5 | 3 | D#3 | 1 |
| m. 51, beat 1 to m. 51, beat 3 | Eb major | F4 | 2 | G2 | 3 |
| m. 51, beat 3 to m. 51, beat 4 | Eb major | D#5 | 1 | D3 | 7 |
| m. 51, beat 4 to m. 52, beat 1 | Eb major | D#5 | 1 | G3 | 3 |
| m. 52, beat 1 to m. 52, beat 3 | Eb major | D5 | 7 | G#2 | 4 |
| m. 52, beat 3 to m. 52, beat 4 | Eb major | C5 | 6 | D#3 | 1 |
| m. 52, beat 4 to m. 53, beat 1 | Eb major | B4 | b6 | G#3 | 4 |
| m. 53, beat 1 to m. 54, beat 1 | Eb major | C5 | 6 | A#2 | 5 |
| m. 54, beat 1 to m. 55, beat 1 | Eb major | G5 | 3 | D#3 | 1 |
| m. 55, beat 1 to m. 55, beat 3 | Eb major | G5 | 3 | G2 | 3 |
| m. 55, beat 3 to m. 55, beat 4 | Eb major | F5 | 2 | D3 | 7 |
| m. 55, beat 4 to m. 56, beat 1 | Eb major | F5 | 2 | G3 | 3 |
| m. 56, beat 1 to m. 57, beat 1 | Eb major | D5 | 7 | G#2 | 4 |
| m. 57, beat 1 to m. 57, beat 2 | Eb major | B4 | b6 | A#2 | 5 |
| m. 57, beat 2 to m. 57, beat 4 | Eb major | C6 | 6 | G#3 | 4 |
| m. 57, beat 4 to m. 58, beat 1 | Eb major | C6 | 6 | G#3 | 4 |
| m. 58, beat 1 to m. 59, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 59, beat 1 to m. 60, beat 1 | Eb major | C6 | 6 | A#2 | 5 |
| m. 60, beat 1 to m. 61, beat 1 | Eb major | G5 | 3 | G3 | 3 |
| m. 61, beat 1 to m. 62, beat 1 | Eb major | C6 | 6 | A#2 | 5 |
| m. 62, beat 1 to m. 63, beat 1 | Eb major | F5 | 2 | D#3 | 1 |
| m. 63, beat 1 to m. 63, beat 4 | Eb major | F5 | 2 | A3 | #4 |
| m. 63, beat 4 to m. 64, beat 1 | Eb major | A5 | #4 | A#3 | 5 |
| m. 64, beat 1 to m. 64, beat 2 | Eb major | A#5 | 5 | G3 | 3 |
| m. 64, beat 2 to m. 64, beat 3 | Eb major | C#6 | b7 | G3 | 3 |
| m. 64, beat 3 to m. 65, beat 1 | Eb major | C#6 | b7 | G#3 | 4 |
| m. 65, beat 1 to m. 65, beat 3 | Eb major | A#6 | 5 | A#2 | 5 |
| m. 65, beat 3 to m. 65, beat 4 | Eb major | D6 | 7 | A#2 | 5 |
| m. 65, beat 4 to m. 66, beat 1 | Eb major | A#5 | 5 | F3 | 2 |
| m. 66, beat 1 to m. 67, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 67, beat 1 to m. 67, beat 4 | Eb major | D#6 | 1 | D#3 | 1 |

## Chord vocabulary

Each distinct Roman numeral of the chords the library numbered, for the whole piece and for each key region, sorted by count, then total length, then first appearance. Count is the number of chord rows with that numeral (every row in the Chords table is one). Runs counts a stretch of adjacent identical numerals in one key region once. Beats is the total length, Share its fraction of the scope's numbered chord length, and First where it first appears. Applied chords are listed on their own. Chords without a numeral are not counted.

### Overall

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 28 | 20 | 107 | 40.7% | m. 2, beat 1 |
| V7 | 10 | 10 | 26 | 9.9% | m. 11, beat 1 |
| IV | 8 | 8 | 23 | 8.7% | m. 10, beat 1 |
| V | 8 | 8 | 17 | 6.5% | m. 1, beat 4 |
| iii6 | 6 | 6 | 12 | 4.6% | m. 11, beat 4 |
| vii°6 | 6 | 6 | 6 | 2.3% | m. 11, beat 3 |
| I6/4 | 5 | 5 | 12 | 4.6% | m. 24, beat 2 |
| V4/2 | 4 | 4 | 4 | 1.5% | m. 3, beat 4 |
| V4/3 | 3 | 3 | 7 | 2.7% | m. 36, beat 1 |
| vii∅4/3 | 3 | 3 | 6 | 2.3% | m. 3, beat 2 |
| I6 | 3 | 3 | 6 | 2.3% | m. 55, beat 4 |
| i | 2 | 2 | 8 | 3.0% | m. 29, beat 1 |
| ii6/5 | 1 | 1 | 2 | 0.8% | m. 33, beat 3 |
| ii6 | 1 | 1 | 2 | 0.8% | m. 64, beat 3 |
| vi6 | 1 | 1 | 1 | 0.4% | m. 25, beat 1 |
| i6 | 1 | 1 | 1 | 0.4% | m. 28, beat 3 |
| viio6 | 1 | 1 | 1 | 0.4% | m. 28, beat 4 |
| vi6/4 | 1 | 1 | 1 | 0.4% | m. 41, beat 1 |
| IV6 | 1 | 1 | 1 | 0.4% | m. 41, beat 4 |
| IV6/4 | 1 | 1 | 1 | 0.4% | m. 52, beat 3 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 36 | 28 | 125 | 47.5% | m. 2, beat 1 |
| V7 | 17 | 17 | 37 | 14.1% | m. 3, beat 4 |
| IV | 10 | 10 | 25 | 9.5% | m. 10, beat 1 |
| V | 8 | 8 | 17 | 6.5% | m. 1, beat 4 |
| iii | 6 | 6 | 12 | 4.6% | m. 11, beat 4 |
| vii° | 6 | 6 | 6 | 2.3% | m. 11, beat 3 |
| i | 3 | 3 | 9 | 3.4% | m. 28, beat 3 |
| vii∅7 | 3 | 3 | 6 | 2.3% | m. 3, beat 2 |
| vi | 2 | 2 | 2 | 0.8% | m. 25, beat 1 |
| ii7 | 1 | 1 | 2 | 0.8% | m. 33, beat 3 |
| ii | 1 | 1 | 2 | 0.8% | m. 64, beat 3 |
| viio | 1 | 1 | 1 | 0.4% | m. 28, beat 4 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/vi | 3 | 3 | 5 | 1.9% | m. 51, beat 1 |
| V6/V | 2 | 2 | 7 | 2.7% | m. 40, beat 1 |
| vii°6/vi | 2 | 2 | 2 | 0.8% | m. 51, beat 3 |
| vii°6/4/V | 1 | 1 | 1 | 0.4% | m. 24, beat 1 |
| vii°6/V | 1 | 1 | 1 | 0.4% | m. 44, beat 3 |
| V7/V | 1 | 1 | 1 | 0.4% | m. 44, beat 4 |
| vii°4/2/vi | 1 | 1 | 1 | 0.4% | m. 52, beat 4 |
| vii°6/5/ii | 1 | 1 | 1 | 0.4% | m. 64, beat 2 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/vi | 3 | 3 | 5 | 1.9% | m. 51, beat 1 |
| V/V | 2 | 2 | 7 | 2.7% | m. 40, beat 1 |
| vii°/V | 2 | 2 | 2 | 0.8% | m. 24, beat 1 |
| vii°/vi | 2 | 2 | 2 | 0.8% | m. 51, beat 3 |
| V7/V | 1 | 1 | 1 | 0.4% | m. 44, beat 4 |
| vii°7/vi | 1 | 1 | 1 | 0.4% | m. 52, beat 4 |
| vii°7/ii | 1 | 1 | 1 | 0.4% | m. 64, beat 2 |

### Eb major (region 0)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 10 | 6 | 40 | 54.8% | m. 2, beat 1 |
| V | 4 | 4 | 7 | 9.6% | m. 1, beat 4 |
| V7 | 3 | 3 | 8 | 11.0% | m. 11, beat 1 |
| vii°6 | 3 | 3 | 3 | 4.1% | m. 11, beat 3 |
| IV | 2 | 2 | 8 | 11.0% | m. 10, beat 1 |
| vii∅4/3 | 2 | 2 | 4 | 5.5% | m. 3, beat 2 |
| V4/2 | 2 | 2 | 2 | 2.7% | m. 3, beat 4 |
| iii6 | 1 | 1 | 1 | 1.4% | m. 11, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 10 | 6 | 40 | 54.8% | m. 2, beat 1 |
| V7 | 5 | 5 | 10 | 13.7% | m. 3, beat 4 |
| V | 4 | 4 | 7 | 9.6% | m. 1, beat 4 |
| vii° | 3 | 3 | 3 | 4.1% | m. 11, beat 3 |
| IV | 2 | 2 | 8 | 11.0% | m. 10, beat 1 |
| vii∅7 | 2 | 2 | 4 | 5.5% | m. 3, beat 2 |
| iii | 1 | 1 | 1 | 1.4% | m. 11, beat 4 |

### Bb major (region 1)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 12 | 37.5% | m. 21, beat 1 |
| V7 | 2 | 2 | 7 | 21.9% | m. 22, beat 1 |
| I6/4 | 2 | 2 | 6 | 18.8% | m. 24, beat 2 |
| IV | 1 | 1 | 4 | 12.5% | m. 20, beat 1 |
| iii6 | 1 | 1 | 1 | 3.1% | m. 22, beat 4 |
| vi6 | 1 | 1 | 1 | 3.1% | m. 25, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 5 | 5 | 18 | 56.2% | m. 21, beat 1 |
| V7 | 2 | 2 | 7 | 21.9% | m. 22, beat 1 |
| IV | 1 | 1 | 4 | 12.5% | m. 20, beat 1 |
| iii | 1 | 1 | 1 | 3.1% | m. 22, beat 4 |
| vi | 1 | 1 | 1 | 3.1% | m. 25, beat 1 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°6/4/V | 1 | 1 | 1 | 3.1% | m. 24, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°/V | 1 | 1 | 1 | 3.1% | m. 24, beat 1 |

### F minor (region 2)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 2 | 2 | 8 | 50.0% | m. 29, beat 1 |
| V | 1 | 1 | 4 | 25.0% | m. 30, beat 1 |
| IV | 1 | 1 | 2 | 12.5% | m. 28, beat 1 |
| i6 | 1 | 1 | 1 | 6.2% | m. 28, beat 3 |
| viio6 | 1 | 1 | 1 | 6.2% | m. 28, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 3 | 3 | 9 | 56.2% | m. 28, beat 3 |
| V | 1 | 1 | 4 | 25.0% | m. 30, beat 1 |
| IV | 1 | 1 | 2 | 12.5% | m. 28, beat 1 |
| viio | 1 | 1 | 1 | 6.2% | m. 28, beat 4 |

### Eb major (region 3)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 15 | 11 | 55 | 38.7% | m. 33, beat 1 |
| V7 | 5 | 5 | 11 | 7.7% | m. 36, beat 4 |
| iii6 | 4 | 4 | 10 | 7.0% | m. 32, beat 1 |
| IV | 4 | 4 | 9 | 6.3% | m. 42, beat 4 |
| V4/3 | 3 | 3 | 7 | 4.9% | m. 36, beat 1 |
| V | 3 | 3 | 6 | 4.2% | m. 34, beat 1 |
| I6/4 | 3 | 3 | 6 | 4.2% | m. 42, beat 1 |
| I6 | 3 | 3 | 6 | 4.2% | m. 55, beat 4 |
| vii°6 | 3 | 3 | 3 | 2.1% | m. 36, beat 3 |
| V4/2 | 2 | 2 | 2 | 1.4% | m. 45, beat 4 |
| ii6/5 | 1 | 1 | 2 | 1.4% | m. 33, beat 3 |
| vii∅4/3 | 1 | 1 | 2 | 1.4% | m. 45, beat 2 |
| ii6 | 1 | 1 | 2 | 1.4% | m. 64, beat 3 |
| vi6/4 | 1 | 1 | 1 | 0.7% | m. 41, beat 1 |
| IV6 | 1 | 1 | 1 | 0.7% | m. 41, beat 4 |
| IV6/4 | 1 | 1 | 1 | 0.7% | m. 52, beat 3 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 21 | 17 | 67 | 47.2% | m. 33, beat 1 |
| V7 | 10 | 10 | 20 | 14.1% | m. 36, beat 1 |
| IV | 6 | 6 | 11 | 7.7% | m. 41, beat 4 |
| iii | 4 | 4 | 10 | 7.0% | m. 32, beat 1 |
| V | 3 | 3 | 6 | 4.2% | m. 34, beat 1 |
| vii° | 3 | 3 | 3 | 2.1% | m. 36, beat 3 |
| ii7 | 1 | 1 | 2 | 1.4% | m. 33, beat 3 |
| vii∅7 | 1 | 1 | 2 | 1.4% | m. 45, beat 2 |
| ii | 1 | 1 | 2 | 1.4% | m. 64, beat 3 |
| vi | 1 | 1 | 1 | 0.7% | m. 41, beat 1 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/vi | 3 | 3 | 5 | 3.5% | m. 51, beat 1 |
| V6/V | 2 | 2 | 7 | 4.9% | m. 40, beat 1 |
| vii°6/vi | 2 | 2 | 2 | 1.4% | m. 51, beat 3 |
| vii°6/V | 1 | 1 | 1 | 0.7% | m. 44, beat 3 |
| V7/V | 1 | 1 | 1 | 0.7% | m. 44, beat 4 |
| vii°4/2/vi | 1 | 1 | 1 | 0.7% | m. 52, beat 4 |
| vii°6/5/ii | 1 | 1 | 1 | 0.7% | m. 64, beat 2 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/vi | 3 | 3 | 5 | 3.5% | m. 51, beat 1 |
| V/V | 2 | 2 | 7 | 4.9% | m. 40, beat 1 |
| vii°/vi | 2 | 2 | 2 | 1.4% | m. 51, beat 3 |
| vii°/V | 1 | 1 | 1 | 0.7% | m. 44, beat 3 |
| V7/V | 1 | 1 | 1 | 0.7% | m. 44, beat 4 |
| vii°7/vi | 1 | 1 | 1 | 0.7% | m. 52, beat 4 |
| vii°7/ii | 1 | 1 | 1 | 0.7% | m. 64, beat 2 |

## Prolongations

The library's standard-harmony analysis found 1 prolongation: a tonic or dominant that lasts through the chords between it and the harmony that closes it. Measures run from the prolonged chord to the closing chord. Numeral is the prolonged chord. Reason is why the library reads the chords between as a prolongation.

| Measures | Function | Numeral | Key | Reason |
| --- | --- | --- | --- | --- |
| mm. 37 to 40 | tonic | I | Eb major | tonic-line |

## Non-chord tones

251 notes sound against a chord without being one of its tones; 165 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 2, beat 1 to m. 3, beat 1 | 12 | unclassified | I |
| m. 3, beat 1 to m. 3, beat 2 | 15 | unclassified | V |
| m. 3, beat 1 to m. 3, beat 2 | 20 | anticipation | V |
| m. 3, beat 2 to m. 3, beat 4 | 23 | escape-tone | half-diminished7 |
| m. 3, beat 2 to m. 3, beat 4 | 27 | unclassified | half-diminished7 |
| m. 6, beat 1 to m. 7, beat 1 | 70 | unclassified | I |
| m. 7, beat 1 to m. 8, beat 1 | 77 | unclassified | V |
| m. 7, beat 1 to m. 8, beat 1 | 78 | appoggiatura | V |
| m. 7, beat 1 to m. 8, beat 1 | 86 | unclassified | V |
| m. 8, beat 1 to m. 9, beat 1 | 85 | unclassified | I |
| m. 9, beat 1 to m. 10, beat 1 | 114 | unclassified | I |
| m. 10, beat 1 to m. 11, beat 1 | 117 | unclassified | IV |
| m. 10, beat 1 to m. 11, beat 1 | 127 | unclassified | IV |
| m. 10, beat 1 to m. 11, beat 1 | 128 | unclassified | IV |
| m. 10, beat 1 to m. 11, beat 1 | 131 | anticipation | IV |
| m. 11, beat 1 to m. 11, beat 3 | 128 | unclassified | dominant7 |
| m. 11, beat 1 to m. 11, beat 3 | 134 | escape-tone | dominant7 |
| m. 11, beat 1 to m. 11, beat 3 | 137 | unclassified | dominant7 |
| m. 11, beat 3 to m. 11, beat 4 | 146 | anticipation | vii° |
| m. 11, beat 4 to m. 12, beat 1 | 148 | pedal | iii |
| m. 12, beat 1 to m. 13, beat 1 | 151 | unclassified | I |
| m. 13, beat 1 to m. 14, beat 1 | 169 | unclassified | I |
| m. 13, beat 1 to m. 14, beat 1 | 173 | unclassified | I |
| m. 13, beat 1 to m. 14, beat 1 | 179 | unclassified | I |
| m. 14, beat 1 to m. 15, beat 1 | 182 | unclassified | IV |
| m. 14, beat 1 to m. 15, beat 1 | 192 | unclassified | IV |
| m. 14, beat 1 to m. 15, beat 1 | 193 | unclassified | IV |
| m. 15, beat 1 to m. 15, beat 2 | 199 | unclassified | V |
| m. 15, beat 1 to m. 15, beat 2 | 202 | unclassified | V |
| m. 15, beat 2 to m. 15, beat 4 | 203 | unclassified | half-diminished7 |
| m. 15, beat 2 to m. 15, beat 4 | 207 | escape-tone | half-diminished7 |
| m. 15, beat 2 to m. 15, beat 4 | 210 | unclassified | half-diminished7 |
| m. 15, beat 4 to m. 16, beat 1 | 205 | unclassified | dominant7 |
| m. 16, beat 1 to m. 17, beat 1 | 212 | unclassified | I |
| m. 16, beat 1 to m. 17, beat 1 | 227 | unclassified | I |
| m. 17, beat 1 to m. 17, beat 2 | 229 | suspension | vii° |
| m. 17, beat 1 to m. 17, beat 2 | 235 | unclassified | vii° |
| m. 17, beat 2 to m. 18, beat 1 | 235 | unclassified | dominant7 |
| m. 17, beat 2 to m. 18, beat 1 | 242 | unclassified | dominant7 |
| m. 18, beat 1 to m. 19, beat 1 | 247 | unclassified | I |
| m. 18, beat 1 to m. 19, beat 1 | 252 | neighbor | I |
| m. 18, beat 1 to m. 19, beat 1 | 255 | escape-tone | I |
| m. 18, beat 1 to m. 19, beat 1 | 268 | unclassified | I |
| m. 19, beat 1 to m. 19, beat 2 | 270 | suspension | vii° |
| m. 19, beat 2 to m. 20, beat 1 | 276 | appoggiatura | dominant7 |
| m. 19, beat 2 to m. 20, beat 1 | 282 | appoggiatura | dominant7 |
| m. 20, beat 1 to m. 21, beat 1 | 288 | unclassified | IV |
| m. 20, beat 1 to m. 21, beat 1 | 300 | unclassified | IV |
| m. 20, beat 1 to m. 21, beat 1 | 302 | unclassified | IV |
| m. 21, beat 1 to m. 22, beat 1 | 303 | unclassified | I |
| m. 22, beat 1 to m. 22, beat 4 | 321 | changing-tone | dominant7 |
| m. 22, beat 1 to m. 22, beat 4 | 324 | changing-tone | dominant7 |
| m. 22, beat 1 to m. 22, beat 4 | 333 | anticipation | dominant7 |
| m. 22, beat 4 to m. 23, beat 1 | 335 | unclassified | iii |
| m. 22, beat 4 to m. 23, beat 1 | 337 | unclassified | iii |
| m. 23, beat 1 to m. 24, beat 1 | 337 | unclassified | I |
| m. 23, beat 1 to m. 24, beat 1 | 348 | appoggiatura | I |
| m. 23, beat 1 to m. 24, beat 1 | 351 | unclassified | I |
| m. 23, beat 1 to m. 24, beat 1 | 354 | anticipation | I |
| m. 24, beat 1 to m. 24, beat 2 | 358 | unclassified | diminished |
| m. 24, beat 1 to m. 24, beat 2 | 360 | anticipation | diminished |
| m. 24, beat 2 to m. 25, beat 1 | 371 | unclassified | I |
| m. 24, beat 2 to m. 25, beat 1 | 372 | anticipation | I |
| m. 25, beat 1 to m. 25, beat 2 | 371 | unclassified | vi |
| m. 25, beat 1 to m. 25, beat 2 | 376 | anticipation | vi |
| m. 25, beat 2 to m. 26, beat 1 | 372 | pedal | I |
| m. 25, beat 2 to m. 26, beat 1 | 391 | unclassified | I |
| m. 26, beat 1 to m. 27, beat 1 | 391 | unclassified | dominant7 |
| m. 26, beat 1 to m. 27, beat 1 | 400 | appoggiatura | dominant7 |
| m. 26, beat 1 to m. 27, beat 1 | 403 | escape-tone | dominant7 |
| m. 27, beat 1 to m. 28, beat 1 | 411 | unclassified | I |
| m. 28, beat 1 to m. 28, beat 3 | 430 | unclassified | major |
| m. 30, beat 1 to m. 31, beat 1 | 460 | anticipation | major |
| m. 30, beat 1 to m. 31, beat 1 | 461 | appoggiatura | major |
| m. 30, beat 1 to m. 31, beat 1 | 468 | appoggiatura | major |
| m. 30, beat 1 to m. 31, beat 1 | 469 | anticipation | major |
| m. 31, beat 1 to m. 32, beat 1 | 472 | unclassified | minor |
| m. 32, beat 1 to m. 33, beat 1 | 489 | appoggiatura | iii |
| m. 32, beat 1 to m. 33, beat 1 | 496 | escape-tone | iii |
| m. 32, beat 1 to m. 33, beat 1 | 498 | unclassified | iii |
| m. 33, beat 1 to m. 33, beat 3 | 498 | unclassified | I |
| m. 34, beat 1 to m. 35, beat 1 | 511 | retardation | V |
| m. 34, beat 1 to m. 35, beat 1 | 518 | appoggiatura | V |
| m. 34, beat 1 to m. 35, beat 1 | 519 | unclassified | V |
| m. 34, beat 1 to m. 35, beat 1 | 526 | escape-tone | V |
| m. 34, beat 1 to m. 35, beat 1 | 527 | anticipation | V |
| m. 35, beat 1 to m. 36, beat 1 | 543 | unclassified | I |
| m. 35, beat 1 to m. 36, beat 1 | 545 | unclassified | I |
| m. 36, beat 1 to m. 36, beat 3 | 545 | unclassified | dominant7 |
| m. 36, beat 4 to m. 37, beat 1 | 559 | passing | dominant7 |
| m. 36, beat 4 to m. 37, beat 1 | 563 | anticipation | dominant7 |
| m. 37, beat 1 to m. 38, beat 1 | 577 | passing | I |
| m. 37, beat 1 to m. 38, beat 1 | 579 | unclassified | I |
| m. 38, beat 1 to m. 39, beat 1 | 585 | unclassified | dominant7 |
| m. 38, beat 1 to m. 39, beat 1 | 588 | appoggiatura | dominant7 |
| m. 38, beat 1 to m. 39, beat 1 | 592 | unclassified | dominant7 |
| m. 38, beat 1 to m. 39, beat 1 | 596 | unclassified | dominant7 |
| m. 38, beat 1 to m. 39, beat 1 | 600 | appoggiatura | dominant7 |
| m. 38, beat 1 to m. 39, beat 1 | 604 | anticipation | dominant7 |
| m. 39, beat 1 to m. 40, beat 1 | 602 | unclassified | I |
| m. 39, beat 1 to m. 40, beat 1 | 607 | anticipation | I |
| m. 39, beat 1 to m. 40, beat 1 | 610 | unclassified | I |
| m. 39, beat 1 to m. 40, beat 1 | 621 | unclassified | I |
| m. 39, beat 1 to m. 40, beat 1 | 624 | unclassified | I |
| m. 40, beat 1 to m. 41, beat 1 | 626 | unclassified | major |
| m. 40, beat 1 to m. 41, beat 1 | 628 | unclassified | major |
| m. 40, beat 1 to m. 41, beat 1 | 630 | unclassified | major |
| m. 40, beat 1 to m. 41, beat 1 | 634 | unclassified | major |
| m. 40, beat 1 to m. 41, beat 1 | 636 | unclassified | major |
| m. 40, beat 1 to m. 41, beat 1 | 638 | unclassified | major |
| m. 40, beat 1 to m. 41, beat 1 | 637 | unclassified | major |
| m. 40, beat 1 to m. 41, beat 1 | 640 | unclassified | major |
| m. 40, beat 1 to m. 41, beat 1 | 641 | unclassified | major |
| m. 40, beat 1 to m. 41, beat 1 | 643 | escape-tone | major |
| m. 40, beat 1 to m. 41, beat 1 | 644 | unclassified | major |
| m. 40, beat 1 to m. 41, beat 1 | 647 | unclassified | major |
| m. 41, beat 1 to m. 41, beat 2 | 651 | unclassified | vi |
| m. 41, beat 3 to m. 41, beat 4 | 652 | unclassified | vii° |
| m. 41, beat 3 to m. 41, beat 4 | 658 | unclassified | vii° |
| m. 41, beat 3 to m. 41, beat 4 | 660 | unclassified | vii° |
| m. 41, beat 3 to m. 41, beat 4 | 663 | unclassified | vii° |
| m. 41, beat 3 to m. 41, beat 4 | 665 | anticipation | vii° |
| m. 41, beat 3 to m. 41, beat 4 | 664 | unclassified | vii° |
| m. 41, beat 4 to m. 42, beat 1 | 664 | retardation | IV |
| m. 41, beat 4 to m. 42, beat 1 | 671 | appoggiatura | IV |
| m. 41, beat 4 to m. 42, beat 1 | 672 | unclassified | IV |
| m. 41, beat 4 to m. 42, beat 1 | 674 | unclassified | IV |
| m. 41, beat 4 to m. 42, beat 1 | 673 | unclassified | IV |
| m. 41, beat 4 to m. 42, beat 1 | 675 | anticipation | IV |
| m. 42, beat 1 to m. 42, beat 3 | 686 | unclassified | I |
| m. 42, beat 1 to m. 42, beat 3 | 688 | unclassified | I |
| m. 42, beat 3 to m. 42, beat 4 | 691 | escape-tone | iii |
| m. 42, beat 3 to m. 42, beat 4 | 695 | anticipation | iii |
| m. 42, beat 4 to m. 43, beat 1 | 693 | unclassified | IV |
| m. 42, beat 4 to m. 43, beat 1 | 698 | neighbor | IV |
| m. 42, beat 4 to m. 43, beat 1 | 700 | unclassified | IV |
| m. 43, beat 1 to m. 44, beat 1 | 700 | unclassified | I |
| m. 44, beat 3 to m. 44, beat 4 | 725 | anticipation | diminished |
| m. 44, beat 4 to m. 45, beat 1 | 730 | anticipation | dominant7 |
| m. 45, beat 1 to m. 45, beat 2 | 734 | anticipation | V |
| m. 45, beat 2 to m. 45, beat 4 | 737 | escape-tone | half-diminished7 |
| m. 45, beat 2 to m. 45, beat 4 | 741 | unclassified | half-diminished7 |
| m. 45, beat 4 to m. 46, beat 1 | 746 | anticipation | dominant7 |
| m. 47, beat 1 to m. 48, beat 1 | 770 | passing | I |
| m. 47, beat 1 to m. 48, beat 1 | 772 | unclassified | I |
| m. 47, beat 1 to m. 48, beat 1 | 776 | unclassified | I |
| m. 48, beat 1 to m. 49, beat 1 | 787 | unclassified | I |
| m. 48, beat 1 to m. 49, beat 1 | 790 | unclassified | I |
| m. 48, beat 1 to m. 49, beat 1 | 793 | unclassified | I |
| m. 48, beat 1 to m. 49, beat 1 | 794 | appoggiatura | I |
| m. 48, beat 1 to m. 49, beat 1 | 797 | unclassified | I |
| m. 48, beat 1 to m. 49, beat 1 | 799 | passing | I |
| m. 48, beat 1 to m. 49, beat 1 | 802 | passing | I |
| m. 49, beat 1 to m. 49, beat 3 | 805 | unclassified | I |
| m. 49, beat 1 to m. 49, beat 3 | 806 | unclassified | I |
| m. 49, beat 1 to m. 49, beat 3 | 808 | unclassified | I |
| m. 49, beat 1 to m. 49, beat 3 | 807 | neighbor | I |
| m. 49, beat 1 to m. 49, beat 3 | 809 | passing | I |
| m. 49, beat 1 to m. 49, beat 3 | 812 | unclassified | I |
| m. 49, beat 1 to m. 49, beat 3 | 814 | unclassified | I |
| m. 49, beat 1 to m. 49, beat 3 | 813 | unclassified | I |
| m. 49, beat 1 to m. 49, beat 3 | 817 | unclassified | I |
| m. 49, beat 1 to m. 49, beat 3 | 816 | unclassified | I |
| m. 49, beat 3 to m. 49, beat 4 | 822 | unclassified | vii° |
| m. 49, beat 3 to m. 49, beat 4 | 823 | unclassified | vii° |
| m. 49, beat 4 to m. 50, beat 1 | 826 | unclassified | dominant7 |
| m. 49, beat 4 to m. 50, beat 1 | 832 | unclassified | dominant7 |
| m. 49, beat 4 to m. 50, beat 1 | 835 | anticipation | dominant7 |
| m. 50, beat 1 to m. 51, beat 1 | 836 | unclassified | I |
| m. 50, beat 1 to m. 51, beat 1 | 839 | unclassified | I |
| m. 51, beat 3 to m. 51, beat 4 | 861 | unclassified | diminished |
| m. 51, beat 4 to m. 52, beat 1 | 861 | suspension | dominant7 |
| m. 52, beat 1 to m. 52, beat 3 | 865 | unclassified | IV |
| m. 52, beat 1 to m. 52, beat 3 | 867 | unclassified | IV |
| m. 52, beat 4 to m. 53, beat 1 | 879 | unclassified | diminished7 |
| m. 52, beat 4 to m. 53, beat 1 | 881 | unclassified | diminished7 |
| m. 52, beat 4 to m. 53, beat 1 | 882 | anticipation | diminished7 |
| m. 53, beat 1 to m. 54, beat 1 | 885 | unclassified | iii |
| m. 53, beat 1 to m. 54, beat 1 | 888 | unclassified | iii |
| m. 53, beat 1 to m. 54, beat 1 | 890 | appoggiatura | iii |
| m. 53, beat 1 to m. 54, beat 1 | 891 | unclassified | iii |
| m. 53, beat 1 to m. 54, beat 1 | 898 | escape-tone | iii |
| m. 54, beat 1 to m. 55, beat 1 | 902 | unclassified | I |
| m. 55, beat 3 to m. 55, beat 4 | 927 | anticipation | diminished |
| m. 55, beat 4 to m. 56, beat 1 | 923 | unclassified | I |
| m. 55, beat 4 to m. 56, beat 1 | 929 | unclassified | I |
| m. 55, beat 4 to m. 56, beat 1 | 930 | appoggiatura | I |
| m. 55, beat 4 to m. 56, beat 1 | 931 | unclassified | I |
| m. 56, beat 1 to m. 57, beat 1 | 933 | unclassified | IV |
| m. 56, beat 1 to m. 57, beat 1 | 942 | unclassified | IV |
| m. 56, beat 1 to m. 57, beat 1 | 945 | unclassified | IV |
| m. 56, beat 1 to m. 57, beat 1 | 948 | unclassified | IV |
| m. 56, beat 1 to m. 57, beat 1 | 949 | unclassified | IV |
| m. 56, beat 1 to m. 57, beat 1 | 952 | appoggiatura | IV |
| m. 56, beat 1 to m. 57, beat 1 | 954 | escape-tone | IV |
| m. 56, beat 1 to m. 57, beat 1 | 956 | unclassified | IV |
| m. 56, beat 1 to m. 57, beat 1 | 957 | unclassified | IV |
| m. 57, beat 1 to m. 57, beat 2 | 957 | unclassified | V |
| m. 57, beat 1 to m. 57, beat 2 | 961 | neighbor | V |
| m. 57, beat 1 to m. 57, beat 2 | 964 | unclassified | V |
| m. 57, beat 2 to m. 57, beat 4 | 968 | unclassified | IV |
| m. 57, beat 2 to m. 57, beat 4 | 969 | escape-tone | IV |
| m. 57, beat 2 to m. 57, beat 4 | 970 | unclassified | IV |
| m. 57, beat 2 to m. 57, beat 4 | 971 | unclassified | IV |
| m. 57, beat 2 to m. 57, beat 4 | 972 | unclassified | IV |
| m. 57, beat 4 to m. 58, beat 1 | 967 | unclassified | dominant7 |
| m. 58, beat 1 to m. 59, beat 1 | 974 | unclassified | I |
| m. 59, beat 1 to m. 60, beat 1 | 991 | appoggiatura | dominant7 |
| m. 59, beat 1 to m. 60, beat 1 | 995 | unclassified | dominant7 |
| m. 59, beat 1 to m. 60, beat 1 | 997 | passing | dominant7 |
| m. 60, beat 1 to m. 61, beat 1 | 1012 | unclassified | I |
| m. 60, beat 1 to m. 61, beat 1 | 1031 | unclassified | I |
| m. 60, beat 1 to m. 61, beat 1 | 1034 | unclassified | I |
| m. 60, beat 1 to m. 61, beat 1 | 1035 | unclassified | I |
| m. 60, beat 1 to m. 61, beat 1 | 1037 | unclassified | I |
| m. 60, beat 1 to m. 61, beat 1 | 1039 | unclassified | I |
| m. 60, beat 1 to m. 61, beat 1 | 1042 | appoggiatura | I |
| m. 60, beat 1 to m. 61, beat 1 | 1043 | unclassified | I |
| m. 61, beat 1 to m. 62, beat 1 | 1043 | unclassified | dominant7 |
| m. 61, beat 1 to m. 62, beat 1 | 1045 | unclassified | dominant7 |
| m. 61, beat 1 to m. 62, beat 1 | 1049 | unclassified | dominant7 |
| m. 61, beat 1 to m. 62, beat 1 | 1051 | unclassified | dominant7 |
| m. 62, beat 1 to m. 63, beat 1 | 1067 | unclassified | I |
| m. 62, beat 1 to m. 63, beat 1 | 1072 | unclassified | I |
| m. 62, beat 1 to m. 63, beat 1 | 1075 | unclassified | I |
| m. 63, beat 1 to m. 63, beat 4 | 1089 | unclassified | major |
| m. 63, beat 1 to m. 63, beat 4 | 1093 | unclassified | major |
| m. 63, beat 1 to m. 63, beat 4 | 1094 | unclassified | major |
| m. 63, beat 1 to m. 63, beat 4 | 1099 | unclassified | major |
| m. 63, beat 1 to m. 63, beat 4 | 1098 | unclassified | major |
| m. 63, beat 1 to m. 63, beat 4 | 1100 | unclassified | major |
| m. 63, beat 1 to m. 63, beat 4 | 1101 | escape-tone | major |
| m. 63, beat 1 to m. 63, beat 4 | 1102 | escape-tone | major |
| m. 63, beat 1 to m. 63, beat 4 | 1104 | unclassified | major |
| m. 63, beat 1 to m. 63, beat 4 | 1105 | anticipation | major |
| m. 63, beat 4 to m. 64, beat 1 | 1109 | unclassified | dominant7 |
| m. 64, beat 1 to m. 64, beat 2 | 1109 | unclassified | I |
| m. 64, beat 3 to m. 65, beat 1 | 1119 | unclassified | ii |
| m. 64, beat 3 to m. 65, beat 1 | 1121 | unclassified | ii |
| m. 64, beat 3 to m. 65, beat 1 | 1125 | appoggiatura | ii |
| m. 64, beat 3 to m. 65, beat 1 | 1127 | unclassified | ii |
| m. 64, beat 3 to m. 65, beat 1 | 1132 | unclassified | ii |
| m. 65, beat 1 to m. 65, beat 3 | 1131 | unclassified | I |
| m. 65, beat 1 to m. 65, beat 3 | 1138 | unclassified | I |
| m. 65, beat 1 to m. 65, beat 3 | 1139 | passing | I |
| m. 65, beat 1 to m. 65, beat 3 | 1142 | passing | I |
| m. 65, beat 3 to m. 65, beat 4 | 1147 | passing | iii |
| m. 65, beat 3 to m. 65, beat 4 | 1150 | passing | iii |
| m. 65, beat 3 to m. 65, beat 4 | 1151 | passing | iii |
| m. 65, beat 4 to m. 66, beat 1 | 1157 | appoggiatura | dominant7 |
| m. 65, beat 4 to m. 66, beat 1 | 1160 | appoggiatura | dominant7 |

## Cadences

The library's standard-harmony analysis found 9 cadences from the chords, the meter and the soprano. Position is where the arrival begins. Key is the key region of the arrival. Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added before a section start.

| Position | Type | Key | Soprano | Beat strength | Boundary |
| --- | --- | --- | --- | --- | --- |
| m. 3, beat 1 | half | Eb major | 1 | strong | no |
| m. 7, beat 1 | half | Eb major | 6 | strong | yes |
| m. 15, beat 1 | half | Eb major | 6 | strong | no |
| m. 18, beat 1 | imperfect-authentic | Eb major | 2 | strong | no |
| m. 26, beat 1 | half | Bb major | b6 | strong | no |
| m. 34, beat 1 | half | Eb major | 7 | strong | yes |
| m. 37, beat 1 | perfect-authentic | Eb major | 1 | strong | no |
| m. 57, beat 1 | half | Eb major | b6 | strong | no |
| m. 61, beat 1 | half | Eb major | 6 | strong | yes |

### Competing readings

A cadence the evidence does not settle has two readings, the likelier first, each with its share of the evidence.

- m7 b1 half cadence (50%), or perfect authentic cadence at m8 b1 (50%)
- m34 b1 half cadence (65%), or imperfect authentic cadence at m35 b1 (35%)
- m61 b1 half cadence (65%), or imperfect authentic cadence at m62 b1 (35%)

### Cadence plan

1. m. 3, beat 1: half in Eb major
2. m. 7, beat 1: half in Eb major
3. m. 15, beat 1: half in Eb major
4. m. 18, beat 1: imperfect-authentic in Eb major
5. m. 26, beat 1: half in Bb major
6. m. 34, beat 1: half in Eb major
7. m. 37, beat 1: perfect-authentic in Eb major
8. m. 57, beat 1: half in Eb major
9. m. 61, beat 1: half in Eb major

**Counts:** half 7, imperfect-authentic 1, perfect-authentic 1.

## Phrases

The library's standard-form analysis found 12 phrases from the soprano, the meter and the cadences. Length counts the measures of the phrase proper; Ends is what closed it: its cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in the piece; Expansion is a lengthening or shortening of the phrase's ending, or an extension after its cadence, with the measures it adds or removes.

| Phrase | Measures | Length | Cadence | Ends | Role | Expansion |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 3 | 3 | half (m. 3, beat 1) | cadence | beginning | - |
| 2 | mm. 4 to 7 | 4 | half (m. 7, beat 1) | cadence | middle | - |
| 3 | mm. 8 to 15 | 8 | half (m. 15, beat 1) | cadence | middle | - |
| 4 | mm. 16 to 18 | 3 | imperfect-authentic (m. 18, beat 1) | cadence | middle | - |
| 5 | mm. 19 to 26 | 8 | half (m. 26, beat 1) | cadence | middle | - |
| 6 | mm. 27 to 34 | 8 | half (m. 34, beat 1) | cadence | middle | - |
| 7 | mm. 35 to 37 | 3 | perfect-authentic (m. 37, beat 1) | cadence | middle | - |
| 8 | mm. 38 to 41 | 4 | none | discontinuity | middle | - |
| 9 | mm. 42 to 57 | 16 | half (m. 57, beat 1) | cadence | middle | internal +8 (mm. 46 to 57) |
| 10 | mm. 58 to 61 | 4 | half (m. 61, beat 1) | cadence | middle | - |
| 11 | mm. 62 to 66 | 5 | none | discontinuity | middle | - |
| 12 | m. 67 | 1 | none | none | ending | - |

- **Phrase 1:** ideas basic-idea (mm. 1 to 3); phrase model D (m. 3, beat 1)
- **Phrase 2:** ideas basic-idea (mm. 4 to 5), contrasting-idea (mm. 6 to 7); phrase model Tb (m. 4, beat 1), Tb (m. 5, beat 1), Tb (m. 6, beat 1), D (m. 7, beat 1)
- **Phrase 3:** ideas basic-idea (mm. 8 to 9), contrasting-idea (mm. 10 to 11), contrasting-idea (mm. 12 to 13), contrasting-idea (mm. 14 to 15); subphrases mm. 8 to 11, mm. 12 to 15; phrase model Tb (m. 8, beat 1), Tb (m. 9, beat 1), PD (m. 14, beat 1), D (m. 15, beat 1)
- **Phrase 4:** ideas basic-idea (mm. 16 to 18); phrase model Tb (m. 16, beat 1), D (m. 17, beat 2), Te (m. 18, beat 1)
- **Phrase 5:** ideas basic-idea (mm. 19 to 20), contrasting-idea (mm. 21 to 22), contrasting-idea (mm. 23 to 24), contrasting-idea (mm. 25 to 26); subphrases mm. 19 to 22, mm. 23 to 26; phrase model D (m. 26, beat 1)
- **Phrase 6:** ideas basic-idea (mm. 27 to 28), contrasting-idea (mm. 29 to 30), contrasting-idea (mm. 31 to 32), contrasting-idea (mm. 33 to 34); subphrases mm. 27 to 30, mm. 31 to 34; phrase model Tb (m. 27, beat 1), PD (m. 33, beat 3), D (m. 34, beat 1)
- **Phrase 7:** ideas basic-idea (mm. 35 to 37); phrase model Tb (m. 35, beat 1), D (m. 36, beat 4), Te (m. 37, beat 1)
- **Phrase 8:** ideas basic-idea (mm. 38 to 39), contrasting-idea (mm. 40 to 41)
- **Phrase 9:** ideas basic-idea (mm. 42 to 43), contrasting-idea (mm. 44 to 45), contrasting-idea (mm. 46 to 47), contrasting-idea (mm. 48 to 49), contrasting-idea (mm. 50 to 51), contrasting-idea (mm. 52 to 53), contrasting-idea (mm. 54 to 55), contrasting-idea (mm. 56 to 57); subphrases mm. 42 to 45, mm. 46 to 57; phrase model Tb (m. 42, beat 1), PD (m. 56, beat 1), D (m. 57, beat 1)
- **Phrase 10:** ideas basic-idea (mm. 58 to 59), contrasting-idea (mm. 60 to 61); phrase model Tb (m. 58, beat 1), D (m. 61, beat 1)
- **Phrase 11:** ideas basic-idea (mm. 62 to 63), contrasting-idea (mm. 64 to 66)
- **Phrase 12:** ideas basic-idea (m. 67)

## Themes

The library's standard-form analysis found 4 themes from the phrases, their ideas and the cadences. Form is the library's name for the pairing of the two parts; Commonality is how common that pairing is; Traits are what the library finds in the continuation.

| Theme | Form | Commonality | Measures | Length | Parts | Traits |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | hybrid | slightly-less-common | mm. 8 to 15 | 8 | compound-basic-idea (mm. 8 to 11), continuation (mm. 12 to 15, half (m. 15, beat 1)) | - |
| 2 | hybrid | slightly-less-common | mm. 19 to 26 | 8 | compound-basic-idea (mm. 19 to 22), continuation (mm. 23 to 26, half (m. 26, beat 1)) | - |
| 3 | hybrid | slightly-less-common | mm. 27 to 34 | 8 | compound-basic-idea (mm. 27 to 30), continuation (mm. 31 to 34, half (m. 34, beat 1)) | - |
| 4 | hybrid | slightly-less-common | mm. 42 to 57 | 16 | compound-basic-idea (mm. 42 to 45), continuation (mm. 46 to 57, half (m. 57, beat 1)) | - |

## Form plan

The kernel's form structure detection grouped the phrases into 3 sections, closing a section at each phrase that ends on a perfect or imperfect authentic cadence; the phrases after the last such cadence form the last section. Sections are lettered by first appearance, and a section whose opening theme returns an earlier section's material takes that letter with a prime. Key is the key the section opens in.

| Section | Measures | Key | Phrases | Closing cadence | Returns |
| --- | --- | --- | --- | --- | --- |
| A | mm. 1 to 18 | Eb major | 1 to 4 | imperfect-authentic (m. 18, beat 1) | - |
| B | mm. 19 to 37 | Eb major | 5 to 7 | perfect-authentic (m. 37, beat 1) | - |
| C | mm. 38 to 67 | Eb major | 8 to 12 | none | - |

### Outline

- form (mm. 1 to 67)
  - section A (mm. 1 to 18, Eb major)
    - phrase 1 (mm. 1 to 3)
      - idea basic-idea (mm. 1 to 3)
    - phrase 2 (mm. 4 to 7)
      - idea basic-idea (mm. 4 to 5)
      - idea contrasting-idea (mm. 6 to 7)
    - phrase 3 (mm. 8 to 15)
      - subphrase 1 (mm. 8 to 11)
        - idea basic-idea (mm. 8 to 9)
        - idea contrasting-idea (mm. 10 to 11)
      - subphrase 2 (mm. 12 to 15)
        - idea contrasting-idea (mm. 12 to 13)
        - idea contrasting-idea (mm. 14 to 15)
    - phrase 4 (mm. 16 to 18)
      - idea basic-idea (mm. 16 to 18)
  - section B (mm. 19 to 37, Eb major)
    - phrase 5 (mm. 19 to 26)
      - subphrase 1 (mm. 19 to 22)
        - idea basic-idea (mm. 19 to 20)
        - idea contrasting-idea (mm. 21 to 22)
      - subphrase 2 (mm. 23 to 26)
        - idea contrasting-idea (mm. 23 to 24)
        - idea contrasting-idea (mm. 25 to 26)
    - phrase 6 (mm. 27 to 34)
      - subphrase 1 (mm. 27 to 30)
        - idea basic-idea (mm. 27 to 28)
        - idea contrasting-idea (mm. 29 to 30)
      - subphrase 2 (mm. 31 to 34)
        - idea contrasting-idea (mm. 31 to 32)
        - idea contrasting-idea (mm. 33 to 34)
    - phrase 7 (mm. 35 to 37)
      - idea basic-idea (mm. 35 to 37)
  - section C (mm. 38 to 67, Eb major)
    - phrase 8 (mm. 38 to 41)
      - idea basic-idea (mm. 38 to 39)
      - idea contrasting-idea (mm. 40 to 41)
    - phrase 9 (mm. 42 to 57)
      - subphrase 1 (mm. 42 to 45)
        - idea basic-idea (mm. 42 to 43)
        - idea contrasting-idea (mm. 44 to 45)
      - subphrase 2 (mm. 46 to 57)
        - idea contrasting-idea (mm. 46 to 47)
        - idea contrasting-idea (mm. 48 to 49)
        - idea contrasting-idea (mm. 50 to 51)
        - idea contrasting-idea (mm. 52 to 53)
        - idea contrasting-idea (mm. 54 to 55)
        - idea contrasting-idea (mm. 56 to 57)
    - phrase 10 (mm. 58 to 61)
      - idea basic-idea (mm. 58 to 59)
      - idea contrasting-idea (mm. 60 to 61)
    - phrase 11 (mm. 62 to 66)
      - idea basic-idea (mm. 62 to 63)
      - idea contrasting-idea (mm. 64 to 66)
    - phrase 12 (m. 67)
      - idea basic-idea (m. 67)

## Motives

The library's motive analysis found 1 motive family in the soprano, the highest sounding note at each attack. A family is a short figure that recurs; Notes is the length of the figure, Pattern whether its occurrences match by scale steps or by semitones, Measures the measures the occurrences start in, Phrases the phrases they lie in (an occurrence across a phrase boundary lies in none), and Transformations how many occurrences are of each kind the library names against the first occurrence (exact, real-transposed, tonal-transposed, inverted, retrograde, augmented, diminished, fragmented, varied).

| Family | Notes | Degrees | Pattern | Occurrences | Measures | Phrases | Transformations |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 4 | 1 1 b3 1 | scale steps | 3 | 31, 50, 58 | 6, 9, 10 | exact 1, real-transposed 2 |

## Melody

The library's melody adapter read the soprano of each phrase as a line of notes: the soprano notes that start in the phrase. Range is the lowest and highest note and the semitones between them; Climax is the first note at the highest pitch, with how many notes sound it when it is not one; Contour counts the intervals between consecutive notes that go up, go down and repeat, and how many of them are steps and leaps.

| Phrase | Measures | Notes | Range | Climax | Up | Down | Same | Steps | Leaps |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 3 | 6 | A#4 to F5 (7) | F5, m. 2, beat 4.02 | 2 | 3 | 0 | 3 | 2 |
| 2 | mm. 4 to 7 | 7 | D#5 to C6 (9) | C6, m. 7, beat 1.97 | 4 | 2 | 0 | 2 | 4 |
| 3 | mm. 8 to 15 | 17 | B4 to C6 (13) | C6, m. 15, beat 2.15 | 4 | 10 | 2 | 9 | 3 |
| 4 | mm. 16 to 18 | 8 | D#5 to C6 (9) | C6, m. 17, beat 1.71 | 3 | 3 | 1 | 4 | 2 |
| 5 | mm. 19 to 26 | 21 | D#5 to D#6 (12) | D#6, m. 22, beat 2.09 | 9 | 9 | 1 | 12 | 5 |
| 6 | mm. 27 to 34 | 21 | D4 to A#5 (20) | A#5, m. 27, beat 2.01 | 7 | 7 | 5 | 11 | 3 |
| 7 | mm. 35 to 37 | 6 | D#5 to D#6 (12) | D#6, m. 37, beat 1.99, 2 notes | 2 | 0 | 3 | 0 | 2 |
| 8 | mm. 38 to 41 | 13 | F5 to G#6 (15) | G#6, m. 38, beat 1.34 | 4 | 8 | 0 | 7 | 5 |
| 9 | mm. 42 to 57 | 47 | F4 to G6 (26) | G6, m. 43, beat 1.97, 2 notes | 18 | 25 | 2 | 12 | 28 |
| 10 | mm. 58 to 61 | 15 | G4 to G6 (24) | G6, m. 60, beat 3.39 | 3 | 10 | 1 | 6 | 5 |
| 11 | mm. 62 to 66 | 16 | G4 to A#6 (27) | A#6, m. 65, beat 1 | 9 | 5 | 1 | 4 | 9 |
| 12 | m. 67 | 1 | D#6 to D#6 (0) | D#6, m. 66, beat 4.99 | 0 | 0 | 0 | 0 | 0 |

### Leaps

A leap is an interval of a third or larger. The library's rule asks a leap of a fourth or larger to be followed by a step in the opposite direction: such a leap is Recovered when one follows, Unrecovered when a step the same way, a repeated note or another leap does, and Not judged when nothing follows it; a third asks nothing (Not required). Consecutive counts the pairs of leaps in a row the library's rule finds fault with.

| Phrase | Leaps | Recovered | Unrecovered | Not required | Not judged | Consecutive |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 2 | 0 | 1 | 1 | 0 | 0 |
| 2 | 4 | 2 | 0 | 2 | 0 | 1 |
| 3 | 3 | 1 | 0 | 1 | 1 | 0 |
| 4 | 2 | 1 | 0 | 1 | 0 | 0 |
| 5 | 5 | 2 | 1 | 0 | 2 | 0 |
| 6 | 3 | 1 | 1 | 1 | 0 | 0 |
| 7 | 2 | 0 | 2 | 0 | 0 | 0 |
| 8 | 5 | 1 | 1 | 3 | 0 | 2 |
| 9 | 28 | 3 | 11 | 13 | 1 | 16 |
| 10 | 5 | 0 | 4 | 1 | 0 | 1 |
| 11 | 9 | 0 | 6 | 3 | 0 | 5 |
| 12 | 0 | 0 | 0 | 0 | 0 | 0 |

### Non-chord tones

Each melody note is set against the chord of the segment it starts in, as the library's chord identification reads it: a Chord tone is one of the chord's tones, a Non-chord tone is one it left over and named, and Unclassified notes are in a segment with no chord or were left unnamed. By type counts the non-chord tones under the library's names.

| Phrase | Notes | Chord tones | Non-chord tones | Unclassified | By type |
| --- | --- | --- | --- | --- | --- |
| 1 | 6 | 4 | 2 | 0 | passing 1, neighbor 1 |
| 2 | 7 | 5 | 1 | 1 | appoggiatura 1 |
| 3 | 17 | 9 | 6 | 2 | passing 5, neighbor 1 |
| 4 | 8 | 5 | 2 | 1 | appoggiatura 2 |
| 5 | 21 | 11 | 4 | 6 | passing 1, neighbor 2, appoggiatura 1 |
| 6 | 21 | 16 | 4 | 1 | passing 2, neighbor 1, escape-tone 1 |
| 7 | 6 | 6 | 0 | 0 | - |
| 8 | 13 | 6 | 3 | 4 | neighbor 2, appoggiatura 1 |
| 9 | 47 | 33 | 6 | 8 | passing 2, appoggiatura 3, escape-tone 1 |
| 10 | 15 | 10 | 3 | 2 | passing 2, appoggiatura 1 |
| 11 | 16 | 11 | 4 | 1 | passing 3, appoggiatura 1 |
| 12 | 1 | 1 | 0 | 0 | - |

### Tendencies

The library's melodic tendencies are soft preferences a well-formed line usually shows: Pitch proximity (steps outnumber leaps, small leaps outnumber large ones), Step declination (descending steps outnumber ascending ones), Step inertia (the line continues in its direction at least as often as it reverses), Melodic regression (extreme notes turn back toward the middle) and Melodic arch (the line rises to a climax in its middle half and ends below it); a phrase either shows a tendency or does not. The seventh degree column counts the notes the library labels the seventh degree that another note follows, judged by its leading tone rule: a leading tone goes up to the tonic by step, and a seventh a whole step below the tonic does not. Followed is the count that did, Broken the count that did not, with where each broken one stands.

| Phrase | Pitch proximity | Step declination | Step inertia | Melodic regression | Melodic arch | Seventh degree followed | Seventh degree broken |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | does not hold | holds | holds | holds | holds | 0 | 1 (m. 3, beat 2.98) |
| 2 | does not hold | does not hold | does not hold | holds | holds | 0 | 0 |
| 3 | holds | holds | holds | does not hold | does not hold | 0 | 2 (m. 10, beat 1.08; m. 14, beat 1.03) |
| 4 | holds | holds | holds | holds | holds | 0 | 0 |
| 5 | holds | holds | does not hold | does not hold | holds | 0 | 2 (m. 22, beat 3.97; m. 23, beat 3.37) |
| 6 | does not hold | does not hold | holds | does not hold | does not hold | 0 | 3 (m. 32, beat 1.33; m. 32, beat 2.33; m. 34, beat 1.04) |
| 7 | does not hold | holds | holds | holds | does not hold | 0 | 0 |
| 8 | holds | holds | does not hold | does not hold | does not hold | 0 | 0 |
| 9 | does not hold | holds | holds | does not hold | does not hold | 0 | 6 (m. 44, beat 4.93; m. 45, beat 3.01; m. 48, beat 3.83; m. 49, beat 3; m. 52, beat 1.02; m. 56, beat 1.01) |
| 10 | does not hold | holds | holds | does not hold | holds | 0 | 0 |
| 11 | does not hold | does not hold | holds | holds | holds | 1 | 1 (m. 65, beat 3) |
| 12 | holds | holds | holds | holds | does not hold | 0 | 0 |

### Development

Each family's occurrences in time order, as the measure they start in and the kind the library names against the family's prototype, which is the original.

- **Family 1:** m31 original > m50 real-transposed > m58 real-transposed

## Progression patterns

22 recurring progressions of two to six chords, counted over the library's Roman numerals: a held chord counts once, and a progression does not cross an unlabelled chord or a change of key. Ranked by count, then length, then first occurrence; the top 10 are shown. Measures are where each occurrence begins and ends.

| Rank | Progression | Count | Measures |
| --- | --- | --- | --- |
| 1 | V7 > I | 13 | mm. 3 to 6, mm. 15 to 16, mm. 17 to 18, mm. 26 to 27, mm. 36 to 37, mm. 38 to 39, mm. 45 to 49, mm. 49 to 50, mm. 57 to 58, mm. 59 to 60, mm. 61 to 62, mm. 63 to 64, mm. 65 to 67 |
| 2 | I > V7 | 6 | mm. 21 to 22, mm. 25 to 26, mm. 35 to 36, mm. 37 to 38, mm. 58 to 59, mm. 60 to 61 |
| 3 | I > V7 > I | 4 | mm. 25 to 27, mm. 37 to 39, mm. 58 to 60, mm. 60 to 62 |
| 4 | iii > I | 4 | mm. 11 to 13, mm. 22 to 23, mm. 32 to 33, mm. 53 to 54 |
| 5 | vii° > V7 | 4 | m. 17, m. 19, m. 36, m. 49 |
| 6 | V > vii∅7 > V7 > I | 3 | mm. 3 to 6, mm. 15 to 16, mm. 45 to 49 |
| 7 | V7 > I > vii° > V7 | 3 | mm. 15 to 17, mm. 17 to 19, mm. 45 to 49 |
| 8 | V7 > I > V7 > I | 3 | mm. 36 to 39, mm. 57 to 60, mm. 59 to 62 |
| 9 | vii° > V7 > I | 3 | mm. 17 to 18, mm. 36 to 37, mm. 49 to 50 |
| 10 | V > I | 3 | mm. 1 to 2, mm. 7 to 9, mm. 34 to 35 |

## Harmonic rhythm

How often the harmony changes. A change is a chord whose base Roman numeral or key differs from the chord before it, counted in the measure it starts in. Changes lists the measures of each row in order; Mean is changes per measure over the row.

| Measures | Changes | Mean |
| --- | --- | --- |
| mm. 1 to 4 | 1, 1, 3, 1 | 1.5 |
| mm. 5 to 8 | 0, 0, 1, 1 | 0.5 |
| mm. 9 to 12 | 0, 1, 3, 1 | 1.25 |
| mm. 13 to 16 | 0, 1, 3, 1 | 1.25 |
| mm. 17 to 20 | 2, 1, 2, 1 | 1.5 |
| mm. 21 to 24 | 1, 2, 1, 2 | 1.5 |
| mm. 25 to 28 | 2, 1, 1, 3 | 1.75 |
| mm. 29 to 32 | 1, 1, 1, 1 | 1 |
| mm. 33 to 36 | 2, 1, 1, 3 | 1.75 |
| mm. 37 to 40 | 1, 1, 1, 1 | 1 |
| mm. 41 to 44 | 4, 3, 1, 2 | 2.5 |
| mm. 45 to 48 | 3, 1, 0, 0 | 1 |
| mm. 49 to 52 | 2, 1, 3, 2 | 2 |
| mm. 53 to 56 | 1, 1, 3, 1 | 1.5 |
| mm. 57 to 60 | 3, 1, 1, 1 | 1.5 |
| mm. 61 to 64 | 1, 1, 2, 3 | 1.75 |
| mm. 65 to 67 | 3, 1, 0 | 1.33 |

**Mean:** 1.45 changes per measure over 67 measures.

### Approach to cadences

Changes per measure over the two measures before each cadence's arrival (Approach), against the two measures before those (Before). Faster is yes when the approach changes harmony more often.

| Cadence | Arrival | Before | Approach | Faster |
| --- | --- | --- | --- | --- |
| 1 | m. 3 | none | 1 | no |
| 2 | m. 7 | 2 | 0 | no |
| 3 | m. 15 | 2 | 0.5 | no |
| 4 | m. 18 | 2 | 1.5 | no |
| 5 | m. 26 | 1.5 | 2 | yes |
| 6 | m. 34 | 1 | 1.5 | yes |
| 7 | m. 37 | 1.5 | 2 | yes |
| 8 | m. 57 | 1 | 2 | yes |
| 9 | m. 61 | 2 | 1 | no |

## Rhythm and meter

**Meter fit:** 12/8, scale 1, no pickup (fit 0.91); no better reading found

### Density

The library's density profile of each phrase, for all its notes and for each line that has a note in it (the soprano and bass of the outer voices, and Other for the rest). A note is in a phrase when its onset is; a chord is one attack. Attacks per beat is read measure by measure over the beats the phrase spans in it, as a mean and the largest; Mean gap is the mean distance between attacks in whole notes; On the beat is the share of attacks exactly on a beat. Durations lists the notes' lengths in whole notes, shortest first, with how many notes have each. The patterns that recur are the motive families' rhythms (see Motives).

| Phrase | Measures | Line | Notes | Attacks | Per beat | Max per beat | Mean gap | On the beat | Durations |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 3 | all | 32 | 32 | 2.67 | 4 | 0.1 | 28.1% | 1/16 (1), 1/8 (13), 1/4 (9), 161/480 (1), 35/96 (1), 3/8 (3), 61/160 (1), 37/96 (1), 191/480 (1), 55/48 (1) |
| 1 | mm. 1 to 3 | soprano | 6 | 6 | 0.5 | 0.75 | 0.6 | 16.7% | 1/16 (1), 161/480 (1), 3/8 (1), 61/160 (1), 191/480 (1), 55/48 (1) |
| 1 | mm. 1 to 3 | bass | 9 | 9 | 0.75 | 1.25 | 0.36 | 88.9% | 1/8 (4), 1/4 (3), 3/8 (2) |
| 1 | mm. 1 to 3 | other | 17 | 17 | 1.42 | 2.25 | 0.16 | 0.0% | 1/8 (9), 1/4 (6), 35/96 (1), 37/96 (1) |
| 2 | mm. 4 to 7 | all | 57 | 54 | 3.38 | 3.75 | 0.11 | 29.6% | 1/8 (25), 1/4 (19), 35/96 (1), 89/240 (1), 3/8 (4), 31/80 (1), 187/480 (1), 13/32 (1), 23/40 (1), 3/4 (1), 37/32 (1), 643/480 (1) |
| 2 | mm. 4 to 7 | soprano | 7 | 7 | 0.44 | 0.5 | 0.87 | 28.6% | 89/240 (1), 31/80 (1), 187/480 (1), 23/40 (1), 3/4 (1), 37/32 (1), 643/480 (1) |
| 2 | mm. 4 to 7 | bass | 16 | 16 | 1 | 1 | 0.38 | 100.0% | 1/8 (5), 1/4 (7), 3/8 (4) |
| 2 | mm. 4 to 7 | other | 34 | 34 | 2.12 | 2.5 | 0.17 | 2.9% | 1/8 (20), 1/4 (12), 35/96 (1), 13/32 (1) |
| 3 | mm. 8 to 15 | all | 126 | 121 | 3.78 | 4.5 | 0.1 | 26.4% | 11/480 (1), 1/32 (3), 17/480 (2), 11/240 (1), 23/480 (2), 1/20 (1), 1/16 (1), 1/8 (49), 23/160 (1), 7/32 (1), 1/4 (38), 41/160 (1), 5/16 (1), 43/120 (1), 29/80 (1), 59/160 (1), ... |
| 3 | mm. 8 to 15 | soprano | 17 | 17 | 0.53 | 0.75 | 0.68 | 11.8% | 17/480 (1), 11/240 (1), 1/16 (1), 23/160 (1), 29/80 (1), 89/240 (1), 3/8 (1), 19/48 (1), 13/32 (1), 33/80 (1), 47/96 (1), 151/240 (1), 101/160 (1), 111/160 (1), 89/120 (1), 9/8 (1), ... |
| 3 | mm. 8 to 15 | bass | 30 | 30 | 0.94 | 1 | 0.4 | 100.0% | 1/8 (8), 1/4 (13), 3/8 (9) |
| 3 | mm. 8 to 15 | other | 79 | 78 | 2.44 | 3.25 | 0.15 | 5.1% | 11/480 (1), 1/32 (3), 17/480 (1), 23/480 (2), 1/20 (1), 1/8 (41), 7/32 (1), 1/4 (25), 41/160 (1), 5/16 (1), 43/120 (1), 59/160 (1) |
| 4 | mm. 16 to 18 | all | 56 | 55 | 4.58 | 5.5 | 0.08 | 21.8% | 1/20 (1), 11/160 (1), 3/40 (1), 19/160 (1), 1/8 (31), 13/96 (2), 17/120 (1), 73/480 (3), 13/80 (2), 43/240 (1), 101/480 (1), 7/30 (1), 23/96 (1), 39/160 (1), 1/4 (6), 49/80 (1), ... |
| 4 | mm. 16 to 18 | soprano | 9 | 9 | 0.75 | 1 | 0.55 | 0.0% | 1/20 (1), 19/160 (1), 1/8 (1), 13/96 (1), 17/120 (1), 73/480 (1), 7/30 (1), 49/80 (1), 9/10 (1) |
| 4 | mm. 16 to 18 | bass | 12 | 12 | 1 | 1 | 0.38 | 100.0% | 1/8 (7), 1/4 (5) |
| 4 | mm. 16 to 18 | other | 35 | 35 | 2.92 | 3.5 | 0.12 | 0.0% | 11/160 (1), 3/40 (1), 1/8 (23), 13/96 (1), 73/480 (2), 13/80 (2), 43/240 (1), 101/480 (1), 23/96 (1), 39/160 (1), 1/4 (1) |
| 5 | mm. 19 to 26 | all | 140 | 125 | 3.91 | 4.75 | 0.1 | 24.8% | 1/32 (1), 3/80 (1), 19/480 (1), 7/160 (1), 13/240 (1), 7/120 (2), 19/240 (1), 3/32 (4), 47/480 (1), 49/480 (1), 53/480 (1), 9/80 (1), 19/160 (1), 29/240 (1), 1/8 (59), 13/96 (1), ... |
| 5 | mm. 19 to 26 | soprano | 20 | 20 | 0.62 | 1 | 0.61 | 10.0% | 19/480 (1), 7/120 (1), 3/32 (1), 49/480 (1), 9/80 (1), 1/8 (1), 13/96 (1), 67/480 (1), 73/480 (1), 5/32 (1), 1/4 (1), 3/8 (1), 91/240 (1), 23/60 (1), 37/96 (1), 187/480 (1), ... |
| 5 | mm. 19 to 26 | bass | 31 | 31 | 0.97 | 1.25 | 0.39 | 93.5% | 1/8 (14), 17/120 (1), 1/4 (16) |
| 5 | mm. 19 to 26 | other | 89 | 83 | 2.59 | 3.25 | 0.14 | 7.2% | 1/32 (1), 3/80 (1), 7/160 (1), 13/240 (1), 7/120 (1), 19/240 (1), 3/32 (3), 47/480 (1), 53/480 (1), 19/160 (1), 29/240 (1), 1/8 (44), 11/80 (1), 3/20 (1), 5/32 (1), 13/80 (1), ... |
| 6 | mm. 27 to 34 | all | 118 | 112 | 3.5 | 4 | 0.11 | 28.6% | 1/32 (1), 1/8 (61), 5/32 (1), 3/16 (1), 23/120 (1), 9/40 (1), 1/4 (36), 133/480 (1), 173/480 (1), 29/80 (1), 89/240 (3), 3/8 (1), 91/240 (1), 61/160 (1), 47/120 (1), 13/32 (1), ... |
| 6 | mm. 27 to 34 | soprano | 21 | 21 | 0.66 | 1 | 0.56 | 23.8% | 1/32 (1), 1/8 (2), 5/32 (1), 3/16 (1), 23/120 (1), 9/40 (1), 1/4 (2), 173/480 (1), 29/80 (1), 89/240 (2), 3/8 (1), 47/120 (1), 13/32 (1), 199/480 (1), 99/160 (2), 25/32 (1), ... |
| 6 | mm. 27 to 34 | bass | 31 | 31 | 0.97 | 1.25 | 0.39 | 96.8% | 1/8 (15), 1/4 (16) |
| 6 | mm. 27 to 34 | other | 66 | 66 | 2.06 | 2.5 | 0.18 | 3.0% | 1/8 (44), 1/4 (18), 133/480 (1), 89/240 (1), 91/240 (1), 61/160 (1) |
| 7 | mm. 35 to 37 | all | 50 | 46 | 3.83 | 4 | 0.1 | 26.1% | 3/32 (1), 17/160 (1), 13/120 (1), 53/480 (1), 9/80 (2), 11/96 (1), 1/8 (29), 21/160 (1), 71/480 (1), 73/480 (1), 7/32 (1), 1/4 (7), 5/8 (1), 151/240 (1), 15/16 (1) |
| 7 | mm. 35 to 37 | soprano | 6 | 6 | 0.5 | 0.5 | 0.83 | 33.3% | 53/480 (1), 11/96 (1), 71/480 (1), 5/8 (1), 151/240 (1), 15/16 (1) |
| 7 | mm. 35 to 37 | bass | 12 | 12 | 1 | 1 | 0.38 | 100.0% | 1/8 (6), 1/4 (6) |
| 7 | mm. 35 to 37 | other | 32 | 31 | 2.58 | 3 | 0.14 | 3.2% | 3/32 (1), 17/160 (1), 13/120 (1), 9/80 (2), 1/8 (23), 21/160 (1), 73/480 (1), 7/32 (1), 1/4 (1) |
| 8 | mm. 38 to 41 | all | 96 | 78 | 4.88 | 5.25 | 0.08 | 20.5% | 1/240 (1), 7/480 (1), 1/32 (1), 1/24 (1), 23/480 (1), 5/96 (1), 9/160 (1), 1/16 (1), 1/15 (1), 11/160 (1), 37/480 (1), 43/480 (1), 3/32 (4), 47/480 (1), 53/480 (2), 9/80 (1), ... |
| 8 | mm. 38 to 41 | soprano | 13 | 13 | 0.81 | 1 | 0.47 | 15.4% | 1/24 (1), 5/96 (1), 9/160 (1), 1/16 (1), 43/480 (1), 1/8 (1), 11/80 (1), 5/32 (2), 13/80 (1), 11/30 (1), 7/16 (1), 19/30 (1) |
| 8 | mm. 38 to 41 | bass | 17 | 17 | 1.06 | 1.25 | 0.36 | 94.1% | 1/8 (5), 1/4 (11), 3/8 (1) |
| 8 | mm. 38 to 41 | other | 66 | 60 | 3.75 | 4.25 | 0.1 | 13.3% | 1/240 (1), 7/480 (1), 1/32 (1), 23/480 (1), 1/15 (1), 11/160 (1), 37/480 (1), 3/32 (4), 47/480 (1), 53/480 (2), 9/80 (1), 11/96 (1), 7/60 (1), 19/160 (1), 1/8 (32), 31/240 (1), ... |
| 9 | mm. 42 to 57 | all | 300 | 263 | 4.11 | 5.75 | 0.09 | 24.0% | 1/96 (1), 1/80 (1), 1/48 (1), 1/32 (1), 3/80 (1), 1/24 (3), 7/160 (1), 5/96 (2), 9/160 (1), 1/16 (29), 17/240 (1), 7/96 (1), 13/160 (1), 41/480 (1), 43/480 (1), 3/32 (12), ... |
| 9 | mm. 42 to 57 | soprano | 47 | 47 | 0.73 | 1.25 | 0.49 | 21.3% | 1/24 (1), 5/96 (2), 1/16 (3), 3/32 (4), 1/10 (1), 1/8 (6), 2/15 (1), 13/96 (1), 5/32 (1), 7/32 (1), 9/40 (1), 59/240 (1), 1/4 (3), 31/96 (1), 11/30 (1), 59/160 (3), ... |
| 9 | mm. 42 to 57 | bass | 62 | 62 | 0.97 | 1.25 | 0.39 | 91.9% | 1/8 (29), 1/4 (25), 3/8 (8) |
| 9 | mm. 42 to 57 | other | 191 | 177 | 2.77 | 5 | 0.13 | 7.3% | 1/96 (1), 1/80 (1), 1/48 (1), 1/32 (1), 3/80 (1), 1/24 (2), 7/160 (1), 9/160 (1), 1/16 (26), 17/240 (1), 7/96 (1), 13/160 (1), 41/480 (1), 43/480 (1), 3/32 (8), 1/10 (1), ... |
| 10 | mm. 58 to 61 | all | 91 | 76 | 4.75 | 6.25 | 0.08 | 21.1% | 1/32 (3), 17/480 (2), 1/24 (1), 1/20 (1), 1/16 (4), 11/160 (1), 17/240 (1), 7/96 (1), 13/160 (1), 41/480 (1), 43/480 (1), 3/32 (3), 1/10 (1), 5/48 (3), 19/160 (1), 1/8 (32), ... |
| 10 | mm. 58 to 61 | soprano | 15 | 15 | 0.94 | 1 | 0.4 | 26.7% | 17/240 (1), 43/480 (1), 3/32 (1), 1/10 (1), 5/48 (1), 1/8 (2), 31/240 (1), 3/20 (1), 41/240 (1), 3/16 (1), 7/32 (1), 1/4 (1), 43/160 (1), 283/480 (1) |
| 10 | mm. 58 to 61 | bass | 16 | 16 | 1 | 1 | 0.38 | 100.0% | 1/8 (3), 1/4 (9), 3/8 (4) |
| 10 | mm. 58 to 61 | other | 60 | 51 | 3.19 | 5 | 0.12 | 3.9% | 1/32 (3), 17/480 (2), 1/24 (1), 1/20 (1), 1/16 (4), 11/160 (1), 7/96 (1), 13/160 (1), 41/480 (1), 3/32 (2), 5/48 (2), 19/160 (1), 1/8 (27), 2/15 (2), 11/80 (1), 5/32 (1), ... |
| 11 | mm. 62 to 66 | all | 113 | 91 | 4.55 | 5.25 | 0.08 | 22.0% | 1/32 (1), 1/16 (16), 11/160 (1), 3/32 (2), 7/60 (1), 1/8 (43), 31/240 (1), 21/160 (2), 2/15 (2), 13/96 (2), 67/480 (2), 5/32 (1), 41/240 (1), 101/480 (1), 103/480 (1), 1/4 (27), ... |
| 11 | mm. 62 to 66 | soprano | 16 | 16 | 0.8 | 1 | 0.46 | 31.2% | 1/16 (3), 1/8 (1), 13/96 (1), 41/240 (1), 1/4 (5), 41/160 (2), 173/480 (1), 91/240 (1), 15/32 (1) |
| 11 | mm. 62 to 66 | bass | 21 | 21 | 1.05 | 1.25 | 0.36 | 95.2% | 1/8 (10), 1/4 (10), 3/8 (1) |
| 11 | mm. 62 to 66 | other | 76 | 66 | 3.3 | 4 | 0.11 | 6.1% | 1/32 (1), 1/16 (13), 11/160 (1), 3/32 (2), 7/60 (1), 1/8 (32), 31/240 (1), 21/160 (2), 2/15 (2), 13/96 (1), 67/480 (2), 5/32 (1), 101/480 (1), 103/480 (1), 1/4 (12), 1/3 (1), ... |
| 12 | m. 67 | all | 9 | 7 | 1.75 | 1.75 | 0.12 | 42.9% | 1/8 (4), 1/4 (2), 3/8 (1), 9/8 (2) |
| 12 | m. 67 | soprano | 1 | 1 | 0.25 | 0.25 | 0 | 100.0% | 9/8 (1) |
| 12 | m. 67 | bass | 3 | 3 | 0.75 | 0.75 | 0.38 | 100.0% | 1/8 (1), 1/4 (1), 3/8 (1) |
| 12 | m. 67 | other | 5 | 5 | 1.25 | 1.25 | 0.16 | 20.0% | 1/8 (3), 1/4 (1), 9/8 (1) |

### Syncopation

A note is syncopated when it is attacked at a weaker metric level and held through a stronger one that no note of its line attacks. Each line (the soprano and bass of the outer voices, and Other for the rest) is read on its own. Strength is the difference of the two levels, 1 to 4: an off-beat note held over the beat is 1, a note on a beat held over the barline is 2, an off-beat note held over the barline is 3.

| Line | Measure | Beat | Attacked at | Held through | Strength |
| --- | --- | --- | --- | --- | --- |
| soprano | 2 | 1.03 | finer | secondary-beat (m. 2, beat 3) | 3 |
| soprano | 2 | 4.02 | finer | offbeat (m. 2, beat 4.33) | 1 |
| soprano | 2 | 4.92 | finer | downbeat (m. 3, beat 1) | 4 |
| soprano | 3 | 2.98 | finer | secondary-beat (m. 3, beat 3) | 3 |
| soprano | 3 | 4.02 | finer | downbeat (m. 4, beat 1) | 4 |
| soprano | 4 | 1.05 | finer | beat (m. 4, beat 2) | 2 |
| soprano | 5 | 1.01 | finer | secondary-beat (m. 5, beat 3) | 3 |
| soprano | 6 | 4.01 | finer | offbeat (m. 6, beat 4.33) | 1 |
| soprano | 7 | 1.97 | finer | beat (m. 7, beat 2) | 2 |
| soprano | 7 | 2.96 | finer | secondary-beat (m. 7, beat 3) | 3 |
| soprano | 8 | 1.01 | finer | downbeat (m. 9, beat 1) | 4 |
| soprano | 9 | 2.99 | finer | secondary-beat (m. 9, beat 3) | 3 |
| soprano | 10 | 1.08 | finer | beat (m. 10, beat 2) | 2 |
| soprano | 10 | 2.02 | finer | secondary-beat (m. 10, beat 3) | 3 |
| soprano | 11 | 1.63 | finer | offbeat (m. 11, beat 1.67) | 1 |
| soprano | 11 | 3.98 | finer | downbeat (m. 12, beat 1) | 4 |
| soprano | 12 | 1.01 | finer | offbeat (m. 12, beat 1.33) | 1 |
| soprano | 12 | 2 | beat | secondary-beat (m. 12, beat 3) | 1 |
| soprano | 13 | 1.03 | finer | beat (m. 13, beat 2) | 2 |
| soprano | 13 | 3.03 | finer | offbeat (m. 13, beat 3.33) | 1 |
| soprano | 13 | 3.36 | finer | beat (m. 13, beat 4) | 2 |
| soprano | 14 | 1.03 | finer | offbeat (m. 14, beat 1.33) | 1 |
| soprano | 14 | 1.98 | finer | secondary-beat (m. 14, beat 3) | 3 |
| soprano | 15 | 1.58 | finer | offbeat (m. 15, beat 1.67) | 1 |
| soprano | 15 | 2.15 | finer | secondary-beat (m. 15, beat 3) | 3 |
| soprano | 16 | 1.02 | finer | secondary-beat (m. 16, beat 3) | 3 |
| soprano | 17 | 1.71 | finer | beat (m. 17, beat 2) | 2 |
| soprano | 17 | 2.03 | finer | secondary-beat (m. 17, beat 3) | 3 |
| soprano | 17 | 4.04 | finer | offbeat (m. 17, beat 4.33) | 1 |
| soprano | 18 | 1.62 | finer | offbeat (m. 18, beat 1.67) | 1 |
| soprano | 18 | 2.64 | finer | offbeat (m. 18, beat 2.67) | 1 |
| soprano | 18 | 4.01 | finer | offbeat (m. 18, beat 4.33) | 1 |
| soprano | 19 | 2.01 | finer | secondary-beat (m. 19, beat 3) | 3 |
| soprano | 19 | 4.01 | finer | offbeat (m. 19, beat 4.33) | 1 |
| soprano | 19 | 4.67 | offbeat | downbeat (m. 20, beat 1) | 3 |
| soprano | 20 | 3.98 | finer | beat (m. 20, beat 4) | 2 |
| soprano | 20 | 4.71 | finer | downbeat (m. 21, beat 1) | 4 |
| soprano | 21 | 1.02 | finer | secondary-beat (m. 21, beat 3) | 3 |
| soprano | 22 | 1.59 | finer | offbeat (m. 22, beat 1.67) | 1 |
| soprano | 22 | 2.09 | finer | secondary-beat (m. 22, beat 3) | 3 |
| soprano | 22 | 3.97 | finer | beat (m. 22, beat 4) | 2 |
| soprano | 23 | 4.64 | finer | offbeat (m. 23, beat 4.67) | 1 |
| soprano | 24 | 1.97 | finer | secondary-beat (m. 24, beat 3) | 3 |
| soprano | 24 | 4.98 | finer | downbeat (m. 25, beat 1) | 4 |
| soprano | 25 | 3.64 | finer | offbeat (m. 25, beat 3.67) | 1 |
| soprano | 25 | 4.99 | finer | downbeat (m. 26, beat 1) | 4 |
| soprano | 26 | 4.96 | finer | downbeat (m. 27, beat 1) | 4 |
| soprano | 27 | 2 | beat | secondary-beat (m. 27, beat 3) | 1 |
| soprano | 28 | 1.98 | finer | beat (m. 28, beat 2) | 2 |
| soprano | 29 | 1.02 | finer | beat (m. 29, beat 2) | 2 |
| soprano | 29 | 3.99 | finer | beat (m. 29, beat 4) | 2 |
| soprano | 30 | 1.03 | finer | offbeat (m. 30, beat 1.33) | 1 |
| soprano | 30 | 2.98 | finer | secondary-beat (m. 30, beat 3) | 3 |
| soprano | 30 | 4.01 | finer | offbeat (m. 30, beat 4.33) | 1 |
| soprano | 32 | 3.02 | finer | beat (m. 32, beat 4) | 2 |
| soprano | 32 | 4.67 | offbeat | downbeat (m. 33, beat 1) | 3 |
| soprano | 33 | 1.05 | finer | secondary-beat (m. 33, beat 3) | 3 |
| soprano | 34 | 1.04 | finer | beat (m. 34, beat 2) | 2 |
| soprano | 34 | 2.01 | finer | offbeat (m. 34, beat 2.33) | 1 |
| soprano | 34 | 4.04 | finer | offbeat (m. 34, beat 4.33) | 1 |
| soprano | 36 | 2 | beat | secondary-beat (m. 36, beat 3) | 1 |
| soprano | 36 | 3.94 | finer | beat (m. 36, beat 4) | 2 |
| soprano | 37 | 1.99 | finer | secondary-beat (m. 37, beat 3) | 3 |
| soprano | 38 | 1.98 | finer | beat (m. 38, beat 2) | 2 |
| soprano | 38 | 2.97 | finer | secondary-beat (m. 38, beat 3) | 3 |
| soprano | 38 | 3.67 | offbeat | beat (m. 38, beat 4) | 1 |
| soprano | 39 | 1.6 | finer | offbeat (m. 39, beat 1.67) | 1 |
| soprano | 39 | 2.75 | finer | secondary-beat (m. 39, beat 3) | 3 |
| soprano | 40 | 1.02 | finer | offbeat (m. 40, beat 1.33) | 1 |
| soprano | 40 | 1.98 | finer | secondary-beat (m. 40, beat 3) | 3 |
| soprano | 41 | 2 | beat | secondary-beat (m. 41, beat 3) | 1 |
| soprano | 41 | 4.28 | finer | offbeat (m. 41, beat 4.33) | 1 |
| soprano | 42 | 3.32 | finer | offbeat (m. 42, beat 3.33) | 1 |
| soprano | 42 | 3.98 | finer | beat (m. 42, beat 4) | 2 |
| soprano | 43 | 1.97 | finer | beat (m. 43, beat 2) | 2 |
| soprano | 43 | 2.97 | finer | secondary-beat (m. 43, beat 3) | 3 |
| soprano | 44 | 1.11 | finer | secondary-beat (m. 44, beat 3) | 3 |
| soprano | 44 | 3.97 | finer | beat (m. 44, beat 4) | 2 |
| soprano | 44 | 4.93 | finer | downbeat (m. 45, beat 1) | 4 |
| soprano | 45 | 1.94 | finer | beat (m. 45, beat 2) | 2 |
| soprano | 45 | 3.01 | finer | offbeat (m. 45, beat 3.33) | 1 |
| soprano | 48 | 1.03 | finer | beat (m. 48, beat 2) | 2 |
| soprano | 48 | 2.08 | finer | secondary-beat (m. 48, beat 3) | 3 |
| soprano | 48 | 3.83 | finer | beat (m. 48, beat 4) | 2 |
| soprano | 51 | 3.98 | finer | beat (m. 51, beat 4) | 2 |
| soprano | 52 | 1.02 | finer | offbeat (m. 52, beat 1.33) | 1 |
| soprano | 52 | 1.97 | finer | secondary-beat (m. 52, beat 3) | 3 |
| soprano | 53 | 1.61 | finer | offbeat (m. 53, beat 1.67) | 1 |
| soprano | 53 | 2.13 | finer | secondary-beat (m. 53, beat 3) | 3 |
| soprano | 53 | 4 | beat | downbeat (m. 54, beat 1) | 2 |
| soprano | 54 | 1.02 | finer | offbeat (m. 54, beat 1.33) | 1 |
| soprano | 54 | 1.98 | finer | secondary-beat (m. 54, beat 3) | 3 |
| soprano | 55 | 1.02 | finer | offbeat (m. 55, beat 1.33) | 1 |
| soprano | 56 | 1.01 | finer | offbeat (m. 56, beat 1.33) | 1 |
| soprano | 56 | 2.01 | finer | secondary-beat (m. 56, beat 3) | 3 |
| soprano | 56 | 4.83 | finer | downbeat (m. 57, beat 1) | 4 |
| soprano | 57 | 2.02 | finer | secondary-beat (m. 57, beat 3) | 3 |
| soprano | 58 | 1.01 | finer | beat (m. 58, beat 2) | 2 |
| soprano | 59 | 1.03 | finer | offbeat (m. 59, beat 1.33) | 1 |
| soprano | 59 | 2.02 | finer | offbeat (m. 59, beat 2.33) | 1 |
| soprano | 59 | 3.99 | finer | beat (m. 59, beat 4) | 2 |
| soprano | 60 | 1.65 | finer | offbeat (m. 60, beat 1.67) | 1 |
| soprano | 61 | 1.13 | finer | offbeat (m. 61, beat 1.33) | 1 |
| soprano | 61 | 1.97 | finer | beat (m. 61, beat 2) | 2 |
| soprano | 62 | 1.58 | finer | offbeat (m. 62, beat 1.67) | 1 |
| soprano | 62 | 2.63 | finer | secondary-beat (m. 62, beat 3) | 3 |
| soprano | 63 | 1.67 | offbeat | beat (m. 63, beat 2) | 1 |
| soprano | 63 | 2.67 | offbeat | secondary-beat (m. 63, beat 3) | 2 |
| soprano | 63 | 3.67 | offbeat | beat (m. 63, beat 4) | 1 |
| soprano | 63 | 4.65 | finer | downbeat (m. 64, beat 1) | 4 |
| soprano | 64 | 1.65 | finer | beat (m. 64, beat 2) | 2 |
| soprano | 64 | 2.64 | finer | offbeat (m. 64, beat 2.67) | 1 |
| soprano | 66 | 2.04 | finer | offbeat (m. 66, beat 2.33) | 1 |
| soprano | 66 | 3.99 | finer | beat (m. 66, beat 4) | 2 |
| bass | 20 | 4.37 | finer | offbeat (m. 20, beat 4.67) | 1 |
| other | 3 | 1.97 | finer | beat (m. 3, beat 2) | 2 |
| other | 7 | 4 | beat | downbeat (m. 8, beat 1) | 2 |
| other | 10 | 4.98 | finer | downbeat (m. 11, beat 1) | 4 |
| other | 11 | 1.99 | finer | beat (m. 11, beat 2) | 2 |
| other | 17 | 3.65 | finer | beat (m. 17, beat 4) | 2 |
| other | 17 | 4.65 | finer | downbeat (m. 18, beat 1) | 4 |
| other | 18 | 1.88 | finer | beat (m. 18, beat 2) | 2 |
| other | 19 | 3.65 | finer | beat (m. 19, beat 4) | 2 |
| other | 22 | 1.92 | finer | beat (m. 22, beat 2) | 2 |
| other | 23 | 1.97 | finer | secondary-beat (m. 23, beat 3) | 3 |
| other | 24 | 3.98 | finer | beat (m. 24, beat 4) | 2 |
| other | 25 | 1.98 | finer | secondary-beat (m. 25, beat 3) | 3 |
| other | 26 | 2 | beat | secondary-beat (m. 26, beat 3) | 1 |
| other | 30 | 1.98 | finer | beat (m. 30, beat 2) | 2 |
| other | 36 | 4.97 | finer | downbeat (m. 37, beat 1) | 4 |
| other | 40 | 3.97 | finer | beat (m. 40, beat 4) | 2 |
| other | 41 | 4.99 | finer | downbeat (m. 42, beat 1) | 4 |
| other | 42 | 4.69 | finer | downbeat (m. 43, beat 1) | 4 |
| other | 43 | 1.97 | finer | beat (m. 43, beat 2) | 2 |
| other | 43 | 4.07 | finer | downbeat (m. 44, beat 1) | 4 |
| other | 45 | 4.04 | finer | offbeat (m. 45, beat 4.67) | 1 |
| other | 47 | 3.99 | finer | beat (m. 47, beat 4) | 2 |
| other | 52 | 4.98 | finer | downbeat (m. 53, beat 1) | 4 |
| other | 55 | 3.96 | finer | beat (m. 55, beat 4) | 2 |
| other | 57 | 1.58 | finer | offbeat (m. 57, beat 1.67) | 1 |
| other | 60 | 2.63 | finer | secondary-beat (m. 60, beat 3) | 3 |
| other | 62 | 1.92 | finer | beat (m. 62, beat 2) | 2 |
| other | 64 | 3.67 | offbeat | beat (m. 64, beat 4) | 1 |
| other | 66 | 2.05 | finer | secondary-beat (m. 66, beat 3) | 3 |
| other | 66 | 2.98 | finer | secondary-beat (m. 66, beat 3) | 3 |
| other | 66 | 2.99 | finer | secondary-beat (m. 66, beat 3) | 3 |

### Hemiola

The library found no hemiola.

### Hypermeter

The library reads each phrase in hypermeasures of two or four measures: the length that fills the phrase and holds the cadence's arrival in the last one. The first measure of each hypermeasure is a hyperdownbeat. A phrase that does not fill its hypermeasures ends in a short one, an internal expansion makes the last one longer, an external expansion adds one of its own and a contraction shortens the last. Score is the library's confidence in the reading. The form adapter does not report elided cadences, so no phrase is read as elided.

| Phrase | Measures | Length | Hyperdownbeats | Hypermeasures | Cadence arrives | Score |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 3 | 2 | 1, 3 | 1 regular, 1 short | m. 3 | 0.83 |
| 2 | mm. 4 to 7 | 4 | 4 | 1 regular | m. 7 | 1 |
| 3 | mm. 8 to 15 | 4 | 8, 12 | 2 regular | m. 15 | 1 |
| 4 | mm. 16 to 18 | 2 | 16, 18 | 1 regular, 1 short | m. 18 | 0.83 |
| 5 | mm. 19 to 26 | 4 | 19, 23 | 2 regular | m. 26 | 1 |
| 6 | mm. 27 to 34 | 4 | 27, 31 | 2 regular | m. 34 | 1 |
| 7 | mm. 35 to 37 | 2 | 35, 37 | 1 regular, 1 short | m. 37 | 0.83 |
| 8 | mm. 38 to 41 | 4 | 38 | 1 regular | none | 1 |
| 9 | mm. 42 to 57 | 4 | 42, 46 | 1 regular, 1 expanded | m. 49 | 1 |
| 10 | mm. 58 to 61 | 4 | 58 | 1 regular | m. 61 | 1 |
| 11 | mm. 62 to 66 | 4 | 62, 66 | 1 regular, 1 short | none | 0.8 |
| 12 | m. 67 | 4 | 67 | 1 short | none | 0 |

### Tuplets and borrowed divisions

A meter divides its beat two ways (simple) or three (compound). A tuplet divides it the other way: a triplet in a simple meter or a duplet in a compound one is a borrowed division. The library names those two and says at which level a group occurs (the whole Beat, a Subdivision of it, or several beats) and whether that is typical. Only triplets (sextuplets included) and duplets are looked for, read from the notes' times alone, rounded to a grid of 96 to the whole note. A run is two or more consecutive attacks of one ratio.

| Ratio | Name | Runs | Attacks | Borrowed |
| --- | --- | --- | --- | --- |
| 3:2 | triplet | 83 | 201 | no |

| Measure | Beat | Ratio | Name | Meter | Borrowed | Level | Attacks |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 2 | 4 | 3:2 | triplet | compound | no | subdivision | 2 |
| 3 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 3 | 1.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 3 | 4 | 3:2 | triplet | compound | no | subdivision | 2 |
| 4 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 7 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 7 | 1.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 10 | 2 | 3:2 | triplet | compound | no | subdivision | 2 |
| 11 | 1.75 | 3:2 | triplet | compound | no | subdivision | 2 |
| 13 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 13 | 3 | 3:2 | triplet | compound | no | not placed | 4 |
| 14 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 15 | 1.83 | 3:2 | triplet | compound | no | subdivision | 2 |
| 16 | 4.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 17 | 2 | 3:2 | triplet | compound | no | subdivision | 2 |
| 18 | 1 | 3:2 | triplet | compound | no | not placed | 10 |
| 18 | 3 | 3:2 | triplet | compound | no | subdivision | 2 |
| 19 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 20 | 3 | 3:2 | triplet | compound | no | multi-beat | 2 |
| 22 | 1.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 22 | 3.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 23 | 1.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 23 | 3.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 23 | 4.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 24 | 1.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 24 | 3.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 24 | 4.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 25 | 1.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 25 | 3.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 25 | 4.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 26 | 4.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 30 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 30 | 2.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 33 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 35 | 4 | 3:2 | triplet | compound | no | subdivision | 2 |
| 35 | 4.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 36 | 3.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 36 | 4.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 37 | 4.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 38 | 2.67 | 3:2 | triplet | compound | no | not placed | 4 |
| 38 | 4.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 39 | 4.33 | 3:2 | triplet | compound | no | not placed | 6 |
| 40 | 1.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 40 | 3.67 | 3:2 | triplet | compound | no | subdivision | 3 |
| 40 | 4.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 41 | 3.33 | 3:2 | triplet | compound | no | subdivision | 3 |
| 41 | 4 | 3:2 | triplet | compound | no | subdivision | 2 |
| 42 | 2.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 42 | 3.33 | 3:2 | triplet | compound | no | not placed | 4 |
| 42 | 4.67 | 3:2 | triplet | compound | no | not placed | 4 |
| 43 | 1.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 43 | 2.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 44 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 44 | 3.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 45 | 1.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 47 | 4.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 48 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 51 | 4.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 52 | 1.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 52 | 4.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 53 | 1.33 | 3:2 | triplet | compound | no | not placed | 5 |
| 55 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 55 | 4.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 57 | 1.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 57 | 1.67 | 3:2 | triplet | compound | no | subdivision | 5 |
| 59 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 59 | 1.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 59 | 2.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 59 | 4.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 60 | 2 | 3:2 | triplet | compound | no | subdivision | 3 |
| 60 | 3 | 3:2 | triplet | compound | no | not placed | 4 |
| 60 | 4.67 | 3:2 | triplet | compound | no | subdivision | 2 |
| 61 | 1.33 | 3:2 | triplet | compound | no | not placed | 6 |
| 61 | 3 | 3:2 | triplet | compound | no | subdivision | 2 |
| 62 | 2 | 3:2 | triplet | compound | no | subdivision | 2 |
| 62 | 3 | 3:2 | triplet | compound | no | subdivision | 2 |
| 63 | 1 | 3:2 | triplet | compound | no | subdivision | 2 |
| 63 | 2 | 3:2 | triplet | compound | no | subdivision | 2 |
| 63 | 4 | 3:2 | triplet | compound | no | subdivision | 2 |
| 64 | 2.33 | 3:2 | triplet | compound | no | subdivision | 2 |
| 66 | 2 | 3:2 | triplet | compound | no | subdivision | 2 |
| 66 | 2.67 | 3:2 | triplet | compound | no | subdivision | 2 |

## Texture

The library's texture adapter read every measure of the piece from the voices the kernel separated in each track or part, classified it, and merged consecutive measures of the same texture into passages. Category is the main texture (homorhythm and melody and accompaniment are homophony). Voices is the most sounding at once, Synchrony the share of attack times where the voices attack together, Pattern the score of a repeated cell (0 to 1: evidence for an accompaniment figure, not a name for it). Confidence is the texture's margin from its thresholds times the voice separation's.

| Measures | Texture | Category | Voices | Synchrony | Pattern | Confidence |
| --- | --- | --- | --- | --- | --- | --- |
| mm. 1 to 3 | monophony | monophony | 1 | 0 | 0.18 | 0.73 |
| m. 4 | melody and accompaniment | homophony | 2 | 0.08 | 0.22 | 0.59 |
| m. 5 | monophony | monophony | 1 | 0 | 0.5 | 0.77 |
| mm. 6 to 7 | melody and accompaniment | homophony | 2 | 0.07 | 0.24 | 0.44 |
| m. 8 | monophony | monophony | 1 | 0 | 0.5 | 0.7 |
| m. 9 | polyphony | polyphony | 2 | 0.08 | 0.17 | 0.75 |
| mm. 10 to 11 | monophony | monophony | 1 | 0 | 0.22 | 0.76 |
| m. 12 | polyphony | polyphony | 2 | 0.08 | 0.14 | 0.49 |
| m. 13 | melody and accompaniment | homophony | 2 | 0.06 | 0.1 | 0.44 |
| m. 14 | polyphony | polyphony | 2 | 0.07 | 0 | 0.56 |
| mm. 15 to 16 | melody and accompaniment | homophony | 2 | 0.06 | 0.14 | 0.34 |
| mm. 17 to 18 | monophony | monophony | 1 | 0 | 0.06 | 0.64 |
| m. 19 | melody and accompaniment | homophony | 2 | 0.19 | 0.07 | 0.64 |
| mm. 20 to 21 | polyphony | polyphony | 2 | 0.13 | 0.17 | 0.58 |
| m. 22 | monophony | monophony | 1 | 0 | 0.17 | 0.77 |
| mm. 23 to 25 | melody and accompaniment | homophony | 2 | 0.1 | 0.3 | 0.53 |
| m. 26 | polyphony | polyphony | 2 | 0.25 | 0.1 | 0.45 |
| mm. 27 to 28 | melody and accompaniment | homophony | 3 | 0.12 | 0.18 | 0.5 |
| mm. 29 to 30 | monophony | monophony | 1 | 0 | 0.27 | 0.8 |
| m. 31 | melody and accompaniment | homophony | 2 | 0.08 | 0.49 | 0.44 |
| m. 32 | polyphony | polyphony | 2 | 0.08 | 0 | 0.62 |
| m. 33 | monophony | monophony | 1 | 0 | 0.15 | 0.77 |
| m. 34 | polyphony | polyphony | 2 | 0.07 | 0.12 | 0.55 |
| mm. 35 to 37 | melody and accompaniment | homophony | 2 | 0.09 | 0.25 | 0.42 |
| mm. 38 to 39 | polyphony | polyphony | 2 | 0.18 | 0.16 | 0.57 |
| m. 40 | melody and accompaniment | homophony | 2 | 0.17 | 0 | 0.42 |
| m. 41 | polyphony | polyphony | 3 | 0.3 | 0 | 0.42 |
| m. 42 | melody and accompaniment | homophony | 2 | 0.19 | 0.19 | 0.37 |
| m. 43 | polyphony | polyphony | 2 | 0.08 | 0.1 | 0.55 |
| mm. 44 to 45 | monophony | monophony | 1 | 0 | 0.21 | 0.75 |
| mm. 46 to 47 | melody and accompaniment | homophony | 2 | 0.23 | 0.19 | 0.5 |
| mm. 48 to 49 | polyphony | polyphony | 3 | 0.32 | 0.15 | 0.42 |
| m. 50 | melody and accompaniment | homophony | 2 | 0.14 | 0.11 | 0.39 |
| m. 51 | monophony | monophony | 1 | 0 | 0.23 | 0.6 |
| mm. 52 to 53 | melody and accompaniment | homophony | 2 | 0.06 | 0.18 | 0.46 |
| m. 54 | monophony | monophony | 1 | 0 | 0.31 | 0.76 |
| mm. 55 to 56 | melody and accompaniment | homophony | 3 | 0.15 | 0.17 | 0.39 |
| m. 57 | polyphony | polyphony | 2 | 0.12 | 0.24 | 0.43 |
| m. 58 | monophony | monophony | 1 | 0 | 0.5 | 0.6 |
| m. 59 | polyphony | polyphony | 2 | 0.26 | 0.17 | 0.52 |
| m. 60 | melody and accompaniment | homophony | 2 | 0.2 | 0.33 | 0.47 |
| m. 61 | polyphony | polyphony | 2 | 0.26 | 0.19 | 0.43 |
| m. 62 | monophony | monophony | 1 | 0 | 0.11 | 0.73 |
| mm. 63 to 64 | polyphony | polyphony | 3 | 0.32 | 0.05 | 0.48 |
| mm. 65 to 66 | melody and accompaniment | homophony | 2 | 0.27 | 0.38 | 0.3 |
| m. 67 | polyphony | polyphony | 3 | 0.14 | 0 | 0.35 |

### Changes

| Measure | From | To | Category changes |
| --- | --- | --- | --- |
| 4 | monophony | melody and accompaniment | yes |
| 5 | melody and accompaniment | monophony | yes |
| 6 | monophony | melody and accompaniment | yes |
| 8 | melody and accompaniment | monophony | yes |
| 9 | monophony | polyphony | yes |
| 10 | polyphony | monophony | yes |
| 12 | monophony | polyphony | yes |
| 13 | polyphony | melody and accompaniment | yes |
| 14 | melody and accompaniment | polyphony | yes |
| 15 | polyphony | melody and accompaniment | yes |
| 17 | melody and accompaniment | monophony | yes |
| 19 | monophony | melody and accompaniment | yes |
| 20 | melody and accompaniment | polyphony | yes |
| 22 | polyphony | monophony | yes |
| 23 | monophony | melody and accompaniment | yes |
| 26 | melody and accompaniment | polyphony | yes |
| 27 | polyphony | melody and accompaniment | yes |
| 29 | melody and accompaniment | monophony | yes |
| 31 | monophony | melody and accompaniment | yes |
| 32 | melody and accompaniment | polyphony | yes |
| 33 | polyphony | monophony | yes |
| 34 | monophony | polyphony | yes |
| 35 | polyphony | melody and accompaniment | yes |
| 38 | melody and accompaniment | polyphony | yes |
| 40 | polyphony | melody and accompaniment | yes |
| 41 | melody and accompaniment | polyphony | yes |
| 42 | polyphony | melody and accompaniment | yes |
| 43 | melody and accompaniment | polyphony | yes |
| 44 | polyphony | monophony | yes |
| 46 | monophony | melody and accompaniment | yes |
| 48 | melody and accompaniment | polyphony | yes |
| 50 | polyphony | melody and accompaniment | yes |
| 51 | melody and accompaniment | monophony | yes |
| 52 | monophony | melody and accompaniment | yes |
| 54 | melody and accompaniment | monophony | yes |
| 55 | monophony | melody and accompaniment | yes |
| 57 | melody and accompaniment | polyphony | yes |
| 58 | polyphony | monophony | yes |
| 59 | monophony | polyphony | yes |
| 60 | polyphony | melody and accompaniment | yes |
| 61 | melody and accompaniment | polyphony | yes |
| 62 | polyphony | monophony | yes |
| 63 | monophony | polyphony | yes |
| 65 | polyphony | melody and accompaniment | yes |
| 67 | melody and accompaniment | polyphony | yes |

## Voice leading

The kernel separated the notes of the whole piece into voices (voice 0 is the top) and the library's counterpoint adapter classified how each pair of voices moves from one chord segment to the next. Parallel motion keeps the interval type, similar motion changes it; a voice that rests in either segment gives no motion. Faults are the library's: parallel fifths and octaves between any pair of voices, direct fifths and octaves in the outer pair, and a seventh or leading tone left unresolved when its chord changes (voice 0 is the top).

4 voices over 107 chord segments, 29 motions between consecutive segments: 7 contrary, 9 similar, 2 parallel, 11 oblique, 0 static.

| Voices | Contrary | Similar | Parallel | Oblique | Static |
| --- | --- | --- | --- | --- | --- |
| 0 and 1 | 0 | 1 | 0 | 1 | 0 |
| 0 and 2 | 2 | 1 | 0 | 1 | 0 |
| 0 and 3 | 3 | 0 | 1 | 3 | 0 |
| 1 and 2 | 0 | 2 | 1 | 1 | 0 |
| 1 and 3 | 0 | 3 | 0 | 2 | 0 |
| 2 and 3 | 2 | 2 | 0 | 3 | 0 |

0 parallel fifths, 0 parallel octaves, 0 direct fifths or octaves in the outer voices, 3 unresolved sevenths, 0 unresolved leading tones.

| Location | Fault | Voices |
| --- | --- | --- |
| m. 3, beat 4 to m. 5, beat 1 | seventh-resolves-down | 3 |
| m. 36, beat 4 to m. 38, beat 1 | seventh-resolves-down | 2 |
| m. 63, beat 4 to m. 64, beat 2 | seventh-resolves-down | 1 |

19 chords with three or more voices sounding: the root is doubled in 9, the third in 0, the fifth in 2, the seventh in 0, no member doubled in 8. Doubling is counted, not judged. Spacing is judged by the library for the 1 chords with four voices all sounding: 0 too wide.

## Markings

The tempo and dynamics markings the score notates, read with the library's standard-markings rules. A tempo holds until the next one, a dynamic until the next in its part. Group and the loudness comparison are the library's; a text it does not recognise has none. Wedges are not read, and articulations are not imported.

The score has no tempo marking.

The score has no dynamics marking.
