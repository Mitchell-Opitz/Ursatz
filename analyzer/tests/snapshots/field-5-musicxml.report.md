# Analysis Report: field-5.musicxml

## Summary

- **File:** field-5.musicxml, 1083 notes.
- **Input:** MusicXML score, 1 part merged into one texture. Meter: notated. Key signature: notated. Pitch spelling: notated.
- **Key:** Bb major, the key with the most time across 5 key regions.
- **Chords:** 170 chord events; 147 matched a chord (86%), 23 did not.
- **Most frequent chords:** I (58), minor7 (26), major7 (24).

## Meter

- **Time signature:** 12/8 (notated)
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
| m. 1, beat 1 to m. 5, beat 1 | Bb major | 0.70 | key signature |
| m. 5, beat 1 to m. 8, beat 1 | D minor | 0.77 | profile |
| m. 8, beat 1 to m. 29, beat 1 | Bb major | 0.66 | profile |
| m. 29, beat 1 to m. 33, beat 1 | F major | 0.89 | profile |
| m. 33, beat 1 to m. 44, beat 1 | Bb major | 0.64 | profile |

## Tonicizations

1 brief excursion of another key that returns to the key before. Span is the stretch in the target key. The library tells a tonicization from a modulation by how long the new key lasts and whether it returns; no cadence is looked for.

| Span | In | Target |
| --- | --- | --- |
| m. 5, beat 1 to m. 8, beat 1 | Bb major | D minor |

## Modulations

A modulation is a lasting new key. Position is where the new key arrives. Type is pivot when the library found a pivot chord and direct when it did not. A return goes back to the key before an excursion. The library gives the change's extent from the length of the new key and whether it returns; no cadence is looked for.

| Position | From | To | Extent | Type | Return |
| --- | --- | --- | --- | --- | --- |
| m. 29, beat 1 | Bb major | F major | weak-modulation | pivot | no |
| m. 33, beat 1 | F major | Bb major | weak-modulation | pivot | yes |

## Pivot chords

The chord at a modulation that is diatonic in both the old and the new key, named by root and quality, with no Roman numeral. Modulation is its row in the Modulations table, counting from 1.

| Position | Chord | Bass | Inversion | From | To | Modulation |
| --- | --- | --- | --- | --- | --- | --- |
| m. 28, beat 4 to m. 29, beat 1 | G minor7 | G | root position | Bb major | F major | 1 |
| m. 33, beat 1 to m. 33, beat 2 | A# major | A# | root position | F major | Bb major | 2 |

## Chords

Positions are measure and beat. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 3 | I | A# | major | A# | root position | 2 | 0.28 | - |
| m. 1, beat 3 to m. 1, beat 4 | no match | - | - | - | - | - | - | - |
| m. 1, beat 4 to m. 2, beat 1 | vi | G | minor | A# | first | 1 | 0.83 | - |
| m. 2, beat 1 to m. 2, beat 2 | minor7 | G | minor7 | A# | first | 1 | 0.01 | I (A# major, score 1.04) |
| m. 2, beat 2 to m. 2, beat 3 | I | A# | major | A# | root position | 1 | 0.24 | - |
| m. 2, beat 3 to m. 2, beat 4 | I | A# | major | F | second | 1 | 0.10 | - |
| m. 2, beat 4 to m. 3, beat 1 | I | A# | major | A# | root position | 1 | 0.44 | - |
| m. 3, beat 1 to m. 3, beat 2 | I | A# | major | A# | root position | 1 | 0.53 | - |
| m. 3, beat 2 to m. 3, beat 3 | I | A# | major | A# | root position | 1 | 0.23 | - |
| m. 3, beat 3 to m. 3, beat 4 | vii° | A | diminished | F# | - | 1 | 0.13 | minor7 (C minor7, score 0.62) |
| m. 3, beat 4 to m. 4, beat 1 | vii° | A | diminished | A | root position | 1 | 0.26 | - |
| m. 4, beat 1 to m. 4, beat 2 | I | A# | major | A# | root position | 1 | 0.21 | - |
| m. 4, beat 2 to m. 4, beat 3 | I | A# | major | A# | root position | 1 | 0.42 | - |
| m. 4, beat 3 to m. 4, beat 4 | I | A# | major | F | second | 1 | 0.55 | - |
| m. 4, beat 4 to m. 5, beat 1 | I | A# | major | A# | root position | 1 | 0.44 | - |
| m. 5, beat 1 to m. 5, beat 2 | no match | - | - | - | - | - | - | - |
| m. 5, beat 2 to m. 5, beat 3 | no match | - | - | - | - | - | - | - |
| m. 5, beat 3 to m. 5, beat 4 | no match | - | - | - | - | - | - | - |
| m. 5, beat 4 to m. 6, beat 1 | no match | - | - | - | - | - | - | - |
| m. 6, beat 1 to m. 6, beat 2 | no match | - | - | - | - | - | - | - |
| m. 6, beat 2 to m. 6, beat 3 | no match | - | - | - | - | - | - | - |
| m. 6, beat 3 to m. 6, beat 4 | no match | - | - | - | - | - | - | - |
| m. 6, beat 4 to m. 7, beat 1 | no match | - | - | - | - | - | - | - |
| m. 7, beat 1 to m. 7, beat 2 | no match | - | - | - | - | - | - | - |
| m. 7, beat 2 to m. 7, beat 3 | no match | - | - | - | - | - | - | - |
| m. 7, beat 3 to m. 7, beat 4 | no match | - | - | - | - | - | - | - |
| m. 7, beat 4 to m. 8, beat 1 | no match | - | - | - | - | - | - | - |
| m. 8, beat 1 to m. 8, beat 2 | V | F | major | F | root position | 1 | 0.22 | - |
| m. 8, beat 2 to m. 8, beat 3 | dominant7 | F | dominant7 | D# | third | 1 | 0.36 | - |
| m. 8, beat 3 to m. 8, beat 4 | I | A# | major | D | first | 1 | 0.29 | - |
| m. 8, beat 4 to m. 9, beat 1 | dominant7 | F | dominant7 | C | second | 1 | 0.18 | - |
| m. 9, beat 1 to m. 9, beat 3 | I | A# | major | A# | root position | 2 | 0.47 | - |
| m. 9, beat 3 to m. 9, beat 4 | minor7 | G | minor7 | G | root position | 1 | 0.58 | - |
| m. 9, beat 4 to m. 10, beat 1 | vi | G | minor | A# | first | 1 | 0.55 | - |
| m. 10, beat 1 to m. 10, beat 2 | minor7 | G | minor7 | A# | first | 1 | 0.00 | I (A# major, score 1.05) |
| m. 10, beat 2 to m. 10, beat 3 | I | A# | major | A# | root position | 1 | 0.54 | - |
| m. 10, beat 3 to m. 10, beat 4 | I | A# | major | F | second | 1 | 0.33 | - |
| m. 10, beat 4 to m. 11, beat 1 | I | A# | major | A# | root position | 1 | 0.44 | - |
| m. 11, beat 1 to m. 11, beat 2 | I | A# | major | A# | root position | 1 | 0.53 | - |
| m. 11, beat 2 to m. 11, beat 3 | I | A# | major | A# | root position | 1 | 0.23 | - |
| m. 11, beat 3 to m. 11, beat 4 | vii° | A | diminished | F# | - | 1 | 0.13 | minor7 (C minor7, score 0.62) |
| m. 11, beat 4 to m. 12, beat 1 | vii° | A | diminished | A | root position | 1 | 0.26 | - |
| m. 12, beat 1 to m. 12, beat 2 | I | A# | major | A# | root position | 1 | 0.21 | - |
| m. 12, beat 2 to m. 12, beat 3 | I | A# | major | A# | root position | 1 | 0.42 | - |
| m. 12, beat 3 to m. 12, beat 4 | I | A# | major | F | second | 1 | 0.55 | - |
| m. 12, beat 4 to m. 13, beat 1 | I | A# | major | A# | root position | 1 | 0.44 | - |
| m. 13, beat 1 to m. 13, beat 2 | IV | D# | major | G | first | 1 | 0.61 | - |
| m. 13, beat 2 to m. 13, beat 3 | IV | D# | major | A# | second | 1 | 0.65 | - |
| m. 13, beat 3 to m. 13, beat 4 | I | A# | major | F | second | 1 | 0.10 | - |
| m. 13, beat 4 to m. 14, beat 1 | I | A# | major | A# | root position | 1 | 0.17 | - |
| m. 14, beat 1 to m. 14, beat 2 | I | A# | major | E | - | 1 | 0.23 | - |
| m. 14, beat 2 to m. 14, beat 3 | minor7 | C | minor7 | A# | third | 1 | 0.47 | - |
| m. 14, beat 3 to m. 14, beat 4 | minor7 | C | minor7 | E | - | 1 | 0.57 | - |
| m. 14, beat 4 to m. 15, beat 1 | dominant7 | F | dominant7 | D# | third | 1 | 0.14 | - |
| m. 15, beat 1 to m. 15, beat 2 | I | A# | major | D | first | 1 | 0.07 | minor7 (D minor7, score 0.98) |
| m. 15, beat 2 to m. 15, beat 3 | minor7 | C | minor7 | D# | first | 1 | 0.28 | - |
| m. 15, beat 3 to m. 15, beat 4 | minor7 | G | minor7 | F | third | 1 | 0.00 | I (A# major, score 1.05) |
| m. 15, beat 4 to m. 16, beat 1 | dominant7 | F | dominant7 | F | root position | 1 | 0.27 | - |
| m. 16, beat 1 to m. 16, beat 2 | no match | - | - | - | - | - | - | - |
| m. 16, beat 2 to m. 16, beat 3 | no match | - | - | - | - | - | - | - |
| m. 16, beat 3 to m. 16, beat 4 | no match | - | - | - | - | - | - | - |
| m. 16, beat 4 to m. 17, beat 1 | vii° | A | diminished | D# | second | 1 | 0.16 | - |
| m. 17, beat 1 to m. 17, beat 2 | I | A# | major | D | first | 1 | 0.05 | iii (D minor, score 1.00) |
| m. 17, beat 2 to m. 17, beat 3 | minor7 | C | minor7 | D | - | 1 | 0.18 | - |
| m. 17, beat 3 to m. 17, beat 4 | minor7 | G | minor7 | D# | - | 1 | 0.00 | I (A# major, score 0.99) |
| m. 17, beat 4 to m. 18, beat 1 | minor7 | D | minor7 | F | first | 1 | 0.10 | major7 (A# major7, score 0.84) |
| m. 18, beat 1 to m. 18, beat 1.33 | major7 | A# | major7 | F | second | 0.33 | 0.52 | - |
| m. 18, beat 1.33 to m. 18, beat 2 | I | A# | major | A# | root position | 0.67 | 0.53 | - |
| m. 18, beat 2 to m. 18, beat 3 | minor7 | G | minor7 | A# | first | 1 | 0.07 | I (A# major, score 0.87) |
| m. 18, beat 3 to m. 18, beat 4 | major7 | A# | major7 | A# | root position | 1 | 0.55 | - |
| m. 18, beat 4 to m. 19, beat 1 | I | A# | major | A# | root position | 1 | 0.48 | - |
| m. 19, beat 1 to m. 19, beat 2 | major7 | A# | major7 | A# | root position | 1 | 0.05 | dominant7 (F dominant7, score 0.69) |
| m. 19, beat 2 to m. 19, beat 3 | major7 | A# | major7 | A# | root position | 1 | 0.19 | - |
| m. 19, beat 3 to m. 19, beat 4 | major7 | A# | major7 | A# | root position | 1 | 0.08 | dominant7 (F dominant7, score 0.68) |
| m. 19, beat 4 to m. 20, beat 1 | dominant7 | F | dominant7 | A# | - | 1 | 0.10 | major7 (A# major7, score 0.60) |
| m. 20, beat 1 to m. 20, beat 2 | I | A# | major | A# | root position | 1 | 0.55 | - |
| m. 20, beat 2 to m. 20, beat 3 | minor7 | G | minor7 | A# | first | 1 | 0.07 | I (A# major, score 0.87) |
| m. 20, beat 3 to m. 20, beat 4 | major7 | A# | major7 | A# | root position | 1 | 0.55 | - |
| m. 20, beat 4 to m. 21, beat 1 | I | A# | major | A# | root position | 1 | 0.48 | - |
| m. 21, beat 1 to m. 21, beat 2 | major7 | A# | major7 | A# | root position | 1 | 0.05 | dominant7 (F dominant7, score 0.69) |
| m. 21, beat 2 to m. 21, beat 3 | major7 | A# | major7 | A# | root position | 1 | 0.19 | - |
| m. 21, beat 3 to m. 21, beat 4 | major7 | A# | major7 | A# | root position | 1 | 0.08 | dominant7 (F dominant7, score 0.68) |
| m. 21, beat 4 to m. 22, beat 1 | dominant7 | F | dominant7 | A# | - | 1 | 0.10 | major7 (A# major7, score 0.60) |
| m. 22, beat 1 to m. 22, beat 4 | I | A# | major | A# | root position | 3 | 0.61 | - |
| m. 22, beat 4 to m. 23, beat 1 | I | A# | major | F | second | 1 | 0.27 | - |
| m. 23, beat 1 to m. 23, beat 3 | I | A# | major | A# | root position | 2 | 0.28 | - |
| m. 23, beat 3 to m. 23, beat 4 | no match | - | - | - | - | - | - | - |
| m. 23, beat 4 to m. 24, beat 1 | vi | G | minor | A# | first | 1 | 0.83 | - |
| m. 24, beat 1 to m. 24, beat 2 | minor7 | G | minor7 | A# | first | 1 | 0.01 | I (A# major, score 1.04) |
| m. 24, beat 2 to m. 24, beat 3 | I | A# | major | A# | root position | 1 | 0.24 | - |
| m. 24, beat 3 to m. 24, beat 4 | I | A# | major | F | second | 1 | 0.07 | minor7 (D minor7, score 0.87) |
| m. 24, beat 4 to m. 25, beat 1 | major7 | A# | major7 | A# | root position | 1 | 0.00 | minor7 (G minor7, score 0.83) |
| m. 25, beat 1 to m. 25, beat 2 | I | A# | major | A# | root position | 1 | 0.14 | - |
| m. 25, beat 2 to m. 25, beat 3 | I | A# | major | A# | root position | 1 | 0.23 | - |
| m. 25, beat 3 to m. 25, beat 4 | vii° | A | diminished | F# | - | 1 | 0.13 | minor7 (C minor7, score 0.62) |
| m. 25, beat 4 to m. 26, beat 1 | vii° | A | diminished | A | root position | 1 | 0.26 | - |
| m. 26, beat 1 to m. 26, beat 2 | I | A# | major | A# | root position | 1 | 1.00 | - |
| m. 26, beat 2 to m. 26, beat 3 | major7 | A# | major7 | A# | root position | 1 | 0.50 | - |
| m. 26, beat 3 to m. 26, beat 4 | I | A# | major | F | second | 1 | 0.55 | - |
| m. 26, beat 4 to m. 27, beat 1 | I | A# | major | A# | root position | 1 | 0.44 | - |
| m. 27, beat 1 to m. 27, beat 2 | I | A# | major | A# | root position | 1 | 0.53 | - |
| m. 27, beat 2 to m. 27, beat 3 | I | A# | major | A# | root position | 1 | 0.22 | - |
| m. 27, beat 3 to m. 27, beat 4 | no match | - | - | - | - | - | - | - |
| m. 27, beat 4 to m. 28, beat 1 | half-diminished7 | A | half-diminished7 | A | root position | 1 | 0.99 | - |
| m. 28, beat 1 to m. 28, beat 2 | iii | D | minor | D | root position | 1 | 0.39 | - |
| m. 28, beat 2 to m. 28, beat 3 | iii | D | minor | A | second | 1 | 0.12 | - |
| m. 28, beat 3 to m. 28, beat 4 | minor7 | G | minor7 | A# | first | 1 | 0.04 | major7 (A# major7, score 0.77) |
| m. 28, beat 4 to m. 29, beat 1 | minor7 | G | minor7 | G | root position | 1 | 0.30 | - |
| m. 29, beat 1 to m. 29, beat 2 | iii | A | minor | C | first | 1 | 0.14 | - |
| m. 29, beat 2 to m. 29, beat 3 | I | F | major | A | first | 1 | 0.12 | - |
| m. 29, beat 3 to m. 29, beat 4 | minor7 | D | minor7 | G# | - | 1 | 0.81 | - |
| m. 29, beat 4 to m. 30, beat 1 | dominant7 | C | dominant7 | G | second | 1 | 0.14 | - |
| m. 30, beat 1 to m. 30, beat 2 | I | F | major | F | root position | 1 | 1.00 | - |
| m. 30, beat 2 to m. 30, beat 3 | I | F | major | D# | - | 1 | 0.50 | - |
| m. 30, beat 3 to m. 30, beat 4 | IV | A# | major | D | first | 1 | 0.29 | - |
| m. 30, beat 4 to m. 31, beat 1 | I | F | major | C | second | 1 | 0.25 | - |
| m. 31, beat 1 to m. 31, beat 2 | minor7 | G | minor7 | B | - | 1 | 0.25 | - |
| m. 31, beat 2 to m. 31, beat 3 | minor7 | G | minor7 | G | root position | 1 | 0.22 | - |
| m. 31, beat 3 to m. 31, beat 4 | no match | - | - | - | - | - | - | - |
| m. 31, beat 4 to m. 32, beat 1 | vi | D | minor | F | first | 1 | 0.17 | - |
| m. 32, beat 1 to m. 32, beat 2 | V | C | major | G | second | 1 | 0.49 | - |
| m. 32, beat 2 to m. 32, beat 3 | minor7 | G | minor7 | G | root position | 1 | 0.16 | - |
| m. 32, beat 3 to m. 32, beat 4 | no match | - | - | - | - | - | - | - |
| m. 32, beat 4 to m. 33, beat 1 | I | F | major | A | first | 1 | 0.58 | - |
| m. 33, beat 1 to m. 33, beat 2 | I | A# | major | A# | root position | 1 | 0.53 | - |
| m. 33, beat 2 to m. 33, beat 3 | I | A# | major | F | second | 1 | 0.12 | - |
| m. 33, beat 3 to m. 33, beat 4 | minor7 | C | minor7 | D# | first | 1 | 0.74 | - |
| m. 33, beat 4 to m. 34, beat 1 | minor7 | C | minor7 | E | - | 1 | 0.01 | vi (G minor, score 0.61) |
| m. 34, beat 1 to m. 34, beat 2 | I | A# | major | F | second | 1 | 0.61 | - |
| m. 34, beat 2 to m. 34, beat 3 | I | A# | major | A# | root position | 1 | 0.76 | - |
| m. 34, beat 3 to m. 34, beat 4 | V | F | major | F | root position | 1 | 0.23 | - |
| m. 34, beat 4 to m. 35, beat 1 | V | F | major | A | first | 1 | 1.00 | - |
| m. 35, beat 1 to m. 35, beat 2 | IV | D# | major | G | first | 1 | 0.61 | - |
| m. 35, beat 2 to m. 35, beat 3 | IV | D# | major | F# | - | 1 | 1.00 | - |
| m. 35, beat 3 to m. 35, beat 4 | I | A# | major | F | second | 1 | 0.10 | - |
| m. 35, beat 4 to m. 36, beat 1 | no match | - | - | - | - | - | - | - |
| m. 36, beat 1 to m. 36, beat 2 | no match | - | - | - | - | - | - | - |
| m. 36, beat 2 to m. 36, beat 3 | minor7 | C | minor7 | A# | third | 1 | 0.01 | vi (G minor, score 0.74) |
| m. 36, beat 3 to m. 36, beat 4 | no match | - | - | - | - | - | - | - |
| m. 36, beat 4 to m. 37, beat 1 | dominant7 | F | dominant7 | D# | third | 1 | 0.14 | - |
| m. 37, beat 1 to m. 37, beat 2 | iii | D | minor | D | root position | 1 | 0.36 | - |
| m. 37, beat 2 to m. 37, beat 3 | ii | C | minor | D# | first | 1 | 0.08 | dominant7 (F dominant7, score 0.76) |
| m. 37, beat 3 to m. 37, beat 4 | minor7 | G | minor7 | F | third | 1 | 0.00 | I (A# major, score 1.05) |
| m. 37, beat 4 to m. 38, beat 1 | minor7 | D | minor7 | F | first | 1 | 0.15 | - |
| m. 38, beat 1 to m. 38, beat 2 | I | A# | major | A# | root position | 1 | 0.56 | - |
| m. 38, beat 2 to m. 38, beat 3 | minor7 | G | minor7 | A# | first | 1 | 0.07 | I (A# major, score 0.87) |
| m. 38, beat 3 to m. 38, beat 4 | major7 | A# | major7 | A# | root position | 1 | 0.55 | - |
| m. 38, beat 4 to m. 39, beat 1 | I | A# | major | A# | root position | 1 | 0.48 | - |
| m. 39, beat 1 to m. 39, beat 2 | major7 | A# | major7 | A# | root position | 1 | 0.05 | dominant7 (F dominant7, score 0.69) |
| m. 39, beat 2 to m. 39, beat 3 | major7 | A# | major7 | A# | root position | 1 | 0.19 | - |
| m. 39, beat 3 to m. 39, beat 4 | major7 | A# | major7 | A# | root position | 1 | 0.08 | dominant7 (F dominant7, score 0.68) |
| m. 39, beat 4 to m. 40, beat 1 | dominant7 | F | dominant7 | A# | - | 1 | 0.10 | major7 (A# major7, score 0.60) |
| m. 40, beat 1 to m. 40, beat 2 | I | A# | major | A# | root position | 1 | 0.55 | - |
| m. 40, beat 2 to m. 40, beat 3 | minor7 | G | minor7 | A# | first | 1 | 0.07 | I (A# major, score 0.87) |
| m. 40, beat 3 to m. 40, beat 4 | major7 | A# | major7 | A# | root position | 1 | 0.55 | - |
| m. 40, beat 4 to m. 41, beat 1 | I | A# | major | A# | root position | 1 | 0.48 | - |
| m. 41, beat 1 to m. 41, beat 2 | major7 | A# | major7 | A# | root position | 1 | 0.05 | dominant7 (F dominant7, score 0.69) |
| m. 41, beat 2 to m. 41, beat 3 | major7 | A# | major7 | A# | root position | 1 | 0.19 | - |
| m. 41, beat 3 to m. 41, beat 4 | major7 | A# | major7 | A# | root position | 1 | 0.08 | dominant7 (F dominant7, score 0.68) |
| m. 41, beat 4 to m. 42, beat 1 | dominant7 | F | dominant7 | A# | - | 1 | 0.10 | major7 (A# major7, score 0.60) |
| m. 42, beat 1 to m. 42, beat 1.33 | I | A# | major | A# | root position | 0.33 | 0.58 | - |
| m. 42, beat 1.33 to m. 42, beat 2 | major7 | D# | major7 | D | third | 0.67 | 0.44 | - |
| m. 42, beat 2 to m. 42, beat 3 | dominant7 | F | dominant7 | F | root position | 1 | 0.00 | minor7 (D minor7, score 0.69) |
| m. 42, beat 3 to m. 42, beat 3.33 | major7 | A# | major7 | F | second | 0.33 | 0.20 | - |
| m. 42, beat 3.33 to m. 42, beat 4 | major7 | D# | major7 | D | third | 0.67 | 0.44 | - |
| m. 42, beat 4 to m. 43, beat 1 | dominant7 | F | dominant7 | F | root position | 1 | 0.00 | minor7 (D minor7, score 0.83) |
| m. 43, beat 1 to m. 43, beat 1.33 | major7 | A# | major7 | F | second | 0.33 | 0.21 | - |
| m. 43, beat 1.33 to m. 43, beat 2 | major7 | D# | major7 | D | third | 0.67 | 0.44 | - |
| m. 43, beat 2 to m. 43, beat 3 | dominant7 | F | dominant7 | D# | third | 1 | 0.45 | - |
| m. 43, beat 3 to m. 44, beat 1 | I | A# | major | A# | root position | 2 | 0.67 | - |
## Outer voices

The highest (soprano) and lowest (bass) sounding note of each chord's span, with its scale degree in that chord's local key. A flat or sharp sign marks a chromatic alteration. A degree is "-" where the span lies in no key region. A rest means the library found no note for that line in the span, a bridged one a short gap inside the line, and a register guard a note the library dropped rather than adopt an accompaniment note. Degrees carry the key region's uncertainty in the analysis document.

| Position | Key | Soprano | Degree | Bass | Degree |
| --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 3 | Bb major | F5 | 5 | A#2 | 1 |
| m. 1, beat 3 to m. 1, beat 4 | Bb major | F5 | 5 | G3 | 6 |
| m. 1, beat 4 to m. 2, beat 1 | Bb major | G5 | 6 | A#3 | 1 |
| m. 2, beat 1 to m. 2, beat 2 | Bb major | G5 | 6 | A#2 | 1 |
| m. 2, beat 2 to m. 2, beat 3 | Bb major | F5 | 5 | A#3 | 1 |
| m. 2, beat 3 to m. 2, beat 4 | Bb major | F5 | 5 | F3 | 5 |
| m. 2, beat 4 to m. 3, beat 1 | Bb major | F4 | 5 | A#3 | 1 |
| m. 3, beat 1 to m. 3, beat 2 | Bb major | D5 | 3 | A#2 | 1 |
| m. 3, beat 2 to m. 3, beat 3 | Bb major | D5 | 3 | A#3 | 1 |
| m. 3, beat 3 to m. 3, beat 4 | Bb major | D#5 | 4 | F#3 | b6 |
| m. 3, beat 4 to m. 4, beat 1 | Bb major | D#5 | 4 | A3 | 7 |
| m. 4, beat 1 to m. 4, beat 2 | Bb major | C5 | 2 | A#2 | 1 |
| m. 4, beat 2 to m. 4, beat 3 | Bb major | D5 | 3 | A#3 | 1 |
| m. 4, beat 3 to m. 4, beat 4 | Bb major | C5 | 2 | F3 | 5 |
| m. 4, beat 4 to m. 5, beat 1 | Bb major | F4 | 5 | A#3 | 1 |
| m. 5, beat 1 to m. 5, beat 2 | D minor | D5 | 1 | A#2 | 6 |
| m. 5, beat 2 to m. 5, beat 3 | D minor | D5 | 1 | A#3 | 6 |
| m. 5, beat 3 to m. 5, beat 4 | D minor | E5 | 2 | A2 | 5 |
| m. 5, beat 4 to m. 6, beat 1 | D minor | A4 | 5 | A3 | 5 |
| m. 6, beat 1 to m. 6, beat 2 | D minor | F5 | 3 | D3 | 1 |
| m. 6, beat 2 to m. 6, beat 3 | D minor | F5 | 3 | A3 | 5 |
| m. 6, beat 3 to m. 6, beat 4 | D minor | E5 | 2 | B2 | #6 |
| m. 6, beat 4 to m. 7, beat 1 | D minor | D5 | 1 | G#3 | #4 |
| m. 7, beat 1 to m. 7, beat 2 | D minor | C5 | 7 | C3 | 7 |
| m. 7, beat 2 to m. 7, beat 3 | D minor | C5 | 7 | G3 | 4 |
| m. 7, beat 3 to m. 7, beat 4 | D minor | D5 | 1 | C3 | 7 |
| m. 7, beat 4 to m. 8, beat 1 | D minor | E5 | 2 | G3 | 4 |
| m. 8, beat 1 to m. 8, beat 2 | Bb major | F5 | 5 | F3 | 5 |
| m. 8, beat 2 to m. 8, beat 3 | Bb major | F6 | 5 | D#3 | 4 |
| m. 8, beat 3 to m. 8, beat 4 | Bb major | A#5 | 1 | D3 | 3 |
| m. 8, beat 4 to m. 9, beat 1 | Bb major | D#5 | 4 | C3 | 2 |
| m. 9, beat 1 to m. 9, beat 3 | Bb major | F5 | 5 | A#2 | 1 |
| m. 9, beat 3 to m. 9, beat 4 | Bb major | F5 | 5 | G3 | 6 |
| m. 9, beat 4 to m. 10, beat 1 | Bb major | G5 | 6 | A#3 | 1 |
| m. 10, beat 1 to m. 10, beat 2 | Bb major | G5 | 6 | A#2 | 1 |
| m. 10, beat 2 to m. 10, beat 3 | Bb major | F5 | 5 | A#3 | 1 |
| m. 10, beat 3 to m. 10, beat 4 | Bb major | F5 | 5 | F3 | 5 |
| m. 10, beat 4 to m. 11, beat 1 | Bb major | F4 | 5 | A#3 | 1 |
| m. 11, beat 1 to m. 11, beat 2 | Bb major | D5 | 3 | A#2 | 1 |
| m. 11, beat 2 to m. 11, beat 3 | Bb major | D5 | 3 | A#3 | 1 |
| m. 11, beat 3 to m. 11, beat 4 | Bb major | D#5 | 4 | F#3 | b6 |
| m. 11, beat 4 to m. 12, beat 1 | Bb major | D#5 | 4 | A3 | 7 |
| m. 12, beat 1 to m. 12, beat 2 | Bb major | C5 | 2 | A#2 | 1 |
| m. 12, beat 2 to m. 12, beat 3 | Bb major | D5 | 3 | A#3 | 1 |
| m. 12, beat 3 to m. 12, beat 4 | Bb major | C5 | 2 | F3 | 5 |
| m. 12, beat 4 to m. 13, beat 1 | Bb major | F4 | 5 | A#3 | 1 |
| m. 13, beat 1 to m. 13, beat 2 | Bb major | D#5 | 4 | G3 | 6 |
| m. 13, beat 2 to m. 13, beat 3 | Bb major | D#5 | 4 | A#3 | 1 |
| m. 13, beat 3 to m. 13, beat 4 | Bb major | D#5 | 4 | F3 | 5 |
| m. 13, beat 4 to m. 14, beat 1 | Bb major | D#5 | 4 | A#3 | 1 |
| m. 14, beat 1 to m. 14, beat 2 | Bb major | D#5 | 4 | E3 | #4 |
| m. 14, beat 2 to m. 14, beat 3 | Bb major | D5 | 3 | A#3 | 1 |
| m. 14, beat 3 to m. 14, beat 4 | Bb major | C5 | 2 | E3 | #4 |
| m. 14, beat 4 to m. 15, beat 1 | Bb major | D5 | 3 | D#3 | 4 |
| m. 15, beat 1 to m. 15, beat 2 | Bb major | F5 | 5 | D3 | 3 |
| m. 15, beat 2 to m. 15, beat 3 | Bb major | F5 | 5 | D#3 | 4 |
| m. 15, beat 3 to m. 15, beat 4 | Bb major | C5 | 2 | F3 | 5 |
| m. 15, beat 4 to m. 16, beat 1 | Bb major | C5 | 2 | F3 | 5 |
| m. 16, beat 1 to m. 16, beat 2 | Bb major | C#5 | b3 | E3 | #4 |
| m. 16, beat 2 to m. 16, beat 3 | Bb major | C#5 | b3 | A#3 | 1 |
| m. 16, beat 3 to m. 16, beat 4 | Bb major | C#5 | b3 | E3 | #4 |
| m. 16, beat 4 to m. 17, beat 1 | Bb major | C#5 | b3 | D#3 | 4 |
| m. 17, beat 1 to m. 17, beat 2 | Bb major | F5 | 5 | D3 | 3 |
| m. 17, beat 2 to m. 17, beat 3 | Bb major | F5 | 5 | D3 | 3 |
| m. 17, beat 3 to m. 17, beat 4 | Bb major | C5 | 2 | D#3 | 4 |
| m. 17, beat 4 to m. 18, beat 1 | Bb major | D5 | 3 | F3 | 5 |
| m. 18, beat 1 to m. 18, beat 1.33 | Bb major | C5 | 2 | F3 | 5 |
| m. 18, beat 1.33 to m. 18, beat 2 | Bb major | F4 | 5 | A#2 | 1 |
| m. 18, beat 2 to m. 18, beat 3 | Bb major | G#4 | b7 | A#2 | 1 |
| m. 18, beat 3 to m. 18, beat 4 | Bb major | B4 | b2 | A#2 | 1 |
| m. 18, beat 4 to m. 19, beat 1 | Bb major | D5 | 3 | A#2 | 1 |
| m. 19, beat 1 to m. 19, beat 2 | Bb major | D#5 | 4 | A#2 | 1 |
| m. 19, beat 2 to m. 19, beat 3 | Bb major | D5 | 3 | A#2 | 1 |
| m. 19, beat 3 to m. 19, beat 4 | Bb major | D#5 | 4 | A#2 | 1 |
| m. 19, beat 4 to m. 20, beat 1 | Bb major | F5 | 5 | A#2 | 1 |
| m. 20, beat 1 to m. 20, beat 2 | Bb major | A#4 | 1 | A#2 | 1 |
| m. 20, beat 2 to m. 20, beat 3 | Bb major | G#4 | b7 | A#2 | 1 |
| m. 20, beat 3 to m. 20, beat 4 | Bb major | B4 | b2 | A#2 | 1 |
| m. 20, beat 4 to m. 21, beat 1 | Bb major | D5 | 3 | A#2 | 1 |
| m. 21, beat 1 to m. 21, beat 2 | Bb major | D#5 | 4 | A#2 | 1 |
| m. 21, beat 2 to m. 21, beat 3 | Bb major | D5 | 3 | A#2 | 1 |
| m. 21, beat 3 to m. 21, beat 4 | Bb major | D#5 | 4 | A#2 | 1 |
| m. 21, beat 4 to m. 22, beat 1 | Bb major | F5 | 5 | A#2 | 1 |
| m. 22, beat 1 to m. 22, beat 4 | Bb major | D5 | 3 | A#2 | 1 |
| m. 22, beat 4 to m. 23, beat 1 | Bb major | D5 | 3 | rest (register guard) | - |
| m. 23, beat 1 to m. 23, beat 3 | Bb major | F5 | 5 | A#2 | 1 |
| m. 23, beat 3 to m. 23, beat 4 | Bb major | F5 | 5 | G3 | 6 |
| m. 23, beat 4 to m. 24, beat 1 | Bb major | G5 | 6 | A#3 | 1 |
| m. 24, beat 1 to m. 24, beat 2 | Bb major | G5 | 6 | A#2 | 1 |
| m. 24, beat 2 to m. 24, beat 3 | Bb major | F5 | 5 | A#3 | 1 |
| m. 24, beat 3 to m. 24, beat 4 | Bb major | C6 | 2 | F3 | 5 |
| m. 24, beat 4 to m. 25, beat 1 | Bb major | A#5 | 1 | A#3 | 1 |
| m. 25, beat 1 to m. 25, beat 2 | Bb major | D#5 | 4 | A#2 | 1 |
| m. 25, beat 2 to m. 25, beat 3 | Bb major | D5 | 3 | A#3 | 1 |
| m. 25, beat 3 to m. 25, beat 4 | Bb major | D#5 | 4 | F#3 | b6 |
| m. 25, beat 4 to m. 26, beat 1 | Bb major | D#5 | 4 | A3 | 7 |
| m. 26, beat 1 to m. 26, beat 2 | Bb major | A#4 | 1 | A#2 | 1 |
| m. 26, beat 2 to m. 26, beat 3 | Bb major | D5 | 3 | A#3 | 1 |
| m. 26, beat 3 to m. 26, beat 4 | Bb major | C5 | 2 | F3 | 5 |
| m. 26, beat 4 to m. 27, beat 1 | Bb major | F4 | 5 | A#3 | 1 |
| m. 27, beat 1 to m. 27, beat 2 | Bb major | D5 | 3 | A#2 | 1 |
| m. 27, beat 2 to m. 27, beat 3 | Bb major | D5 | 3 | A#3 | 1 |
| m. 27, beat 3 to m. 27, beat 4 | Bb major | E5 | #4 | A2 | 7 |
| m. 27, beat 4 to m. 28, beat 1 | Bb major | E5 | #4 | A3 | 7 |
| m. 28, beat 1 to m. 28, beat 2 | Bb major | F5 | 5 | D3 | 3 |
| m. 28, beat 2 to m. 28, beat 3 | Bb major | F5 | 5 | A3 | 7 |
| m. 28, beat 3 to m. 28, beat 4 | Bb major | A#5 | 1 | A#2 | 1 |
| m. 28, beat 4 to m. 29, beat 1 | Bb major | A5 | 7 | G3 | 6 |
| m. 29, beat 1 to m. 29, beat 2 | F major | D5 | 6 | C3 | 5 |
| m. 29, beat 2 to m. 29, beat 3 | F major | C5 | 5 | A3 | 3 |
| m. 29, beat 3 to m. 29, beat 4 | F major | D5 | 6 | G#3 | b3 |
| m. 29, beat 4 to m. 30, beat 1 | F major | E5 | 7 | G3 | 2 |
| m. 30, beat 1 to m. 30, beat 2 | F major | F5 | 1 | F3 | 1 |
| m. 30, beat 2 to m. 30, beat 3 | F major | F5 | 1 | D#3 | b7 |
| m. 30, beat 3 to m. 30, beat 4 | F major | F5 | 1 | D3 | 6 |
| m. 30, beat 4 to m. 31, beat 1 | F major | F5 | 1 | C3 | 5 |
| m. 31, beat 1 to m. 31, beat 2 | F major | F5 | 1 | B2 | #4 |
| m. 31, beat 2 to m. 31, beat 3 | F major | G5 | 2 | G3 | 2 |
| m. 31, beat 3 to m. 31, beat 4 | F major | F5 | 1 | C3 | 5 |
| m. 31, beat 4 to m. 32, beat 1 | F major | D#5 | b7 | F3 | 1 |
| m. 32, beat 1 to m. 32, beat 2 | F major | D5 | 6 | G3 | 2 |
| m. 32, beat 2 to m. 32, beat 3 | F major | D5 | 6 | G3 | 2 |
| m. 32, beat 3 to m. 32, beat 4 | F major | D#5 | b7 | C4 | 5 |
| m. 32, beat 4 to m. 33, beat 1 | F major | F5 | 1 | A3 | 3 |
| m. 33, beat 1 to m. 33, beat 2 | Bb major | D5 | 3 | A#2 | 1 |
| m. 33, beat 2 to m. 33, beat 3 | Bb major | D5 | 3 | F3 | 5 |
| m. 33, beat 3 to m. 33, beat 4 | Bb major | C5 | 2 | D#3 | 4 |
| m. 33, beat 4 to m. 34, beat 1 | Bb major | G4 | 6 | E3 | #4 |
| m. 34, beat 1 to m. 34, beat 2 | Bb major | A#4 | 1 | F3 | 5 |
| m. 34, beat 2 to m. 34, beat 3 | Bb major | A#4 | 1 | A#3 | 1 |
| m. 34, beat 3 to m. 34, beat 4 | Bb major | A#4 | 1 | F3 | 5 |
| m. 34, beat 4 to m. 35, beat 1 | Bb major | F5 | 5 | A3 | 7 |
| m. 35, beat 1 to m. 35, beat 2 | Bb major | E5 | #4 | G3 | 6 |
| m. 35, beat 2 to m. 35, beat 3 | Bb major | D#5 | 4 | F#3 | b6 |
| m. 35, beat 3 to m. 35, beat 4 | Bb major | D#5 | 4 | F3 | 5 |
| m. 35, beat 4 to m. 36, beat 1 | Bb major | D5 | 3 | A#3 | 1 |
| m. 36, beat 1 to m. 36, beat 2 | Bb major | A#5 | 1 | E3 | #4 |
| m. 36, beat 2 to m. 36, beat 3 | Bb major | A#6 | 1 | A#3 | 1 |
| m. 36, beat 3 to m. 36, beat 4 | Bb major | A6 | 7 | E3 | #4 |
| m. 36, beat 4 to m. 37, beat 1 | Bb major | rest (register guard) | - | D#3 | 4 |
| m. 37, beat 1 to m. 37, beat 2 | Bb major | F5 | 5 | D3 | 3 |
| m. 37, beat 2 to m. 37, beat 3 | Bb major | F5 | 5 | D#3 | 4 |
| m. 37, beat 3 to m. 37, beat 4 | Bb major | C5 | 2 | F3 | 5 |
| m. 37, beat 4 to m. 38, beat 1 | Bb major | D5 | 3 | F3 | 5 |
| m. 38, beat 1 to m. 38, beat 2 | Bb major | C5 | 2 | A#2 | 1 |
| m. 38, beat 2 to m. 38, beat 3 | Bb major | G#4 | b7 | A#2 | 1 |
| m. 38, beat 3 to m. 38, beat 4 | Bb major | B4 | b2 | A#2 | 1 |
| m. 38, beat 4 to m. 39, beat 1 | Bb major | D5 | 3 | A#2 | 1 |
| m. 39, beat 1 to m. 39, beat 2 | Bb major | D#5 | 4 | A#2 | 1 |
| m. 39, beat 2 to m. 39, beat 3 | Bb major | D5 | 3 | A#2 | 1 |
| m. 39, beat 3 to m. 39, beat 4 | Bb major | D#5 | 4 | A#2 | 1 |
| m. 39, beat 4 to m. 40, beat 1 | Bb major | F5 | 5 | A#2 | 1 |
| m. 40, beat 1 to m. 40, beat 2 | Bb major | A#4 | 1 | A#2 | 1 |
| m. 40, beat 2 to m. 40, beat 3 | Bb major | G#4 | b7 | A#2 | 1 |
| m. 40, beat 3 to m. 40, beat 4 | Bb major | B4 | b2 | A#2 | 1 |
| m. 40, beat 4 to m. 41, beat 1 | Bb major | D5 | 3 | A#2 | 1 |
| m. 41, beat 1 to m. 41, beat 2 | Bb major | D#5 | 4 | A#2 | 1 |
| m. 41, beat 2 to m. 41, beat 3 | Bb major | D5 | 3 | A#2 | 1 |
| m. 41, beat 3 to m. 41, beat 4 | Bb major | D#5 | 4 | A#2 | 1 |
| m. 41, beat 4 to m. 42, beat 1 | Bb major | F5 | 5 | A#2 | 1 |
| m. 42, beat 1 to m. 42, beat 1.33 | Bb major | A#4 | 1 | A#2 | 1 |
| m. 42, beat 1.33 to m. 42, beat 2 | Bb major | D#5 | 4 | rest (register guard) | - |
| m. 42, beat 2 to m. 42, beat 3 | Bb major | A5 | 7 | rest (register guard) | - |
| m. 42, beat 3 to m. 42, beat 3.33 | Bb major | A#5 | 1 | F4 | 5 |
| m. 42, beat 3.33 to m. 42, beat 4 | Bb major | rest (bridged) | - | D3 | 3 |
| m. 42, beat 4 to m. 43, beat 1 | Bb major | D5 | 3 | F2 | 5 |
| m. 43, beat 1 to m. 43, beat 1.33 | Bb major | A#4 | 1 | F2 | 5 |
| m. 43, beat 1.33 to m. 43, beat 2 | Bb major | rest (register guard) | - | D2 | 3 |
| m. 43, beat 2 to m. 43, beat 3 | Bb major | rest (register guard) | - | D#2 | 4 |
| m. 43, beat 3 to m. 44, beat 1 | Bb major | A#3 | 1 | A#1 | 1 |
## Non-chord tones

227 notes sound against a chord without being one of its tones; 145 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 1, beat 4 to m. 2, beat 1 | 7 | unclassified | vi |
| m. 1, beat 4 to m. 2, beat 1 | 12 | unclassified | vi |
| m. 1, beat 4 to m. 2, beat 1 | 13 | unclassified | vi |
| m. 3, beat 3 to m. 3, beat 4 | 30 | unclassified | vii° |
| m. 3, beat 3 to m. 3, beat 4 | 36 | suspension | vii° |
| m. 3, beat 3 to m. 3, beat 4 | 38 | unclassified | vii° |
| m. 4, beat 1 to m. 4, beat 2 | 41 | unclassified | I |
| m. 4, beat 2 to m. 4, beat 3 | 53 | escape-tone | I |
| m. 4, beat 3 to m. 4, beat 4 | 53 | unclassified | I |
| m. 8, beat 1 to m. 8, beat 2 | 102 | unclassified | V |
| m. 8, beat 1 to m. 8, beat 2 | 105 | unclassified | V |
| m. 8, beat 3 to m. 8, beat 4 | 114 | suspension | I |
| m. 8, beat 4 to m. 9, beat 1 | 119 | unclassified | dominant7 |
| m. 8, beat 4 to m. 9, beat 1 | 125 | anticipation | dominant7 |
| m. 9, beat 3 to m. 9, beat 4 | 134 | unclassified | minor7 |
| m. 9, beat 3 to m. 9, beat 4 | 136 | unclassified | minor7 |
| m. 9, beat 4 to m. 10, beat 1 | 134 | unclassified | vi |
| m. 9, beat 4 to m. 10, beat 1 | 140 | unclassified | vi |
| m. 9, beat 4 to m. 10, beat 1 | 141 | unclassified | vi |
| m. 11, beat 3 to m. 11, beat 4 | 159 | unclassified | vii° |
| m. 11, beat 3 to m. 11, beat 4 | 165 | suspension | vii° |
| m. 11, beat 3 to m. 11, beat 4 | 167 | unclassified | vii° |
| m. 12, beat 1 to m. 12, beat 2 | 170 | unclassified | I |
| m. 12, beat 2 to m. 12, beat 3 | 182 | escape-tone | I |
| m. 12, beat 3 to m. 12, beat 4 | 182 | unclassified | I |
| m. 13, beat 3 to m. 13, beat 4 | 190 | unclassified | I |
| m. 13, beat 4 to m. 14, beat 1 | 203 | unclassified | I |
| m. 14, beat 1 to m. 14, beat 2 | 203 | unclassified | I |
| m. 14, beat 1 to m. 14, beat 2 | 206 | unclassified | I |
| m. 14, beat 1 to m. 14, beat 2 | 207 | unclassified | I |
| m. 14, beat 2 to m. 14, beat 3 | 205 | unclassified | minor7 |
| m. 14, beat 3 to m. 14, beat 4 | 213 | unclassified | minor7 |
| m. 14, beat 4 to m. 15, beat 1 | 215 | unclassified | dominant7 |
| m. 14, beat 4 to m. 15, beat 1 | 219 | unclassified | dominant7 |
| m. 14, beat 4 to m. 15, beat 1 | 222 | neighbor | dominant7 |
| m. 15, beat 1 to m. 15, beat 2 | 223 | unclassified | I |
| m. 15, beat 2 to m. 15, beat 3 | 224 | unclassified | minor7 |
| m. 15, beat 3 to m. 15, beat 4 | 231 | unclassified | minor7 |
| m. 15, beat 4 to m. 16, beat 1 | 233 | unclassified | dominant7 |
| m. 15, beat 4 to m. 16, beat 1 | 236 | suspension | dominant7 |
| m. 16, beat 4 to m. 17, beat 1 | 241 | unclassified | vii° |
| m. 16, beat 4 to m. 17, beat 1 | 250 | suspension | vii° |
| m. 16, beat 4 to m. 17, beat 1 | 253 | unclassified | vii° |
| m. 17, beat 2 to m. 17, beat 3 | 255 | unclassified | minor7 |
| m. 17, beat 2 to m. 17, beat 3 | 256 | unclassified | minor7 |
| m. 17, beat 3 to m. 17, beat 4 | 260 | unclassified | minor7 |
| m. 17, beat 3 to m. 17, beat 4 | 262 | unclassified | minor7 |
| m. 17, beat 4 to m. 18, beat 1 | 264 | unclassified | minor7 |
| m. 17, beat 4 to m. 18, beat 1 | 267 | unclassified | minor7 |
| m. 17, beat 4 to m. 18, beat 1 | 270 | unclassified | minor7 |
| m. 18, beat 1 to m. 18, beat 1.33 | 271 | unclassified | major7 |
| m. 18, beat 1.33 to m. 18, beat 2 | 276 | appoggiatura | I |
| m. 18, beat 2 to m. 18, beat 3 | 286 | passing | minor7 |
| m. 18, beat 2 to m. 18, beat 3 | 296 | passing | minor7 |
| m. 18, beat 3 to m. 18, beat 4 | 311 | passing | major7 |
| m. 18, beat 4 to m. 19, beat 1 | 316 | passing | I |
| m. 18, beat 4 to m. 19, beat 1 | 321 | passing | I |
| m. 19, beat 1 to m. 19, beat 2 | 331 | neighbor | major7 |
| m. 19, beat 1 to m. 19, beat 2 | 332 | unclassified | major7 |
| m. 19, beat 1 to m. 19, beat 2 | 337 | unclassified | major7 |
| m. 19, beat 1 to m. 19, beat 2 | 341 | passing | major7 |
| m. 19, beat 1 to m. 19, beat 2 | 342 | unclassified | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 346 | neighbor | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 347 | unclassified | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 351 | passing | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 352 | unclassified | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 357 | unclassified | major7 |
| m. 19, beat 3 to m. 19, beat 4 | 361 | neighbor | major7 |
| m. 19, beat 3 to m. 19, beat 4 | 362 | anticipation | major7 |
| m. 19, beat 3 to m. 19, beat 4 | 367 | anticipation | major7 |
| m. 19, beat 3 to m. 19, beat 4 | 371 | unclassified | major7 |
| m. 19, beat 3 to m. 19, beat 4 | 372 | anticipation | major7 |
| m. 19, beat 4 to m. 20, beat 1 | 376 | neighbor | dominant7 |
| m. 19, beat 4 to m. 20, beat 1 | 379 | unclassified | dominant7 |
| m. 19, beat 4 to m. 20, beat 1 | 384 | unclassified | dominant7 |
| m. 19, beat 4 to m. 20, beat 1 | 389 | unclassified | dominant7 |
| m. 20, beat 1 to m. 20, beat 2 | 395 | appoggiatura | I |
| m. 20, beat 2 to m. 20, beat 3 | 405 | passing | minor7 |
| m. 20, beat 2 to m. 20, beat 3 | 415 | passing | minor7 |
| m. 20, beat 3 to m. 20, beat 4 | 430 | passing | major7 |
| m. 20, beat 4 to m. 21, beat 1 | 435 | passing | I |
| m. 20, beat 4 to m. 21, beat 1 | 440 | passing | I |
| m. 21, beat 1 to m. 21, beat 2 | 450 | neighbor | major7 |
| m. 21, beat 1 to m. 21, beat 2 | 451 | unclassified | major7 |
| m. 21, beat 1 to m. 21, beat 2 | 456 | unclassified | major7 |
| m. 21, beat 1 to m. 21, beat 2 | 460 | passing | major7 |
| m. 21, beat 1 to m. 21, beat 2 | 461 | unclassified | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 465 | neighbor | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 466 | unclassified | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 470 | passing | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 471 | unclassified | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 476 | unclassified | major7 |
| m. 21, beat 3 to m. 21, beat 4 | 480 | neighbor | major7 |
| m. 21, beat 3 to m. 21, beat 4 | 481 | anticipation | major7 |
| m. 21, beat 3 to m. 21, beat 4 | 486 | anticipation | major7 |
| m. 21, beat 3 to m. 21, beat 4 | 490 | unclassified | major7 |
| m. 21, beat 3 to m. 21, beat 4 | 491 | anticipation | major7 |
| m. 21, beat 4 to m. 22, beat 1 | 495 | neighbor | dominant7 |
| m. 21, beat 4 to m. 22, beat 1 | 498 | unclassified | dominant7 |
| m. 21, beat 4 to m. 22, beat 1 | 503 | unclassified | dominant7 |
| m. 21, beat 4 to m. 22, beat 1 | 508 | unclassified | dominant7 |
| m. 23, beat 4 to m. 24, beat 1 | 534 | unclassified | vi |
| m. 23, beat 4 to m. 24, beat 1 | 539 | unclassified | vi |
| m. 23, beat 4 to m. 24, beat 1 | 540 | unclassified | vi |
| m. 24, beat 3 to m. 24, beat 4 | 554 | unclassified | I |
| m. 24, beat 4 to m. 25, beat 1 | 559 | unclassified | major7 |
| m. 24, beat 4 to m. 25, beat 1 | 562 | unclassified | major7 |
| m. 24, beat 4 to m. 25, beat 1 | 564 | unclassified | major7 |
| m. 25, beat 1 to m. 25, beat 2 | 564 | unclassified | I |
| m. 25, beat 3 to m. 25, beat 4 | 565 | unclassified | vii° |
| m. 25, beat 3 to m. 25, beat 4 | 571 | unclassified | vii° |
| m. 25, beat 3 to m. 25, beat 4 | 573 | unclassified | vii° |
| m. 26, beat 2 to m. 26, beat 3 | 590 | escape-tone | major7 |
| m. 26, beat 3 to m. 26, beat 4 | 590 | unclassified | I |
| m. 27, beat 4 to m. 28, beat 1 | 605 | unclassified | half-diminished7 |
| m. 27, beat 4 to m. 28, beat 1 | 611 | unclassified | half-diminished7 |
| m. 28, beat 3 to m. 28, beat 4 | 620 | unclassified | minor7 |
| m. 28, beat 3 to m. 28, beat 4 | 626 | unclassified | minor7 |
| m. 28, beat 4 to m. 29, beat 1 | 626 | unclassified | minor7 |
| m. 28, beat 4 to m. 29, beat 1 | 631 | unclassified | minor7 |
| m. 29, beat 1 to m. 29, beat 2 | 633 | unclassified | iii |
| m. 29, beat 3 to m. 29, beat 4 | 642 | unclassified | minor7 |
| m. 29, beat 3 to m. 29, beat 4 | 644 | passing | minor7 |
| m. 29, beat 4 to m. 30, beat 1 | 641 | unclassified | dominant7 |
| m. 29, beat 4 to m. 30, beat 1 | 644 | unclassified | dominant7 |
| m. 30, beat 2 to m. 30, beat 3 | 653 | unclassified | I |
| m. 30, beat 3 to m. 30, beat 4 | 657 | suspension | IV |
| m. 30, beat 4 to m. 31, beat 1 | 662 | unclassified | I |
| m. 30, beat 4 to m. 31, beat 1 | 665 | escape-tone | I |
| m. 30, beat 4 to m. 31, beat 1 | 666 | unclassified | I |
| m. 30, beat 4 to m. 31, beat 1 | 668 | anticipation | I |
| m. 31, beat 1 to m. 31, beat 2 | 670 | unclassified | minor7 |
| m. 31, beat 2 to m. 31, beat 3 | 675 | unclassified | minor7 |
| m. 31, beat 4 to m. 32, beat 1 | 679 | unclassified | vi |
| m. 31, beat 4 to m. 32, beat 1 | 682 | unclassified | vi |
| m. 31, beat 4 to m. 32, beat 1 | 686 | unclassified | vi |
| m. 32, beat 1 to m. 32, beat 2 | 683 | unclassified | V |
| m. 32, beat 1 to m. 32, beat 2 | 686 | unclassified | V |
| m. 32, beat 1 to m. 32, beat 2 | 690 | unclassified | V |
| m. 32, beat 2 to m. 32, beat 3 | 687 | unclassified | minor7 |
| m. 32, beat 2 to m. 32, beat 3 | 690 | unclassified | minor7 |
| m. 32, beat 2 to m. 32, beat 3 | 694 | appoggiatura | minor7 |
| m. 32, beat 4 to m. 33, beat 1 | 698 | unclassified | I |
| m. 32, beat 4 to m. 33, beat 1 | 703 | unclassified | I |
| m. 33, beat 4 to m. 34, beat 1 | 716 | unclassified | minor7 |
| m. 34, beat 3 to m. 34, beat 4 | 719 | unclassified | V |
| m. 34, beat 3 to m. 34, beat 4 | 725 | suspension | V |
| m. 34, beat 4 to m. 35, beat 1 | 732 | unclassified | V |
| m. 35, beat 1 to m. 35, beat 2 | 732 | unclassified | IV |
| m. 35, beat 2 to m. 35, beat 3 | 737 | unclassified | IV |
| m. 35, beat 2 to m. 35, beat 3 | 738 | unclassified | IV |
| m. 35, beat 3 to m. 35, beat 4 | 733 | unclassified | I |
| m. 36, beat 2 to m. 36, beat 3 | 756 | unclassified | minor7 |
| m. 36, beat 2 to m. 36, beat 3 | 759 | unclassified | minor7 |
| m. 36, beat 2 to m. 36, beat 3 | 764 | unclassified | minor7 |
| m. 36, beat 4 to m. 37, beat 1 | 768 | unclassified | dominant7 |
| m. 36, beat 4 to m. 37, beat 1 | 772 | unclassified | dominant7 |
| m. 36, beat 4 to m. 37, beat 1 | 775 | neighbor | dominant7 |
| m. 37, beat 1 to m. 37, beat 2 | 779 | unclassified | iii |
| m. 37, beat 1 to m. 37, beat 2 | 780 | unclassified | iii |
| m. 37, beat 2 to m. 37, beat 3 | 777 | unclassified | ii |
| m. 37, beat 2 to m. 37, beat 3 | 780 | unclassified | ii |
| m. 37, beat 3 to m. 37, beat 4 | 784 | unclassified | minor7 |
| m. 37, beat 4 to m. 38, beat 1 | 786 | unclassified | minor7 |
| m. 37, beat 4 to m. 38, beat 1 | 789 | unclassified | minor7 |
| m. 37, beat 4 to m. 38, beat 1 | 792 | unclassified | minor7 |
| m. 38, beat 1 to m. 38, beat 2 | 793 | unclassified | I |
| m. 38, beat 1 to m. 38, beat 2 | 798 | appoggiatura | I |
| m. 38, beat 2 to m. 38, beat 3 | 808 | passing | minor7 |
| m. 38, beat 2 to m. 38, beat 3 | 818 | passing | minor7 |
| m. 38, beat 3 to m. 38, beat 4 | 833 | passing | major7 |
| m. 38, beat 4 to m. 39, beat 1 | 838 | passing | I |
| m. 38, beat 4 to m. 39, beat 1 | 843 | passing | I |
| m. 39, beat 1 to m. 39, beat 2 | 853 | neighbor | major7 |
| m. 39, beat 1 to m. 39, beat 2 | 854 | unclassified | major7 |
| m. 39, beat 1 to m. 39, beat 2 | 859 | unclassified | major7 |
| m. 39, beat 1 to m. 39, beat 2 | 863 | passing | major7 |
| m. 39, beat 1 to m. 39, beat 2 | 864 | unclassified | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 868 | neighbor | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 869 | unclassified | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 873 | passing | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 874 | unclassified | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 879 | unclassified | major7 |
| m. 39, beat 3 to m. 39, beat 4 | 883 | neighbor | major7 |
| m. 39, beat 3 to m. 39, beat 4 | 884 | anticipation | major7 |
| m. 39, beat 3 to m. 39, beat 4 | 889 | anticipation | major7 |
| m. 39, beat 3 to m. 39, beat 4 | 893 | unclassified | major7 |
| m. 39, beat 3 to m. 39, beat 4 | 894 | anticipation | major7 |
| m. 39, beat 4 to m. 40, beat 1 | 898 | neighbor | dominant7 |
| m. 39, beat 4 to m. 40, beat 1 | 901 | unclassified | dominant7 |
| m. 39, beat 4 to m. 40, beat 1 | 906 | unclassified | dominant7 |
| m. 39, beat 4 to m. 40, beat 1 | 911 | unclassified | dominant7 |
| m. 40, beat 1 to m. 40, beat 2 | 917 | appoggiatura | I |
| m. 40, beat 2 to m. 40, beat 3 | 927 | passing | minor7 |
| m. 40, beat 2 to m. 40, beat 3 | 937 | passing | minor7 |
| m. 40, beat 3 to m. 40, beat 4 | 952 | passing | major7 |
| m. 40, beat 4 to m. 41, beat 1 | 957 | passing | I |
| m. 40, beat 4 to m. 41, beat 1 | 962 | passing | I |
| m. 41, beat 1 to m. 41, beat 2 | 972 | neighbor | major7 |
| m. 41, beat 1 to m. 41, beat 2 | 973 | unclassified | major7 |
| m. 41, beat 1 to m. 41, beat 2 | 978 | unclassified | major7 |
| m. 41, beat 1 to m. 41, beat 2 | 982 | passing | major7 |
| m. 41, beat 1 to m. 41, beat 2 | 983 | unclassified | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 987 | neighbor | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 988 | unclassified | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 992 | passing | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 993 | unclassified | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 998 | unclassified | major7 |
| m. 41, beat 3 to m. 41, beat 4 | 1002 | neighbor | major7 |
| m. 41, beat 3 to m. 41, beat 4 | 1003 | anticipation | major7 |
| m. 41, beat 3 to m. 41, beat 4 | 1008 | anticipation | major7 |
| m. 41, beat 3 to m. 41, beat 4 | 1012 | unclassified | major7 |
| m. 41, beat 3 to m. 41, beat 4 | 1013 | anticipation | major7 |
| m. 41, beat 4 to m. 42, beat 1 | 1017 | neighbor | dominant7 |
| m. 41, beat 4 to m. 42, beat 1 | 1020 | anticipation | dominant7 |
| m. 41, beat 4 to m. 42, beat 1 | 1025 | anticipation | dominant7 |
| m. 41, beat 4 to m. 42, beat 1 | 1030 | anticipation | dominant7 |
| m. 42, beat 2 to m. 42, beat 3 | 1039 | unclassified | dominant7 |
| m. 42, beat 2 to m. 42, beat 3 | 1040 | neighbor | dominant7 |
| m. 42, beat 2 to m. 42, beat 3 | 1041 | unclassified | dominant7 |
| m. 42, beat 3 to m. 42, beat 3.33 | 1043 | unclassified | major7 |
| m. 42, beat 4 to m. 43, beat 1 | 1051 | unclassified | dominant7 |
| m. 42, beat 4 to m. 43, beat 1 | 1056 | escape-tone | dominant7 |
| m. 42, beat 4 to m. 43, beat 1 | 1057 | unclassified | dominant7 |
| m. 43, beat 1 to m. 43, beat 1.33 | 1059 | unclassified | major7 |
| m. 43, beat 2 to m. 43, beat 3 | 1069 | unclassified | dominant7 |
| m. 43, beat 2 to m. 43, beat 3 | 1071 | unclassified | dominant7 |
