# Analysis Report: field-1.musicxml

## Summary

- **File:** field-1.musicxml, 1191 notes.
- **Input:** MusicXML score, 1 part merged into one texture. Meter: notated. Key signature: inferred. Pitch spelling: notated.
- **Key:** Eb major, the key with the most time across 4 key regions.
- **Chords:** 268 chord events; 261 matched a chord (97%), 7 did not.
- **Most frequent chords:** I (97), dominant7 (42), major7 (26).

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
| m. 1, beat 1 to m. 21, beat 1 | Eb major | 0.72 | profile |
| m. 21, beat 1 to m. 28, beat 1 | Bb major | 0.97 | profile |
| m. 28, beat 1 to m. 33, beat 1 | F minor | 0.77 | profile |
| m. 33, beat 1 to m. 68, beat 1 | Eb major | 0.72 | profile |

## Tonicizations

1 brief excursion of another key that returns to the key before. Span is the stretch in the target key. The library tells a tonicization from a modulation by how long the new key lasts, whether it returns, and whether a cadence arrives in it; without a cadence it is a tonicization however long.

| Span | In | Target |
| --- | --- | --- |
| m. 21, beat 1 to m. 28, beat 1 | Eb major | Bb major |

## Modulations

A modulation is a lasting new key. Position is where the new key arrives. Type is pivot when the library found a pivot chord and direct when it did not. A return goes back to the key before an excursion. The library gives the change's extent from the length of the new key, whether it returns, and whether a cadence arrives in it.

| Position | From | To | Extent | Type | Return |
| --- | --- | --- | --- | --- | --- |
| m. 28, beat 1 | Bb major | F minor | modulation | pivot | no |
| m. 33, beat 1 | F minor | Eb major | modulation | pivot | no |

## Pivot chords

The chord at a modulation that is diatonic in both the old and the new key, named by root and quality, with no Roman numeral. Modulation is its row in the Modulations table, counting from 1.

| Position | Chord | Bass | Inversion | From | To | Modulation |
| --- | --- | --- | --- | --- | --- | --- |
| m. 27, beat 1 to m. 27, beat 2 | C minor7 | A# | third | Bb major | F minor | 1 |
| m. 33, beat 1 to m. 33, beat 2 | D# major | D# | root position | F minor | Eb major | 2 |

## Chords

Positions are measure and beat. Numeral is the Roman numeral in the local key region, which is named where it changes. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Numeral | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 4 to m. 2, beat 1 | I | I6/4 (in Eb major) | Eb | major | A# | second | 1 | 0.93 | - |
| m. 2, beat 1 to m. 2, beat 2 | I | I | D# | major | D# | root position | 1 | 0.93 | - |
| m. 2, beat 2 to m. 2, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.54 | - |
| m. 2, beat 3 to m. 2, beat 4 | I | I6 | D# | major | G | first | 1 | 0.54 | - |
| m. 2, beat 4 to m. 3, beat 1 | minor7 | iii6/5 | G | minor7 | A# | first | 1 | 0.20 | - |
| m. 3, beat 1 to m. 3, beat 2 | V | V | A# | major | A# | root position | 1 | 0.12 | - |
| m. 3, beat 2 to m. 3, beat 3 | half-diminished7 | vii∅4/3 | D | half-diminished7 | G# | second | 1 | 0.08 | IV (G# major, score 0.74) |
| m. 3, beat 3 to m. 3, beat 4 | V | V6/4 | A# | major | F | second | 1 | 0.07 | half-diminished7 (D half-diminished7, score 0.98) |
| m. 3, beat 4 to m. 3, beat 4.33 | dominant7 | V4/2 | A# | dominant7 | G# | third | 0.33 | 0.55 | - |
| m. 3, beat 4.33 to m. 4, beat 1 | V | V | A# | major | A# | root position | 0.67 | 0.97 | - |
| m. 4, beat 1 to m. 4, beat 2 | I | I | D# | major | D# | root position | 1 | 0.76 | - |
| m. 4, beat 2 to m. 4, beat 3 | I | I6 | D# | major | G | first | 1 | 0.76 | - |
| m. 4, beat 3 to m. 4, beat 4 | I | I6/4 | D# | major | A# | second | 1 | 0.08 | iii (G minor, score 0.97) |
| m. 4, beat 4 to m. 5, beat 1 | I | I6 | D# | major | G | first | 1 | 0.08 | iii (G minor, score 0.97) |
| m. 5, beat 1 to m. 5, beat 2 | I | I | D# | major | D# | root position | 1 | 0.46 | - |
| m. 5, beat 2 to m. 5, beat 3 | I | I6 | D# | major | G | first | 1 | 0.09 | iii (G minor, score 0.96) |
| m. 5, beat 3 to m. 5, beat 4 | I | I6/4 | D# | major | A# | second | 1 | 0.08 | iii (G minor, score 0.97) |
| m. 5, beat 4 to m. 6, beat 1 | I | I6 | D# | major | G | first | 1 | 0.14 | - |
| m. 6, beat 1 to m. 6, beat 2 | I | I | D# | major | D# | root position | 1 | 1.00 | - |
| m. 6, beat 2 to m. 6, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.48 | - |
| m. 6, beat 3 to m. 6, beat 4 | I | I6 | D# | major | G | first | 1 | 0.48 | - |
| m. 6, beat 4 to m. 7, beat 1 | minor7 | iii6/5 | G | minor7 | A# | first | 1 | 0.25 | - |
| m. 7, beat 1 to m. 7, beat 2 | V | V | A# | major | A# | root position | 1 | 0.60 | - |
| m. 7, beat 2 to m. 7, beat 3 | half-diminished7 | vii∅4/3 | D | half-diminished7 | G# | second | 1 | 0.08 | IV (G# major, score 0.73) |
| m. 7, beat 3 to m. 7, beat 4 | V | V6/4 | A# | major | F | second | 1 | 0.46 | - |
| m. 7, beat 4 to m. 8, beat 1 | dominant7 | V4/2 | A# | dominant7 | G# | third | 1 | 0.27 | - |
| m. 8, beat 1 to m. 8, beat 2 | major7 | I7 | D# | major7 | D# | root position | 1 | 0.91 | - |
| m. 8, beat 2 to m. 8, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.54 | - |
| m. 8, beat 3 to m. 8, beat 4 | I | I6 | D# | major | G | first | 1 | 0.54 | - |
| m. 8, beat 4 to m. 9, beat 1 | I | I6/4 | D# | major | A# | second | 1 | 0.60 | - |
| m. 9, beat 1 to m. 9, beat 2 | I | I | D# | major | D# | root position | 1 | 0.97 | - |
| m. 9, beat 2 to m. 9, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.21 | - |
| m. 9, beat 3 to m. 9, beat 4 | I | I6 | D# | major | G | first | 1 | 0.21 | - |
| m. 9, beat 4 to m. 10, beat 1 | major7 | I4/3 | D# | major7 | A# | second | 1 | 0.51 | - |
| m. 10, beat 1 to m. 10, beat 2 | vii° | vii°6/4 | D | diminished | G# | second | 1 | 0.26 | - |
| m. 10, beat 2 to m. 10, beat 3 | IV | IV6 | G# | major | C | first | 1 | 0.19 | - |
| m. 10, beat 3 to m. 10, beat 4 | IV | IV | G# | major | G# | root position | 1 | 0.61 | - |
| m. 10, beat 4 to m. 11, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - |
| m. 11, beat 1 to m. 11, beat 2 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.42 | - |
| m. 11, beat 2 to m. 11, beat 3 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.68 | - |
| m. 11, beat 3 to m. 11, beat 4 | vii° | vii°6 | D | diminished | F | first | 1 | 0.11 | - |
| m. 11, beat 4 to m. 12, beat 1 | iii | iii6 | G | minor | A# | first | 1 | 0.38 | - |
| m. 12, beat 1 to m. 12, beat 2 | I | I | D# | major | D# | root position | 1 | 0.09 | minor7 (F minor7, score 0.65) |
| m. 12, beat 2 to m. 12, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.44 | - |
| m. 12, beat 3 to m. 12, beat 4 | I | I6 | D# | major | G | first | 1 | 0.44 | - |
| m. 12, beat 4 to m. 13, beat 1 | I | I6/4 | D# | major | A# | second | 1 | 0.60 | - |
| m. 13, beat 1 to m. 13, beat 2 | I | I | D# | major | D# | root position | 1 | 0.62 | - |
| m. 13, beat 2 to m. 13, beat 3 | minor7 | iii6/5 | G | minor7 | A# | first | 1 | 0.21 | - |
| m. 13, beat 3 to m. 13, beat 4 | I | I6 | D# | major | G | first | 1 | 0.31 | - |
| m. 13, beat 4 to m. 14, beat 1 | major7 | I4/3 | D# | major7 | A# | second | 1 | 0.49 | - |
| m. 14, beat 1 to m. 14, beat 2 | half-diminished7 | vii∅4/3 | D | half-diminished7 | G# | second | 1 | 0.08 | IV (G# major, score 0.72) |
| m. 14, beat 2 to m. 14, beat 3 | IV | IV6 | G# | major | C | first | 1 | 0.19 | - |
| m. 14, beat 3 to m. 14, beat 4 | IV | IV | G# | major | G# | root position | 1 | 0.69 | - |
| m. 14, beat 4 to m. 15, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - |
| m. 15, beat 1 to m. 15, beat 2 | V | V | A# | major | A# | root position | 1 | 0.28 | - |
| m. 15, beat 2 to m. 15, beat 3 | half-diminished7 | vii∅4/3 | D | half-diminished7 | G# | second | 1 | 0.06 | dominant7 (A# dominant7, score 0.72) |
| m. 15, beat 3 to m. 15, beat 4 | half-diminished7 | vii∅6/5 | D | half-diminished7 | F | first | 1 | 0.27 | - |
| m. 15, beat 4 to m. 16, beat 1 | dominant7 | V4/2 | A# | dominant7 | G# | third | 1 | 0.26 | - |
| m. 16, beat 1 to m. 16, beat 2 | major7 | I7 | D# | major7 | D# | root position | 1 | 1.00 | - |
| m. 16, beat 2 to m. 16, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.69 | - |
| m. 16, beat 3 to m. 16, beat 4 | I | I | D# | major | D# | root position | 1 | 0.99 | - |
| m. 16, beat 4 to m. 17, beat 1 | I | I6/4 | D# | major | A# | second | 1 | 0.38 | - |
| m. 17, beat 1 to m. 17, beat 2 | half-diminished7 | vii∅6/5 | D | half-diminished7 | F | first | 1 | 0.27 | - |
| m. 17, beat 2 to m. 17, beat 3 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.50 | - |
| m. 17, beat 3 to m. 17, beat 4 | minor7 | iii4/2 | G | minor7 | F | third | 1 | 0.01 | dominant7 (A# dominant7, score 0.94) |
| m. 17, beat 4 to m. 18, beat 1 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.22 | - |
| m. 18, beat 1 to m. 18, beat 2 | major7 | I7 | D# | major7 | D# | root position | 1 | 0.20 | - |
| m. 18, beat 2 to m. 18, beat 3 | major7 | I4/3 | D# | major7 | A# | second | 1 | 0.43 | - |
| m. 18, beat 3 to m. 18, beat 4 | I | I | D# | major | D# | root position | 1 | 0.67 | - |
| m. 18, beat 4 to m. 19, beat 1 | I | I6/4 | D# | major | A# | second | 1 | 0.15 | - |
| m. 19, beat 1 to m. 19, beat 2 | vii° | vii°6 | D | diminished | F | first | 1 | 0.35 | - |
| m. 19, beat 2 to m. 19, beat 3 | half-diminished7 | vii∅4/3 | D | half-diminished7 | A# | - | 1 | 0.05 | IV (G# major, score 0.64) |
| m. 19, beat 3 to m. 19, beat 4 | half-diminished7 | vii∅6/5 | D | half-diminished7 | F | first | 1 | 0.07 | minor7 (G minor7, score 0.78) |
| m. 19, beat 4 to m. 19, beat 4.33 | iii | iii6 | G | minor | A# | first | 0.33 | 0.23 | - |
| m. 19, beat 4.33 to m. 20, beat 1 | dominant7 | V4/2 | A# | dominant7 | G# | third | 0.67 | 0.05 | vii° (D diminished, score 1.00) |
| m. 20, beat 1 to m. 20, beat 2 | I | I | D# | major | D# | root position | 1 | 0.25 | - |
| m. 20, beat 2 to m. 20, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.69 | - |
| m. 20, beat 3 to m. 20, beat 4 | I | I | D# | major | D# | root position | 1 | 0.45 | - |
| m. 20, beat 4 to m. 20, beat 4.37 | minor7 | ii4/2 | F | minor7 | D# | third | 0.37 | 0.27 | - |
| m. 20, beat 4.37 to m. 21, beat 1 | major7 | I4/2 | D# | major7 | D | third | 0.63 | 0.45 | - |
| m. 21, beat 1 to m. 21, beat 2 | I | I (in Bb major) | A# | major | A# | root position | 1 | 0.44 | - |
| m. 21, beat 2 to m. 21, beat 3 | I | I6/4 | A# | major | F | second | 1 | 0.09 | iii (D minor, score 0.95) |
| m. 21, beat 3 to m. 21, beat 4 | I | I6 | A# | major | D | first | 1 | 0.09 | iii (D minor, score 0.95) |
| m. 21, beat 4 to m. 22, beat 1 | I | I6/4 | A# | major | F | second | 1 | 0.15 | - |
| m. 22, beat 1 to m. 22, beat 2 | dominant7 | V7 | F | dominant7 | F | root position | 1 | 0.45 | - |
| m. 22, beat 2 to m. 22, beat 3 | dominant7 | V7 | F | dominant7 | F | root position | 1 | 0.56 | - |
| m. 22, beat 3 to m. 22, beat 4 | dominant7 | V7 | F | dominant7 | F | root position | 1 | 0.39 | - |
| m. 22, beat 4 to m. 23, beat 1 | minor7 | iii6/5 | D | minor7 | F | first | 1 | 0.43 | - |
| m. 23, beat 1 to m. 23, beat 2 | minor7 | ii4/2 | C | minor7 | A# | third | 1 | 0.10 | I (A# major, score 0.62) |
| m. 23, beat 2 to m. 23, beat 3 | I | I6/4 | A# | major | F | second | 1 | 0.53 | - |
| m. 23, beat 3 to m. 23, beat 4 | I | I6 | A# | major | D | first | 1 | 0.32 | - |
| m. 23, beat 4 to m. 24, beat 1 | major7 | I4/3 | A# | major7 | F | second | 1 | 0.00 | minor7 (G minor7, score 0.88) |
| m. 24, beat 1 to m. 24, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - |
| m. 24, beat 2 to m. 24, beat 3 | I | I6/4 | A# | major | F | second | 1 | 0.09 | iii (D minor, score 0.95) |
| m. 24, beat 3 to m. 24, beat 4 | I | I6 | A# | major | D | first | 1 | 0.10 | - |
| m. 24, beat 4 to m. 25, beat 1 | minor7 | vi4/2 | G | minor7 | F | third | 1 | 0.01 | I (A# major, score 0.97) |
| m. 25, beat 1 to m. 25, beat 2 | minor7 | vi6/5 | G | minor7 | A# | first | 1 | 0.00 | major7 (D# major7, score 1.04) |
| m. 25, beat 2 to m. 25, beat 3 | minor7 | vi4/2 | G | minor7 | F | third | 1 | 0.00 | I (A# major, score 1.05) |
| m. 25, beat 3 to m. 25, beat 4 | I | I6 | A# | major | D | first | 1 | 0.10 | - |
| m. 25, beat 4 to m. 26, beat 1 | I | I6/4 | A# | major | F | second | 1 | 0.17 | - |
| m. 26, beat 1 to m. 26, beat 3 | dominant7 | V7 | F | dominant7 | F | root position | 2 | 0.84 | - |
| m. 26, beat 3 to m. 26, beat 4 | dominant7 | V4/3 | F | dominant7 | C | second | 1 | 0.15 | - |
| m. 26, beat 4 to m. 27, beat 1 | dominant7 | V7 | F | dominant7 | F | root position | 1 | 0.61 | - |
| m. 27, beat 1 to m. 27, beat 2 | minor7 | ii4/2 | C | minor7 | A# | third | 1 | 0.13 | I (A# major, score 0.61) |
| m. 27, beat 2 to m. 27, beat 3 | I | I6/4 | A# | major | F | second | 1 | 0.45 | - |
| m. 27, beat 3 to m. 27, beat 4 | I | I6 | A# | major | D | first | 1 | 0.45 | - |
| m. 27, beat 4 to m. 28, beat 1 | I | I6/4 | A# | major | F | second | 1 | 0.30 | - |
| m. 28, beat 1 to m. 28, beat 2 | major | IV (in F minor) | A# | major | A# | root position | 1 | 0.67 | - |
| m. 28, beat 2 to m. 28, beat 3 | minor | iv | A# | minor | A# | root position | 1 | 0.35 | - |
| m. 28, beat 3 to m. 28, beat 3.33 | minor | i6 | F | minor | G# | first | 0.33 | 0.87 | - |
| m. 28, beat 3.33 to m. 28, beat 4 | minor | i | F | minor | F | root position | 0.67 | 0.87 | - |
| m. 28, beat 4 to m. 29, beat 1 | diminished | viio6 | E | diminished | G | first | 1 | 0.20 | - |
| m. 29, beat 1 to m. 29, beat 2 | minor | i | F | minor | F | root position | 1 | 0.58 | - |
| m. 29, beat 2 to m. 29, beat 3 | minor | i6 | F | minor | G# | first | 1 | 0.23 | - |
| m. 29, beat 3 to m. 29, beat 4 | minor | i | F | minor | F | root position | 1 | 0.68 | - |
| m. 29, beat 4 to m. 30, beat 1 | minor | i6 | F | minor | G# | first | 1 | 0.13 | - |
| m. 30, beat 1 to m. 30, beat 2 | major | V | C | major | C | root position | 1 | 0.46 | - |
| m. 30, beat 2 to m. 30, beat 3 | half-diminished7 | ii/o6/5 | G | half-diminished7 | A# | first | 1 | 0.19 | - |
| m. 30, beat 3 to m. 30, beat 4 | major | V | C | major | C | root position | 1 | 0.35 | - |
| m. 30, beat 4 to m. 31, beat 1 | augmented | III+6 | G# | augmented | A# | - | 1 | 0.04 | dominant7 (C dominant7, score 0.65) |
| m. 31, beat 1 to m. 31, beat 2 | half-diminished7 | ii/o4/2 | G | half-diminished7 | F | third | 1 | 0.21 | - |
| m. 31, beat 2 to m. 31, beat 3 | minor | i6 | F | minor | G# | first | 1 | 0.50 | - |
| m. 31, beat 3 to m. 31, beat 4 | minor | i | F | minor | F | root position | 1 | 1.00 | - |
| m. 31, beat 4 to m. 32, beat 1 | minor | i6 | F | minor | G# | first | 1 | 0.22 | - |
| m. 32, beat 1 to m. 32, beat 2 | major | IV | A# | major | A# | root position | 1 | 0.81 | - |
| m. 32, beat 2 to m. 32, beat 3 | dominant7 | IV4/2 | A# | dominant7 | G# | third | 1 | 0.37 | - |
| m. 32, beat 3 to m. 32, beat 4 | minor | ii6 | G | minor | A# | first | 1 | 0.41 | - |
| m. 32, beat 4 to m. 33, beat 1 | minor7 | ii6/5 | G | minor7 | G# | - | 1 | 0.09 | dominant7 (A# dominant7, score 0.67) |
| m. 33, beat 1 to m. 33, beat 2 | I | I (in Eb major) | D# | major | D# | root position | 1 | 0.33 | - |
| m. 33, beat 2 to m. 33, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.54 | - |
| m. 33, beat 3 to m. 33, beat 4 | minor7 | ii6/5 | F | minor7 | G# | first | 1 | 0.31 | - |
| m. 33, beat 4 to m. 34, beat 1 | vi | vi | C | minor | C | root position | 1 | 0.02 | minor7 (F minor7, score 0.90) |
| m. 34, beat 1 to m. 34, beat 2 | V | V | A# | major | A# | root position | 1 | 0.59 | - |
| m. 34, beat 2 to m. 34, beat 3 | dominant7 | V4/2 | A# | dominant7 | G# | third | 1 | 0.07 | IV (G# major, score 0.60) |
| m. 34, beat 3 to m. 34, beat 4 | V | V | A# | major | A# | root position | 1 | 0.46 | - |
| m. 34, beat 4 to m. 35, beat 1 | minor7 | iii6/5 | G | minor7 | G# | - | 1 | 0.29 | - |
| m. 35, beat 1 to m. 35, beat 2 | I | I | D# | major | D# | root position | 1 | 1.00 | - |
| m. 35, beat 2 to m. 35, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.69 | - |
| m. 35, beat 3 to m. 35, beat 4 | I | I | D# | major | D# | root position | 1 | 1.00 | - |
| m. 35, beat 4 to m. 36, beat 1 | major7 | I4/3 | D# | major7 | A# | second | 1 | 0.04 | minor7 (C minor7, score 0.86) |
| m. 36, beat 1 to m. 36, beat 2 | dominant7 | V4/3 | A# | dominant7 | F | second | 1 | 0.15 | - |
| m. 36, beat 2 to m. 36, beat 3 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.31 | - |
| m. 36, beat 3 to m. 36, beat 4 | vii° | vii°6 | D | diminished | F | first | 1 | 0.10 | - |
| m. 36, beat 4 to m. 37, beat 1 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.25 | - |
| m. 37, beat 1 to m. 37, beat 2 | I | I | D# | major | D# | root position | 1 | 1.00 | - |
| m. 37, beat 2 to m. 37, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.69 | - |
| m. 37, beat 3 to m. 37, beat 4 | I | I | D# | major | D# | root position | 1 | 1.00 | - |
| m. 37, beat 4 to m. 38, beat 1 | major7 | I4/3 | D# | major7 | A# | second | 1 | 0.03 | minor7 (C minor7, score 0.90) |
| m. 38, beat 1 to m. 38, beat 2 | dominant7 | V4/3 | A# | dominant7 | F | second | 1 | 0.07 | minor7 (G minor7, score 0.90) |
| m. 38, beat 2 to m. 38, beat 3 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.19 | - |
| m. 38, beat 3 to m. 38, beat 4 | half-diminished7 | vii∅6/5 | D | half-diminished7 | F | first | 1 | 0.06 | dominant7 (A# dominant7, score 0.78) |
| m. 38, beat 4 to m. 39, beat 1 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.20 | - |
| m. 39, beat 1 to m. 39, beat 2 | major7 | I7 | D# | major7 | D# | root position | 1 | 0.21 | - |
| m. 39, beat 2 to m. 39, beat 3 | major7 | I4/3 | D# | major7 | A# | second | 1 | 0.55 | - |
| m. 39, beat 3 to m. 39, beat 4 | I | I | D# | major | D# | root position | 1 | 0.57 | - |
| m. 39, beat 4 to m. 40, beat 1 | major7 | I4/3 | D# | major7 | A# | second | 1 | 0.37 | - |
| m. 40, beat 1 to m. 40, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - |
| m. 40, beat 2 to m. 40, beat 3 | minor7 | ii4/3 | F | minor7 | A | - | 1 | 0.71 | - |
| m. 40, beat 3 to m. 40, beat 4 | vii° | vii°6/4 | D | diminished | G# | second | 1 | 0.10 | ii (F minor, score 0.60) |
| m. 40, beat 4 to m. 41, beat 1 | V | V | A# | major | A# | root position | 1 | 0.36 | - |
| m. 41, beat 1 to m. 41, beat 2 | minor7 | vi4/3 | C | minor7 | G | second | 1 | 0.16 | - |
| m. 41, beat 2 to m. 41, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - |
| m. 41, beat 3 to m. 41, beat 4 | dominant7 | V4/3 | A# | dominant7 | F | second | 1 | 0.05 | minor7 (F minor7, score 0.72) |
| m. 41, beat 4 to m. 41, beat 4.33 | major7 | IV6/5 | G# | major7 | C | first | 0.33 | 0.07 | vi (C minor, score 0.97) |
| m. 41, beat 4.33 to m. 42, beat 1 | ii | ii6 | F | minor | G# | first | 0.67 | 0.03 | major7 (G# major7, score 0.86) |
| m. 42, beat 1 to m. 42, beat 2 | I | I6/4 | D# | major | A# | second | 1 | 0.28 | - |
| m. 42, beat 2 to m. 42, beat 3 | major7 | I6/5 | D# | major7 | G | first | 1 | 0.01 | minor7 (C minor7, score 0.93) |
| m. 42, beat 3 to m. 42, beat 4 | iii | iii6 | G | minor | A# | first | 1 | 0.13 | - |
| m. 42, beat 4 to m. 43, beat 1 | dominant7 | V4/2 | A# | dominant7 | G# | third | 1 | 0.13 | - |
| m. 43, beat 1 to m. 43, beat 2 | major7 | I7 | D# | major7 | D# | root position | 1 | 0.66 | - |
| m. 43, beat 2 to m. 43, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.20 | - |
| m. 43, beat 3 to m. 43, beat 4 | I | I | D# | major | D# | root position | 1 | 0.90 | - |
| m. 43, beat 4.07 to m. 44, beat 1 | I | I6/4 | Eb | major | A# | second | 0.93 | 0.90 | - |
| m. 44, beat 1 to m. 44, beat 2 | I | I | D# | major | D# | root position | 1 | 0.73 | - |
| m. 44, beat 2 to m. 44, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.60 | - |
| m. 44, beat 3 to m. 44, beat 4 | vi | vi | C | minor | C | root position | 1 | 0.15 | - |
| m. 44, beat 4 to m. 45, beat 1 | vii° | vii°6 | D | diminished | F | first | 1 | 0.01 | minor7 (F minor7, score 0.77) |
| m. 45, beat 1 to m. 45, beat 2 | V | V | A# | major | A# | root position | 1 | 0.53 | - |
| m. 45, beat 2 to m. 45, beat 3 | half-diminished7 | vii∅4/3 | D | half-diminished7 | G# | second | 1 | 0.07 | IV (G# major, score 0.74) |
| m. 45, beat 3 to m. 45, beat 4 | V | V6/4 | A# | major | F | second | 1 | 0.07 | half-diminished7 (D half-diminished7, score 0.97) |
| m. 45, beat 4 to m. 45, beat 4.33 | dominant7 | V4/2 | A# | dominant7 | G# | third | 0.33 | 0.54 | - |
| m. 45, beat 4.33 to m. 46, beat 1 | V | V | A# | major | A# | root position | 0.67 | 0.14 | - |
| m. 46, beat 1 to m. 46, beat 2 | I | I | D# | major | D# | root position | 1 | 0.81 | - |
| m. 46, beat 2 to m. 46, beat 4 | I | I6/4 | D# | major | A# | second | 2 | 0.32 | - |
| m. 46, beat 4 to m. 47, beat 1 | I | I6 | D# | major | G | first | 1 | 0.11 | - |
| m. 47, beat 1 to m. 47, beat 2 | I | I | D# | major | D# | root position | 1 | 0.40 | - |
| m. 47, beat 2 to m. 47, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.09 | iii (G minor, score 0.95) |
| m. 47, beat 3 to m. 47, beat 4 | I | I6 | D# | major | G | first | 1 | 0.08 | iii (G minor, score 0.88) |
| m. 47, beat 4 to m. 48, beat 1 | I | I6/4 | D# | major | A# | second | 1 | 0.14 | - |
| m. 48, beat 1 to m. 48, beat 2 | I | I | D# | major | D# | root position | 1 | 0.93 | - |
| m. 48, beat 2 to m. 48, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.48 | - |
| m. 48, beat 3 to m. 48, beat 4 | major7 | I6/5 | D# | major7 | G | first | 1 | 0.36 | - |
| m. 48, beat 4 to m. 49, beat 1 | minor7 | vi4/2 | C | minor7 | A# | third | 1 | 0.04 | major7 (D# major7, score 0.84) |
| m. 49, beat 1 to m. 49, beat 2 | major7 | I4/3 | D# | major7 | A# | second | 1 | 0.14 | dominant7 (A# dominant7, score 0.60) |
| m. 49, beat 2 to m. 49, beat 3 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.23 | - |
| m. 49, beat 3 to m. 49, beat 4 | dominant7 | V4/3 | A# | dominant7 | F | second | 1 | 0.03 | half-diminished7 (D half-diminished7, score 0.99) |
| m. 49, beat 4 to m. 50, beat 1 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.18 | - |
| m. 50, beat 1 to m. 50, beat 2 | I | I | D# | major | D# | root position | 1 | 0.04 | minor7 (F minor7, score 0.66) |
| m. 50, beat 2 to m. 50, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.46 | - |
| m. 50, beat 3 to m. 50, beat 4 | I | I6 | D# | major | G | first | 1 | 0.46 | - |
| m. 50, beat 4 to m. 51, beat 1 | I | I6/4 | D# | major | A# | second | 1 | 0.22 | - |
| m. 51, beat 1 to m. 51, beat 3 | minor7 | iii7 | G | minor7 | G | root position | 2 | 0.78 | - |
| m. 51, beat 3 to m. 51, beat 4 | vii° | vii° | D | diminished | D | root position | 1 | 0.65 | - |
| m. 51, beat 4 to m. 52, beat 1 | major7 | I6/5 | D# | major7 | G | first | 1 | 0.48 | - |
| m. 52, beat 1 to m. 52, beat 2 | IV | IV | G# | major | G# | root position | 1 | 0.06 | half-diminished7 (D half-diminished7, score 0.66) |
| m. 52, beat 2 to m. 52, beat 3 | IV | IV | G# | major | G# | root position | 1 | 0.29 | - |
| m. 52, beat 3 to m. 52, beat 4 | IV | IV6/4 | Ab | major | D# | second | 1 | 0.29 | - |
| m. 52, beat 4 to m. 53, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - |
| m. 53, beat 1 to m. 53, beat 2 | V | V | A# | major | A# | root position | 1 | 0.31 | - |
| m. 53, beat 2 to m. 53, beat 3 | dominant7 | V4/2 | A# | dominant7 | G# | third | 1 | 0.29 | - |
| m. 53, beat 3 to m. 53, beat 4 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.59 | - |
| m. 53, beat 4 to m. 54, beat 1 | iii | iii6 | G | minor | G# | - | 1 | 0.30 | - |
| m. 54, beat 1 to m. 54, beat 2 | I | I | D# | major | D# | root position | 1 | 0.10 | minor7 (F minor7, score 0.65) |
| m. 54, beat 2 to m. 54, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.53 | - |
| m. 54, beat 3 to m. 54, beat 4 | I | I6 | D# | major | G | first | 1 | 0.53 | - |
| m. 54, beat 4 to m. 55, beat 1 | I | I6/4 | D# | major | A# | second | 1 | 0.22 | - |
| m. 55, beat 1 to m. 55, beat 3 | minor7 | iii7 | G | minor7 | G | root position | 2 | 0.69 | - |
| m. 55, beat 3 to m. 55, beat 4 | vii° | vii° | D | diminished | D | root position | 1 | 0.40 | - |
| m. 55, beat 4 to m. 56, beat 1 | major7 | I6/5 | D# | major7 | G | first | 1 | 0.50 | - |
| m. 56, beat 1 to m. 56, beat 2 | IV | IV | G# | major | G# | root position | 1 | 0.20 | - |
| m. 56, beat 2 to m. 56, beat 3 | IV | IV | G# | major | G# | root position | 1 | 0.66 | - |
| m. 56, beat 3 to m. 56, beat 4 | major7 | IV4/3 | G# | major7 | D# | second | 1 | 0.07 | minor7 (C minor7, score 0.92) |
| m. 56, beat 4 to m. 57, beat 1 | minor7 | ii6/5 | F | minor7 | G# | first | 1 | 0.30 | - |
| m. 57, beat 1 to m. 57, beat 1.33 | V | V | A# | major | A# | root position | 0.33 | 1.00 | - |
| m. 57, beat 1.33 to m. 57, beat 2 | V | V | A# | major | A# | root position | 0.67 | 0.28 | - |
| m. 57, beat 2 to m. 57, beat 3 | IV | IV | G# | major | G# | root position | 1 | 0.01 | half-diminished7 (D half-diminished7, score 0.76) |
| m. 57, beat 3 to m. 57, beat 4 | minor7 | vi4/2 | C | minor7 | A# | third | 1 | 0.17 | - |
| m. 57, beat 4 to m. 58, beat 1 | dominant7 | V4/2 | A# | dominant7 | G# | third | 1 | 0.28 | - |
| m. 58, beat 1 to m. 58, beat 2 | major7 | I7 | D# | major7 | D# | root position | 1 | 0.92 | - |
| m. 58, beat 2 to m. 58, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.43 | - |
| m. 58, beat 3 to m. 58, beat 4 | I | I6 | D# | major | G | first | 1 | 0.43 | - |
| m. 58, beat 4 to m. 59, beat 1 | I | I6/4 | D# | major | A# | second | 1 | 0.22 | - |
| m. 59, beat 1 to m. 59, beat 2 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.34 | - |
| m. 59, beat 2 to m. 59, beat 3 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.69 | - |
| m. 59, beat 3 to m. 59, beat 4 | dominant7 | V4/3 | A# | dominant7 | F | second | 1 | 0.21 | - |
| m. 59, beat 4 to m. 60, beat 1 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.32 | - |
| m. 60, beat 1 to m. 60, beat 2 | I | I6 | D# | major | G | first | 1 | 0.36 | - |
| m. 60, beat 2 to m. 60, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.20 | - |
| m. 60, beat 3 to m. 60, beat 4 | major7 | I6/5 | D# | major7 | G | first | 1 | 0.22 | - |
| m. 60, beat 4 to m. 61, beat 1 | major7 | I4/3 | D# | major7 | A# | second | 1 | 0.15 | - |
| m. 61, beat 1 to m. 61, beat 2 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.43 | - |
| m. 61, beat 2 to m. 61, beat 3 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.66 | - |
| m. 61, beat 3 to m. 61, beat 4 | dominant7 | V4/3 | A# | dominant7 | F | second | 1 | 0.23 | - |
| m. 61, beat 4 to m. 62, beat 1 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.42 | - |
| m. 62, beat 1 to m. 62, beat 2 | major7 | I7 | D# | major7 | D# | root position | 1 | 0.33 | - |
| m. 62, beat 2 to m. 62, beat 3 | major7 | I4/3 | D# | major7 | A# | second | 1 | 0.28 | - |
| m. 62, beat 3 to m. 62, beat 4 | I | I6 | D# | major | G | first | 1 | 0.08 | iii (G minor, score 0.97) |
| m. 62, beat 4 to m. 63, beat 1 | I | I6/4 | D# | major | A# | second | 1 | 0.18 | - |
| m. 63, beat 1 to m. 63, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - |
| m. 63, beat 2 to m. 63, beat 3 | minor7 | ii6/5 | F | minor7 | G# | first | 1 | 0.20 | - |
| m. 63, beat 3 to m. 63, beat 4 | vii° | vii°6/4 | D | diminished | G# | second | 1 | 0.15 | ii (F minor, score 0.54) |
| m. 63, beat 4 to m. 64, beat 1 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.37 | - |
| m. 64, beat 1 to m. 64, beat 2 | I | I6 | D# | major | G | first | 1 | 0.18 | - |
| m. 64, beat 2 to m. 64, beat 3 | iii | vii°6/5/ii | G | minor | G | root position | 1 | 0.78 | - |
| m. 64, beat 3 to m. 64, beat 4 | ii | ii6 | F | minor | G# | first | 1 | 0.12 | major7 (G# major7, score 0.68) |
| m. 64, beat 4 to m. 65, beat 1 | ii | ii6 | F | minor | G# | first | 1 | 0.02 | dominant7 (A# dominant7, score 0.74) |
| m. 65, beat 1 to m. 65, beat 1.33 | V | V | A# | major | A# | root position | 0.33 | 1.00 | - |
| m. 65, beat 1.33 to m. 65, beat 2 | I | I6/4 | D# | major | A# | second | 0.67 | 0.14 | - |
| m. 65, beat 2 to m. 65, beat 3 | I | I6 | D# | major | G | first | 1 | 0.14 | - |
| m. 65, beat 3 to m. 65, beat 4 | dominant7 | V7 | A# | dominant7 | A# | root position | 1 | 0.00 | iii (G minor, score 0.87) |
| m. 65, beat 4 to m. 66, beat 1 | dominant7 | V4/3 | A# | dominant7 | F | second | 1 | 0.18 | - |
| m. 66, beat 1 to m. 66, beat 2 | I | I | D# | major | D# | root position | 1 | 0.97 | - |
| m. 66, beat 2 to m. 66, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.10 | - |
| m. 66, beat 3 to m. 66, beat 4 | I | I | D# | major | D# | root position | 1 | 0.56 | - |
| m. 66, beat 4 to m. 67, beat 1 | I | I6/4 | D# | major | A# | second | 1 | 0.08 | iii (G minor, score 0.97) |
| m. 67, beat 1 to m. 67, beat 2 | I | I | D# | major | D# | root position | 1 | 0.74 | - |
| m. 67, beat 2 to m. 67, beat 3 | I | I6/4 | D# | major | A# | second | 1 | 0.40 | - |
| m. 67, beat 3 to m. 67, beat 4 | I | I | D# | major | D# | root position | 1 | 0.86 | - |

## Applied chords

Chords the library reads as applied (secondary) chords: the chord as it reads in the tonicized key, the chord it tonicizes and that chord's tonic. The last column checks each against the tonicization claims above for the same span; where they disagree both are shown and neither is changed.

| Position | Applied chord | Target | Resolution | Tonicization |
| --- | --- | --- | --- | --- |
| m. 64, beat 2 to m. 64, beat 3 | vii°6/5 (secondary-leading-tone) | ii (F) | to the target | DISAGREES: no tonicization is reported here |
## Outer voices

The highest (soprano) and lowest (bass) sounding note of each chord's span, with its scale degree in that chord's local key. A flat or sharp sign marks a chromatic alteration. A degree is "-" where the span lies in no key region. A rest means the library found no note for that line in the span, a bridged one a short gap inside the line, and a register guard a note the library dropped rather than adopt an accompaniment note. Degrees carry the key region's uncertainty in the analysis document.

| Position | Key | Soprano | Degree | Bass | Degree |
| --- | --- | --- | --- | --- | --- |
| m. 1, beat 4 to m. 2, beat 1 | Eb major | A#4 | 5 | A#4 | 5 |
| m. 2, beat 1 to m. 2, beat 2 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 2, beat 2 to m. 2, beat 3 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 2, beat 3 to m. 2, beat 4 | Eb major | D#5 | 1 | G3 | 3 |
| m. 2, beat 4 to m. 3, beat 1 | Eb major | F5 | 2 | A#3 | 5 |
| m. 3, beat 1 to m. 3, beat 2 | Eb major | D#5 | 1 | A#2 | 5 |
| m. 3, beat 2 to m. 3, beat 3 | Eb major | D5 | 7 | G#3 | 4 |
| m. 3, beat 3 to m. 3, beat 4 | Eb major | D5 | 7 | F3 | 2 |
| m. 3, beat 4 to m. 3, beat 4.33 | Eb major | D5 | 7 | G#3 | 4 |
| m. 3, beat 4.33 to m. 4, beat 1 | Eb major | A#4 | 5 | A#3 | 5 |
| m. 4, beat 1 to m. 4, beat 2 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 4, beat 2 to m. 4, beat 3 | Eb major | D#5 | 1 | G3 | 3 |
| m. 4, beat 3 to m. 4, beat 4 | Eb major | G5 | 3 | A#3 | 5 |
| m. 4, beat 4 to m. 5, beat 1 | Eb major | G5 | 3 | G3 | 3 |
| m. 5, beat 1 to m. 5, beat 2 | Eb major | A#5 | 5 | D#3 | 1 |
| m. 5, beat 2 to m. 5, beat 3 | Eb major | A#5 | 5 | G3 | 3 |
| m. 5, beat 3 to m. 5, beat 4 | Eb major | A#5 | 5 | A#3 | 5 |
| m. 5, beat 4 to m. 6, beat 1 | Eb major | A#5 | 5 | G3 | 3 |
| m. 6, beat 1 to m. 6, beat 2 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 6, beat 2 to m. 6, beat 3 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 6, beat 3 to m. 6, beat 4 | Eb major | D#5 | 1 | G3 | 3 |
| m. 6, beat 4 to m. 7, beat 1 | Eb major | F5 | 2 | A#3 | 5 |
| m. 7, beat 1 to m. 7, beat 2 | Eb major | C6 | 6 | A#2 | 5 |
| m. 7, beat 2 to m. 7, beat 3 | Eb major | C6 | 6 | G#3 | 4 |
| m. 7, beat 3 to m. 7, beat 4 | Eb major | A#5 | 5 | F3 | 2 |
| m. 7, beat 4 to m. 8, beat 1 | Eb major | A#5 | 5 | G#3 | 4 |
| m. 8, beat 1 to m. 8, beat 2 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 8, beat 2 to m. 8, beat 3 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 8, beat 3 to m. 8, beat 4 | Eb major | D#5 | 1 | G3 | 3 |
| m. 8, beat 4 to m. 9, beat 1 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 9, beat 1 to m. 9, beat 2 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 9, beat 2 to m. 9, beat 3 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 9, beat 3 to m. 9, beat 4 | Eb major | D#5 | 1 | G3 | 3 |
| m. 9, beat 4 to m. 10, beat 1 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 10, beat 1 to m. 10, beat 2 | Eb major | D5 | 7 | G#2 | 4 |
| m. 10, beat 2 to m. 10, beat 3 | Eb major | D5 | 7 | rest (register guard) | - |
| m. 10, beat 3 to m. 10, beat 4 | Eb major | C5 | 6 | G#3 | 4 |
| m. 10, beat 4 to m. 11, beat 1 | Eb major | C5 | 6 | B3 | b6 |
| m. 11, beat 1 to m. 11, beat 2 | Eb major | C5 | 6 | A#2 | 5 |
| m. 11, beat 2 to m. 11, beat 3 | Eb major | G#5 | 4 | A#3 | 5 |
| m. 11, beat 3 to m. 11, beat 4 | Eb major | G5 | 3 | F3 | 2 |
| m. 11, beat 4 to m. 12, beat 1 | Eb major | G5 | 3 | A#3 | 5 |
| m. 12, beat 1 to m. 12, beat 2 | Eb major | G5 | 3 | D#3 | 1 |
| m. 12, beat 2 to m. 12, beat 3 | Eb major | F5 | 2 | A#3 | 5 |
| m. 12, beat 3 to m. 12, beat 4 | Eb major | D#5 | 1 | G3 | 3 |
| m. 12, beat 4 to m. 13, beat 1 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 13, beat 1 to m. 13, beat 2 | Eb major | G5 | 3 | D#3 | 1 |
| m. 13, beat 2 to m. 13, beat 3 | Eb major | G5 | 3 | A#3 | 5 |
| m. 13, beat 3 to m. 13, beat 4 | Eb major | F5 | 2 | G3 | 3 |
| m. 13, beat 4 to m. 14, beat 1 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 14, beat 1 to m. 14, beat 2 | Eb major | D5 | 7 | G#2 | 4 |
| m. 14, beat 2 to m. 14, beat 3 | Eb major | D5 | 7 | rest (register guard) | - |
| m. 14, beat 3 to m. 14, beat 4 | Eb major | C5 | 6 | G#3 | 4 |
| m. 14, beat 4 to m. 15, beat 1 | Eb major | B4 | b6 | B3 | b6 |
| m. 15, beat 1 to m. 15, beat 2 | Eb major | C5 | 6 | A#2 | 5 |
| m. 15, beat 2 to m. 15, beat 3 | Eb major | C6 | 6 | G#3 | 4 |
| m. 15, beat 3 to m. 15, beat 4 | Eb major | C6 | 6 | F3 | 2 |
| m. 15, beat 4 to m. 16, beat 1 | Eb major | C6 | 6 | G#3 | 4 |
| m. 16, beat 1 to m. 16, beat 2 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 16, beat 2 to m. 16, beat 3 | Eb major | D#5 | 1 | A#2 | 5 |
| m. 16, beat 3 to m. 16, beat 4 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 16, beat 4 to m. 17, beat 1 | Eb major | F5 | 2 | A#2 | 5 |
| m. 17, beat 1 to m. 17, beat 2 | Eb major | C6 | 6 | F3 | 2 |
| m. 17, beat 2 to m. 17, beat 3 | Eb major | C6 | 6 | A#2 | 5 |
| m. 17, beat 3 to m. 17, beat 4 | Eb major | A#5 | 5 | F3 | 2 |
| m. 17, beat 4 to m. 18, beat 1 | Eb major | G#5 | 4 | A#2 | 5 |
| m. 18, beat 1 to m. 18, beat 2 | Eb major | F5 | 2 | D#3 | 1 |
| m. 18, beat 2 to m. 18, beat 3 | Eb major | G5 | 3 | A#2 | 5 |
| m. 18, beat 3 to m. 18, beat 4 | Eb major | G5 | 3 | D#3 | 1 |
| m. 18, beat 4 to m. 19, beat 1 | Eb major | G5 | 3 | A#2 | 5 |
| m. 19, beat 1 to m. 19, beat 2 | Eb major | D#5 | 1 | F3 | 2 |
| m. 19, beat 2 to m. 19, beat 3 | Eb major | C6 | 6 | A#2 | 5 |
| m. 19, beat 3 to m. 19, beat 4 | Eb major | C6 | 6 | F3 | 2 |
| m. 19, beat 4 to m. 19, beat 4.33 | Eb major | A#5 | 5 | A#2 | 5 |
| m. 19, beat 4.33 to m. 20, beat 1 | Eb major | A#5 | 5 | G#3 | 4 |
| m. 20, beat 1 to m. 20, beat 2 | Eb major | F5 | 2 | D#3 | 1 |
| m. 20, beat 2 to m. 20, beat 3 | Eb major | rest (register guard) | - | A#2 | 5 |
| m. 20, beat 3 to m. 20, beat 4 | Eb major | F5 | 2 | D#2 | 1 |
| m. 20, beat 4 to m. 20, beat 4.37 | Eb major | F5 | 2 | rest (register guard) | - |
| m. 20, beat 4.37 to m. 21, beat 1 | Eb major | D#5 | 1 | D5 | 7 |
| m. 21, beat 1 to m. 21, beat 2 | Bb major | F5 | 5 | A#2 | 1 |
| m. 21, beat 2 to m. 21, beat 3 | Bb major | F5 | 5 | F3 | 5 |
| m. 21, beat 3 to m. 21, beat 4 | Bb major | F5 | 5 | D3 | 3 |
| m. 21, beat 4 to m. 22, beat 1 | Bb major | F5 | 5 | F3 | 5 |
| m. 22, beat 1 to m. 22, beat 2 | Bb major | G5 | 6 | F2 | 5 |
| m. 22, beat 2 to m. 22, beat 3 | Bb major | D#6 | 4 | F3 | 5 |
| m. 22, beat 3 to m. 22, beat 4 | Bb major | D#6 | 4 | F2 | 5 |
| m. 22, beat 4 to m. 23, beat 1 | Bb major | D6 | 3 | F3 | 5 |
| m. 23, beat 1 to m. 23, beat 2 | Bb major | D6 | 3 | A#2 | 1 |
| m. 23, beat 2 to m. 23, beat 3 | Bb major | C6 | 2 | F3 | 5 |
| m. 23, beat 3 to m. 23, beat 4 | Bb major | D6 | 3 | D3 | 3 |
| m. 23, beat 4 to m. 24, beat 1 | Bb major | C6 | 2 | F3 | 5 |
| m. 24, beat 1 to m. 24, beat 2 | Bb major | G5 | 6 | A#2 | 1 |
| m. 24, beat 2 to m. 24, beat 3 | Bb major | F5 | 5 | F3 | 5 |
| m. 24, beat 3 to m. 24, beat 4 | Bb major | F5 | 5 | D3 | 3 |
| m. 24, beat 4 to m. 25, beat 1 | Bb major | G5 | 6 | F3 | 5 |
| m. 25, beat 1 to m. 25, beat 2 | Bb major | G5 | 6 | A#2 | 1 |
| m. 25, beat 2 to m. 25, beat 3 | Bb major | G5 | 6 | F3 | 5 |
| m. 25, beat 3 to m. 25, beat 4 | Bb major | F5 | 5 | D3 | 3 |
| m. 25, beat 4 to m. 26, beat 1 | Bb major | F#5 | b6 | F3 | 5 |
| m. 26, beat 1 to m. 26, beat 3 | Bb major | F#5 | b6 | F2 | 5 |
| m. 26, beat 3 to m. 26, beat 4 | Bb major | G5 | 6 | C3 | 2 |
| m. 26, beat 4 to m. 27, beat 1 | Bb major | C6 | 2 | F3 | 5 |
| m. 27, beat 1 to m. 27, beat 2 | Bb major | C6 | 2 | A#2 | 1 |
| m. 27, beat 2 to m. 27, beat 3 | Bb major | C6 | 2 | F3 | 5 |
| m. 27, beat 3 to m. 27, beat 4 | Bb major | A#5 | 1 | D3 | 3 |
| m. 27, beat 4 to m. 28, beat 1 | Bb major | A#5 | 1 | F3 | 5 |
| m. 28, beat 1 to m. 28, beat 2 | F minor | F5 | 1 | A#2 | 4 |
| m. 28, beat 2 to m. 28, beat 3 | F minor | F5 | 1 | A#2 | 4 |
| m. 28, beat 3 to m. 28, beat 3.33 | F minor | F5 | 1 | G#2 | 3 |
| m. 28, beat 3.33 to m. 28, beat 4 | F minor | F5 | 1 | F3 | 1 |
| m. 28, beat 4 to m. 29, beat 1 | F minor | G5 | 2 | G2 | 2 |
| m. 29, beat 1 to m. 29, beat 2 | F minor | G#5 | 3 | F2 | 1 |
| m. 29, beat 2 to m. 29, beat 3 | F minor | G#5 | 3 | rest (register guard) | - |
| m. 29, beat 3 to m. 29, beat 4 | F minor | G#5 | 3 | F3 | 1 |
| m. 29, beat 4 to m. 30, beat 1 | F minor | G#5 | 3 | G#3 | 3 |
| m. 30, beat 1 to m. 30, beat 2 | F minor | G5 | 2 | C3 | 5 |
| m. 30, beat 2 to m. 30, beat 3 | F minor | G5 | 2 | A#3 | 4 |
| m. 30, beat 3 to m. 30, beat 4 | F minor | G5 | 2 | C3 | 5 |
| m. 30, beat 4 to m. 31, beat 1 | F minor | G#5 | 3 | A#3 | 4 |
| m. 31, beat 1 to m. 31, beat 2 | F minor | G#5 | 3 | F2 | 1 |
| m. 31, beat 2 to m. 31, beat 3 | F minor | G5 | 2 | rest (register guard) | - |
| m. 31, beat 3 to m. 31, beat 4 | F minor | rest (register guard) | - | F3 | 1 |
| m. 31, beat 4 to m. 32, beat 1 | F minor | F4 | 1 | G#3 | 3 |
| m. 32, beat 1 to m. 32, beat 2 | F minor | D4 | #6 | A#2 | 4 |
| m. 32, beat 2 to m. 32, beat 3 | F minor | D4 | #6 | G#3 | 3 |
| m. 32, beat 3 to m. 32, beat 4 | F minor | G5 | 2 | A#2 | 4 |
| m. 32, beat 4 to m. 33, beat 1 | F minor | G5 | 2 | G#3 | 3 |
| m. 33, beat 1 to m. 33, beat 2 | Eb major | F5 | 2 | D#3 | 1 |
| m. 33, beat 2 to m. 33, beat 3 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 33, beat 3 to m. 33, beat 4 | Eb major | D#5 | 1 | G#3 | 4 |
| m. 33, beat 4 to m. 34, beat 1 | Eb major | D#5 | 1 | C4 | 6 |
| m. 34, beat 1 to m. 34, beat 2 | Eb major | D5 | 7 | A#2 | 5 |
| m. 34, beat 2 to m. 34, beat 3 | Eb major | D#5 | 1 | G#3 | 4 |
| m. 34, beat 3 to m. 34, beat 4 | Eb major | F5 | 2 | A#2 | 5 |
| m. 34, beat 4 to m. 35, beat 1 | Eb major | G5 | 3 | G#3 | 4 |
| m. 35, beat 1 to m. 35, beat 2 | Eb major | G5 | 3 | D#3 | 1 |
| m. 35, beat 2 to m. 35, beat 3 | Eb major | D#5 | 1 | A#2 | 5 |
| m. 35, beat 3 to m. 35, beat 4 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 35, beat 4 to m. 36, beat 1 | Eb major | D#5 | 1 | A#2 | 5 |
| m. 36, beat 1 to m. 36, beat 2 | Eb major | G#5 | 4 | F3 | 2 |
| m. 36, beat 2 to m. 36, beat 3 | Eb major | G#5 | 4 | A#2 | 5 |
| m. 36, beat 3 to m. 36, beat 4 | Eb major | G#5 | 4 | F3 | 2 |
| m. 36, beat 4 to m. 37, beat 1 | Eb major | G#5 | 4 | A#2 | 5 |
| m. 37, beat 1 to m. 37, beat 2 | Eb major | D#6 | 1 | D#3 | 1 |
| m. 37, beat 2 to m. 37, beat 3 | Eb major | D#6 | 1 | A#2 | 5 |
| m. 37, beat 3 to m. 37, beat 4 | Eb major | D#6 | 1 | D#3 | 1 |
| m. 37, beat 4 to m. 38, beat 1 | Eb major | D#6 | 1 | A#2 | 5 |
| m. 38, beat 1 to m. 38, beat 2 | Eb major | G#6 | 4 | F3 | 2 |
| m. 38, beat 2 to m. 38, beat 3 | Eb major | F6 | 2 | A#2 | 5 |
| m. 38, beat 3 to m. 38, beat 4 | Eb major | C6 | 6 | F3 | 2 |
| m. 38, beat 4 to m. 39, beat 1 | Eb major | A5 | #4 | A#2 | 5 |
| m. 39, beat 1 to m. 39, beat 2 | Eb major | F5 | 2 | D#3 | 1 |
| m. 39, beat 2 to m. 39, beat 3 | Eb major | G5 | 3 | A#2 | 5 |
| m. 39, beat 3 to m. 39, beat 4 | Eb major | G5 | 3 | D#3 | 1 |
| m. 39, beat 4 to m. 40, beat 1 | Eb major | F5 | 2 | A#2 | 5 |
| m. 40, beat 1 to m. 40, beat 2 | Eb major | G5 | 3 | A2 | #4 |
| m. 40, beat 2 to m. 40, beat 3 | Eb major | F5 | 2 | A3 | #4 |
| m. 40, beat 3 to m. 40, beat 4 | Eb major | G5 | 3 | G#3 | 4 |
| m. 40, beat 4 to m. 41, beat 1 | Eb major | C6 | 6 | A#3 | 5 |
| m. 41, beat 1 to m. 41, beat 2 | Eb major | C6 | 6 | G3 | 3 |
| m. 41, beat 2 to m. 41, beat 3 | Eb major | A#5 | 5 | F#3 | b3 |
| m. 41, beat 3 to m. 41, beat 4 | Eb major | A#5 | 5 | F3 | 2 |
| m. 41, beat 4 to m. 41, beat 4.33 | Eb major | G#5 | 4 | C4 | 6 |
| m. 41, beat 4.33 to m. 42, beat 1 | Eb major | G#5 | 4 | G#3 | 4 |
| m. 42, beat 1 to m. 42, beat 2 | Eb major | D#6 | 1 | A#2 | 5 |
| m. 42, beat 2 to m. 42, beat 3 | Eb major | D#6 | 1 | G3 | 3 |
| m. 42, beat 3 to m. 42, beat 4 | Eb major | C6 | 6 | A#2 | 5 |
| m. 42, beat 4 to m. 43, beat 1 | Eb major | G#5 | 4 | G#3 | 4 |
| m. 43, beat 1 to m. 43, beat 2 | Eb major | G6 | 3 | D#3 | 1 |
| m. 43, beat 2 to m. 43, beat 3 | Eb major | G6 | 3 | A#3 | 5 |
| m. 43, beat 3 to m. 43, beat 4 | Eb major | D#6 | 1 | D#3 | 1 |
| m. 43, beat 4.07 to m. 44, beat 1 | Eb major | rest (register guard) | - | rest (register guard) | - |
| m. 44, beat 1 to m. 44, beat 2 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 44, beat 2 to m. 44, beat 3 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 44, beat 3 to m. 44, beat 4 | Eb major | F5 | 2 | C3 | 6 |
| m. 44, beat 4 to m. 45, beat 1 | Eb major | F5 | 2 | F3 | 2 |
| m. 45, beat 1 to m. 45, beat 2 | Eb major | D5 | 7 | A#2 | 5 |
| m. 45, beat 2 to m. 45, beat 3 | Eb major | C5 | 6 | G#3 | 4 |
| m. 45, beat 3 to m. 45, beat 4 | Eb major | D5 | 7 | F3 | 2 |
| m. 45, beat 4 to m. 45, beat 4.33 | Eb major | D5 | 7 | G#3 | 4 |
| m. 45, beat 4.33 to m. 46, beat 1 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 46, beat 1 to m. 46, beat 2 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 46, beat 2 to m. 46, beat 4 | Eb major | G5 | 3 | A#3 | 5 |
| m. 46, beat 4 to m. 47, beat 1 | Eb major | A#5 | 5 | G3 | 3 |
| m. 47, beat 1 to m. 47, beat 2 | Eb major | A#5 | 5 | D#3 | 1 |
| m. 47, beat 2 to m. 47, beat 3 | Eb major | A#5 | 5 | A#3 | 5 |
| m. 47, beat 3 to m. 47, beat 4 | Eb major | A#5 | 5 | G3 | 3 |
| m. 47, beat 4 to m. 48, beat 1 | Eb major | A5 | #4 | A#3 | 5 |
| m. 48, beat 1 to m. 48, beat 2 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 48, beat 2 to m. 48, beat 3 | Eb major | D#6 | 1 | A#3 | 5 |
| m. 48, beat 3 to m. 48, beat 4 | Eb major | G6 | 3 | G3 | 3 |
| m. 48, beat 4 to m. 49, beat 1 | Eb major | D6 | 7 | A#3 | 5 |
| m. 49, beat 1 to m. 49, beat 2 | Eb major | D#5 | 1 | A#2 | 5 |
| m. 49, beat 2 to m. 49, beat 3 | Eb major | A#5 | 5 | A#3 | 5 |
| m. 49, beat 3 to m. 49, beat 4 | Eb major | D6 | 7 | F3 | 2 |
| m. 49, beat 4 to m. 50, beat 1 | Eb major | A#5 | 5 | A#3 | 5 |
| m. 50, beat 1 to m. 50, beat 2 | Eb major | G5 | 3 | D#3 | 1 |
| m. 50, beat 2 to m. 50, beat 3 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 50, beat 3 to m. 50, beat 4 | Eb major | G4 | 3 | G3 | 3 |
| m. 50, beat 4 to m. 51, beat 1 | Eb major | G4 | 3 | A#3 | 5 |
| m. 51, beat 1 to m. 51, beat 3 | Eb major | F4 | 2 | G2 | 3 |
| m. 51, beat 3 to m. 51, beat 4 | Eb major | D#5 | 1 | D3 | 7 |
| m. 51, beat 4 to m. 52, beat 1 | Eb major | D#5 | 1 | G3 | 3 |
| m. 52, beat 1 to m. 52, beat 2 | Eb major | D5 | 7 | G#2 | 4 |
| m. 52, beat 2 to m. 52, beat 3 | Eb major | C5 | 6 | G#3 | 4 |
| m. 52, beat 3 to m. 52, beat 4 | Eb major | C5 | 6 | D#3 | 1 |
| m. 52, beat 4 to m. 53, beat 1 | Eb major | B4 | b6 | G#3 | 4 |
| m. 53, beat 1 to m. 53, beat 2 | Eb major | C5 | 6 | A#2 | 5 |
| m. 53, beat 2 to m. 53, beat 3 | Eb major | G#5 | 4 | G#3 | 4 |
| m. 53, beat 3 to m. 53, beat 4 | Eb major | G#5 | 4 | A#2 | 5 |
| m. 53, beat 4 to m. 54, beat 1 | Eb major | G5 | 3 | G#3 | 4 |
| m. 54, beat 1 to m. 54, beat 2 | Eb major | G5 | 3 | D#3 | 1 |
| m. 54, beat 2 to m. 54, beat 3 | Eb major | F5 | 2 | A#3 | 5 |
| m. 54, beat 3 to m. 54, beat 4 | Eb major | D#5 | 1 | G3 | 3 |
| m. 54, beat 4 to m. 55, beat 1 | Eb major | G4 | 3 | A#3 | 5 |
| m. 55, beat 1 to m. 55, beat 3 | Eb major | G5 | 3 | G2 | 3 |
| m. 55, beat 3 to m. 55, beat 4 | Eb major | F5 | 2 | D3 | 7 |
| m. 55, beat 4 to m. 56, beat 1 | Eb major | F5 | 2 | G3 | 3 |
| m. 56, beat 1 to m. 56, beat 2 | Eb major | D5 | 7 | G#2 | 4 |
| m. 56, beat 2 to m. 56, beat 3 | Eb major | G#5 | 4 | G#3 | 4 |
| m. 56, beat 3 to m. 56, beat 4 | Eb major | C6 | 6 | D#3 | 1 |
| m. 56, beat 4 to m. 57, beat 1 | Eb major | F5 | 2 | G#3 | 4 |
| m. 57, beat 1 to m. 57, beat 1.33 | Eb major | B4 | b6 | A#2 | 5 |
| m. 57, beat 1.33 to m. 57, beat 2 | Eb major | C5 | 6 | A#3 | 5 |
| m. 57, beat 2 to m. 57, beat 3 | Eb major | C6 | 6 | G#3 | 4 |
| m. 57, beat 3 to m. 57, beat 4 | Eb major | C6 | 6 | A#2 | 5 |
| m. 57, beat 4 to m. 58, beat 1 | Eb major | C6 | 6 | G#3 | 4 |
| m. 58, beat 1 to m. 58, beat 2 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 58, beat 2 to m. 58, beat 3 | Eb major | D#5 | 1 | A#3 | 5 |
| m. 58, beat 3 to m. 58, beat 4 | Eb major | G4 | 3 | G3 | 3 |
| m. 58, beat 4 to m. 59, beat 1 | Eb major | G4 | 3 | A#3 | 5 |
| m. 59, beat 1 to m. 59, beat 2 | Eb major | C6 | 6 | A#2 | 5 |
| m. 59, beat 2 to m. 59, beat 3 | Eb major | B5 | b6 | A#3 | 5 |
| m. 59, beat 3 to m. 59, beat 4 | Eb major | A#5 | 5 | F3 | 2 |
| m. 59, beat 4 to m. 60, beat 1 | Eb major | G#5 | 4 | A#3 | 5 |
| m. 60, beat 1 to m. 60, beat 2 | Eb major | G5 | 3 | G3 | 3 |
| m. 60, beat 2 to m. 60, beat 3 | Eb major | D#6 | 1 | A#3 | 5 |
| m. 60, beat 3 to m. 60, beat 4 | Eb major | G6 | 3 | G3 | 3 |
| m. 60, beat 4 to m. 61, beat 1 | Eb major | F6 | 2 | A#3 | 5 |
| m. 61, beat 1 to m. 61, beat 2 | Eb major | C6 | 6 | A#2 | 5 |
| m. 61, beat 2 to m. 61, beat 3 | Eb major | B5 | b6 | A#3 | 5 |
| m. 61, beat 3 to m. 61, beat 4 | Eb major | A#5 | 5 | F3 | 2 |
| m. 61, beat 4 to m. 62, beat 1 | Eb major | G#5 | 4 | A#3 | 5 |
| m. 62, beat 1 to m. 62, beat 2 | Eb major | F5 | 2 | D#3 | 1 |
| m. 62, beat 2 to m. 62, beat 3 | Eb major | G5 | 3 | A#3 | 5 |
| m. 62, beat 3 to m. 62, beat 4 | Eb major | G5 | 3 | G3 | 3 |
| m. 62, beat 4 to m. 63, beat 1 | Eb major | G4 | 3 | A#3 | 5 |
| m. 63, beat 1 to m. 63, beat 2 | Eb major | F5 | 2 | A3 | #4 |
| m. 63, beat 2 to m. 63, beat 3 | Eb major | G5 | 3 | G#3 | 4 |
| m. 63, beat 3 to m. 63, beat 4 | Eb major | G#5 | 4 | G#3 | 4 |
| m. 63, beat 4 to m. 64, beat 1 | Eb major | A5 | #4 | A#3 | 5 |
| m. 64, beat 1 to m. 64, beat 2 | Eb major | A#5 | 5 | G3 | 3 |
| m. 64, beat 2 to m. 64, beat 3 | Eb major | C#6 | b7 | G3 | 3 |
| m. 64, beat 3 to m. 64, beat 4 | Eb major | C#6 | b7 | G#3 | 4 |
| m. 64, beat 4 to m. 65, beat 1 | Eb major | A#5 | 5 | G#3 | 4 |
| m. 65, beat 1 to m. 65, beat 1.33 | Eb major | A#6 | 5 | A#2 | 5 |
| m. 65, beat 1.33 to m. 65, beat 2 | Eb major | A#6 | 5 | A#3 | 5 |
| m. 65, beat 2 to m. 65, beat 3 | Eb major | A#6 | 5 | G3 | 3 |
| m. 65, beat 3 to m. 65, beat 4 | Eb major | D6 | 7 | A#2 | 5 |
| m. 65, beat 4 to m. 66, beat 1 | Eb major | A#5 | 5 | F3 | 2 |
| m. 66, beat 1 to m. 66, beat 2 | Eb major | D#5 | 1 | D#3 | 1 |
| m. 66, beat 2 to m. 66, beat 3 | Eb major | G6 | 3 | A#3 | 5 |
| m. 66, beat 3 to m. 66, beat 4 | Eb major | G6 | 3 | D#3 | 1 |
| m. 66, beat 4 to m. 67, beat 1 | Eb major | G6 | 3 | A#3 | 5 |
| m. 67, beat 1 to m. 67, beat 2 | Eb major | D#6 | 1 | D#3 | 1 |
| m. 67, beat 2 to m. 67, beat 3 | Eb major | D#6 | 1 | A#3 | 5 |
| m. 67, beat 3 to m. 67, beat 4 | Eb major | D#6 | 1 | D#3 | 1 |

## Chord vocabulary

Each distinct Roman numeral of the chords the library numbered, for the whole piece and for each key region, sorted by count, then total length, then first appearance. Count is the number of chord rows with that numeral (every row in the Chords table is one). Runs counts a stretch of adjacent identical numerals in one key region once. Beats is the total length, Share its fraction of the scope's numbered chord length, and First where it first appears. Applied chords are listed on their own. Chords without a numeral are not counted.

### Overall

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I6/4 | 44 | 44 | 44.59 | 17.4% | m. 1, beat 4 |
| I | 29 | 29 | 29 | 11.3% | m. 2, beat 1 |
| V7 | 24 | 19 | 25 | 9.7% | m. 11, beat 1 |
| I6 | 24 | 24 | 24 | 9.3% | m. 2, beat 3 |
| V | 15 | 14 | 12.67 | 4.9% | m. 3, beat 1 |
| I4/3 | 11 | 11 | 11 | 4.3% | m. 9, beat 4 |
| IV | 9 | 7 | 9 | 3.5% | m. 10, beat 3 |
| V4/2 | 9 | 9 | 7.33 | 2.9% | m. 3, beat 4 |
| V4/3 | 8 | 8 | 8 | 3.1% | m. 26, beat 3 |
| I7 | 7 | 7 | 7 | 2.7% | m. 8, beat 1 |
| vii∅4/3 | 6 | 6 | 6 | 2.3% | m. 3, beat 2 |
| iii6/5 | 5 | 5 | 5 | 1.9% | m. 2, beat 4 |
| I6/5 | 5 | 5 | 5 | 1.9% | m. 42, beat 2 |
| i6 | 5 | 5 | 4.33 | 1.7% | m. 28, beat 3 |
| vii°6 | 4 | 4 | 4 | 1.6% | m. 11, beat 3 |
| vii∅6/5 | 4 | 4 | 4 | 1.6% | m. 15, beat 3 |
| vi4/2 | 4 | 4 | 4 | 1.6% | m. 24, beat 4 |
| ii6/5 | 4 | 4 | 4 | 1.6% | m. 32, beat 4 |
| i | 4 | 4 | 3.67 | 1.4% | m. 28, beat 3.33 |
| ii6 | 4 | 3 | 3.67 | 1.4% | m. 32, beat 3 |
| iii6 | 4 | 4 | 3.33 | 1.3% | m. 11, beat 4 |
| V6/4 | 3 | 3 | 3 | 1.2% | m. 3, beat 3 |
| vii°6/4 | 3 | 3 | 3 | 1.2% | m. 10, beat 1 |
| ii4/2 | 3 | 3 | 2.37 | 0.9% | m. 20, beat 4 |
| iii7 | 2 | 2 | 4 | 1.6% | m. 51, beat 1 |
| IV6 | 2 | 2 | 2 | 0.8% | m. 10, beat 2 |
| vi | 2 | 2 | 2 | 0.8% | m. 33, beat 4 |
| vii° | 2 | 2 | 2 | 0.8% | m. 51, beat 3 |
| iii4/2 | 1 | 1 | 1 | 0.4% | m. 17, beat 3 |
| vi6/5 | 1 | 1 | 1 | 0.4% | m. 25, beat 1 |
| iv | 1 | 1 | 1 | 0.4% | m. 28, beat 2 |
| viio6 | 1 | 1 | 1 | 0.4% | m. 28, beat 4 |
| ii/o6/5 | 1 | 1 | 1 | 0.4% | m. 30, beat 2 |
| III+6 | 1 | 1 | 1 | 0.4% | m. 30, beat 4 |
| ii/o4/2 | 1 | 1 | 1 | 0.4% | m. 31, beat 1 |
| IV4/2 | 1 | 1 | 1 | 0.4% | m. 32, beat 2 |
| ii4/3 | 1 | 1 | 1 | 0.4% | m. 40, beat 2 |
| vi4/3 | 1 | 1 | 1 | 0.4% | m. 41, beat 1 |
| IV6/4 | 1 | 1 | 1 | 0.4% | m. 52, beat 3 |
| IV4/3 | 1 | 1 | 1 | 0.4% | m. 56, beat 3 |
| I4/2 | 1 | 1 | 0.63 | 0.2% | m. 20, beat 4.37 |
| IV6/5 | 1 | 1 | 0.33 | 0.1% | m. 41, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 97 | 97 | 97.59 | 38.0% | m. 1, beat 4 |
| V7 | 41 | 36 | 40.33 | 15.7% | m. 3, beat 4 |
| I7 | 24 | 24 | 23.63 | 9.2% | m. 8, beat 1 |
| V | 18 | 17 | 15.67 | 6.1% | m. 3, beat 1 |
| IV | 12 | 10 | 12 | 4.7% | m. 10, beat 2 |
| vii∅7 | 10 | 10 | 10 | 3.9% | m. 3, beat 2 |
| vii° | 9 | 9 | 9 | 3.5% | m. 10, beat 1 |
| i | 9 | 9 | 8 | 3.1% | m. 28, beat 3 |
| iii7 | 8 | 8 | 10 | 3.9% | m. 2, beat 4 |
| ii7 | 8 | 8 | 7.37 | 2.9% | m. 20, beat 4 |
| vi7 | 6 | 6 | 6 | 2.3% | m. 24, beat 4 |
| ii | 4 | 3 | 3.67 | 1.4% | m. 32, beat 3 |
| iii | 4 | 4 | 3.33 | 1.3% | m. 11, beat 4 |
| IV7 | 3 | 3 | 2.33 | 0.9% | m. 32, beat 2 |
| ii/o7 | 2 | 2 | 2 | 0.8% | m. 30, beat 2 |
| vi | 2 | 2 | 2 | 0.8% | m. 33, beat 4 |
| iv | 1 | 1 | 1 | 0.4% | m. 28, beat 2 |
| viio | 1 | 1 | 1 | 0.4% | m. 28, beat 4 |
| III+ | 1 | 1 | 1 | 0.4% | m. 30, beat 4 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°6/5/ii | 1 | 1 | 1 | 0.4% | m. 64, beat 2 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°7/ii | 1 | 1 | 1 | 0.4% | m. 64, beat 2 |

### Eb major (region 0)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I6/4 | 14 | 14 | 14 | 18.7% | m. 1, beat 4 |
| I | 11 | 11 | 11 | 14.7% | m. 2, beat 1 |
| I6 | 10 | 10 | 10 | 13.3% | m. 2, beat 3 |
| vii∅4/3 | 5 | 5 | 5 | 6.7% | m. 3, beat 2 |
| V7 | 4 | 3 | 4 | 5.3% | m. 11, beat 1 |
| V | 4 | 4 | 3.67 | 4.9% | m. 3, beat 1 |
| V4/2 | 4 | 4 | 3 | 4.0% | m. 3, beat 4 |
| iii6/5 | 3 | 3 | 3 | 4.0% | m. 2, beat 4 |
| I7 | 3 | 3 | 3 | 4.0% | m. 8, beat 1 |
| I4/3 | 3 | 3 | 3 | 4.0% | m. 9, beat 4 |
| vii∅6/5 | 3 | 3 | 3 | 4.0% | m. 15, beat 3 |
| V6/4 | 2 | 2 | 2 | 2.7% | m. 3, beat 3 |
| IV6 | 2 | 2 | 2 | 2.7% | m. 10, beat 2 |
| IV | 2 | 2 | 2 | 2.7% | m. 10, beat 3 |
| vii°6 | 2 | 2 | 2 | 2.7% | m. 11, beat 3 |
| iii6 | 2 | 2 | 1.33 | 1.8% | m. 11, beat 4 |
| vii°6/4 | 1 | 1 | 1 | 1.3% | m. 10, beat 1 |
| iii4/2 | 1 | 1 | 1 | 1.3% | m. 17, beat 3 |
| I4/2 | 1 | 1 | 0.63 | 0.8% | m. 20, beat 4.37 |
| ii4/2 | 1 | 1 | 0.37 | 0.5% | m. 20, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 35 | 35 | 35 | 46.7% | m. 1, beat 4 |
| vii∅7 | 8 | 8 | 8 | 10.7% | m. 3, beat 2 |
| V7 | 8 | 7 | 7 | 9.3% | m. 3, beat 4 |
| I7 | 7 | 7 | 6.63 | 8.8% | m. 8, beat 1 |
| V | 6 | 6 | 5.67 | 7.6% | m. 3, beat 1 |
| iii7 | 4 | 4 | 4 | 5.3% | m. 2, beat 4 |
| IV | 4 | 4 | 4 | 5.3% | m. 10, beat 2 |
| vii° | 3 | 3 | 3 | 4.0% | m. 10, beat 1 |
| iii | 2 | 2 | 1.33 | 1.8% | m. 11, beat 4 |
| ii7 | 1 | 1 | 0.37 | 0.5% | m. 20, beat 4 |

### Bb major (region 1)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I6/4 | 7 | 7 | 7 | 25.9% | m. 21, beat 2 |
| V7 | 5 | 3 | 6 | 22.2% | m. 22, beat 1 |
| I6 | 5 | 5 | 5 | 18.5% | m. 21, beat 3 |
| ii4/2 | 2 | 2 | 2 | 7.4% | m. 23, beat 1 |
| vi4/2 | 2 | 2 | 2 | 7.4% | m. 24, beat 4 |
| I | 1 | 1 | 1 | 3.7% | m. 21, beat 1 |
| iii6/5 | 1 | 1 | 1 | 3.7% | m. 22, beat 4 |
| I4/3 | 1 | 1 | 1 | 3.7% | m. 23, beat 4 |
| vi6/5 | 1 | 1 | 1 | 3.7% | m. 25, beat 1 |
| V4/3 | 1 | 1 | 1 | 3.7% | m. 26, beat 3 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 13 | 13 | 13 | 48.1% | m. 21, beat 1 |
| V7 | 6 | 4 | 7 | 25.9% | m. 22, beat 1 |
| vi7 | 3 | 3 | 3 | 11.1% | m. 24, beat 4 |
| ii7 | 2 | 2 | 2 | 7.4% | m. 23, beat 1 |
| iii7 | 1 | 1 | 1 | 3.7% | m. 22, beat 4 |
| I7 | 1 | 1 | 1 | 3.7% | m. 23, beat 4 |

### F minor (region 2)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i6 | 5 | 5 | 4.33 | 21.7% | m. 28, beat 3 |
| i | 4 | 4 | 3.67 | 18.3% | m. 28, beat 3.33 |
| IV | 2 | 2 | 2 | 10.0% | m. 28, beat 1 |
| V | 2 | 2 | 2 | 10.0% | m. 30, beat 1 |
| iv | 1 | 1 | 1 | 5.0% | m. 28, beat 2 |
| viio6 | 1 | 1 | 1 | 5.0% | m. 28, beat 4 |
| ii/o6/5 | 1 | 1 | 1 | 5.0% | m. 30, beat 2 |
| III+6 | 1 | 1 | 1 | 5.0% | m. 30, beat 4 |
| ii/o4/2 | 1 | 1 | 1 | 5.0% | m. 31, beat 1 |
| IV4/2 | 1 | 1 | 1 | 5.0% | m. 32, beat 2 |
| ii6 | 1 | 1 | 1 | 5.0% | m. 32, beat 3 |
| ii6/5 | 1 | 1 | 1 | 5.0% | m. 32, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 9 | 9 | 8 | 40.0% | m. 28, beat 3 |
| IV | 2 | 2 | 2 | 10.0% | m. 28, beat 1 |
| V | 2 | 2 | 2 | 10.0% | m. 30, beat 1 |
| ii/o7 | 2 | 2 | 2 | 10.0% | m. 30, beat 2 |
| iv | 1 | 1 | 1 | 5.0% | m. 28, beat 2 |
| viio | 1 | 1 | 1 | 5.0% | m. 28, beat 4 |
| III+ | 1 | 1 | 1 | 5.0% | m. 30, beat 4 |
| IV7 | 1 | 1 | 1 | 5.0% | m. 32, beat 2 |
| ii | 1 | 1 | 1 | 5.0% | m. 32, beat 3 |
| ii7 | 1 | 1 | 1 | 5.0% | m. 32, beat 4 |

### Eb major (region 3)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I6/4 | 23 | 23 | 23.59 | 17.5% | m. 33, beat 2 |
| I | 17 | 17 | 17 | 12.6% | m. 33, beat 1 |
| V7 | 15 | 13 | 15 | 11.1% | m. 36, beat 2 |
| I6 | 9 | 9 | 9 | 6.7% | m. 46, beat 4 |
| V | 9 | 8 | 7 | 5.2% | m. 34, beat 1 |
| I4/3 | 7 | 7 | 7 | 5.2% | m. 35, beat 4 |
| V4/3 | 7 | 7 | 7 | 5.2% | m. 36, beat 1 |
| I6/5 | 5 | 5 | 5 | 3.7% | m. 42, beat 2 |
| IV | 5 | 3 | 5 | 3.7% | m. 52, beat 1 |
| V4/2 | 5 | 5 | 4.33 | 3.2% | m. 34, beat 2 |
| I7 | 4 | 4 | 4 | 3.0% | m. 39, beat 1 |
| ii6/5 | 3 | 3 | 3 | 2.2% | m. 33, beat 3 |
| ii6 | 3 | 2 | 2.67 | 2.0% | m. 41, beat 4.33 |
| iii7 | 2 | 2 | 4 | 3.0% | m. 51, beat 1 |
| vi | 2 | 2 | 2 | 1.5% | m. 33, beat 4 |
| vii°6 | 2 | 2 | 2 | 1.5% | m. 36, beat 3 |
| vii°6/4 | 2 | 2 | 2 | 1.5% | m. 40, beat 3 |
| iii6 | 2 | 2 | 2 | 1.5% | m. 42, beat 3 |
| vi4/2 | 2 | 2 | 2 | 1.5% | m. 48, beat 4 |
| vii° | 2 | 2 | 2 | 1.5% | m. 51, beat 3 |
| iii6/5 | 1 | 1 | 1 | 0.7% | m. 34, beat 4 |
| vii∅6/5 | 1 | 1 | 1 | 0.7% | m. 38, beat 3 |
| ii4/3 | 1 | 1 | 1 | 0.7% | m. 40, beat 2 |
| vi4/3 | 1 | 1 | 1 | 0.7% | m. 41, beat 1 |
| vii∅4/3 | 1 | 1 | 1 | 0.7% | m. 45, beat 2 |
| V6/4 | 1 | 1 | 1 | 0.7% | m. 45, beat 3 |
| IV6/4 | 1 | 1 | 1 | 0.7% | m. 52, beat 3 |
| IV4/3 | 1 | 1 | 1 | 0.7% | m. 56, beat 3 |
| IV6/5 | 1 | 1 | 0.33 | 0.2% | m. 41, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 49 | 49 | 49.59 | 36.8% | m. 33, beat 1 |
| V7 | 27 | 25 | 26.33 | 19.5% | m. 34, beat 2 |
| I7 | 16 | 16 | 16 | 11.9% | m. 35, beat 4 |
| V | 10 | 9 | 8 | 5.9% | m. 34, beat 1 |
| vii° | 6 | 6 | 6 | 4.4% | m. 36, beat 3 |
| IV | 6 | 4 | 6 | 4.4% | m. 52, beat 1 |
| ii7 | 4 | 4 | 4 | 3.0% | m. 33, beat 3 |
| iii7 | 3 | 3 | 5 | 3.7% | m. 34, beat 4 |
| vi7 | 3 | 3 | 3 | 2.2% | m. 41, beat 1 |
| ii | 3 | 2 | 2.67 | 2.0% | m. 41, beat 4.33 |
| vi | 2 | 2 | 2 | 1.5% | m. 33, beat 4 |
| vii∅7 | 2 | 2 | 2 | 1.5% | m. 38, beat 3 |
| iii | 2 | 2 | 2 | 1.5% | m. 42, beat 3 |
| IV7 | 2 | 2 | 1.33 | 1.0% | m. 41, beat 4 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°6/5/ii | 1 | 1 | 1 | 0.7% | m. 64, beat 2 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°7/ii | 1 | 1 | 1 | 0.7% | m. 64, beat 2 |

## Non-chord tones

217 notes sound against a chord without being one of its tones; 145 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 2, beat 4 to m. 3, beat 1 | 2 | unclassified | minor7 |
| m. 2, beat 4 to m. 3, beat 1 | 14 | unclassified | minor7 |
| m. 2, beat 4 to m. 3, beat 1 | 15 | unclassified | minor7 |
| m. 3, beat 1 to m. 3, beat 2 | 15 | unclassified | V |
| m. 3, beat 1 to m. 3, beat 2 | 20 | anticipation | V |
| m. 3, beat 2 to m. 3, beat 3 | 23 | escape-tone | half-diminished7 |
| m. 3, beat 3 to m. 3, beat 4 | 20 | unclassified | V |
| m. 6, beat 4 to m. 7, beat 1 | 59 | unclassified | minor7 |
| m. 6, beat 4 to m. 7, beat 1 | 72 | unclassified | minor7 |
| m. 7, beat 1 to m. 7, beat 2 | 77 | anticipation | V |
| m. 7, beat 2 to m. 7, beat 3 | 80 | unclassified | half-diminished7 |
| m. 7, beat 2 to m. 7, beat 3 | 81 | anticipation | half-diminished7 |
| m. 10, beat 1 to m. 10, beat 2 | 119 | unclassified | vii° |
| m. 10, beat 2 to m. 10, beat 3 | 117 | unclassified | IV |
| m. 11, beat 1 to m. 11, beat 2 | 128 | unclassified | dominant7 |
| m. 11, beat 1 to m. 11, beat 2 | 134 | escape-tone | dominant7 |
| m. 11, beat 1 to m. 11, beat 2 | 137 | unclassified | dominant7 |
| m. 11, beat 2 to m. 11, beat 3 | 137 | unclassified | dominant7 |
| m. 11, beat 3 to m. 11, beat 4 | 146 | anticipation | vii° |
| m. 11, beat 4 to m. 12, beat 1 | 148 | unclassified | iii |
| m. 12, beat 1 to m. 12, beat 2 | 151 | unclassified | I |
| m. 12, beat 2 to m. 12, beat 3 | 151 | unclassified | I |
| m. 13, beat 2 to m. 13, beat 3 | 171 | unclassified | minor7 |
| m. 13, beat 3 to m. 13, beat 4 | 173 | unclassified | I |
| m. 14, beat 1 to m. 14, beat 2 | 184 | unclassified | half-diminished7 |
| m. 14, beat 2 to m. 14, beat 3 | 182 | unclassified | IV |
| m. 15, beat 1 to m. 15, beat 2 | 199 | unclassified | V |
| m. 15, beat 1 to m. 15, beat 2 | 202 | unclassified | V |
| m. 15, beat 2 to m. 15, beat 3 | 203 | unclassified | half-diminished7 |
| m. 15, beat 2 to m. 15, beat 3 | 207 | escape-tone | half-diminished7 |
| m. 15, beat 3 to m. 15, beat 4 | 210 | unclassified | half-diminished7 |
| m. 15, beat 4 to m. 16, beat 1 | 205 | unclassified | dominant7 |
| m. 16, beat 4 to m. 17, beat 1 | 227 | unclassified | I |
| m. 17, beat 1 to m. 17, beat 2 | 229 | suspension | half-diminished7 |
| m. 17, beat 2 to m. 17, beat 3 | 235 | unclassified | dominant7 |
| m. 17, beat 3 to m. 17, beat 4 | 243 | anticipation | minor7 |
| m. 17, beat 4 to m. 18, beat 1 | 242 | unclassified | dominant7 |
| m. 18, beat 1 to m. 18, beat 2 | 247 | unclassified | major7 |
| m. 18, beat 1 to m. 18, beat 2 | 252 | neighbor | major7 |
| m. 18, beat 4 to m. 19, beat 1 | 268 | unclassified | I |
| m. 19, beat 1 to m. 19, beat 2 | 270 | suspension | vii° |
| m. 19, beat 2 to m. 19, beat 3 | 275 | unclassified | half-diminished7 |
| m. 19, beat 3 to m. 19, beat 4 | 280 | unclassified | half-diminished7 |
| m. 19, beat 3 to m. 19, beat 4 | 282 | unclassified | half-diminished7 |
| m. 20, beat 1 to m. 20, beat 2 | 288 | unclassified | I |
| m. 20, beat 3 to m. 20, beat 4 | 300 | anticipation | I |
| m. 21, beat 1 to m. 21, beat 2 | 303 | unclassified | I |
| m. 22, beat 1 to m. 22, beat 2 | 321 | escape-tone | dominant7 |
| m. 22, beat 1 to m. 22, beat 2 | 324 | appoggiatura | dominant7 |
| m. 22, beat 3 to m. 22, beat 4 | 333 | anticipation | dominant7 |
| m. 22, beat 4 to m. 23, beat 1 | 335 | unclassified | minor7 |
| m. 23, beat 1 to m. 23, beat 2 | 333 | unclassified | minor7 |
| m. 23, beat 1 to m. 23, beat 2 | 339 | unclassified | minor7 |
| m. 23, beat 2 to m. 23, beat 3 | 337 | unclassified | I |
| m. 23, beat 3 to m. 23, beat 4 | 348 | appoggiatura | I |
| m. 23, beat 4 to m. 24, beat 1 | 348 | suspension | major7 |
| m. 23, beat 4 to m. 24, beat 1 | 354 | unclassified | major7 |
| m. 24, beat 4 to m. 25, beat 1 | 371 | unclassified | minor7 |
| m. 25, beat 1 to m. 25, beat 2 | 371 | unclassified | minor7 |
| m. 25, beat 4 to m. 26, beat 1 | 391 | escape-tone | I |
| m. 26, beat 1 to m. 26, beat 3 | 391 | unclassified | dominant7 |
| m. 26, beat 3 to m. 26, beat 4 | 400 | appoggiatura | dominant7 |
| m. 26, beat 3 to m. 26, beat 4 | 403 | escape-tone | dominant7 |
| m. 27, beat 1 to m. 27, beat 2 | 413 | unclassified | minor7 |
| m. 27, beat 2 to m. 27, beat 3 | 411 | unclassified | I |
| m. 30, beat 1 to m. 30, beat 2 | 460 | anticipation | major |
| m. 30, beat 2 to m. 30, beat 3 | 462 | unclassified | half-diminished7 |
| m. 30, beat 2 to m. 30, beat 3 | 463 | escape-tone | half-diminished7 |
| m. 30, beat 3 to m. 30, beat 4 | 460 | unclassified | major |
| m. 30, beat 4 to m. 31, beat 1 | 464 | unclassified | augmented |
| m. 30, beat 4 to m. 31, beat 1 | 468 | appoggiatura | augmented |
| m. 30, beat 4 to m. 31, beat 1 | 472 | anticipation | augmented |
| m. 31, beat 1 to m. 31, beat 2 | 469 | unclassified | half-diminished7 |
| m. 31, beat 1 to m. 31, beat 2 | 475 | appoggiatura | half-diminished7 |
| m. 31, beat 2 to m. 31, beat 3 | 472 | unclassified | minor |
| m. 32, beat 4 to m. 33, beat 1 | 496 | escape-tone | minor7 |
| m. 33, beat 1 to m. 33, beat 2 | 498 | unclassified | I |
| m. 33, beat 4 to m. 34, beat 1 | 512 | unclassified | vi |
| m. 34, beat 1 to m. 34, beat 2 | 511 | unclassified | V |
| m. 34, beat 2 to m. 34, beat 3 | 519 | unclassified | dominant7 |
| m. 34, beat 3 to m. 34, beat 4 | 519 | unclassified | V |
| m. 34, beat 4 to m. 35, beat 1 | 526 | escape-tone | minor7 |
| m. 35, beat 4 to m. 36, beat 1 | 545 | unclassified | major7 |
| m. 36, beat 1 to m. 36, beat 2 | 545 | unclassified | dominant7 |
| m. 36, beat 4 to m. 37, beat 1 | 559 | passing | dominant7 |
| m. 36, beat 4 to m. 37, beat 1 | 563 | anticipation | dominant7 |
| m. 37, beat 4 to m. 38, beat 1 | 579 | unclassified | major7 |
| m. 38, beat 1 to m. 38, beat 2 | 585 | unclassified | dominant7 |
| m. 38, beat 2 to m. 38, beat 3 | 588 | appoggiatura | dominant7 |
| m. 38, beat 2 to m. 38, beat 3 | 592 | anticipation | dominant7 |
| m. 38, beat 3 to m. 38, beat 4 | 594 | appoggiatura | half-diminished7 |
| m. 38, beat 3 to m. 38, beat 4 | 596 | unclassified | half-diminished7 |
| m. 38, beat 4 to m. 39, beat 1 | 596 | unclassified | dominant7 |
| m. 38, beat 4 to m. 39, beat 1 | 600 | appoggiatura | dominant7 |
| m. 38, beat 4 to m. 39, beat 1 | 604 | anticipation | dominant7 |
| m. 39, beat 1 to m. 39, beat 2 | 602 | unclassified | major7 |
| m. 39, beat 1 to m. 39, beat 2 | 607 | unclassified | major7 |
| m. 39, beat 4 to m. 40, beat 1 | 621 | unclassified | major7 |
| m. 40, beat 2 to m. 40, beat 3 | 632 | unclassified | minor7 |
| m. 40, beat 2 to m. 40, beat 3 | 634 | unclassified | minor7 |
| m. 40, beat 3 to m. 40, beat 4 | 637 | unclassified | vii° |
| m. 40, beat 3 to m. 40, beat 4 | 641 | unclassified | vii° |
| m. 40, beat 4 to m. 41, beat 1 | 644 | unclassified | V |
| m. 40, beat 4 to m. 41, beat 1 | 648 | anticipation | V |
| m. 41, beat 3 to m. 41, beat 4 | 658 | anticipation | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 660 | unclassified | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 663 | unclassified | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 664 | anticipation | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 665 | anticipation | dominant7 |
| m. 41, beat 4.33 to m. 42, beat 1 | 672 | unclassified | ii |
| m. 41, beat 4.33 to m. 42, beat 1 | 674 | unclassified | ii |
| m. 41, beat 4.33 to m. 42, beat 1 | 675 | anticipation | ii |
| m. 42, beat 2 to m. 42, beat 3 | 688 | unclassified | major7 |
| m. 42, beat 3 to m. 42, beat 4 | 691 | escape-tone | iii |
| m. 42, beat 3 to m. 42, beat 4 | 695 | unclassified | iii |
| m. 42, beat 4 to m. 43, beat 1 | 693 | unclassified | dominant7 |
| m. 42, beat 4 to m. 43, beat 1 | 697 | appoggiatura | dominant7 |
| m. 44, beat 3 to m. 44, beat 4 | 724 | unclassified | vi |
| m. 44, beat 3 to m. 44, beat 4 | 725 | anticipation | vi |
| m. 44, beat 4 to m. 45, beat 1 | 716 | suspension | vii° |
| m. 44, beat 4 to m. 45, beat 1 | 727 | unclassified | vii° |
| m. 44, beat 4 to m. 45, beat 1 | 728 | unclassified | vii° |
| m. 44, beat 4 to m. 45, beat 1 | 729 | unclassified | vii° |
| m. 45, beat 1 to m. 45, beat 2 | 734 | anticipation | V |
| m. 45, beat 2 to m. 45, beat 3 | 737 | escape-tone | half-diminished7 |
| m. 45, beat 3 to m. 45, beat 4 | 734 | unclassified | V |
| m. 45, beat 4.33 to m. 46, beat 1 | 746 | anticipation | V |
| m. 47, beat 3 to m. 47, beat 4 | 770 | passing | I |
| m. 47, beat 3 to m. 47, beat 4 | 772 | unclassified | I |
| m. 47, beat 4 to m. 48, beat 1 | 770 | suspension | I |
| m. 47, beat 4 to m. 48, beat 1 | 772 | unclassified | I |
| m. 47, beat 4 to m. 48, beat 1 | 776 | unclassified | I |
| m. 48, beat 3 to m. 48, beat 4 | 790 | unclassified | major7 |
| m. 48, beat 4 to m. 49, beat 1 | 793 | unclassified | minor7 |
| m. 48, beat 4 to m. 49, beat 1 | 797 | unclassified | minor7 |
| m. 48, beat 4 to m. 49, beat 1 | 799 | passing | minor7 |
| m. 48, beat 4 to m. 49, beat 1 | 802 | passing | minor7 |
| m. 49, beat 1 to m. 49, beat 2 | 805 | unclassified | major7 |
| m. 49, beat 1 to m. 49, beat 2 | 807 | neighbor | major7 |
| m. 49, beat 2 to m. 49, beat 3 | 810 | passing | dominant7 |
| m. 49, beat 2 to m. 49, beat 3 | 812 | unclassified | dominant7 |
| m. 49, beat 2 to m. 49, beat 3 | 815 | unclassified | dominant7 |
| m. 49, beat 3 to m. 49, beat 4 | 822 | unclassified | dominant7 |
| m. 49, beat 4 to m. 50, beat 1 | 826 | unclassified | dominant7 |
| m. 49, beat 4 to m. 50, beat 1 | 832 | unclassified | dominant7 |
| m. 49, beat 4 to m. 50, beat 1 | 835 | anticipation | dominant7 |
| m. 50, beat 1 to m. 50, beat 2 | 836 | unclassified | I |
| m. 50, beat 1 to m. 50, beat 2 | 839 | unclassified | I |
| m. 51, beat 1 to m. 51, beat 3 | 854 | unclassified | minor7 |
| m. 51, beat 1 to m. 51, beat 3 | 857 | unclassified | minor7 |
| m. 51, beat 3 to m. 51, beat 4 | 860 | unclassified | vii° |
| m. 51, beat 3 to m. 51, beat 4 | 861 | unclassified | vii° |
| m. 51, beat 4 to m. 52, beat 1 | 863 | unclassified | major7 |
| m. 51, beat 4 to m. 52, beat 1 | 865 | unclassified | major7 |
| m. 52, beat 1 to m. 52, beat 2 | 865 | unclassified | IV |
| m. 52, beat 1 to m. 52, beat 2 | 867 | unclassified | IV |
| m. 53, beat 1 to m. 53, beat 2 | 885 | unclassified | V |
| m. 53, beat 1 to m. 53, beat 2 | 888 | unclassified | V |
| m. 53, beat 4 to m. 54, beat 1 | 898 | escape-tone | iii |
| m. 54, beat 1 to m. 54, beat 2 | 902 | unclassified | I |
| m. 54, beat 2 to m. 54, beat 3 | 902 | unclassified | I |
| m. 55, beat 1 to m. 55, beat 3 | 918 | unclassified | minor7 |
| m. 55, beat 1 to m. 55, beat 3 | 922 | unclassified | minor7 |
| m. 55, beat 3 to m. 55, beat 4 | 926 | unclassified | vii° |
| m. 55, beat 3 to m. 55, beat 4 | 927 | anticipation | vii° |
| m. 55, beat 4 to m. 56, beat 1 | 923 | unclassified | major7 |
| m. 55, beat 4 to m. 56, beat 1 | 929 | unclassified | major7 |
| m. 55, beat 4 to m. 56, beat 1 | 931 | unclassified | major7 |
| m. 56, beat 1 to m. 56, beat 2 | 933 | unclassified | IV |
| m. 56, beat 3 to m. 56, beat 4 | 945 | unclassified | major7 |
| m. 56, beat 4 to m. 57, beat 1 | 952 | appoggiatura | minor7 |
| m. 56, beat 4 to m. 57, beat 1 | 956 | unclassified | minor7 |
| m. 56, beat 4 to m. 57, beat 1 | 957 | appoggiatura | minor7 |
| m. 57, beat 1 to m. 57, beat 1.33 | 957 | unclassified | V |
| m. 57, beat 1.33 to m. 57, beat 2 | 961 | neighbor | V |
| m. 57, beat 1.33 to m. 57, beat 2 | 964 | unclassified | V |
| m. 57, beat 2 to m. 57, beat 3 | 968 | unclassified | IV |
| m. 57, beat 2 to m. 57, beat 3 | 969 | escape-tone | IV |
| m. 57, beat 3 to m. 57, beat 4 | 971 | unclassified | minor7 |
| m. 57, beat 4 to m. 58, beat 1 | 967 | unclassified | dominant7 |
| m. 59, beat 1 to m. 59, beat 2 | 991 | appoggiatura | dominant7 |
| m. 59, beat 1 to m. 59, beat 2 | 995 | unclassified | dominant7 |
| m. 59, beat 2 to m. 59, beat 3 | 997 | passing | dominant7 |
| m. 60, beat 1 to m. 60, beat 2 | 1012 | unclassified | I |
| m. 60, beat 4 to m. 61, beat 1 | 1035 | unclassified | major7 |
| m. 60, beat 4 to m. 61, beat 1 | 1039 | unclassified | major7 |
| m. 60, beat 4 to m. 61, beat 1 | 1043 | unclassified | major7 |
| m. 61, beat 1 to m. 61, beat 2 | 1043 | unclassified | dominant7 |
| m. 61, beat 1 to m. 61, beat 2 | 1045 | unclassified | dominant7 |
| m. 61, beat 1 to m. 61, beat 2 | 1049 | unclassified | dominant7 |
| m. 61, beat 1 to m. 61, beat 2 | 1051 | unclassified | dominant7 |
| m. 61, beat 2 to m. 61, beat 3 | 1051 | unclassified | dominant7 |
| m. 62, beat 1 to m. 62, beat 2 | 1067 | unclassified | major7 |
| m. 62, beat 1 to m. 62, beat 2 | 1072 | unclassified | major7 |
| m. 63, beat 2 to m. 63, beat 3 | 1098 | unclassified | minor7 |
| m. 63, beat 3 to m. 63, beat 4 | 1098 | unclassified | vii° |
| m. 63, beat 3 to m. 63, beat 4 | 1101 | escape-tone | vii° |
| m. 63, beat 3 to m. 63, beat 4 | 1102 | escape-tone | vii° |
| m. 63, beat 4 to m. 64, beat 1 | 1109 | escape-tone | dominant7 |
| m. 64, beat 1 to m. 64, beat 2 | 1109 | unclassified | I |
| m. 64, beat 2 to m. 64, beat 3 | 1117 | unclassified | iii |
| m. 64, beat 2 to m. 64, beat 3 | 1118 | unclassified | iii |
| m. 64, beat 2 to m. 64, beat 3 | 1119 | unclassified | iii |
| m. 64, beat 3 to m. 64, beat 4 | 1119 | unclassified | ii |
| m. 64, beat 3 to m. 64, beat 4 | 1121 | unclassified | ii |
| m. 64, beat 3 to m. 64, beat 4 | 1125 | appoggiatura | ii |
| m. 64, beat 4 to m. 65, beat 1 | 1125 | unclassified | ii |
| m. 64, beat 4 to m. 65, beat 1 | 1127 | unclassified | ii |
| m. 64, beat 4 to m. 65, beat 1 | 1132 | unclassified | ii |
| m. 65, beat 2 to m. 65, beat 3 | 1138 | unclassified | I |
| m. 65, beat 2 to m. 65, beat 3 | 1139 | passing | I |
| m. 65, beat 2 to m. 65, beat 3 | 1142 | passing | I |
| m. 65, beat 3 to m. 65, beat 4 | 1147 | passing | dominant7 |
| m. 65, beat 3 to m. 65, beat 4 | 1150 | passing | dominant7 |
| m. 65, beat 3 to m. 65, beat 4 | 1153 | escape-tone | dominant7 |
| m. 65, beat 4 to m. 66, beat 1 | 1157 | appoggiatura | dominant7 |
| m. 65, beat 4 to m. 66, beat 1 | 1160 | appoggiatura | dominant7 |

## Cadences

The library's standard-harmony analysis found 12 cadences from the chords, the meter and the soprano. Position is where the arrival begins. Key is the key region of the arrival. Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added before a section start.

| Position | Type | Key | Soprano | Beat strength | Boundary |
| --- | --- | --- | --- | --- | --- |
| m. 3, beat 1 | half | Eb major | 1 | strong | no |
| m. 4, beat 1 | perfect-authentic | Eb major | 1 | strong | no |
| m. 7, beat 1 | half | Eb major | 6 | strong | no |
| m. 18, beat 1 | subverted | Eb major | 2 | strong | no |
| m. 30, beat 1 | half | F minor | 2 | strong | no |
| m. 30, beat 3 | half | F minor | 2 | medium | no |
| m. 34, beat 1 | half | Eb major | 7 | strong | no |
| m. 37, beat 1 | perfect-authentic | Eb major | 1 | strong | no |
| m. 46, beat 1 | perfect-authentic | Eb major | 1 | strong | no |
| m. 50, beat 1 | imperfect-authentic | Eb major | 3 | strong | no |
| m. 57, beat 1 | half | Eb major | b6 | strong | no |
| m. 65, beat 3 | half | Eb major | 7 | medium | no |

### Cadence plan

1. m. 3, beat 1: half in Eb major
2. m. 4, beat 1: perfect-authentic in Eb major
3. m. 7, beat 1: half in Eb major
4. m. 18, beat 1: subverted in Eb major
5. m. 30, beat 1: half in F minor
6. m. 30, beat 3: half in F minor
7. m. 34, beat 1: half in Eb major
8. m. 37, beat 1: perfect-authentic in Eb major
9. m. 46, beat 1: perfect-authentic in Eb major
10. m. 50, beat 1: imperfect-authentic in Eb major
11. m. 57, beat 1: half in Eb major
12. m. 65, beat 3: half in Eb major

**Counts:** half 7, perfect-authentic 3, subverted 1, imperfect-authentic 1.
