# Analysis Report: field-1.musicxml

## Summary

- **File:** field-1.musicxml, 1191 notes.
- **Input:** MusicXML score, 1 part merged into one texture. Meter: notated. Key signature: inferred. Pitch spelling: notated.
- **Key:** Eb major, the key with the most time across 4 key regions.
- **Chords:** 90 chord events; 84 matched a chord (93%), 6 did not.
- **Most frequent chords:** I (36), dominant7 (10), V (8).

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
| m. 1, beat 1 to m. 21, beat 1 | Eb major | 0.71 | profile |
| m. 21, beat 1 to m. 28, beat 1 | Bb major | 0.97 | profile |
| m. 28, beat 1 to m. 32, beat 1 | F minor | 0.77 | profile |
| m. 32, beat 1 to m. 68, beat 1 | Eb major | 0.74 | profile |

## Modulations

A modulation is a lasting new key. Position is where the new key arrives. Type is pivot when the library found a pivot chord and direct when it did not. A return goes back to the key before an excursion. The library gives the change's extent from the length of the new key, whether it returns, and whether a cadence arrives in it.

| Position | From | To | Extent | Type | Return |
| --- | --- | --- | --- | --- | --- |
| m. 21, beat 1 | Eb major | Bb major | modulation | pivot | no |
| m. 28, beat 1 | Bb major | F minor | modulation | direct | no |
| m. 32, beat 1 | F minor | Eb major | modulation | pivot | no |

## Pivot chords

The chord at a modulation that is diatonic in both the old and the new key, named by root and quality, with no Roman numeral. Modulation is its row in the Modulations table, counting from 1.

| Position | Chord | Bass | Inversion | From | To | Modulation |
| --- | --- | --- | --- | --- | --- | --- |
| m. 21, beat 1 to m. 22, beat 1 | A# major | A# | root position | Eb major | Bb major | 1 |
| m. 32, beat 1 to m. 32, beat 3 | A# dominant7 | A# | root position | F minor | Eb major | 3 |

## Chords

Positions are measure and beat. Numeral is the Roman numeral in the local key region, which is named where it changes. Function is the library's harmonic function of the chord: an applied chord is a dominant toward its target degree, and a predominant is strong (IV, ii) or weak (iii, vi). Six-four is the library's type for a chord in second inversion. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Numeral | Function | Six-four | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 4 to m. 2, beat 1 | I | I6/4 (in Eb major) | tonic | - | Eb | major | A# | second | 1 | 0.69 | - |
| m. 2, beat 1 to m. 3, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.69 | - |
| m. 3, beat 1 to m. 4, beat 1 | V | V | dominant | - | A# | major | A# | root position | 4 | 0.51 | - |
| m. 4, beat 1 to m. 5, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 5, beat 1 to m. 6, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 6, beat 1 to m. 7, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.58 | - |
| m. 7, beat 1 to m. 8, beat 1 | V | V | dominant | - | A# | major | A# | root position | 4 | 0.81 | - |
| m. 8, beat 1 to m. 9, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 9, beat 1 to m. 10, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 10, beat 1 to m. 11, beat 1 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 4 | 0.39 | - |
| m. 11, beat 1 to m. 11, beat 4 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 3 | 0.26 | - |
| m. 11, beat 4 to m. 12, beat 1 | iii | iii6 | predominant (weak) | - | G | minor | A# | first | 1 | 0.49 | - |
| m. 12, beat 1 to m. 13, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.69 | - |
| m. 13, beat 1 to m. 14, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.80 | - |
| m. 14, beat 1 to m. 15, beat 1 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 4 | 0.40 | - |
| m. 15, beat 1 to m. 16, beat 1 | V | V | dominant | - | A# | major | A# | root position | 4 | 0.69 | - |
| m. 16, beat 1 to m. 17, beat 1 | I | I | tonic | - | D# | major | A# | second | 4 | 0.46 | - |
| m. 17, beat 1 to m. 18, beat 1 | dominant7 | V4/3 | dominant | - | A# | dominant7 | A# | root position | 4 | 0.07 | vii° (D diminished, score 0.77) |
| m. 18, beat 1 to m. 19, beat 1 | I | I | tonic | - | D# | major | A# | second | 4 | 0.56 | - |
| m. 19, beat 1 to m. 20, beat 1 | vii° | vii°6 | dominant | - | D | diminished | A# | - | 4 | 0.03 | dominant7 (A# dominant7, score 0.73) |
| m. 20, beat 1 to m. 21, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.47 | - |
| m. 21, beat 1 to m. 22, beat 1 | I | I (in Bb major) | tonic | - | A# | major | A# | root position | 4 | 0.70 | - |
| m. 22, beat 1 to m. 22, beat 4 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 3 | 0.33 | - |
| m. 22, beat 4 to m. 23, beat 1 | iii | iii6 | predominant (weak) | - | D | minor | F | first | 1 | 0.55 | - |
| m. 23, beat 1 to m. 24, beat 1 | I | I | tonic | - | A# | major | A# | root position | 4 | 0.10 | vi (G minor, score 0.75) |
| m. 24, beat 1 to m. 24, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
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
| m. 32, beat 1 to m. 32, beat 3 | dominant7 | V7 (in Eb major) | dominant | - | A# | dominant7 | A# | root position | 2 | 1.00 | - |
| m. 32, beat 3 to m. 32, beat 4 | iii | iii6 | predominant (weak) | - | G | minor | A# | first | 1 | 0.41 | - |
| m. 32, beat 4 to m. 33, beat 1 | vii° | vii°6/4 | dominant | - | D | diminished | G# | second | 1 | 0.06 | dominant7 (A# dominant7, score 0.52) |
| m. 33, beat 1 to m. 33, beat 3 | I | I | tonic | - | D# | major | D# | root position | 2 | 0.60 | - |
| m. 33, beat 3 to m. 34, beat 1 | minor7 | ii6/5 | predominant (strong) | - | F | minor7 | G# | first | 2 | 0.01 | IV (G# major, score 0.89) |
| m. 34, beat 1 to m. 35, beat 1 | V | V | dominant | - | A# | major | A# | root position | 4 | 0.02 | iii (G minor, score 0.73) |
| m. 35, beat 1 to m. 36, beat 1 | I | I | tonic | - | D# | major | A# | second | 4 | 0.02 | vi (C minor, score 0.98) |
| m. 36, beat 1 to m. 37, beat 1 | vii° | vii°6 | dominant | - | D | diminished | A# | - | 4 | 0.03 | dominant7 (A# dominant7, score 0.86) |
| m. 37, beat 1 to m. 38, beat 1 | I | I | tonic | - | D# | major | A# | second | 4 | 0.04 | vi (C minor, score 0.95) |
| m. 38, beat 1 to m. 39, beat 1 | dominant7 | V4/3 | dominant | - | A# | dominant7 | A# | root position | 4 | 0.03 | vii° (D diminished, score 0.73) |
| m. 39, beat 1 to m. 40, beat 1 | I | I | tonic | - | D# | major | A# | second | 4 | 0.55 | - |
| m. 40, beat 1 to m. 40, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 40, beat 3 to m. 40, beat 4 | vii° | vii°6/4 | dominant | - | D | diminished | G# | second | 1 | 0.10 | ii (F minor, score 0.60) |
| m. 40, beat 4 to m. 41, beat 1 | V | V | dominant | - | A# | major | A# | root position | 1 | 0.95 | - |
| m. 41, beat 1 to m. 41, beat 2 | vi | vi6/4 | predominant (weak) | - | C | minor | G | second | 1 | 0.07 | I (D# major, score 0.88) |
| m. 41, beat 2 to m. 41, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 41, beat 3 to m. 41, beat 4 | vii° | vii°6 | dominant | - | D | diminished | F | first | 1 | 0.05 | V (A# major, score 0.66) |
| m. 41, beat 4 to m. 42, beat 1 | major7 | IV6/5 | predominant (strong) | - | G# | major7 | G# | root position | 1 | 0.10 | minor7 (F minor7, score 0.70) |
| m. 42, beat 1 to m. 42, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 2 | 0.10 | minor7 (C minor7, score 0.84) |
| m. 42, beat 3 to m. 42, beat 4 | iii | iii6 | predominant (weak) | - | G | minor | A# | first | 1 | 0.14 | - |
| m. 42, beat 4 to m. 43, beat 1 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 1 | 0.04 | dominant7 (A# dominant7, score 0.74) |
| m. 43, beat 1 to m. 44, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 44, beat 1 to m. 44, beat 3 | I | I | tonic | - | D# | major | D# | root position | 2 | 1.00 | - |
| m. 44, beat 3 to m. 45, beat 1 | vi | vi | predominant (weak) | - | C | minor | C | root position | 2 | 0.04 | minor7 (F minor7, score 0.62) |
| m. 45, beat 1 to m. 46, beat 1 | V | V | dominant | - | A# | major | A# | root position | 4 | 0.51 | - |
| m. 46, beat 1 to m. 47, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 47, beat 1 to m. 48, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.68 | - |
| m. 48, beat 1 to m. 49, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.12 | - |
| m. 49, beat 1 to m. 50, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 4 | 0.25 | - |
| m. 50, beat 1 to m. 51, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.77 | - |
| m. 51, beat 1 to m. 52, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 52, beat 1 to m. 52, beat 3 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 2 | 0.35 | - |
| m. 52, beat 3 to m. 52, beat 4 | IV | IV6/4 | predominant (strong) | - | Ab | major | D# | second | 1 | 0.35 | - |
| m. 52, beat 4 to m. 53, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 53, beat 1 to m. 54, beat 1 | iii | iii6 | predominant (weak) | - | G | minor | A# | first | 4 | 0.02 | dominant7 (A# dominant7, score 0.77) |
| m. 54, beat 1 to m. 55, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.77 | - |
| m. 55, beat 1 to m. 56, beat 1 | minor7 | iii7 | predominant (weak) | - | G | minor7 | G | root position | 4 | 0.27 | - |
| m. 56, beat 1 to m. 57, beat 1 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 4 | 0.14 | - |
| m. 57, beat 1 to m. 58, beat 1 | V | V | dominant | - | A# | major | A# | root position | 4 | 0.44 | - |
| m. 58, beat 1 to m. 59, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 59, beat 1 to m. 60, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 4 | 0.88 | - |
| m. 60, beat 1 to m. 61, beat 1 | I | I6 | tonic | - | D# | major | G | first | 4 | 0.15 | - |
| m. 61, beat 1 to m. 62, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 4 | 0.85 | - |
| m. 62, beat 1 to m. 63, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 0.90 | - |
| m. 63, beat 1 to m. 63, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 63, beat 3 to m. 63, beat 4 | vii° | vii°6/4 | dominant | - | D | diminished | G# | second | 1 | 0.15 | ii (F minor, score 0.54) |
| m. 63, beat 4 to m. 64, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 1.00 | - |
| m. 64, beat 1 to m. 64, beat 3 | I | vii°6/5/ii | dominant of 2 | - | D# | major | G | first | 2 | 0.13 | iii (G minor, score 0.68) |
| m. 64, beat 3 to m. 65, beat 1 | ii | ii6 | predominant (strong) | - | F | minor | G# | first | 2 | 0.29 | - |
| m. 65, beat 1 to m. 65, beat 3 | I | I6/4 | dominant | cadential | D# | major | A# | second | 2 | 0.11 | - |
| m. 65, beat 3 to m. 66, beat 1 | V | V | dominant | - | A# | major | A# | root position | 2 | 0.09 | iii (G minor, score 0.67) |
| m. 66, beat 1 to m. 67, beat 1 | I | I | tonic | - | D# | major | D# | root position | 4 | 1.00 | - |
| m. 67, beat 1 to m. 67, beat 4 | I | I | tonic | - | D# | major | D# | root position | 3 | 1.00 | - |

## Applied chords

Chords the library reads as applied (secondary) chords: the chord as it reads in the tonicized key, the chord it tonicizes and that chord's tonic. The last column checks each against the tonicization claims above for the same span; where they disagree both are shown and neither is changed.

| Position | Applied chord | Target | Resolution | Tonicization |
| --- | --- | --- | --- | --- |
| m. 64, beat 1 to m. 64, beat 3 | vii°6/5 (secondary-leading-tone) | ii (F) | to the target | DISAGREES: no tonicization is reported here |
## Outer voices

The highest (soprano) and lowest (bass) sounding note of each chord's span, with its scale degree in that chord's local key. A flat or sharp sign marks a chromatic alteration. A degree is "-" where the span lies in no key region. A rest means the library found no note for that line in the span, a bridged one a short gap inside the line, and a register guard a note the library dropped rather than adopt an accompaniment note. Degrees carry the key region's uncertainty in the analysis document.

| Position | Key | Soprano | Degree | Bass | Degree |
| --- | --- | --- | --- | --- | --- |
| m. 1, beat 4 to m. 2, beat 1 | Eb major | A#4 | 5 | A#4 | 5 |
| m. 2, beat 1 to m. 3, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 3, beat 1 to m. 4, beat 1 | Eb major | D#5 | 1 | A#2 | 5 |
| m. 4, beat 1 to m. 5, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 5, beat 1 to m. 6, beat 1 | Eb major | A#5 | 5 | D#3 | 1 |
| m. 6, beat 1 to m. 7, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 7, beat 1 to m. 8, beat 1 | Eb major | C6 | 6 | A#2 | 5 |
| m. 8, beat 1 to m. 9, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 9, beat 1 to m. 10, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 10, beat 1 to m. 11, beat 1 | Eb major | D5 | 7 | G#2 | 4 |
| m. 11, beat 1 to m. 11, beat 4 | Eb major | C5 | 6 | A#2 | 5 |
| m. 11, beat 4 to m. 12, beat 1 | Eb major | G5 | 3 | A#3 | 5 |
| m. 12, beat 1 to m. 13, beat 1 | Eb major | G5 | 3 | D#3 | 1 |
| m. 13, beat 1 to m. 14, beat 1 | Eb major | G5 | 3 | D#3 | 1 |
| m. 14, beat 1 to m. 15, beat 1 | Eb major | D5 | 7 | G#2 | 4 |
| m. 15, beat 1 to m. 16, beat 1 | Eb major | C5 | 6 | A#2 | 5 |
| m. 16, beat 1 to m. 17, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 17, beat 1 to m. 18, beat 1 | Eb major | C6 | 6 | F3 | 2 |
| m. 18, beat 1 to m. 19, beat 1 | Eb major | F5 | 2 | D#3 | 1 |
| m. 19, beat 1 to m. 20, beat 1 | Eb major | D#5 | 1 | F3 | 2 |
| m. 20, beat 1 to m. 21, beat 1 | Eb major | F5 | 2 | D#3 | 1 |
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
| m. 32, beat 1 to m. 32, beat 3 | Eb major | D4 | 7 | A#2 | 5 |
| m. 32, beat 3 to m. 32, beat 4 | Eb major | G5 | 3 | A#2 | 5 |
| m. 32, beat 4 to m. 33, beat 1 | Eb major | G5 | 3 | G#3 | 4 |
| m. 33, beat 1 to m. 33, beat 3 | Eb major | F5 | 2 | D#3 | 1 |
| m. 33, beat 3 to m. 34, beat 1 | Eb major | D#5 | 1 | G#3 | 4 |
| m. 34, beat 1 to m. 35, beat 1 | Eb major | D5 | 7 | A#2 | 5 |
| m. 35, beat 1 to m. 36, beat 1 | Eb major | G5 | 3 | D#3 | 1 |
| m. 36, beat 1 to m. 37, beat 1 | Eb major | G#5 | 4 | F3 | 2 |
| m. 37, beat 1 to m. 38, beat 1 | Eb major | D#6 | 1 | D#3 | 1 |
| m. 38, beat 1 to m. 39, beat 1 | Eb major | G#6 | 4 | F3 | 2 |
| m. 39, beat 1 to m. 40, beat 1 | Eb major | F5 | 2 | D#3 | 1 |
| m. 40, beat 1 to m. 40, beat 3 | Eb major | G5 | 3 | A2 | #4 |
| m. 40, beat 3 to m. 40, beat 4 | Eb major | G5 | 3 | G#3 | 4 |
| m. 40, beat 4 to m. 41, beat 1 | Eb major | C6 | 6 | A#3 | 5 |
| m. 41, beat 1 to m. 41, beat 2 | Eb major | C6 | 6 | G3 | 3 |
| m. 41, beat 2 to m. 41, beat 3 | Eb major | A#5 | 5 | F#3 | b3 |
| m. 41, beat 3 to m. 41, beat 4 | Eb major | A#5 | 5 | F3 | 2 |
| m. 41, beat 4 to m. 42, beat 1 | Eb major | G#5 | 4 | C4 | 6 |
| m. 42, beat 1 to m. 42, beat 3 | Eb major | D#6 | 1 | A#2 | 5 |
| m. 42, beat 3 to m. 42, beat 4 | Eb major | C6 | 6 | A#2 | 5 |
| m. 42, beat 4 to m. 43, beat 1 | Eb major | G#5 | 4 | G#3 | 4 |
| m. 43, beat 1 to m. 44, beat 1 | Eb major | G6 | 3 | D#3 | 1 |
| m. 44, beat 1 to m. 44, beat 3 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 44, beat 3 to m. 45, beat 1 | Eb major | F5 | 2 | C3 | 6 |
| m. 45, beat 1 to m. 46, beat 1 | Eb major | D5 | 7 | A#2 | 5 |
| m. 46, beat 1 to m. 47, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 47, beat 1 to m. 48, beat 1 | Eb major | A#5 | 5 | D#3 | 1 |
| m. 48, beat 1 to m. 49, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 49, beat 1 to m. 50, beat 1 | Eb major | D#5 | 1 | A#2 | 5 |
| m. 50, beat 1 to m. 51, beat 1 | Eb major | G5 | 3 | D#3 | 1 |
| m. 51, beat 1 to m. 52, beat 1 | Eb major | F4 | 2 | G2 | 3 |
| m. 52, beat 1 to m. 52, beat 3 | Eb major | D5 | 7 | G#2 | 4 |
| m. 52, beat 3 to m. 52, beat 4 | Eb major | C5 | 6 | D#3 | 1 |
| m. 52, beat 4 to m. 53, beat 1 | Eb major | B4 | b6 | G#3 | 4 |
| m. 53, beat 1 to m. 54, beat 1 | Eb major | C5 | 6 | A#2 | 5 |
| m. 54, beat 1 to m. 55, beat 1 | Eb major | G5 | 3 | D#3 | 1 |
| m. 55, beat 1 to m. 56, beat 1 | Eb major | G5 | 3 | G2 | 3 |
| m. 56, beat 1 to m. 57, beat 1 | Eb major | D5 | 7 | G#2 | 4 |
| m. 57, beat 1 to m. 58, beat 1 | Eb major | B4 | b6 | A#2 | 5 |
| m. 58, beat 1 to m. 59, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 59, beat 1 to m. 60, beat 1 | Eb major | C6 | 6 | A#2 | 5 |
| m. 60, beat 1 to m. 61, beat 1 | Eb major | G5 | 3 | G3 | 3 |
| m. 61, beat 1 to m. 62, beat 1 | Eb major | C6 | 6 | A#2 | 5 |
| m. 62, beat 1 to m. 63, beat 1 | Eb major | F5 | 2 | D#3 | 1 |
| m. 63, beat 1 to m. 63, beat 3 | Eb major | F5 | 2 | A3 | #4 |
| m. 63, beat 3 to m. 63, beat 4 | Eb major | G#5 | 4 | G#3 | 4 |
| m. 63, beat 4 to m. 64, beat 1 | Eb major | A5 | #4 | A#3 | 5 |
| m. 64, beat 1 to m. 64, beat 3 | Eb major | A#5 | 5 | G3 | 3 |
| m. 64, beat 3 to m. 65, beat 1 | Eb major | C#6 | b7 | G#3 | 4 |
| m. 65, beat 1 to m. 65, beat 3 | Eb major | A#6 | 5 | A#2 | 5 |
| m. 65, beat 3 to m. 66, beat 1 | Eb major | D6 | 7 | A#2 | 5 |
| m. 66, beat 1 to m. 67, beat 1 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 67, beat 1 to m. 67, beat 4 | Eb major | D#6 | 1 | D#3 | 1 |

## Chord vocabulary

Each distinct Roman numeral of the chords the library numbered, for the whole piece and for each key region, sorted by count, then total length, then first appearance. Count is the number of chord rows with that numeral (every row in the Chords table is one). Runs counts a stretch of adjacent identical numerals in one key region once. Beats is the total length, Share its fraction of the scope's numbered chord length, and First where it first appears. Applied chords are listed on their own. Chords without a numeral are not counted.

### Overall

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 29 | 21 | 111 | 43.9% | m. 2, beat 1 |
| V | 9 | 9 | 31 | 12.3% | m. 3, beat 1 |
| V7 | 8 | 8 | 25 | 9.9% | m. 11, beat 1 |
| IV | 6 | 6 | 17 | 6.7% | m. 10, beat 1 |
| I6/4 | 5 | 5 | 11 | 4.3% | m. 1, beat 4 |
| iii6 | 5 | 5 | 8 | 3.2% | m. 11, beat 4 |
| vii°6 | 3 | 3 | 9 | 3.6% | m. 19, beat 1 |
| vii°6/4 | 3 | 3 | 3 | 1.2% | m. 32, beat 4 |
| V4/3 | 2 | 2 | 8 | 3.2% | m. 17, beat 1 |
| i | 2 | 2 | 8 | 3.2% | m. 29, beat 1 |
| iii7 | 1 | 1 | 4 | 1.6% | m. 55, beat 1 |
| I6 | 1 | 1 | 4 | 1.6% | m. 60, beat 1 |
| ii6/5 | 1 | 1 | 2 | 0.8% | m. 33, beat 3 |
| vi | 1 | 1 | 2 | 0.8% | m. 44, beat 3 |
| ii6 | 1 | 1 | 2 | 0.8% | m. 64, beat 3 |
| vi6 | 1 | 1 | 1 | 0.4% | m. 25, beat 1 |
| i6 | 1 | 1 | 1 | 0.4% | m. 28, beat 3 |
| viio6 | 1 | 1 | 1 | 0.4% | m. 28, beat 4 |
| vi6/4 | 1 | 1 | 1 | 0.4% | m. 41, beat 1 |
| IV6/5 | 1 | 1 | 1 | 0.4% | m. 41, beat 4 |
| IV6/4 | 1 | 1 | 1 | 0.4% | m. 52, beat 3 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 35 | 27 | 126 | 49.8% | m. 1, beat 4 |
| V7 | 10 | 10 | 33 | 13.0% | m. 11, beat 1 |
| V | 9 | 9 | 31 | 12.3% | m. 3, beat 1 |
| IV | 7 | 7 | 18 | 7.1% | m. 10, beat 1 |
| vii° | 6 | 6 | 12 | 4.7% | m. 19, beat 1 |
| iii | 5 | 5 | 8 | 3.2% | m. 11, beat 4 |
| i | 3 | 3 | 9 | 3.6% | m. 28, beat 3 |
| vi | 3 | 3 | 4 | 1.6% | m. 25, beat 1 |
| iii7 | 1 | 1 | 4 | 1.6% | m. 55, beat 1 |
| ii7 | 1 | 1 | 2 | 0.8% | m. 33, beat 3 |
| ii | 1 | 1 | 2 | 0.8% | m. 64, beat 3 |
| viio | 1 | 1 | 1 | 0.4% | m. 28, beat 4 |
| IV7 | 1 | 1 | 1 | 0.4% | m. 41, beat 4 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°6/5/ii | 1 | 1 | 2 | 0.8% | m. 64, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°7/ii | 1 | 1 | 2 | 0.8% | m. 64, beat 1 |

### Eb major (region 0)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 11 | 7 | 44 | 57.1% | m. 2, beat 1 |
| V | 3 | 3 | 12 | 15.6% | m. 3, beat 1 |
| IV | 2 | 2 | 8 | 10.4% | m. 10, beat 1 |
| V4/3 | 1 | 1 | 4 | 5.2% | m. 17, beat 1 |
| vii°6 | 1 | 1 | 4 | 5.2% | m. 19, beat 1 |
| V7 | 1 | 1 | 3 | 3.9% | m. 11, beat 1 |
| I6/4 | 1 | 1 | 1 | 1.3% | m. 1, beat 4 |
| iii6 | 1 | 1 | 1 | 1.3% | m. 11, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 12 | 8 | 45 | 58.4% | m. 1, beat 4 |
| V | 3 | 3 | 12 | 15.6% | m. 3, beat 1 |
| IV | 2 | 2 | 8 | 10.4% | m. 10, beat 1 |
| V7 | 2 | 2 | 7 | 9.1% | m. 11, beat 1 |
| vii° | 1 | 1 | 4 | 5.2% | m. 19, beat 1 |
| iii | 1 | 1 | 1 | 1.3% | m. 11, beat 4 |

### Bb major (region 1)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 12 | 44.4% | m. 21, beat 1 |
| V7 | 2 | 2 | 7 | 25.9% | m. 22, beat 1 |
| I6/4 | 2 | 2 | 6 | 22.2% | m. 24, beat 2 |
| iii6 | 1 | 1 | 1 | 3.7% | m. 22, beat 4 |
| vi6 | 1 | 1 | 1 | 3.7% | m. 25, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 5 | 5 | 18 | 66.7% | m. 21, beat 1 |
| V7 | 2 | 2 | 7 | 25.9% | m. 22, beat 1 |
| iii | 1 | 1 | 1 | 3.7% | m. 22, beat 4 |
| vi | 1 | 1 | 1 | 3.7% | m. 25, beat 1 |

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
| I | 15 | 11 | 55 | 41.4% | m. 33, beat 1 |
| V7 | 5 | 5 | 15 | 11.3% | m. 32, beat 1 |
| V | 5 | 5 | 15 | 11.3% | m. 34, beat 1 |
| IV | 3 | 3 | 7 | 5.3% | m. 42, beat 4 |
| iii6 | 3 | 3 | 6 | 4.5% | m. 32, beat 3 |
| vii°6/4 | 3 | 3 | 3 | 2.3% | m. 32, beat 4 |
| vii°6 | 2 | 2 | 5 | 3.8% | m. 36, beat 1 |
| I6/4 | 2 | 2 | 4 | 3.0% | m. 42, beat 1 |
| V4/3 | 1 | 1 | 4 | 3.0% | m. 38, beat 1 |
| iii7 | 1 | 1 | 4 | 3.0% | m. 55, beat 1 |
| I6 | 1 | 1 | 4 | 3.0% | m. 60, beat 1 |
| ii6/5 | 1 | 1 | 2 | 1.5% | m. 33, beat 3 |
| vi | 1 | 1 | 2 | 1.5% | m. 44, beat 3 |
| ii6 | 1 | 1 | 2 | 1.5% | m. 64, beat 3 |
| vi6/4 | 1 | 1 | 1 | 0.8% | m. 41, beat 1 |
| IV6/5 | 1 | 1 | 1 | 0.8% | m. 41, beat 4 |
| IV6/4 | 1 | 1 | 1 | 0.8% | m. 52, beat 3 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 18 | 14 | 63 | 47.4% | m. 33, beat 1 |
| V7 | 6 | 6 | 19 | 14.3% | m. 32, beat 1 |
| V | 5 | 5 | 15 | 11.3% | m. 34, beat 1 |
| vii° | 5 | 5 | 8 | 6.0% | m. 32, beat 4 |
| IV | 4 | 4 | 8 | 6.0% | m. 42, beat 4 |
| iii | 3 | 3 | 6 | 4.5% | m. 32, beat 3 |
| vi | 2 | 2 | 3 | 2.3% | m. 41, beat 1 |
| iii7 | 1 | 1 | 4 | 3.0% | m. 55, beat 1 |
| ii7 | 1 | 1 | 2 | 1.5% | m. 33, beat 3 |
| ii | 1 | 1 | 2 | 1.5% | m. 64, beat 3 |
| IV7 | 1 | 1 | 1 | 0.8% | m. 41, beat 4 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°6/5/ii | 1 | 1 | 2 | 1.5% | m. 64, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°7/ii | 1 | 1 | 2 | 1.5% | m. 64, beat 1 |

## Prolongations

The library's standard-harmony analysis found 4 prolongations: a tonic or dominant that lasts through the chords between it and the harmony that closes it. Measures run from the prolonged chord to the closing chord. Numeral is the prolonged chord. Reason is why the library reads the chords between as a prolongation.

| Measures | Function | Numeral | Key | Reason |
| --- | --- | --- | --- | --- |
| mm. 16 to 19 | tonic | I | Eb major | tonic-line |
| mm. 18 to 21 | tonic | I | Eb major | tonic-line |
| mm. 35 to 38 | tonic | I | Eb major | tonic-line |
| mm. 37 to 40 | tonic | I | Eb major | tonic-line |

## Non-chord tones

231 notes sound against a chord without being one of its tones; 156 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 2, beat 1 to m. 3, beat 1 | 12 | unclassified | I |
| m. 3, beat 1 to m. 4, beat 1 | 15 | unclassified | V |
| m. 3, beat 1 to m. 4, beat 1 | 20 | unclassified | V |
| m. 3, beat 1 to m. 4, beat 1 | 21 | appoggiatura | V |
| m. 3, beat 1 to m. 4, beat 1 | 28 | unclassified | V |
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
| m. 11, beat 1 to m. 11, beat 4 | 128 | unclassified | dominant7 |
| m. 11, beat 1 to m. 11, beat 4 | 134 | escape-tone | dominant7 |
| m. 11, beat 1 to m. 11, beat 4 | 137 | unclassified | dominant7 |
| m. 11, beat 1 to m. 11, beat 4 | 146 | anticipation | dominant7 |
| m. 11, beat 4 to m. 12, beat 1 | 148 | unclassified | iii |
| m. 12, beat 1 to m. 13, beat 1 | 151 | unclassified | I |
| m. 13, beat 1 to m. 14, beat 1 | 169 | unclassified | I |
| m. 13, beat 1 to m. 14, beat 1 | 173 | unclassified | I |
| m. 13, beat 1 to m. 14, beat 1 | 179 | unclassified | I |
| m. 14, beat 1 to m. 15, beat 1 | 182 | unclassified | IV |
| m. 14, beat 1 to m. 15, beat 1 | 192 | unclassified | IV |
| m. 14, beat 1 to m. 15, beat 1 | 193 | unclassified | IV |
| m. 15, beat 1 to m. 16, beat 1 | 199 | unclassified | V |
| m. 15, beat 1 to m. 16, beat 1 | 202 | unclassified | V |
| m. 15, beat 1 to m. 16, beat 1 | 204 | neighbor | V |
| m. 15, beat 1 to m. 16, beat 1 | 205 | unclassified | V |
| m. 15, beat 1 to m. 16, beat 1 | 211 | unclassified | V |
| m. 16, beat 1 to m. 17, beat 1 | 212 | unclassified | I |
| m. 16, beat 1 to m. 17, beat 1 | 227 | unclassified | I |
| m. 17, beat 1 to m. 18, beat 1 | 229 | suspension | dominant7 |
| m. 17, beat 1 to m. 18, beat 1 | 235 | unclassified | dominant7 |
| m. 17, beat 1 to m. 18, beat 1 | 242 | unclassified | dominant7 |
| m. 18, beat 1 to m. 19, beat 1 | 247 | unclassified | I |
| m. 18, beat 1 to m. 19, beat 1 | 252 | neighbor | I |
| m. 18, beat 1 to m. 19, beat 1 | 255 | escape-tone | I |
| m. 18, beat 1 to m. 19, beat 1 | 268 | unclassified | I |
| m. 19, beat 1 to m. 20, beat 1 | 270 | suspension | vii° |
| m. 19, beat 1 to m. 20, beat 1 | 275 | unclassified | vii° |
| m. 19, beat 1 to m. 20, beat 1 | 276 | appoggiatura | vii° |
| m. 19, beat 1 to m. 20, beat 1 | 280 | unclassified | vii° |
| m. 19, beat 1 to m. 20, beat 1 | 282 | appoggiatura | vii° |
| m. 19, beat 1 to m. 20, beat 1 | 284 | unclassified | vii° |
| m. 19, beat 1 to m. 20, beat 1 | 285 | unclassified | vii° |
| m. 20, beat 1 to m. 21, beat 1 | 288 | unclassified | I |
| m. 20, beat 1 to m. 21, beat 1 | 300 | unclassified | I |
| m. 20, beat 1 to m. 21, beat 1 | 302 | unclassified | I |
| m. 21, beat 1 to m. 22, beat 1 | 303 | unclassified | I |
| m. 22, beat 1 to m. 22, beat 4 | 321 | changing-tone | dominant7 |
| m. 22, beat 1 to m. 22, beat 4 | 324 | changing-tone | dominant7 |
| m. 22, beat 1 to m. 22, beat 4 | 333 | anticipation | dominant7 |
| m. 22, beat 4 to m. 23, beat 1 | 335 | unclassified | iii |
| m. 22, beat 4 to m. 23, beat 1 | 337 | unclassified | iii |
| m. 23, beat 1 to m. 24, beat 1 | 337 | unclassified | I |
| m. 23, beat 1 to m. 24, beat 1 | 348 | appoggiatura | I |
| m. 23, beat 1 to m. 24, beat 1 | 351 | unclassified | I |
| m. 23, beat 1 to m. 24, beat 1 | 354 | unclassified | I |
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
| m. 32, beat 4 to m. 33, beat 1 | 493 | unclassified | vii° |
| m. 32, beat 4 to m. 33, beat 1 | 499 | unclassified | vii° |
| m. 33, beat 1 to m. 33, beat 3 | 498 | unclassified | I |
| m. 34, beat 1 to m. 35, beat 1 | 511 | retardation | V |
| m. 34, beat 1 to m. 35, beat 1 | 518 | appoggiatura | V |
| m. 34, beat 1 to m. 35, beat 1 | 519 | unclassified | V |
| m. 34, beat 1 to m. 35, beat 1 | 526 | escape-tone | V |
| m. 34, beat 1 to m. 35, beat 1 | 527 | anticipation | V |
| m. 35, beat 1 to m. 36, beat 1 | 543 | unclassified | I |
| m. 35, beat 1 to m. 36, beat 1 | 545 | unclassified | I |
| m. 36, beat 1 to m. 37, beat 1 | 545 | unclassified | vii° |
| m. 36, beat 1 to m. 37, beat 1 | 547 | unclassified | vii° |
| m. 36, beat 1 to m. 37, beat 1 | 551 | unclassified | vii° |
| m. 36, beat 1 to m. 37, beat 1 | 558 | unclassified | vii° |
| m. 36, beat 1 to m. 37, beat 1 | 559 | passing | vii° |
| m. 36, beat 1 to m. 37, beat 1 | 563 | anticipation | vii° |
| m. 37, beat 1 to m. 38, beat 1 | 577 | passing | I |
| m. 37, beat 1 to m. 38, beat 1 | 579 | unclassified | I |
| m. 38, beat 1 to m. 39, beat 1 | 585 | unclassified | dominant7 |
| m. 38, beat 1 to m. 39, beat 1 | 588 | appoggiatura | dominant7 |
| m. 38, beat 1 to m. 39, beat 1 | 592 | unclassified | dominant7 |
| m. 38, beat 1 to m. 39, beat 1 | 596 | unclassified | dominant7 |
| m. 38, beat 1 to m. 39, beat 1 | 600 | appoggiatura | dominant7 |
| m. 38, beat 1 to m. 39, beat 1 | 604 | anticipation | dominant7 |
| m. 39, beat 1 to m. 40, beat 1 | 602 | unclassified | I |
| m. 39, beat 1 to m. 40, beat 1 | 607 | pedal | I |
| m. 39, beat 1 to m. 40, beat 1 | 610 | unclassified | I |
| m. 39, beat 1 to m. 40, beat 1 | 621 | unclassified | I |
| m. 39, beat 1 to m. 40, beat 1 | 624 | unclassified | I |
| m. 40, beat 3 to m. 40, beat 4 | 637 | unclassified | vii° |
| m. 40, beat 3 to m. 40, beat 4 | 641 | unclassified | vii° |
| m. 40, beat 4 to m. 41, beat 1 | 644 | unclassified | V |
| m. 40, beat 4 to m. 41, beat 1 | 648 | anticipation | V |
| m. 41, beat 1 to m. 41, beat 2 | 651 | unclassified | vi |
| m. 41, beat 3 to m. 41, beat 4 | 652 | unclassified | vii° |
| m. 41, beat 3 to m. 41, beat 4 | 658 | unclassified | vii° |
| m. 41, beat 3 to m. 41, beat 4 | 660 | unclassified | vii° |
| m. 41, beat 3 to m. 41, beat 4 | 663 | unclassified | vii° |
| m. 41, beat 3 to m. 41, beat 4 | 665 | anticipation | vii° |
| m. 41, beat 3 to m. 41, beat 4 | 664 | anticipation | vii° |
| m. 41, beat 4 to m. 42, beat 1 | 671 | appoggiatura | major7 |
| m. 41, beat 4 to m. 42, beat 1 | 672 | unclassified | major7 |
| m. 41, beat 4 to m. 42, beat 1 | 673 | unclassified | major7 |
| m. 42, beat 1 to m. 42, beat 3 | 686 | unclassified | I |
| m. 42, beat 1 to m. 42, beat 3 | 688 | unclassified | I |
| m. 42, beat 3 to m. 42, beat 4 | 691 | escape-tone | iii |
| m. 42, beat 3 to m. 42, beat 4 | 695 | anticipation | iii |
| m. 42, beat 4 to m. 43, beat 1 | 693 | unclassified | IV |
| m. 42, beat 4 to m. 43, beat 1 | 698 | neighbor | IV |
| m. 42, beat 4 to m. 43, beat 1 | 700 | unclassified | IV |
| m. 43, beat 1 to m. 44, beat 1 | 700 | unclassified | I |
| m. 44, beat 3 to m. 45, beat 1 | 724 | unclassified | vi |
| m. 44, beat 3 to m. 45, beat 1 | 725 | unclassified | vi |
| m. 44, beat 3 to m. 45, beat 1 | 726 | unclassified | vi |
| m. 44, beat 3 to m. 45, beat 1 | 728 | unclassified | vi |
| m. 44, beat 3 to m. 45, beat 1 | 730 | anticipation | vi |
| m. 45, beat 1 to m. 46, beat 1 | 734 | unclassified | V |
| m. 45, beat 1 to m. 46, beat 1 | 735 | appoggiatura | V |
| m. 45, beat 1 to m. 46, beat 1 | 742 | unclassified | V |
| m. 45, beat 1 to m. 46, beat 1 | 746 | anticipation | V |
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
| m. 49, beat 1 to m. 50, beat 1 | 803 | passing | dominant7 |
| m. 49, beat 1 to m. 50, beat 1 | 807 | neighbor | dominant7 |
| m. 49, beat 1 to m. 50, beat 1 | 810 | passing | dominant7 |
| m. 49, beat 1 to m. 50, beat 1 | 812 | unclassified | dominant7 |
| m. 49, beat 1 to m. 50, beat 1 | 815 | unclassified | dominant7 |
| m. 49, beat 1 to m. 50, beat 1 | 822 | unclassified | dominant7 |
| m. 49, beat 1 to m. 50, beat 1 | 826 | unclassified | dominant7 |
| m. 49, beat 1 to m. 50, beat 1 | 832 | unclassified | dominant7 |
| m. 49, beat 1 to m. 50, beat 1 | 835 | anticipation | dominant7 |
| m. 50, beat 1 to m. 51, beat 1 | 836 | unclassified | I |
| m. 50, beat 1 to m. 51, beat 1 | 839 | unclassified | I |
| m. 52, beat 1 to m. 52, beat 3 | 865 | unclassified | IV |
| m. 52, beat 1 to m. 52, beat 3 | 867 | unclassified | IV |
| m. 53, beat 1 to m. 54, beat 1 | 885 | unclassified | iii |
| m. 53, beat 1 to m. 54, beat 1 | 888 | unclassified | iii |
| m. 53, beat 1 to m. 54, beat 1 | 890 | appoggiatura | iii |
| m. 53, beat 1 to m. 54, beat 1 | 891 | unclassified | iii |
| m. 53, beat 1 to m. 54, beat 1 | 898 | escape-tone | iii |
| m. 54, beat 1 to m. 55, beat 1 | 902 | unclassified | I |
| m. 55, beat 1 to m. 56, beat 1 | 918 | unclassified | minor7 |
| m. 55, beat 1 to m. 56, beat 1 | 922 | unclassified | minor7 |
| m. 55, beat 1 to m. 56, beat 1 | 926 | unclassified | minor7 |
| m. 55, beat 1 to m. 56, beat 1 | 927 | unclassified | minor7 |
| m. 55, beat 1 to m. 56, beat 1 | 931 | unclassified | minor7 |
| m. 56, beat 1 to m. 57, beat 1 | 933 | unclassified | IV |
| m. 56, beat 1 to m. 57, beat 1 | 942 | unclassified | IV |
| m. 56, beat 1 to m. 57, beat 1 | 945 | unclassified | IV |
| m. 56, beat 1 to m. 57, beat 1 | 948 | unclassified | IV |
| m. 56, beat 1 to m. 57, beat 1 | 949 | unclassified | IV |
| m. 56, beat 1 to m. 57, beat 1 | 952 | appoggiatura | IV |
| m. 56, beat 1 to m. 57, beat 1 | 954 | escape-tone | IV |
| m. 56, beat 1 to m. 57, beat 1 | 956 | unclassified | IV |
| m. 56, beat 1 to m. 57, beat 1 | 957 | unclassified | IV |
| m. 57, beat 1 to m. 58, beat 1 | 957 | unclassified | V |
| m. 57, beat 1 to m. 58, beat 1 | 961 | neighbor | V |
| m. 57, beat 1 to m. 58, beat 1 | 964 | unclassified | V |
| m. 57, beat 1 to m. 58, beat 1 | 966 | neighbor | V |
| m. 57, beat 1 to m. 58, beat 1 | 967 | unclassified | V |
| m. 57, beat 1 to m. 58, beat 1 | 973 | appoggiatura | V |
| m. 57, beat 1 to m. 58, beat 1 | 976 | unclassified | V |
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
| m. 63, beat 3 to m. 63, beat 4 | 1098 | unclassified | vii° |
| m. 63, beat 3 to m. 63, beat 4 | 1101 | escape-tone | vii° |
| m. 63, beat 3 to m. 63, beat 4 | 1102 | escape-tone | vii° |
| m. 63, beat 4 to m. 64, beat 1 | 1109 | unclassified | dominant7 |
| m. 64, beat 1 to m. 64, beat 3 | 1109 | unclassified | I |
| m. 64, beat 1 to m. 64, beat 3 | 1117 | unclassified | I |
| m. 64, beat 1 to m. 64, beat 3 | 1118 | unclassified | I |
| m. 64, beat 1 to m. 64, beat 3 | 1119 | unclassified | I |
| m. 64, beat 3 to m. 65, beat 1 | 1119 | unclassified | ii |
| m. 64, beat 3 to m. 65, beat 1 | 1121 | unclassified | ii |
| m. 64, beat 3 to m. 65, beat 1 | 1125 | appoggiatura | ii |
| m. 64, beat 3 to m. 65, beat 1 | 1127 | unclassified | ii |
| m. 64, beat 3 to m. 65, beat 1 | 1132 | unclassified | ii |
| m. 65, beat 1 to m. 65, beat 3 | 1131 | unclassified | I |
| m. 65, beat 1 to m. 65, beat 3 | 1138 | unclassified | I |
| m. 65, beat 1 to m. 65, beat 3 | 1139 | passing | I |
| m. 65, beat 1 to m. 65, beat 3 | 1142 | passing | I |
| m. 65, beat 3 to m. 66, beat 1 | 1147 | passing | V |
| m. 65, beat 3 to m. 66, beat 1 | 1150 | passing | V |
| m. 65, beat 3 to m. 66, beat 1 | 1151 | passing | V |
| m. 65, beat 3 to m. 66, beat 1 | 1153 | escape-tone | V |
| m. 65, beat 3 to m. 66, beat 1 | 1156 | unclassified | V |
| m. 65, beat 3 to m. 66, beat 1 | 1157 | appoggiatura | V |
| m. 65, beat 3 to m. 66, beat 1 | 1161 | unclassified | V |
| m. 65, beat 3 to m. 66, beat 1 | 1160 | appoggiatura | V |

## Cadences

The library's standard-harmony analysis found 15 cadences from the chords, the meter and the soprano. Position is where the arrival begins. Key is the key region of the arrival. Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added before a section start.

| Position | Type | Key | Soprano | Beat strength | Boundary |
| --- | --- | --- | --- | --- | --- |
| m. 4, beat 1 | perfect-authentic | Eb major | 1 | strong | no |
| m. 8, beat 1 | perfect-authentic | Eb major | 1 | strong | no |
| m. 11, beat 1 | half | Eb major | 6 | strong | no |
| m. 16, beat 1 | perfect-authentic | Eb major | 1 | strong | no |
| m. 22, beat 1 | half | Bb major | 6 | strong | no |
| m. 27, beat 1 | imperfect-authentic | Bb major | 2 | strong | no |
| m. 31, beat 1 | authentic-unspecified | F minor | 3 | strong | no |
| m. 32, beat 1 | half | Eb major | 7 | strong | no |
| m. 35, beat 1 | imperfect-authentic | Eb major | 3 | strong | no |
| m. 46, beat 1 | perfect-authentic | Eb major | 1 | strong | no |
| m. 50, beat 1 | imperfect-authentic | Eb major | 3 | strong | no |
| m. 58, beat 1 | perfect-authentic | Eb major | 1 | strong | no |
| m. 60, beat 1 | evaded | Eb major | 3 | strong | no |
| m. 62, beat 1 | imperfect-authentic | Eb major | 2 | strong | no |
| m. 66, beat 1 | perfect-authentic | Eb major | 1 | strong | no |

### Competing readings

A cadence the evidence does not settle has two readings, the likelier first, each with its share of the evidence.

- m60 b1 evaded cadence (65%), or tonic prolongation (35%)

### Cadence plan

1. m. 4, beat 1: perfect-authentic in Eb major
2. m. 8, beat 1: perfect-authentic in Eb major
3. m. 11, beat 1: half in Eb major
4. m. 16, beat 1: perfect-authentic in Eb major
5. m. 22, beat 1: half in Bb major
6. m. 27, beat 1: imperfect-authentic in Bb major
7. m. 31, beat 1: authentic-unspecified in F minor
8. m. 32, beat 1: half in Eb major
9. m. 35, beat 1: imperfect-authentic in Eb major
10. m. 46, beat 1: perfect-authentic in Eb major
11. m. 50, beat 1: imperfect-authentic in Eb major
12. m. 58, beat 1: perfect-authentic in Eb major
13. m. 60, beat 1: evaded in Eb major
14. m. 62, beat 1: imperfect-authentic in Eb major
15. m. 66, beat 1: perfect-authentic in Eb major

**Counts:** perfect-authentic 6, half 3, imperfect-authentic 4, authentic-unspecified 1, evaded 1.

## Phrases

The library's standard-form analysis found 14 phrases from the soprano, the meter and the cadences. Length counts the measures of the phrase proper; Ends is what closed it: its cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in the piece; Expansion is a lengthening or shortening of the phrase's ending, or an extension after its cadence, with the measures it adds or removes.

| Phrase | Measures | Length | Cadence | Ends | Role | Expansion |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 6 | 6 | perfect-authentic (m. 4, beat 1) | cadence | beginning | contraction -2 (mm. 5 to 6) |
| 2 | mm. 7 to 9 | 3 | perfect-authentic (m. 8, beat 1) | cadence | middle | - |
| 3 | mm. 10 to 11 | 2 | half (m. 11, beat 1) | cadence | middle | - |
| 4 | mm. 12 to 16 | 5 | perfect-authentic (m. 16, beat 1) | cadence | middle | - |
| 5 | mm. 17 to 22 | 6 | half (m. 22, beat 1) | cadence | middle | contraction -2 (mm. 21 to 22) |
| 6 | mm. 23 to 27 | 5 | imperfect-authentic (m. 27, beat 1) | cadence | middle | - |
| 7 | mm. 28 to 31 | 4 | authentic-unspecified (m. 31, beat 1) | cadence | middle | - |
| 8 | m. 32 | 1 | half (m. 32, beat 1) | cadence | middle | - |
| 9 | mm. 33 to 35 | 3 | imperfect-authentic (m. 35, beat 1) | cadence | middle | - |
| 10 | mm. 36 to 48 | 13 | perfect-authentic (m. 46, beat 1) | cadence | middle | internal +5 (mm. 40 to 48) |
| 11 | mm. 49 to 50 | 2 | imperfect-authentic (m. 50, beat 1) | cadence | middle | - |
| 12 | mm. 51 to 58 | 8 | perfect-authentic (m. 58, beat 1) | cadence | middle | - |
| 13 | mm. 59 to 62 | 4 | imperfect-authentic (m. 62, beat 1) | cadence | middle | - |
| 14 | mm. 63 to 67 | 5 | perfect-authentic (m. 66, beat 1) | cadence | ending | internal +1 (mm. 63 to 67) |

- **Phrase 1:** ideas basic-idea (mm. 1 to 2), contrasting-idea (mm. 3 to 4), contrasting-idea (mm. 5 to 6); subphrases mm. 1 to 4, mm. 5 to 6; phrase model Tb (m. 1, beat 4), D (m. 3, beat 1), Te (m. 4, beat 1), Te (m. 5, beat 1), Te (m. 6, beat 1)
- **Phrase 2:** ideas basic-idea (mm. 7 to 9); phrase model D (m. 7, beat 1), Te (m. 8, beat 1), Te (m. 9, beat 1)
- **Phrase 3:** ideas basic-idea (mm. 10 to 11); phrase model PD (m. 10, beat 1), D (m. 11, beat 1)
- **Phrase 4:** ideas basic-idea (mm. 12 to 13), contrasting-idea (mm. 14 to 16); phrase model Tb (m. 12, beat 1), Tb (m. 13, beat 1), PD (m. 14, beat 1), D (m. 15, beat 1), Te (m. 16, beat 1)
- **Phrase 5:** ideas basic-idea (mm. 17 to 18), contrasting-idea (mm. 19 to 20), contrasting-idea (mm. 21 to 22); subphrases mm. 17 to 20, mm. 21 to 22; phrase model D (m. 22, beat 1)
- **Phrase 6:** ideas basic-idea (mm. 23 to 24), contrasting-idea (mm. 25 to 27); phrase model Tb (m. 23, beat 1), D (m. 26, beat 1), Te (m. 27, beat 1)
- **Phrase 7:** ideas basic-idea (mm. 28 to 29), contrasting-idea (mm. 30 to 31)
- **Phrase 8:** ideas basic-idea (m. 32); phrase model D (m. 32, beat 1)
- **Phrase 9:** ideas basic-idea (mm. 33 to 35); phrase model Tb (m. 33, beat 1), PD (m. 33, beat 3), D (m. 34, beat 1), Te (m. 35, beat 1)
- **Phrase 10:** ideas basic-idea (mm. 36 to 37), contrasting-idea (mm. 38 to 39), contrasting-idea (mm. 40 to 41), contrasting-idea (mm. 42 to 43), contrasting-idea (mm. 44 to 45), contrasting-idea (mm. 46 to 48); subphrases mm. 36 to 39, mm. 40 to 48; phrase model D (m. 45, beat 1), Te (m. 46, beat 1), Te (m. 47, beat 1), Te (m. 48, beat 1)
- **Phrase 11:** ideas basic-idea (mm. 49 to 50); phrase model D (m. 49, beat 1), Te (m. 50, beat 1)
- **Phrase 12:** ideas basic-idea (mm. 51 to 52), contrasting-idea (mm. 53 to 54), contrasting-idea (mm. 55 to 56), contrasting-idea (mm. 57 to 58); subphrases mm. 51 to 54, mm. 55 to 58; phrase model PD (m. 56, beat 1), D (m. 57, beat 1), Te (m. 58, beat 1)
- **Phrase 13:** ideas basic-idea (mm. 59 to 60), contrasting-idea (mm. 61 to 62); phrase model D (m. 61, beat 1), Te (m. 62, beat 1)
- **Phrase 14:** ideas basic-idea (mm. 63 to 64), contrasting-idea (mm. 65 to 67); phrase model PD (m. 64, beat 3), D (m. 65, beat 3), Te (m. 66, beat 1), Te (m. 67, beat 1)

## Themes

The library's standard-form analysis found 5 themes from the phrases, their ideas and the cadences. Form is the library's name for the pairing of the two parts; Commonality is how common that pairing is; Traits are what the library finds in the continuation.

| Theme | Form | Commonality | Measures | Length | Parts | Traits |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | hybrid | slightly-less-common | mm. 1 to 6 | 6 | compound-basic-idea (mm. 1 to 4), continuation (mm. 5 to 6, perfect-authentic (m. 4, beat 1)) | - |
| 2 | hybrid | slightly-less-common | mm. 17 to 22 | 6 | compound-basic-idea (mm. 17 to 20), continuation (mm. 21 to 22, half (m. 22, beat 1)) | - |
| 3 | hybrid | slightly-less-common | mm. 36 to 48 | 13 | compound-basic-idea (mm. 36 to 39), continuation (mm. 40 to 48, perfect-authentic (m. 46, beat 1)) | - |
| 4 | hybrid | slightly-less-common | mm. 51 to 58 | 8 | compound-basic-idea (mm. 51 to 54), continuation (mm. 55 to 58, perfect-authentic (m. 58, beat 1)) | - |
| 5 | period | very-common | mm. 59 to 67 | 9 | antecedent (mm. 59 to 62, imperfect-authentic (m. 62, beat 1)), consequent (mm. 63 to 67, perfect-authentic (m. 66, beat 1)) | - |

## Form plan

The kernel's form structure detection grouped the phrases into 10 sections, closing a section at each phrase that ends on a perfect or imperfect authentic cadence; the phrases after the last such cadence form the last section. Sections are lettered by first appearance, and a section whose opening theme returns an earlier section's material takes that letter with a prime. Key is the key the section opens in.

| Section | Measures | Key | Phrases | Closing cadence | Returns |
| --- | --- | --- | --- | --- | --- |
| A | mm. 1 to 6 | Eb major | 1 | perfect-authentic (m. 4, beat 1) | - |
| B | mm. 7 to 9 | Eb major | 2 | perfect-authentic (m. 8, beat 1) | - |
| C | mm. 10 to 16 | Eb major | 3 to 4 | perfect-authentic (m. 16, beat 1) | - |
| D | mm. 17 to 27 | Eb major | 5 to 6 | imperfect-authentic (m. 27, beat 1) | - |
| E | mm. 28 to 35 | F minor | 7 to 9 | imperfect-authentic (m. 35, beat 1) | - |
| F | mm. 36 to 48 | Eb major | 10 | perfect-authentic (m. 46, beat 1) | - |
| G | mm. 49 to 50 | Eb major | 11 | imperfect-authentic (m. 50, beat 1) | - |
| H | mm. 51 to 58 | Eb major | 12 | perfect-authentic (m. 58, beat 1) | - |
| I | mm. 59 to 62 | Eb major | 13 | imperfect-authentic (m. 62, beat 1) | - |
| J | mm. 63 to 67 | Eb major | 14 | perfect-authentic (m. 66, beat 1) | - |

### Outline

- form (mm. 1 to 67)
  - section A (mm. 1 to 6, Eb major)
    - phrase 1 (mm. 1 to 6)
      - subphrase 1 (mm. 1 to 4)
        - idea basic-idea (mm. 1 to 2)
        - idea contrasting-idea (mm. 3 to 4)
      - subphrase 2 (mm. 5 to 6)
        - idea contrasting-idea (mm. 5 to 6)
  - section B (mm. 7 to 9, Eb major)
    - phrase 2 (mm. 7 to 9)
      - idea basic-idea (mm. 7 to 9)
  - section C (mm. 10 to 16, Eb major)
    - phrase 3 (mm. 10 to 11)
      - idea basic-idea (mm. 10 to 11)
    - phrase 4 (mm. 12 to 16)
      - idea basic-idea (mm. 12 to 13)
      - idea contrasting-idea (mm. 14 to 16)
  - section D (mm. 17 to 27, Eb major)
    - phrase 5 (mm. 17 to 22)
      - subphrase 1 (mm. 17 to 20)
        - idea basic-idea (mm. 17 to 18)
        - idea contrasting-idea (mm. 19 to 20)
      - subphrase 2 (mm. 21 to 22)
        - idea contrasting-idea (mm. 21 to 22)
    - phrase 6 (mm. 23 to 27)
      - idea basic-idea (mm. 23 to 24)
      - idea contrasting-idea (mm. 25 to 27)
  - section E (mm. 28 to 35, F minor)
    - phrase 7 (mm. 28 to 31)
      - idea basic-idea (mm. 28 to 29)
      - idea contrasting-idea (mm. 30 to 31)
    - phrase 8 (m. 32)
      - idea basic-idea (m. 32)
    - phrase 9 (mm. 33 to 35)
      - idea basic-idea (mm. 33 to 35)
  - section F (mm. 36 to 48, Eb major)
    - phrase 10 (mm. 36 to 48)
      - subphrase 1 (mm. 36 to 39)
        - idea basic-idea (mm. 36 to 37)
        - idea contrasting-idea (mm. 38 to 39)
      - subphrase 2 (mm. 40 to 48)
        - idea contrasting-idea (mm. 40 to 41)
        - idea contrasting-idea (mm. 42 to 43)
        - idea contrasting-idea (mm. 44 to 45)
        - idea contrasting-idea (mm. 46 to 48)
  - section G (mm. 49 to 50, Eb major)
    - phrase 11 (mm. 49 to 50)
      - idea basic-idea (mm. 49 to 50)
  - section H (mm. 51 to 58, Eb major)
    - phrase 12 (mm. 51 to 58)
      - subphrase 1 (mm. 51 to 54)
        - idea basic-idea (mm. 51 to 52)
        - idea contrasting-idea (mm. 53 to 54)
      - subphrase 2 (mm. 55 to 58)
        - idea contrasting-idea (mm. 55 to 56)
        - idea contrasting-idea (mm. 57 to 58)
  - section I (mm. 59 to 62, Eb major)
    - phrase 13 (mm. 59 to 62)
      - idea basic-idea (mm. 59 to 60)
      - idea contrasting-idea (mm. 61 to 62)
  - section J (mm. 63 to 67, Eb major)
    - phrase 14 (mm. 63 to 67)
      - idea basic-idea (mm. 63 to 64)
      - idea contrasting-idea (mm. 65 to 67)

## Motives

The library's motive analysis found 1 motive family in the soprano, the highest sounding note at each attack. A family is a short figure that recurs; Notes is the length of the figure, Pattern whether its occurrences match by scale steps or by semitones, Measures the measures the occurrences start in, Phrases the phrases they lie in (an occurrence across a phrase boundary lies in none), and Transformations how many occurrences are of each kind the library names against the first occurrence (exact, real-transposed, tonal-transposed, inverted, retrograde, augmented, diminished, fragmented, varied).

| Family | Notes | Degrees | Pattern | Occurrences | Measures | Phrases | Transformations |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 4 | 1 1 b3 1 | scale steps | 3 | 31, 50, 58 | 7, 11, 12 | exact 1, real-transposed 2 |

## Melody

The library's melody adapter read the soprano of each phrase as a line of notes: the soprano notes that start in the phrase. Range is the lowest and highest note and the semitones between them; Climax is the first note at the highest pitch, with how many notes sound it when it is not one; Contour counts the intervals between consecutive notes that go up, go down and repeat, and how many of them are steps and leaps.

| Phrase | Measures | Notes | Range | Climax | Up | Down | Same | Steps | Leaps |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 6 | 11 | A#4 to A#5 (12) | A#5, m. 5, beat 1.01 | 6 | 4 | 0 | 4 | 6 |
| 2 | mm. 7 to 9 | 4 | D#5 to C6 (9) | C6, m. 7, beat 1.97 | 0 | 2 | 1 | 1 | 1 |
| 3 | mm. 10 to 11 | 5 | C5 to G#5 (8) | G#5, m. 11, beat 2.16 | 1 | 2 | 1 | 2 | 1 |
| 4 | mm. 12 to 16 | 12 | B4 to C6 (13) | C6, m. 15, beat 2.15 | 4 | 7 | 0 | 6 | 3 |
| 5 | mm. 17 to 22 | 16 | D#5 to D#6 (12) | D#6, m. 22, beat 2.09 | 5 | 7 | 2 | 7 | 5 |
| 6 | mm. 23 to 27 | 11 | F5 to D6 (9) | D6, m. 23, beat 3.37 | 5 | 5 | 0 | 7 | 2 |
| 7 | mm. 28 to 31 | 11 | F4 to G#5 (15) | G#5, m. 29, beat 1.02, 4 notes | 3 | 2 | 4 | 5 | 0 |
| 8 | m. 32 | 2 | D4 to D4 (0) | D4, m. 32, beat 1.33, 2 notes | 0 | 0 | 1 | 0 | 0 |
| 9 | mm. 33 to 35 | 8 | D5 to G5 (5) | G5, m. 34, beat 4.04 | 3 | 3 | 1 | 5 | 1 |
| 10 | mm. 36 to 48 | 36 | C5 to G#6 (20) | G#6, m. 38, beat 1.34 | 15 | 16 | 3 | 12 | 19 |
| 11 | mm. 49 to 50 | 8 | G4 to D6 (19) | D6, m. 49, beat 3 | 2 | 4 | 1 | 0 | 6 |
| 12 | mm. 51 to 58 | 23 | F4 to C6 (19) | C6, m. 56, beat 3.33, 2 notes | 8 | 13 | 1 | 6 | 12 |
| 13 | mm. 59 to 62 | 15 | G4 to G6 (24) | G6, m. 60, beat 3.39 | 3 | 11 | 0 | 7 | 5 |
| 14 | mm. 63 to 67 | 14 | D#5 to A#6 (19) | A#6, m. 65, beat 1 | 7 | 5 | 1 | 3 | 8 |

### Leaps

A leap is an interval of a third or larger. The library's rule asks a leap of a fourth or larger to be followed by a step in the opposite direction: such a leap is Recovered when one follows, Unrecovered when a step the same way, a repeated note or another leap does, and Not judged when nothing follows it; a third asks nothing (Not required). Consecutive counts the pairs of leaps in a row the library's rule finds fault with.

| Phrase | Leaps | Recovered | Unrecovered | Not required | Not judged | Consecutive |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 6 | 1 | 2 | 3 | 0 | 3 |
| 2 | 1 | 0 | 1 | 0 | 0 | 0 |
| 3 | 1 | 1 | 0 | 0 | 0 | 0 |
| 4 | 3 | 1 | 1 | 1 | 0 | 0 |
| 5 | 5 | 1 | 0 | 2 | 2 | 0 |
| 6 | 2 | 1 | 1 | 0 | 0 | 0 |
| 7 | 0 | 0 | 0 | 0 | 0 | 0 |
| 8 | 0 | 0 | 0 | 0 | 0 | 0 |
| 9 | 1 | 0 | 0 | 1 | 0 | 0 |
| 10 | 19 | 1 | 7 | 10 | 1 | 8 |
| 11 | 6 | 0 | 2 | 4 | 0 | 5 |
| 12 | 12 | 3 | 7 | 2 | 0 | 6 |
| 13 | 5 | 0 | 2 | 2 | 1 | 1 |
| 14 | 8 | 0 | 4 | 4 | 0 | 5 |

### Non-chord tones

Each melody note is set against the chord of the segment it starts in, as the library's chord identification reads it: a Chord tone is one of the chord's tones, a Non-chord tone is one it left over and named, and Unclassified notes are in a segment with no chord or were left unnamed. By type counts the non-chord tones under the library's names.

| Phrase | Notes | Chord tones | Non-chord tones | Unclassified | By type |
| --- | --- | --- | --- | --- | --- |
| 1 | 11 | 8 | 2 | 1 | passing 1, neighbor 1 |
| 2 | 4 | 3 | 1 | 0 | appoggiatura 1 |
| 3 | 5 | 2 | 2 | 1 | passing 2 |
| 4 | 12 | 5 | 4 | 3 | passing 3, neighbor 1 |
| 5 | 16 | 8 | 3 | 5 | appoggiatura 3 |
| 6 | 11 | 5 | 3 | 3 | passing 1, neighbor 2 |
| 7 | 11 | 9 | 1 | 1 | neighbor 1 |
| 8 | 2 | 2 | 0 | 0 | - |
| 9 | 8 | 5 | 3 | 0 | passing 2, escape-tone 1 |
| 10 | 36 | 21 | 3 | 12 | neighbor 1, appoggiatura 1, escape-tone 1 |
| 11 | 8 | 7 | 0 | 1 | - |
| 12 | 23 | 11 | 3 | 9 | passing 1, appoggiatura 2 |
| 13 | 15 | 9 | 4 | 2 | passing 2, appoggiatura 2 |
| 14 | 14 | 9 | 1 | 4 | passing 1 |

### Tendencies

The library's melodic tendencies are soft preferences a well-formed line usually shows: Pitch proximity (steps outnumber leaps, small leaps outnumber large ones), Step declination (descending steps outnumber ascending ones), Step inertia (the line continues in its direction at least as often as it reverses), Melodic regression (extreme notes turn back toward the middle) and Melodic arch (the line rises to a climax in its middle half and ends below it); a phrase either shows a tendency or does not. The seventh degree column counts the notes the library labels the seventh degree that another note follows, judged by its leading tone rule: a leading tone goes up to the tonic by step, and a seventh a whole step below the tonic does not. Followed is the count that did, Broken the count that did not, with where each broken one stands.

| Phrase | Pitch proximity | Step declination | Step inertia | Melodic regression | Melodic arch | Seventh degree followed | Seventh degree broken |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | does not hold | does not hold | holds | does not hold | holds | 0 | 1 (m. 3, beat 2.98) |
| 2 | does not hold | holds | holds | holds | does not hold | 0 | 0 |
| 3 | does not hold | holds | does not hold | does not hold | holds | 0 | 1 (m. 10, beat 1.08) |
| 4 | does not hold | holds | holds | does not hold | holds | 0 | 1 (m. 14, beat 1.03) |
| 5 | does not hold | holds | holds | does not hold | does not hold | 0 | 0 |
| 6 | holds | holds | does not hold | does not hold | does not hold | 0 | 1 (m. 23, beat 3.37) |
| 7 | does not hold | does not hold | holds | does not hold | holds | 0 | 0 |
| 8 | holds | holds | holds | holds | does not hold | 0 | 1 (m. 32, beat 1.33) |
| 9 | does not hold | does not hold | holds | does not hold | holds | 0 | 1 (m. 34, beat 1.04) |
| 10 | does not hold | holds | does not hold | does not hold | does not hold | 0 | 2 (m. 44, beat 4.93; m. 45, beat 3.01) |
| 11 | does not hold | holds | holds | does not hold | holds | 0 | 1 (m. 49, beat 3) |
| 12 | does not hold | holds | holds | does not hold | holds | 0 | 2 (m. 52, beat 1.02; m. 56, beat 1.01) |
| 13 | holds | holds | holds | does not hold | holds | 0 | 0 |
| 14 | does not hold | does not hold | holds | holds | holds | 1 | 1 (m. 65, beat 3) |

### Development

Each family's occurrences in time order, as the measure they start in and the kind the library names against the family's prototype, which is the original.

- **Family 1:** m31 original > m50 real-transposed > m58 real-transposed

## Progression patterns

13 recurring progressions of two to six chords, counted over the library's Roman numerals: a held chord counts once, and a progression does not cross an unlabelled chord or a change of key. Ranked by count, then length, then first occurrence; the top 10 are shown. Measures are where each occurrence begins and ends.

| Rank | Progression | Count | Measures |
| --- | --- | --- | --- |
| 1 | V > I | 7 | mm. 3 to 6, mm. 7 to 9, mm. 15 to 16, mm. 34 to 35, mm. 45 to 48, mm. 57 to 58, mm. 65 to 67 |
| 2 | I > V7 | 7 | mm. 16 to 17, mm. 21 to 22, mm. 25 to 26, mm. 37 to 38, mm. 46 to 49, mm. 58 to 59, mm. 60 to 61 |
| 3 | I > V7 > I | 6 | mm. 16 to 18, mm. 25 to 27, mm. 37 to 39, mm. 46 to 50, mm. 58 to 60, mm. 60 to 62 |
| 4 | V > I > V7 > I | 3 | mm. 15 to 18, mm. 45 to 50, mm. 57 to 60 |
| 5 | I > V > I | 3 | mm. 1 to 6, mm. 4 to 9, mm. 65 to 67 |
| 6 | V7 > iii | 3 | m. 11, m. 22, m. 32 |
| 7 | iii > I | 3 | mm. 11 to 13, mm. 22 to 23, mm. 53 to 54 |
| 8 | vii° > I | 3 | mm. 19 to 20, mm. 32 to 33, mm. 36 to 37 |
| 9 | IV > V > I > V7 > I | 2 | mm. 14 to 18, mm. 56 to 60 |
| 10 | V7 > iii > I | 2 | mm. 11 to 13, mm. 22 to 23 |

## Harmonic rhythm

How often the harmony changes. A change is a chord whose base Roman numeral or key differs from the chord before it, counted in the measure it starts in. Changes lists the measures of each row in order; Mean is changes per measure over the row.

| Measures | Changes | Mean |
| --- | --- | --- |
| mm. 1 to 4 | 1, 0, 1, 1 | 0.75 |
| mm. 5 to 8 | 0, 0, 1, 1 | 0.5 |
| mm. 9 to 12 | 0, 1, 2, 1 | 1 |
| mm. 13 to 16 | 0, 1, 1, 1 | 0.75 |
| mm. 17 to 20 | 1, 1, 1, 1 | 1 |
| mm. 21 to 24 | 1, 2, 1, 2 | 1.5 |
| mm. 25 to 28 | 2, 1, 1, 3 | 1.75 |
| mm. 29 to 32 | 1, 1, 1, 3 | 1.5 |
| mm. 33 to 36 | 2, 1, 1, 1 | 1.25 |
| mm. 37 to 40 | 1, 1, 1, 3 | 1.5 |
| mm. 41 to 44 | 4, 3, 1, 1 | 2.25 |
| mm. 45 to 48 | 1, 1, 0, 0 | 0.5 |
| mm. 49 to 52 | 1, 1, 1, 2 | 1.25 |
| mm. 53 to 56 | 1, 1, 1, 1 | 1 |
| mm. 57 to 60 | 1, 1, 1, 1 | 1 |
| mm. 61 to 64 | 1, 1, 3, 2 | 1.75 |
| mm. 65 to 67 | 2, 1, 0 | 1 |

**Mean:** 1.19 changes per measure over 67 measures.

### Approach to cadences

Changes per measure over the two measures before each cadence's arrival (Approach), against the two measures before those (Before). Faster is yes when the approach changes harmony more often.

| Cadence | Arrival | Before | Approach | Faster |
| --- | --- | --- | --- | --- |
| 1 | m. 4 | 1 | 0.5 | no |
| 2 | m. 8 | 0.5 | 0.5 | no |
| 3 | m. 11 | 1 | 0.5 | no |
| 4 | m. 16 | 0.5 | 1 | yes |
| 5 | m. 22 | 1 | 1 | no |
| 6 | m. 27 | 1.5 | 1.5 | no |
| 7 | m. 31 | 2 | 1 | no |
| 8 | m. 32 | 2 | 1 | no |
| 9 | m. 35 | 2 | 1.5 | no |
| 10 | m. 46 | 2 | 1 | no |
| 11 | m. 50 | 0.5 | 0.5 | no |
| 12 | m. 58 | 1 | 1 | no |
| 13 | m. 60 | 1 | 1 | no |
| 14 | m. 62 | 1 | 1 | no |
| 15 | m. 66 | 2 | 2 | no |

## Rhythm and meter

**Meter fit:** 12/8, scale 1, no pickup (fit 0.94); no better reading found

### Density

The library's density profile of each phrase, for all its notes and for each line that has a note in it (the soprano and bass of the outer voices, and Other for the rest). A note is in a phrase when its onset is; a chord is one attack. Attacks per beat is read measure by measure over the beats the phrase spans in it, as a mean and the largest; Mean gap is the mean distance between attacks in whole notes; On the beat is the share of attacks exactly on a beat. Durations lists the notes' lengths in whole notes, shortest first, with how many notes have each. The patterns that recur are the motive families' rhythms (see Motives).

| Phrase | Measures | Line | Notes | Attacks | Per beat | Max per beat | Mean gap | On the beat | Durations |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 6 | all | 73 | 71 | 2.96 | 4 | 0.11 | 29.6% | 1/16 (1), 1/8 (31), 1/4 (23), 161/480 (1), 35/96 (1), 89/240 (1), 3/8 (7), 61/160 (1), 37/96 (1), 191/480 (1), 23/40 (1), 3/4 (1), 55/48 (1), 37/32 (1), 643/480 (1) |
| 1 | mm. 1 to 6 | soprano | 11 | 11 | 0.46 | 0.75 | 0.75 | 27.3% | 1/16 (1), 161/480 (1), 89/240 (1), 3/8 (1), 61/160 (1), 191/480 (1), 23/40 (1), 3/4 (1), 55/48 (1), 37/32 (1), 643/480 (1) |
| 1 | mm. 1 to 6 | bass | 21 | 21 | 0.88 | 1.25 | 0.36 | 95.2% | 1/8 (7), 1/4 (8), 3/8 (6) |
| 1 | mm. 1 to 6 | other | 41 | 41 | 1.71 | 2.25 | 0.18 | 0.0% | 1/8 (24), 1/4 (15), 35/96 (1), 37/96 (1) |
| 2 | mm. 7 to 9 | all | 43 | 41 | 3.42 | 3.75 | 0.11 | 29.3% | 1/32 (1), 1/8 (19), 1/4 (15), 35/96 (1), 3/8 (2), 31/80 (1), 187/480 (1), 13/32 (1), 151/240 (1), 913/480 (1) |
| 2 | mm. 7 to 9 | soprano | 4 | 4 | 0.33 | 0.5 | 1.13 | 0.0% | 31/80 (1), 187/480 (1), 151/240 (1), 913/480 (1) |
| 2 | mm. 7 to 9 | bass | 12 | 12 | 1 | 1 | 0.38 | 100.0% | 1/8 (4), 1/4 (6), 3/8 (2) |
| 2 | mm. 7 to 9 | other | 27 | 26 | 2.17 | 2.5 | 0.17 | 3.8% | 1/32 (1), 1/8 (15), 1/4 (9), 35/96 (1), 13/32 (1) |
| 3 | mm. 10 to 11 | all | 31 | 31 | 4.5 | 5 | 0.09 | 22.6% | 1/32 (1), 17/480 (2), 11/240 (1), 1/20 (1), 1/8 (11), 1/4 (8), 41/160 (1), 59/160 (1), 3/8 (2), 13/32 (1), 33/80 (1), 89/120 (1) |
| 3 | mm. 10 to 11 | soprano | 5 | 5 | 0.75 | 1 | 0.65 | 0.0% | 17/480 (1), 11/240 (1), 13/32 (1), 33/80 (1), 89/120 (1) |
| 3 | mm. 10 to 11 | bass | 6 | 6 | 0.88 | 1 | 0.45 | 100.0% | 1/8 (2), 1/4 (2), 3/8 (2) |
| 3 | mm. 10 to 11 | other | 20 | 20 | 2.88 | 3 | 0.12 | 5.0% | 1/32 (1), 17/480 (1), 1/20 (1), 1/8 (9), 1/4 (6), 41/160 (1), 59/160 (1) |
| 4 | mm. 12 to 16 | all | 81 | 76 | 3.8 | 4.5 | 0.1 | 26.3% | 11/480 (1), 1/32 (1), 23/480 (2), 1/16 (1), 1/8 (36), 13/96 (1), 23/160 (1), 73/480 (1), 7/32 (1), 1/4 (21), 5/16 (1), 43/120 (1), 29/80 (1), 89/240 (1), 3/8 (5), 19/48 (1), ... |
| 4 | mm. 12 to 16 | soprano | 12 | 12 | 0.6 | 0.75 | 0.66 | 16.7% | 1/16 (1), 1/8 (1), 23/160 (1), 29/80 (1), 89/240 (1), 3/8 (1), 19/48 (1), 47/96 (1), 101/160 (1), 111/160 (1), 9/10 (1), 9/8 (1) |
| 4 | mm. 12 to 16 | bass | 19 | 19 | 0.95 | 1 | 0.4 | 100.0% | 1/8 (7), 1/4 (8), 3/8 (4) |
| 4 | mm. 12 to 16 | other | 50 | 50 | 2.5 | 3.25 | 0.15 | 6.0% | 11/480 (1), 1/32 (1), 23/480 (2), 1/8 (28), 13/96 (1), 73/480 (1), 7/32 (1), 1/4 (13), 5/16 (1), 43/120 (1) |
| 5 | mm. 17 to 22 | all | 102 | 96 | 4.22 | 5.5 | 0.09 | 22.9% | 3/80 (1), 19/480 (1), 7/160 (1), 1/20 (1), 7/120 (1), 11/160 (1), 3/40 (1), 3/32 (1), 53/480 (1), 19/160 (1), 1/8 (47), 13/96 (1), 67/480 (1), 17/120 (2), 73/480 (3), 5/32 (2), ... |
| 5 | mm. 17 to 22 | soprano | 17 | 17 | 0.75 | 1 | 0.52 | 5.9% | 19/480 (1), 1/20 (1), 7/120 (1), 19/160 (1), 13/96 (1), 67/480 (1), 17/120 (1), 73/480 (2), 5/32 (1), 7/30 (1), 1/4 (1), 37/96 (1), 119/240 (1), 49/80 (1), 343/480 (1), 159/160 (1) |
| 5 | mm. 17 to 22 | bass | 23 | 23 | 1 | 1.25 | 0.38 | 91.3% | 1/8 (12), 17/120 (1), 1/4 (10) |
| 5 | mm. 17 to 22 | other | 62 | 60 | 2.64 | 3.5 | 0.14 | 5.0% | 3/80 (1), 7/160 (1), 11/160 (1), 3/40 (1), 3/32 (1), 53/480 (1), 1/8 (35), 73/480 (1), 5/32 (1), 13/80 (3), 43/240 (1), 101/480 (1), 17/80 (1), 23/96 (1), 39/160 (1), 1/4 (7), ... |
| 6 | mm. 23 to 27 | all | 88 | 78 | 3.9 | 4.5 | 0.1 | 25.6% | 1/32 (1), 13/240 (1), 7/120 (1), 19/240 (1), 3/32 (3), 47/480 (1), 49/480 (1), 9/80 (1), 19/160 (1), 29/240 (1), 1/8 (35), 13/96 (1), 11/80 (1), 3/20 (1), 1/4 (27), 77/240 (1), ... |
| 6 | mm. 23 to 27 | soprano | 11 | 11 | 0.55 | 1 | 0.64 | 18.2% | 3/32 (1), 49/480 (1), 9/80 (1), 1/8 (1), 13/96 (1), 3/8 (1), 91/240 (1), 23/60 (1), 187/480 (1), 73/96 (1), 25/32 (1) |
| 6 | mm. 23 to 27 | bass | 19 | 19 | 0.95 | 1 | 0.4 | 100.0% | 1/8 (7), 1/4 (12) |
| 6 | mm. 23 to 27 | other | 58 | 54 | 2.7 | 3.25 | 0.14 | 5.6% | 1/32 (1), 13/240 (1), 7/120 (1), 19/240 (1), 3/32 (2), 47/480 (1), 19/160 (1), 29/240 (1), 1/8 (27), 11/80 (1), 3/20 (1), 1/4 (15), 77/240 (1), 3/8 (1), 199/480 (1), 15/32 (1), ... |
| 7 | mm. 28 to 31 | all | 61 | 58 | 3.62 | 4 | 0.1 | 27.6% | 1/32 (1), 1/8 (30), 3/16 (1), 23/120 (1), 9/40 (1), 1/4 (19), 133/480 (1), 29/80 (1), 89/240 (1), 3/8 (1), 91/240 (1), 61/160 (1), 199/480 (1), 99/160 (1) |
| 7 | mm. 28 to 31 | soprano | 11 | 11 | 0.69 | 0.75 | 0.54 | 27.3% | 1/32 (1), 1/8 (1), 3/16 (1), 23/120 (1), 9/40 (1), 1/4 (1), 29/80 (1), 89/240 (1), 3/8 (1), 199/480 (1), 99/160 (1) |
| 7 | mm. 28 to 31 | bass | 15 | 15 | 0.94 | 1.25 | 0.4 | 93.3% | 1/8 (8), 1/4 (7) |
| 7 | mm. 28 to 31 | other | 35 | 35 | 2.19 | 2.5 | 0.17 | 5.7% | 1/8 (21), 1/4 (11), 133/480 (1), 91/240 (1), 61/160 (1) |
| 8 | m. 32 | all | 6 | 6 | 3 | 3 | 0.12 | 33.3% | 1/8 (4), 1/4 (2) |
| 8 | m. 32 | soprano | 2 | 2 | 1 | 1 | 0.38 | 0.0% | 1/8 (1), 1/4 (1) |
| 8 | m. 32 | bass | 2 | 2 | 1 | 1 | 0.38 | 100.0% | 1/8 (1), 1/4 (1) |
| 8 | m. 32 | other | 2 | 2 | 1 | 1 | 0.38 | 0.0% | 1/8 (2) |
| 9 | mm. 33 to 35 | all | 46 | 44 | 3.67 | 3.75 | 0.1 | 27.3% | 9/80 (1), 11/96 (1), 1/8 (26), 73/480 (1), 1/4 (10), 173/480 (1), 89/240 (2), 47/120 (1), 13/32 (1), 15/16 (1), 177/160 (1) |
| 9 | mm. 33 to 35 | soprano | 7 | 7 | 0.58 | 1 | 0.69 | 28.6% | 11/96 (1), 173/480 (1), 89/240 (1), 47/120 (1), 13/32 (1), 15/16 (1), 177/160 (1) |
| 9 | mm. 33 to 35 | bass | 12 | 12 | 1 | 1 | 0.38 | 100.0% | 1/8 (6), 1/4 (6) |
| 9 | mm. 33 to 35 | other | 27 | 27 | 2.25 | 2.5 | 0.16 | 0.0% | 9/80 (1), 1/8 (20), 73/480 (1), 1/4 (4), 89/240 (1) |
| 10 | mm. 36 to 48 | all | 256 | 220 | 4.23 | 5.25 | 0.09 | 23.2% | 1/240 (1), 7/480 (1), 1/32 (2), 3/80 (1), 1/24 (3), 23/480 (1), 5/96 (1), 9/160 (1), 1/16 (9), 1/15 (1), 11/160 (1), 37/480 (1), 13/160 (1), 41/480 (1), 43/480 (2), 3/32 (8), ... |
| 10 | mm. 36 to 48 | soprano | 36 | 36 | 0.69 | 1 | 0.53 | 19.4% | 1/24 (2), 5/96 (1), 9/160 (1), 1/16 (1), 43/480 (1), 3/32 (1), 1/10 (1), 53/480 (1), 1/8 (4), 2/15 (1), 13/96 (1), 11/80 (1), 71/480 (1), 5/32 (3), 13/80 (1), 31/96 (1), ... |
| 10 | mm. 36 to 48 | bass | 52 | 52 | 1 | 1.25 | 0.38 | 92.3% | 1/8 (22), 1/4 (26), 3/8 (4) |
| 10 | mm. 36 to 48 | other | 168 | 156 | 3 | 4.25 | 0.13 | 9.0% | 1/240 (1), 7/480 (1), 1/32 (2), 3/80 (1), 1/24 (1), 23/480 (1), 1/16 (8), 1/15 (1), 11/160 (1), 37/480 (1), 13/160 (1), 41/480 (1), 43/480 (1), 3/32 (7), 47/480 (1), 1/10 (1), ... |
| 11 | mm. 49 to 50 | all | 49 | 37 | 4.62 | 5.75 | 0.08 | 21.6% | 1/16 (16), 3/32 (4), 1/8 (14), 7/32 (1), 1/4 (10), 5/16 (1), 3/8 (3) |
| 11 | mm. 49 to 50 | soprano | 8 | 8 | 1 | 1 | 0.39 | 37.5% | 1/16 (2), 3/32 (1), 1/8 (1), 7/32 (1), 1/4 (2), 3/8 (1) |
| 11 | mm. 49 to 50 | bass | 8 | 8 | 1 | 1 | 0.38 | 100.0% | 1/8 (2), 1/4 (4), 3/8 (2) |
| 11 | mm. 49 to 50 | other | 33 | 28 | 3.5 | 5 | 0.1 | 10.7% | 1/16 (14), 3/32 (3), 1/8 (11), 1/4 (4), 5/16 (1) |
| 12 | mm. 51 to 58 | all | 138 | 128 | 4 | 5.25 | 0.09 | 25.0% | 1/96 (1), 1/80 (1), 1/48 (1), 1/24 (1), 7/160 (1), 5/96 (2), 9/160 (1), 1/16 (5), 17/240 (1), 7/96 (1), 3/32 (5), 1/8 (61), 17/120 (1), 9/40 (1), 59/240 (1), 1/4 (35), ... |
| 12 | mm. 51 to 58 | soprano | 23 | 23 | 0.72 | 1.25 | 0.51 | 13.0% | 5/96 (2), 1/16 (1), 3/32 (2), 1/8 (3), 9/40 (1), 59/240 (1), 1/4 (2), 11/30 (1), 59/160 (2), 3/8 (1), 13/32 (1), 223/480 (1), 19/40 (1), 247/480 (1), 283/480 (1), 61/96 (1), ... |
| 12 | mm. 51 to 58 | bass | 31 | 31 | 0.97 | 1.25 | 0.39 | 93.5% | 1/8 (15), 1/4 (12), 3/8 (4) |
| 12 | mm. 51 to 58 | other | 84 | 80 | 2.5 | 3.5 | 0.15 | 6.2% | 1/96 (1), 1/80 (1), 1/48 (1), 1/24 (1), 7/160 (1), 9/160 (1), 1/16 (4), 17/240 (1), 7/96 (1), 3/32 (3), 1/8 (43), 17/120 (1), 1/4 (21), 41/160 (1), 127/480 (1), 163/480 (1), ... |
| 13 | mm. 59 to 62 | all | 97 | 82 | 5.12 | 6.25 | 0.07 | 19.5% | 1/32 (3), 17/480 (2), 1/24 (1), 1/20 (1), 1/16 (5), 11/160 (2), 17/240 (1), 7/96 (1), 13/160 (1), 41/480 (1), 43/480 (1), 3/32 (4), 1/10 (1), 5/48 (3), 19/160 (1), 1/8 (31), ... |
| 13 | mm. 59 to 62 | soprano | 15 | 15 | 0.94 | 1 | 0.41 | 26.7% | 1/16 (1), 17/240 (1), 43/480 (1), 3/32 (1), 1/10 (1), 5/48 (1), 1/8 (1), 31/240 (1), 3/20 (1), 41/240 (2), 3/16 (1), 7/32 (1), 1/4 (1), 43/160 (1) |
| 13 | mm. 59 to 62 | bass | 16 | 16 | 1 | 1 | 0.38 | 100.0% | 1/8 (3), 1/4 (9), 3/8 (4) |
| 13 | mm. 59 to 62 | other | 66 | 57 | 3.56 | 5 | 0.1 | 3.5% | 1/32 (3), 17/480 (2), 1/24 (1), 1/20 (1), 1/16 (4), 11/160 (2), 7/96 (1), 13/160 (1), 41/480 (1), 3/32 (3), 5/48 (2), 19/160 (1), 1/8 (27), 2/15 (2), 11/80 (1), 5/32 (1), ... |
| 14 | mm. 63 to 67 | all | 103 | 79 | 4.07 | 5.25 | 0.09 | 24.1% | 1/32 (1), 1/16 (15), 3/32 (1), 7/60 (1), 1/8 (42), 31/240 (1), 21/160 (2), 2/15 (2), 13/96 (2), 67/480 (2), 5/32 (1), 1/4 (23), 41/160 (2), 173/480 (1), 3/8 (2), 91/240 (1), ... |
| 14 | mm. 63 to 67 | soprano | 14 | 14 | 0.72 | 1 | 0.44 | 42.9% | 1/16 (2), 1/8 (1), 13/96 (1), 1/4 (4), 41/160 (2), 173/480 (1), 91/240 (1), 15/32 (1), 9/8 (1) |
| 14 | mm. 63 to 67 | bass | 20 | 20 | 1.05 | 1.25 | 0.36 | 95.0% | 1/8 (10), 1/4 (9), 3/8 (1) |
| 14 | mm. 63 to 67 | other | 69 | 59 | 3.03 | 4 | 0.11 | 8.5% | 1/32 (1), 1/16 (13), 3/32 (1), 7/60 (1), 1/8 (31), 31/240 (1), 21/160 (2), 2/15 (2), 13/96 (1), 67/480 (2), 5/32 (1), 1/4 (10), 3/8 (1), 31/80 (1), 9/8 (1) |

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
| 1 | mm. 1 to 6 | 4 | 1, 5 | 1 regular, 1 contracted | m. 4 | 0.5 |
| 2 | mm. 7 to 9 | 4 | 7 | 1 short | m. 8 | 0.5 |
| 3 | mm. 10 to 11 | 2 | 10 | 1 regular | m. 11 | 1 |
| 4 | mm. 12 to 16 | 4 | 12, 16 | 1 regular, 1 short | m. 16 | 0.9 |
| 5 | mm. 17 to 22 | 4 | 17, 21 | 1 regular, 1 contracted | m. 22 | 1 |
| 6 | mm. 23 to 27 | 4 | 23, 27 | 1 regular, 1 short | m. 27 | 0.9 |
| 7 | mm. 28 to 31 | 4 | 28 | 1 regular | m. 31 | 1 |
| 8 | m. 32 | 4 | 32 | 1 short | m. 32 | 0.5 |
| 9 | mm. 33 to 35 | 2 | 33, 35 | 1 regular, 1 short | m. 35 | 0.83 |
| 10 | mm. 36 to 48 | 4 | 36, 40 | 1 regular, 1 expanded | m. 43 | 1 |
| 11 | mm. 49 to 50 | 2 | 49 | 1 regular | m. 50 | 1 |
| 12 | mm. 51 to 58 | 4 | 51, 55 | 2 regular | m. 58 | 1 |
| 13 | mm. 59 to 62 | 4 | 59 | 1 regular | m. 62 | 1 |
| 14 | mm. 63 to 67 | 4 | 63 | 1 expanded | m. 66 | 1 |

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

4 voices over 90 chord segments, 23 motions between consecutive segments: 10 contrary, 6 similar, 1 parallel, 6 oblique, 0 static.

| Voices | Contrary | Similar | Parallel | Oblique | Static |
| --- | --- | --- | --- | --- | --- |
| 0 and 1 | 0 | 1 | 0 | 1 | 0 |
| 0 and 2 | 3 | 1 | 0 | 1 | 0 |
| 0 and 3 | 2 | 0 | 1 | 2 | 0 |
| 1 and 2 | 1 | 1 | 0 | 0 | 0 |
| 1 and 3 | 2 | 2 | 0 | 1 | 0 |
| 2 and 3 | 2 | 1 | 0 | 1 | 0 |

0 parallel fifths, 0 parallel octaves, 0 direct fifths or octaves in the outer voices, 1 unresolved seventh, 0 unresolved leading tones.

| Location | Fault | Voices |
| --- | --- | --- |
| m. 63, beat 4 to m. 64, beat 3 | seventh-resolves-down | 1 |

18 chords with three or more voices sounding: the root is doubled in 8, the third in 0, the fifth in 2, the seventh in 0, no member doubled in 8. Doubling is counted, not judged. Spacing is judged by the library for the 1 chords with four voices all sounding: 0 too wide.

## Markings

The tempo and dynamics markings the score notates, read with the library's standard-markings rules. A tempo holds until the next one, a dynamic until the next in its part. Group and the loudness comparison are the library's; a text it does not recognise has none. Wedges are not read, and articulations are not imported.

The score has no tempo marking.

The score has no dynamics marking.
