# Analysis Report: Field 1.mid

## Summary

- **File:** Field 1.mid, 1191 notes.
- **Input:** MIDI file. Meter: embedded in the file. Key signature: inferred. Pitch spelling: by rule.
- **Key:** Eb major, the key with the most time across 4 key regions.
- **Chords:** 268 chord events; 241 matched a chord (90%), 27 did not.
- **Most frequent chords:** I (95), dominant7 (41), major7 (26).

## Meter

- **Time signature:** 12/8
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
| m. 28, beat 1 to m. 33, beat 1 | F minor | 0.75 | profile |
| m. 33, beat 1 to m. 68, beat 1 | Eb major | 0.73 | profile |

## Modulations

A modulation is a lasting new key. Position is where the new key arrives. Type is pivot when the library found a pivot chord and direct when it did not. A return goes back to the key before an excursion. The library gives the change's extent from the length of the new key and whether it returns; no cadence is looked for.

| Position | From | To | Extent | Type | Return |
| --- | --- | --- | --- | --- | --- |
| m. 21, beat 1 | Eb major | Bb major | modulation | pivot | no |
| m. 28, beat 1 | Bb major | F minor | modulation | pivot | no |
| m. 33, beat 1 | F minor | Eb major | modulation | pivot | no |

## Pivot chords

The chord at a modulation that is diatonic in both the old and the new key, named by root and quality, with no Roman numeral. Modulation is its row in the Modulations table, counting from 1.

| Position | Chord | Bass | Inversion | From | To | Modulation |
| --- | --- | --- | --- | --- | --- | --- |
| m. 21, beat 1 to m. 21, beat 2 | Bb major | Bb | root position | Eb major | Bb major | 1 |
| m. 27, beat 1 to m. 27, beat 2 | C minor7 | Bb | third | Bb major | F minor | 2 |
| m. 33, beat 1 to m. 33, beat 2 | Eb major | Eb | root position | F minor | Eb major | 3 |

## Chords

Positions are measure and beat. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 4 to m. 2, beat 1 | V | Bb | major | Bb | root position | 1 | 1.00 | - |
| m. 2, beat 1 to m. 2, beat 2 | I | Eb | major | Eb | root position | 1 | 0.94 | - |
| m. 2, beat 2 to m. 2, beat 3 | I | Eb | major | Bb | second | 1 | 0.50 | - |
| m. 2, beat 3 to m. 2, beat 4 | I | Eb | major | G | first | 1 | 0.48 | - |
| m. 2, beat 4 to m. 3, beat 1 | minor7 | G | minor7 | Bb | first | 1 | 0.18 | - |
| m. 3, beat 1 to m. 3, beat 2 | V | Bb | major | Bb | root position | 1 | 0.12 | - |
| m. 3, beat 2 to m. 3, beat 3 | half-diminished7 | D | half-diminished7 | Ab | second | 1 | 0.06 | IV (Ab major, score 0.76) |
| m. 3, beat 3 to m. 3, beat 4 | V | Bb | major | F | second | 1 | 0.06 | half-diminished7 (D half-diminished7, score 0.99) |
| m. 3, beat 4 to m. 3, beat 4.33 | dominant7 | Bb | dominant7 | Ab | third | 0.33 | 0.49 | - |
| m. 3, beat 4.33 to m. 4, beat 1 | V | Bb | major | Bb | root position | 0.67 | 0.97 | - |
| m. 4, beat 1 to m. 4, beat 2 | I | Eb | major | Eb | root position | 1 | 0.77 | - |
| m. 4, beat 2 to m. 4, beat 3 | I | Eb | major | G | first | 1 | 0.49 | - |
| m. 4, beat 3 to m. 4, beat 4 | I | Eb | major | Bb | second | 1 | 0.07 | iii (G minor, score 0.98) |
| m. 4, beat 4 to m. 5, beat 1 | I | Eb | major | G | first | 1 | 0.24 | - |
| m. 5, beat 1 to m. 5, beat 2 | I | Eb | major | Eb | root position | 1 | 0.51 | - |
| m. 5, beat 2 to m. 5, beat 3 | I | Eb | major | G | first | 1 | 0.08 | iii (G minor, score 0.97) |
| m. 5, beat 3 to m. 5, beat 4 | I | Eb | major | Bb | second | 1 | 0.07 | iii (G minor, score 0.97) |
| m. 5, beat 4 to m. 6, beat 1 | I | Eb | major | G | first | 1 | 0.12 | - |
| m. 6, beat 1 to m. 6, beat 2 | I | Eb | major | Eb | root position | 1 | 1.00 | - |
| m. 6, beat 2 to m. 6, beat 3 | I | Eb | major | Bb | second | 1 | 0.44 | - |
| m. 6, beat 3 to m. 6, beat 4 | I | Eb | major | G | first | 1 | 0.48 | - |
| m. 6, beat 4 to m. 7, beat 1 | minor7 | G | minor7 | Bb | first | 1 | 0.22 | - |
| m. 7, beat 1 to m. 7, beat 2 | V | Bb | major | Bb | root position | 1 | 0.62 | - |
| m. 7, beat 2 to m. 7, beat 3 | half-diminished7 | D | half-diminished7 | Ab | second | 1 | 0.06 | IV (Ab major, score 0.75) |
| m. 7, beat 3 to m. 7, beat 4 | V | Bb | major | F | second | 1 | 0.41 | - |
| m. 7, beat 4 to m. 8, beat 1 | dominant7 | Bb | dominant7 | Ab | third | 1 | 0.27 | - |
| m. 8, beat 1 to m. 8, beat 2 | major7 | Eb | major7 | Eb | root position | 1 | 0.93 | - |
| m. 8, beat 2 to m. 8, beat 3 | I | Eb | major | Bb | second | 1 | 0.50 | - |
| m. 8, beat 3 to m. 8, beat 4 | I | Eb | major | G | first | 1 | 0.48 | - |
| m. 8, beat 4 to m. 9, beat 1 | I | Eb | major | Bb | second | 1 | 0.55 | - |
| m. 9, beat 1 to m. 9, beat 2 | I | Eb | major | Eb | root position | 1 | 0.97 | - |
| m. 9, beat 2 to m. 9, beat 3 | I | Eb | major | Bb | second | 1 | 0.19 | - |
| m. 9, beat 3 to m. 9, beat 4 | I | Eb | major | G | first | 1 | 0.48 | - |
| m. 9, beat 4 to m. 10, beat 1 | major7 | Eb | major7 | Bb | second | 1 | 0.46 | - |
| m. 10, beat 1 to m. 10, beat 2 | vii° | D | diminished | Ab | second | 1 | 0.24 | - |
| m. 10, beat 2 to m. 10, beat 3 | IV | Ab | major | C | first | 1 | 0.19 | - |
| m. 10, beat 3 to m. 10, beat 4 | IV | Ab | major | Ab | root position | 1 | 0.66 | - |
| m. 10, beat 4 to m. 11, beat 1 | no match | - | - | - | - | - | - | - |
| m. 11, beat 1 to m. 11, beat 2 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.40 | - |
| m. 11, beat 2 to m. 11, beat 3 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.75 | - |
| m. 11, beat 3 to m. 11, beat 4 | vii° | D | diminished | F | first | 1 | 0.09 | ii (F minor, score 0.96) |
| m. 11, beat 4 to m. 12, beat 1 | iii | G | minor | Bb | first | 1 | 0.33 | - |
| m. 12, beat 1 to m. 12, beat 2 | I | Eb | major | Eb | root position | 1 | 0.10 | minor7 (F minor7, score 0.68) |
| m. 12, beat 2 to m. 12, beat 3 | I | Eb | major | Bb | second | 1 | 0.40 | - |
| m. 12, beat 3 to m. 12, beat 4 | I | Eb | major | G | first | 1 | 0.48 | - |
| m. 12, beat 4 to m. 13, beat 1 | I | Eb | major | Bb | second | 1 | 0.55 | - |
| m. 13, beat 1 to m. 13, beat 2 | I | Eb | major | Eb | root position | 1 | 0.66 | - |
| m. 13, beat 2 to m. 13, beat 3 | minor7 | G | minor7 | Bb | first | 1 | 0.18 | - |
| m. 13, beat 3 to m. 13, beat 4 | I | Eb | major | G | first | 1 | 0.29 | - |
| m. 13, beat 4 to m. 14, beat 1 | major7 | Eb | major7 | Bb | second | 1 | 0.45 | - |
| m. 14, beat 1 to m. 14, beat 2 | half-diminished7 | D | half-diminished7 | Ab | second | 1 | 0.06 | IV (Ab major, score 0.75) |
| m. 14, beat 2 to m. 14, beat 3 | IV | Ab | major | C | first | 1 | 0.18 | - |
| m. 14, beat 3 to m. 14, beat 4 | IV | Ab | major | Ab | root position | 1 | 0.73 | - |
| m. 14, beat 4 to m. 15, beat 1 | no match | - | - | - | - | - | - | - |
| m. 15, beat 1 to m. 15, beat 2 | V | Bb | major | Bb | root position | 1 | 0.28 | - |
| m. 15, beat 2 to m. 15, beat 3 | half-diminished7 | D | half-diminished7 | Ab | second | 1 | 0.03 | dominant7 (Bb dominant7, score 0.75) |
| m. 15, beat 3 to m. 15, beat 4 | half-diminished7 | D | half-diminished7 | F | first | 1 | 0.23 | - |
| m. 15, beat 4 to m. 16, beat 1 | dominant7 | Bb | dominant7 | Ab | third | 1 | 0.24 | - |
| m. 16, beat 1 to m. 16, beat 2 | major7 | Eb | major7 | Eb | root position | 1 | 1.00 | - |
| m. 16, beat 2 to m. 16, beat 3 | I | Eb | major | Bb | second | 1 | 0.66 | - |
| m. 16, beat 3 to m. 16, beat 4 | I | Eb | major | Eb | root position | 1 | 1.00 | - |
| m. 16, beat 4 to m. 17, beat 1 | I | Eb | major | Bb | second | 1 | 0.36 | - |
| m. 17, beat 1 to m. 17, beat 2 | half-diminished7 | D | half-diminished7 | F | first | 1 | 0.23 | - |
| m. 17, beat 2 to m. 17, beat 3 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.50 | - |
| m. 17, beat 3 to m. 17, beat 4 | minor7 | G | minor7 | F | third | 1 | 0.01 | dominant7 (Bb dominant7, score 0.96) |
| m. 17, beat 4 to m. 18, beat 1 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.27 | - |
| m. 18, beat 1 to m. 18, beat 2 | major7 | Eb | major7 | Eb | root position | 1 | 0.20 | - |
| m. 18, beat 2 to m. 18, beat 3 | major7 | Eb | major7 | Bb | second | 1 | 0.41 | - |
| m. 18, beat 3 to m. 18, beat 4 | I | Eb | major | Eb | root position | 1 | 0.72 | - |
| m. 18, beat 4 to m. 19, beat 1 | I | Eb | major | Bb | second | 1 | 0.14 | - |
| m. 19, beat 1 to m. 19, beat 2 | vii° | D | diminished | F | first | 1 | 0.29 | - |
| m. 19, beat 2 to m. 19, beat 3 | half-diminished7 | D | half-diminished7 | Bb | - | 1 | 0.04 | IV (Ab major, score 0.60) |
| m. 19, beat 3 to m. 19, beat 4 | half-diminished7 | D | half-diminished7 | F | first | 1 | 0.06 | minor7 (G minor7, score 0.82) |
| m. 19, beat 4 to m. 19, beat 4.33 | iii | G | minor | Bb | first | 0.33 | 0.22 | - |
| m. 19, beat 4.33 to m. 20, beat 1 | dominant7 | Bb | dominant7 | Ab | third | 0.67 | 0.05 | vii° (D diminished, score 1.00) |
| m. 20, beat 1 to m. 20, beat 2 | I | Eb | major | Eb | root position | 1 | 0.24 | - |
| m. 20, beat 2 to m. 20, beat 3 | I | Eb | major | Bb | second | 1 | 0.68 | - |
| m. 20, beat 3 to m. 20, beat 4 | I | Eb | major | Eb | root position | 1 | 0.45 | - |
| m. 20, beat 4 to m. 20, beat 4.37 | minor7 | F | minor7 | Eb | third | 0.37 | 0.21 | - |
| m. 20, beat 4.37 to m. 21, beat 1 | major7 | Eb | major7 | D | third | 0.63 | 0.37 | - |
| m. 21, beat 1 to m. 21, beat 2 | I | Bb | major | Bb | root position | 1 | 0.41 | - |
| m. 21, beat 2 to m. 21, beat 3 | I | Bb | major | F | second | 1 | 0.09 | iii (D minor, score 0.96) |
| m. 21, beat 3 to m. 21, beat 4 | I | Bb | major | D | first | 1 | 0.08 | iii (D minor, score 0.96) |
| m. 21, beat 4 to m. 22, beat 1 | I | Bb | major | F | second | 1 | 0.14 | - |
| m. 22, beat 1 to m. 22, beat 2 | dominant7 | F | dominant7 | F | root position | 1 | 0.42 | - |
| m. 22, beat 2 to m. 22, beat 3 | dominant7 | F | dominant7 | F | root position | 1 | 0.61 | - |
| m. 22, beat 3 to m. 22, beat 4 | dominant7 | F | dominant7 | F | root position | 1 | 0.44 | - |
| m. 22, beat 4 to m. 23, beat 1 | minor7 | D | minor7 | F | first | 1 | 0.39 | - |
| m. 23, beat 1 to m. 23, beat 2 | minor7 | C | minor7 | Bb | third | 1 | 0.07 | I (Bb major, score 0.66) |
| m. 23, beat 2 to m. 23, beat 3 | I | Bb | major | F | second | 1 | 0.49 | - |
| m. 23, beat 3 to m. 23, beat 4 | I | Bb | major | D | first | 1 | 0.31 | - |
| m. 23, beat 4 to m. 24, beat 1 | major7 | Bb | major7 | F | second | 1 | 0.00 | minor7 (G minor7, score 0.90) |
| m. 24, beat 1 to m. 24, beat 2 | minor7 | G | minor7 | Bb | first | 1 | 0.02 | I (Bb major, score 0.52) |
| m. 24, beat 2 to m. 24, beat 3 | I | Bb | major | F | second | 1 | 0.09 | iii (D minor, score 0.96) |
| m. 24, beat 3 to m. 24, beat 4 | I | Bb | major | D | first | 1 | 0.09 | iii (D minor, score 0.95) |
| m. 24, beat 4 to m. 25, beat 1 | minor7 | G | minor7 | F | third | 1 | 0.01 | I (Bb major, score 0.97) |
| m. 25, beat 1 to m. 25, beat 2 | minor7 | G | minor7 | Bb | first | 1 | 0.00 | major7 (Eb major7, score 1.04) |
| m. 25, beat 2 to m. 25, beat 3 | minor7 | G | minor7 | F | third | 1 | 0.00 | I (Bb major, score 1.05) |
| m. 25, beat 3 to m. 25, beat 4 | I | Bb | major | D | first | 1 | 0.09 | iii (D minor, score 0.95) |
| m. 25, beat 4 to m. 26, beat 1 | I | Bb | major | F | second | 1 | 0.15 | - |
| m. 26, beat 1 to m. 26, beat 3 | dominant7 | F | dominant7 | F | root position | 2 | 0.85 | - |
| m. 26, beat 3 to m. 26, beat 4 | dominant7 | F | dominant7 | C | second | 1 | 0.13 | - |
| m. 26, beat 4 to m. 27, beat 1 | dominant7 | F | dominant7 | F | root position | 1 | 0.64 | - |
| m. 27, beat 1 to m. 27, beat 2 | minor7 | C | minor7 | Bb | third | 1 | 0.10 | I (Bb major, score 0.65) |
| m. 27, beat 2 to m. 27, beat 3 | I | Bb | major | F | second | 1 | 0.41 | - |
| m. 27, beat 3 to m. 27, beat 4 | I | Bb | major | D | first | 1 | 0.55 | - |
| m. 27, beat 4 to m. 28, beat 1 | I | Bb | major | F | second | 1 | 0.26 | - |
| m. 28, beat 1 to m. 28, beat 2 | no match | - | - | - | - | - | - | - |
| m. 28, beat 2 to m. 28, beat 3 | no match | - | - | - | - | - | - | - |
| m. 28, beat 3 to m. 28, beat 3.33 | no match | - | - | - | - | - | - | - |
| m. 28, beat 3.33 to m. 28, beat 4 | no match | - | - | - | - | - | - | - |
| m. 28, beat 4 to m. 29, beat 1 | no match | - | - | - | - | - | - | - |
| m. 29, beat 1 to m. 29, beat 2 | no match | - | - | - | - | - | - | - |
| m. 29, beat 2 to m. 29, beat 3 | no match | - | - | - | - | - | - | - |
| m. 29, beat 3 to m. 29, beat 4 | no match | - | - | - | - | - | - | - |
| m. 29, beat 4 to m. 30, beat 1 | no match | - | - | - | - | - | - | - |
| m. 30, beat 1 to m. 30, beat 2 | no match | - | - | - | - | - | - | - |
| m. 30, beat 2 to m. 30, beat 3 | no match | - | - | - | - | - | - | - |
| m. 30, beat 3 to m. 30, beat 4 | no match | - | - | - | - | - | - | - |
| m. 30, beat 4 to m. 31, beat 1 | no match | - | - | - | - | - | - | - |
| m. 31, beat 1 to m. 31, beat 2 | no match | - | - | - | - | - | - | - |
| m. 31, beat 2 to m. 31, beat 3 | no match | - | - | - | - | - | - | - |
| m. 31, beat 3 to m. 31, beat 4 | no match | - | - | - | - | - | - | - |
| m. 31, beat 4 to m. 32, beat 1 | no match | - | - | - | - | - | - | - |
| m. 32, beat 1 to m. 32, beat 2 | no match | - | - | - | - | - | - | - |
| m. 32, beat 2 to m. 32, beat 3 | no match | - | - | - | - | - | - | - |
| m. 32, beat 3 to m. 32, beat 4 | no match | - | - | - | - | - | - | - |
| m. 32, beat 4 to m. 33, beat 1 | no match | - | - | - | - | - | - | - |
| m. 33, beat 1 to m. 33, beat 2 | I | Eb | major | Eb | root position | 1 | 0.31 | - |
| m. 33, beat 2 to m. 33, beat 3 | I | Eb | major | Bb | second | 1 | 0.50 | - |
| m. 33, beat 3 to m. 33, beat 4 | minor7 | F | minor7 | Ab | first | 1 | 0.29 | - |
| m. 33, beat 4 to m. 34, beat 1 | vi | C | minor | C | root position | 1 | 0.03 | minor7 (F minor7, score 0.90) |
| m. 34, beat 1 to m. 34, beat 2 | V | Bb | major | Bb | root position | 1 | 0.62 | - |
| m. 34, beat 2 to m. 34, beat 3 | dominant7 | Bb | dominant7 | Ab | third | 1 | 0.08 | IV (Ab major, score 0.63) |
| m. 34, beat 3 to m. 34, beat 4 | V | Bb | major | Bb | root position | 1 | 0.50 | - |
| m. 34, beat 4 to m. 35, beat 1 | minor7 | G | minor7 | Ab | - | 1 | 0.21 | - |
| m. 35, beat 1 to m. 35, beat 2 | I | Eb | major | Eb | root position | 1 | 1.00 | - |
| m. 35, beat 2 to m. 35, beat 3 | I | Eb | major | Bb | second | 1 | 0.66 | - |
| m. 35, beat 3 to m. 35, beat 4 | I | Eb | major | Eb | root position | 1 | 1.00 | - |
| m. 35, beat 4 to m. 36, beat 1 | major7 | Eb | major7 | Bb | second | 1 | 0.04 | minor7 (C minor7, score 0.87) |
| m. 36, beat 1 to m. 36, beat 2 | dominant7 | Bb | dominant7 | F | second | 1 | 0.13 | - |
| m. 36, beat 2 to m. 36, beat 3 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.35 | - |
| m. 36, beat 3 to m. 36, beat 4 | vii° | D | diminished | F | first | 1 | 0.09 | ii (F minor, score 0.95) |
| m. 36, beat 4 to m. 37, beat 1 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.24 | - |
| m. 37, beat 1 to m. 37, beat 2 | I | Eb | major | Eb | root position | 1 | 1.00 | - |
| m. 37, beat 2 to m. 37, beat 3 | I | Eb | major | Bb | second | 1 | 0.66 | - |
| m. 37, beat 3 to m. 37, beat 4 | I | Eb | major | Eb | root position | 1 | 1.00 | - |
| m. 37, beat 4 to m. 38, beat 1 | major7 | Eb | major7 | Bb | second | 1 | 0.02 | minor7 (C minor7, score 0.91) |
| m. 38, beat 1 to m. 38, beat 2 | dominant7 | Bb | dominant7 | F | second | 1 | 0.06 | minor7 (G minor7, score 0.93) |
| m. 38, beat 2 to m. 38, beat 3 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.23 | - |
| m. 38, beat 3 to m. 38, beat 4 | half-diminished7 | D | half-diminished7 | F | first | 1 | 0.05 | dominant7 (Bb dominant7, score 0.82) |
| m. 38, beat 4 to m. 39, beat 1 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.18 | - |
| m. 39, beat 1 to m. 39, beat 2 | major7 | Eb | major7 | Eb | root position | 1 | 0.20 | - |
| m. 39, beat 2 to m. 39, beat 3 | major7 | Eb | major7 | Bb | second | 1 | 0.52 | - |
| m. 39, beat 3 to m. 39, beat 4 | I | Eb | major | Eb | root position | 1 | 0.63 | - |
| m. 39, beat 4 to m. 40, beat 1 | major7 | Eb | major7 | Bb | second | 1 | 0.35 | - |
| m. 40, beat 1 to m. 40, beat 2 | no match | - | - | - | - | - | - | - |
| m. 40, beat 2 to m. 40, beat 3 | minor7 | F | minor7 | A | - | 1 | 0.71 | - |
| m. 40, beat 3 to m. 40, beat 4 | vii° | D | diminished | Ab | second | 1 | 0.09 | ii (F minor, score 0.64) |
| m. 40, beat 4 to m. 41, beat 1 | V | Bb | major | Bb | root position | 1 | 0.43 | - |
| m. 41, beat 1 to m. 41, beat 2 | minor7 | C | minor7 | G | second | 1 | 0.13 | - |
| m. 41, beat 2 to m. 41, beat 3 | no match | - | - | - | - | - | - | - |
| m. 41, beat 3 to m. 41, beat 4 | dominant7 | Bb | dominant7 | F | second | 1 | 0.05 | minor7 (F minor7, score 0.76) |
| m. 41, beat 4 to m. 41, beat 4.33 | major7 | Ab | major7 | C | first | 0.33 | 0.07 | vi (C minor, score 0.98) |
| m. 41, beat 4.33 to m. 42, beat 1 | ii | F | minor | Ab | first | 0.67 | 0.03 | major7 (Ab major7, score 0.88) |
| m. 42, beat 1 to m. 42, beat 2 | I | Eb | major | Bb | second | 1 | 0.26 | - |
| m. 42, beat 2 to m. 42, beat 3 | major7 | Eb | major7 | G | first | 1 | 0.01 | minor7 (C minor7, score 0.94) |
| m. 42, beat 3 to m. 42, beat 4 | iii | G | minor | Bb | first | 1 | 0.13 | - |
| m. 42, beat 4 to m. 43, beat 1 | dominant7 | Bb | dominant7 | Ab | third | 1 | 0.12 | - |
| m. 43, beat 1 to m. 43, beat 2 | major7 | Eb | major7 | Eb | root position | 1 | 0.70 | - |
| m. 43, beat 2 to m. 43, beat 3 | I | Eb | major | Bb | second | 1 | 0.18 | - |
| m. 43, beat 3 to m. 43, beat 4 | I | Eb | major | Eb | root position | 1 | 0.93 | - |
| m. 43, beat 4.07 to m. 44, beat 1 | V | Bb | major | Bb | root position | 0.93 | 1.00 | - |
| m. 44, beat 1 to m. 44, beat 2 | I | Eb | major | Eb | root position | 1 | 0.75 | - |
| m. 44, beat 2 to m. 44, beat 3 | I | Eb | major | Bb | second | 1 | 0.55 | - |
| m. 44, beat 3 to m. 44, beat 4 | vi | C | minor | C | root position | 1 | 0.15 | - |
| m. 44, beat 4 to m. 45, beat 1 | vii° | D | diminished | F | first | 1 | 0.02 | minor7 (F minor7, score 0.77) |
| m. 45, beat 1 to m. 45, beat 2 | V | Bb | major | Bb | root position | 1 | 0.58 | - |
| m. 45, beat 2 to m. 45, beat 3 | half-diminished7 | D | half-diminished7 | Ab | second | 1 | 0.05 | IV (Ab major, score 0.77) |
| m. 45, beat 3 to m. 45, beat 4 | V | Bb | major | F | second | 1 | 0.06 | half-diminished7 (D half-diminished7, score 0.98) |
| m. 45, beat 4 to m. 45, beat 4.33 | dominant7 | Bb | dominant7 | Ab | third | 0.33 | 0.47 | - |
| m. 45, beat 4.33 to m. 46, beat 1 | V | Bb | major | Bb | root position | 0.67 | 0.14 | - |
| m. 46, beat 1 to m. 46, beat 2 | I | Eb | major | Eb | root position | 1 | 0.82 | - |
| m. 46, beat 2 to m. 46, beat 4 | I | Eb | major | Bb | second | 2 | 0.29 | - |
| m. 46, beat 4 to m. 47, beat 1 | I | Eb | major | G | first | 1 | 0.11 | - |
| m. 47, beat 1 to m. 47, beat 2 | I | Eb | major | Eb | root position | 1 | 0.45 | - |
| m. 47, beat 2 to m. 47, beat 3 | I | Eb | major | Bb | second | 1 | 0.09 | iii (G minor, score 0.96) |
| m. 47, beat 3 to m. 47, beat 4 | I | Eb | major | G | first | 1 | 0.07 | iii (G minor, score 0.90) |
| m. 47, beat 4 to m. 48, beat 1 | I | Eb | major | Bb | second | 1 | 0.12 | minor7 (G minor7, score 0.70) |
| m. 48, beat 1 to m. 48, beat 2 | I | Eb | major | Eb | root position | 1 | 0.94 | - |
| m. 48, beat 2 to m. 48, beat 3 | I | Eb | major | Bb | second | 1 | 0.44 | - |
| m. 48, beat 3 to m. 48, beat 4 | major7 | Eb | major7 | G | first | 1 | 0.34 | - |
| m. 48, beat 4 to m. 49, beat 1 | minor7 | C | minor7 | Bb | third | 1 | 0.03 | major7 (Eb major7, score 0.86) |
| m. 49, beat 1 to m. 49, beat 2 | major7 | Eb | major7 | Bb | second | 1 | 0.11 | dominant7 (Bb dominant7, score 0.63) |
| m. 49, beat 2 to m. 49, beat 3 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.20 | - |
| m. 49, beat 3 to m. 49, beat 4 | dominant7 | Bb | dominant7 | F | second | 1 | 0.02 | half-diminished7 (D half-diminished7, score 0.99) |
| m. 49, beat 4 to m. 50, beat 1 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.16 | - |
| m. 50, beat 1 to m. 50, beat 2 | I | Eb | major | Eb | root position | 1 | 0.05 | minor7 (F minor7, score 0.68) |
| m. 50, beat 2 to m. 50, beat 3 | I | Eb | major | Bb | second | 1 | 0.42 | - |
| m. 50, beat 3 to m. 50, beat 4 | I | Eb | major | G | first | 1 | 0.23 | - |
| m. 50, beat 4 to m. 51, beat 1 | I | Eb | major | Bb | second | 1 | 0.19 | - |
| m. 51, beat 1 to m. 51, beat 3 | minor7 | G | minor7 | G | root position | 2 | 0.82 | - |
| m. 51, beat 3 to m. 51, beat 4 | vii° | D | diminished | D | root position | 1 | 0.57 | - |
| m. 51, beat 4 to m. 52, beat 1 | major7 | Eb | major7 | G | first | 1 | 0.44 | - |
| m. 52, beat 1 to m. 52, beat 2 | IV | Ab | major | Ab | root position | 1 | 0.07 | half-diminished7 (D half-diminished7, score 0.69) |
| m. 52, beat 2 to m. 52, beat 3 | IV | Ab | major | Ab | root position | 1 | 0.35 | - |
| m. 52, beat 3 to m. 52, beat 4 | vi | C | minor | Eb | first | 1 | 0.42 | - |
| m. 52, beat 4 to m. 53, beat 1 | no match | - | - | - | - | - | - | - |
| m. 53, beat 1 to m. 53, beat 2 | V | Bb | major | Bb | root position | 1 | 0.30 | - |
| m. 53, beat 2 to m. 53, beat 3 | dominant7 | Bb | dominant7 | Ab | third | 1 | 0.28 | - |
| m. 53, beat 3 to m. 53, beat 4 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.63 | - |
| m. 53, beat 4 to m. 54, beat 1 | iii | G | minor | Ab | - | 1 | 0.23 | - |
| m. 54, beat 1 to m. 54, beat 2 | I | Eb | major | Eb | root position | 1 | 0.10 | minor7 (F minor7, score 0.68) |
| m. 54, beat 2 to m. 54, beat 3 | I | Eb | major | Bb | second | 1 | 0.49 | - |
| m. 54, beat 3 to m. 54, beat 4 | I | Eb | major | G | first | 1 | 0.32 | - |
| m. 54, beat 4 to m. 55, beat 1 | I | Eb | major | Bb | second | 1 | 0.19 | - |
| m. 55, beat 1 to m. 55, beat 3 | minor7 | G | minor7 | G | root position | 2 | 0.70 | - |
| m. 55, beat 3 to m. 55, beat 4 | vii° | D | diminished | D | root position | 1 | 0.45 | - |
| m. 55, beat 4 to m. 56, beat 1 | major7 | Eb | major7 | G | first | 1 | 0.45 | - |
| m. 56, beat 1 to m. 56, beat 2 | IV | Ab | major | Ab | root position | 1 | 0.19 | - |
| m. 56, beat 2 to m. 56, beat 3 | IV | Ab | major | Ab | root position | 1 | 0.68 | - |
| m. 56, beat 3 to m. 56, beat 4 | major7 | Ab | major7 | Eb | second | 1 | 0.06 | minor7 (C minor7, score 0.93) |
| m. 56, beat 4 to m. 57, beat 1 | minor7 | F | minor7 | Ab | first | 1 | 0.27 | - |
| m. 57, beat 1 to m. 57, beat 1.33 | V | Bb | major | Bb | root position | 0.33 | 1.00 | - |
| m. 57, beat 1.33 to m. 57, beat 2 | V | Bb | major | Bb | root position | 0.67 | 0.28 | - |
| m. 57, beat 2 to m. 57, beat 3 | IV | Ab | major | Ab | root position | 1 | 0.03 | half-diminished7 (D half-diminished7, score 0.78) |
| m. 57, beat 3 to m. 57, beat 4 | minor7 | C | minor7 | Bb | third | 1 | 0.14 | V (Bb major, score 0.60) |
| m. 57, beat 4 to m. 58, beat 1 | dominant7 | Bb | dominant7 | Ab | third | 1 | 0.27 | - |
| m. 58, beat 1 to m. 58, beat 2 | major7 | Eb | major7 | Eb | root position | 1 | 0.93 | - |
| m. 58, beat 2 to m. 58, beat 3 | I | Eb | major | Bb | second | 1 | 0.39 | - |
| m. 58, beat 3 to m. 58, beat 4 | I | Eb | major | G | first | 1 | 0.23 | - |
| m. 58, beat 4 to m. 59, beat 1 | I | Eb | major | Bb | second | 1 | 0.19 | - |
| m. 59, beat 1 to m. 59, beat 2 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.42 | - |
| m. 59, beat 2 to m. 59, beat 3 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.74 | - |
| m. 59, beat 3 to m. 59, beat 4 | dominant7 | Bb | dominant7 | F | second | 1 | 0.18 | - |
| m. 59, beat 4 to m. 60, beat 1 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.38 | - |
| m. 60, beat 1 to m. 60, beat 2 | I | Eb | major | G | first | 1 | 0.32 | - |
| m. 60, beat 2 to m. 60, beat 3 | I | Eb | major | Bb | second | 1 | 0.18 | - |
| m. 60, beat 3 to m. 60, beat 4 | major7 | Eb | major7 | G | first | 1 | 0.20 | - |
| m. 60, beat 4 to m. 61, beat 1 | major7 | Eb | major7 | Bb | second | 1 | 0.13 | - |
| m. 61, beat 1 to m. 61, beat 2 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.46 | - |
| m. 61, beat 2 to m. 61, beat 3 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.71 | - |
| m. 61, beat 3 to m. 61, beat 4 | dominant7 | Bb | dominant7 | F | second | 1 | 0.20 | - |
| m. 61, beat 4 to m. 62, beat 1 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.50 | - |
| m. 62, beat 1 to m. 62, beat 2 | major7 | Eb | major7 | Eb | root position | 1 | 0.32 | - |
| m. 62, beat 2 to m. 62, beat 3 | major7 | Eb | major7 | Bb | second | 1 | 0.26 | - |
| m. 62, beat 3 to m. 62, beat 4 | I | Eb | major | G | first | 1 | 0.07 | iii (G minor, score 0.98) |
| m. 62, beat 4 to m. 63, beat 1 | I | Eb | major | Bb | second | 1 | 0.16 | - |
| m. 63, beat 1 to m. 63, beat 2 | no match | - | - | - | - | - | - | - |
| m. 63, beat 2 to m. 63, beat 3 | minor7 | F | minor7 | Ab | first | 1 | 0.18 | - |
| m. 63, beat 3 to m. 63, beat 4 | vii° | D | diminished | Ab | second | 1 | 0.14 | ii (F minor, score 0.57) |
| m. 63, beat 4 to m. 64, beat 1 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.40 | - |
| m. 64, beat 1 to m. 64, beat 2 | I | Eb | major | G | first | 1 | 0.15 | - |
| m. 64, beat 2 to m. 64, beat 3 | iii | G | minor | G | root position | 1 | 0.83 | - |
| m. 64, beat 3 to m. 64, beat 4 | ii | F | minor | Ab | first | 1 | 0.10 | major7 (Ab major7, score 0.73) |
| m. 64, beat 4 to m. 65, beat 1 | ii | F | minor | Ab | first | 1 | 0.03 | dominant7 (Bb dominant7, score 0.75) |
| m. 65, beat 1 to m. 65, beat 1.33 | V | Bb | major | Bb | root position | 0.33 | 1.00 | - |
| m. 65, beat 1.33 to m. 65, beat 2 | I | Eb | major | Bb | second | 0.67 | 0.20 | - |
| m. 65, beat 2 to m. 65, beat 3 | I | Eb | major | G | first | 1 | 0.12 | - |
| m. 65, beat 3 to m. 65, beat 4 | dominant7 | Bb | dominant7 | Bb | root position | 1 | 0.00 | iii (G minor, score 0.88) |
| m. 65, beat 4 to m. 66, beat 1 | dominant7 | Bb | dominant7 | F | second | 1 | 0.17 | - |
| m. 66, beat 1 to m. 66, beat 2 | I | Eb | major | Eb | root position | 1 | 0.98 | - |
| m. 66, beat 2 to m. 66, beat 3 | I | Eb | major | Bb | second | 1 | 0.09 | iii (G minor, score 0.95) |
| m. 66, beat 3 to m. 66, beat 4 | I | Eb | major | Eb | root position | 1 | 0.58 | - |
| m. 66, beat 4 to m. 67, beat 1 | I | Eb | major | Bb | second | 1 | 0.08 | iii (G minor, score 0.97) |
| m. 67, beat 1 to m. 67, beat 2 | I | Eb | major | Eb | root position | 1 | 0.75 | - |
| m. 67, beat 2 to m. 67, beat 3 | I | Eb | major | Bb | second | 1 | 0.37 | - |
| m. 67, beat 3 to m. 67, beat 4 | I | Eb | major | Eb | root position | 1 | 0.89 | - |
## Outer voices

The highest (soprano) and lowest (bass) sounding note of each chord's span, with its scale degree in that chord's local key. A flat or sharp sign marks a chromatic alteration. A degree is "-" where the span lies in no key region. A rest means the library found no note for that line in the span, a bridged one a short gap inside the line, and a register guard a note the library dropped rather than adopt an accompaniment note. Degrees carry the key region's uncertainty in the analysis document.

| Position | Key | Soprano | Degree | Bass | Degree |
| --- | --- | --- | --- | --- | --- |
| m. 1, beat 4 to m. 2, beat 1 | Eb major | Bb4 | 5 | Bb4 | 5 |
| m. 2, beat 1 to m. 2, beat 2 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 2, beat 2 to m. 2, beat 3 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 2, beat 3 to m. 2, beat 4 | Eb major | Eb5 | 1 | G3 | 3 |
| m. 2, beat 4 to m. 3, beat 1 | Eb major | F5 | 2 | Bb3 | 5 |
| m. 3, beat 1 to m. 3, beat 2 | Eb major | Eb5 | 1 | Bb2 | 5 |
| m. 3, beat 2 to m. 3, beat 3 | Eb major | D5 | 7 | Ab3 | 4 |
| m. 3, beat 3 to m. 3, beat 4 | Eb major | D5 | 7 | F3 | 2 |
| m. 3, beat 4 to m. 3, beat 4.33 | Eb major | D5 | 7 | Ab3 | 4 |
| m. 3, beat 4.33 to m. 4, beat 1 | Eb major | Bb4 | 5 | Bb3 | 5 |
| m. 4, beat 1 to m. 4, beat 2 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 4, beat 2 to m. 4, beat 3 | Eb major | Eb5 | 1 | G3 | 3 |
| m. 4, beat 3 to m. 4, beat 4 | Eb major | G5 | 3 | Bb3 | 5 |
| m. 4, beat 4 to m. 5, beat 1 | Eb major | G5 | 3 | G3 | 3 |
| m. 5, beat 1 to m. 5, beat 2 | Eb major | Bb5 | 5 | Eb3 | 1 |
| m. 5, beat 2 to m. 5, beat 3 | Eb major | Bb5 | 5 | G3 | 3 |
| m. 5, beat 3 to m. 5, beat 4 | Eb major | Bb5 | 5 | Bb3 | 5 |
| m. 5, beat 4 to m. 6, beat 1 | Eb major | Bb5 | 5 | G3 | 3 |
| m. 6, beat 1 to m. 6, beat 2 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 6, beat 2 to m. 6, beat 3 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 6, beat 3 to m. 6, beat 4 | Eb major | Eb5 | 1 | G3 | 3 |
| m. 6, beat 4 to m. 7, beat 1 | Eb major | F5 | 2 | Bb3 | 5 |
| m. 7, beat 1 to m. 7, beat 2 | Eb major | C6 | 6 | Bb2 | 5 |
| m. 7, beat 2 to m. 7, beat 3 | Eb major | C6 | 6 | Ab3 | 4 |
| m. 7, beat 3 to m. 7, beat 4 | Eb major | Bb5 | 5 | F3 | 2 |
| m. 7, beat 4 to m. 8, beat 1 | Eb major | Bb5 | 5 | Ab3 | 4 |
| m. 8, beat 1 to m. 8, beat 2 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 8, beat 2 to m. 8, beat 3 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 8, beat 3 to m. 8, beat 4 | Eb major | Eb5 | 1 | G3 | 3 |
| m. 8, beat 4 to m. 9, beat 1 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 9, beat 1 to m. 9, beat 2 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 9, beat 2 to m. 9, beat 3 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 9, beat 3 to m. 9, beat 4 | Eb major | Eb5 | 1 | G3 | 3 |
| m. 9, beat 4 to m. 10, beat 1 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 10, beat 1 to m. 10, beat 2 | Eb major | D5 | 7 | Ab2 | 4 |
| m. 10, beat 2 to m. 10, beat 3 | Eb major | D5 | 7 | rest (register guard) | - |
| m. 10, beat 3 to m. 10, beat 4 | Eb major | C5 | 6 | Ab3 | 4 |
| m. 10, beat 4 to m. 11, beat 1 | Eb major | C5 | 6 | B3 | b6 |
| m. 11, beat 1 to m. 11, beat 2 | Eb major | C5 | 6 | Bb2 | 5 |
| m. 11, beat 2 to m. 11, beat 3 | Eb major | Ab5 | 4 | Bb3 | 5 |
| m. 11, beat 3 to m. 11, beat 4 | Eb major | G5 | 3 | F3 | 2 |
| m. 11, beat 4 to m. 12, beat 1 | Eb major | G5 | 3 | Bb3 | 5 |
| m. 12, beat 1 to m. 12, beat 2 | Eb major | G5 | 3 | Eb3 | 1 |
| m. 12, beat 2 to m. 12, beat 3 | Eb major | F5 | 2 | Bb3 | 5 |
| m. 12, beat 3 to m. 12, beat 4 | Eb major | Eb5 | 1 | G3 | 3 |
| m. 12, beat 4 to m. 13, beat 1 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 13, beat 1 to m. 13, beat 2 | Eb major | G5 | 3 | Eb3 | 1 |
| m. 13, beat 2 to m. 13, beat 3 | Eb major | G5 | 3 | Bb3 | 5 |
| m. 13, beat 3 to m. 13, beat 4 | Eb major | F5 | 2 | G3 | 3 |
| m. 13, beat 4 to m. 14, beat 1 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 14, beat 1 to m. 14, beat 2 | Eb major | D5 | 7 | Ab2 | 4 |
| m. 14, beat 2 to m. 14, beat 3 | Eb major | D5 | 7 | rest (register guard) | - |
| m. 14, beat 3 to m. 14, beat 4 | Eb major | C5 | 6 | Ab3 | 4 |
| m. 14, beat 4 to m. 15, beat 1 | Eb major | B4 | b6 | B3 | b6 |
| m. 15, beat 1 to m. 15, beat 2 | Eb major | C5 | 6 | Bb2 | 5 |
| m. 15, beat 2 to m. 15, beat 3 | Eb major | C6 | 6 | Ab3 | 4 |
| m. 15, beat 3 to m. 15, beat 4 | Eb major | C6 | 6 | F3 | 2 |
| m. 15, beat 4 to m. 16, beat 1 | Eb major | C6 | 6 | Ab3 | 4 |
| m. 16, beat 1 to m. 16, beat 2 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 16, beat 2 to m. 16, beat 3 | Eb major | Eb5 | 1 | Bb2 | 5 |
| m. 16, beat 3 to m. 16, beat 4 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 16, beat 4 to m. 17, beat 1 | Eb major | F5 | 2 | Bb2 | 5 |
| m. 17, beat 1 to m. 17, beat 2 | Eb major | C6 | 6 | F3 | 2 |
| m. 17, beat 2 to m. 17, beat 3 | Eb major | C6 | 6 | Bb2 | 5 |
| m. 17, beat 3 to m. 17, beat 4 | Eb major | Bb5 | 5 | F3 | 2 |
| m. 17, beat 4 to m. 18, beat 1 | Eb major | Ab5 | 4 | Bb2 | 5 |
| m. 18, beat 1 to m. 18, beat 2 | Eb major | F5 | 2 | Eb3 | 1 |
| m. 18, beat 2 to m. 18, beat 3 | Eb major | G5 | 3 | Bb2 | 5 |
| m. 18, beat 3 to m. 18, beat 4 | Eb major | G5 | 3 | Eb3 | 1 |
| m. 18, beat 4 to m. 19, beat 1 | Eb major | G5 | 3 | Bb2 | 5 |
| m. 19, beat 1 to m. 19, beat 2 | Eb major | Eb5 | 1 | F3 | 2 |
| m. 19, beat 2 to m. 19, beat 3 | Eb major | C6 | 6 | Bb2 | 5 |
| m. 19, beat 3 to m. 19, beat 4 | Eb major | C6 | 6 | F3 | 2 |
| m. 19, beat 4 to m. 19, beat 4.33 | Eb major | Bb5 | 5 | Bb2 | 5 |
| m. 19, beat 4.33 to m. 20, beat 1 | Eb major | Bb5 | 5 | Ab3 | 4 |
| m. 20, beat 1 to m. 20, beat 2 | Eb major | F5 | 2 | Eb3 | 1 |
| m. 20, beat 2 to m. 20, beat 3 | Eb major | rest (register guard) | - | Bb2 | 5 |
| m. 20, beat 3 to m. 20, beat 4 | Eb major | F5 | 2 | Eb2 | 1 |
| m. 20, beat 4 to m. 20, beat 4.37 | Eb major | F5 | 2 | rest (register guard) | - |
| m. 20, beat 4.37 to m. 21, beat 1 | Eb major | Eb5 | 1 | D5 | 7 |
| m. 21, beat 1 to m. 21, beat 2 | Bb major | F5 | 5 | Bb2 | 1 |
| m. 21, beat 2 to m. 21, beat 3 | Bb major | F5 | 5 | F3 | 5 |
| m. 21, beat 3 to m. 21, beat 4 | Bb major | F5 | 5 | D3 | 3 |
| m. 21, beat 4 to m. 22, beat 1 | Bb major | F5 | 5 | F3 | 5 |
| m. 22, beat 1 to m. 22, beat 2 | Bb major | G5 | 6 | F2 | 5 |
| m. 22, beat 2 to m. 22, beat 3 | Bb major | Eb6 | 4 | F3 | 5 |
| m. 22, beat 3 to m. 22, beat 4 | Bb major | Eb6 | 4 | F2 | 5 |
| m. 22, beat 4 to m. 23, beat 1 | Bb major | D6 | 3 | F3 | 5 |
| m. 23, beat 1 to m. 23, beat 2 | Bb major | D6 | 3 | Bb2 | 1 |
| m. 23, beat 2 to m. 23, beat 3 | Bb major | C6 | 2 | F3 | 5 |
| m. 23, beat 3 to m. 23, beat 4 | Bb major | D6 | 3 | D3 | 3 |
| m. 23, beat 4 to m. 24, beat 1 | Bb major | C6 | 2 | F3 | 5 |
| m. 24, beat 1 to m. 24, beat 2 | Bb major | G5 | 6 | Bb2 | 1 |
| m. 24, beat 2 to m. 24, beat 3 | Bb major | F5 | 5 | F3 | 5 |
| m. 24, beat 3 to m. 24, beat 4 | Bb major | F5 | 5 | D3 | 3 |
| m. 24, beat 4 to m. 25, beat 1 | Bb major | G5 | 6 | F3 | 5 |
| m. 25, beat 1 to m. 25, beat 2 | Bb major | G5 | 6 | Bb2 | 1 |
| m. 25, beat 2 to m. 25, beat 3 | Bb major | G5 | 6 | F3 | 5 |
| m. 25, beat 3 to m. 25, beat 4 | Bb major | F5 | 5 | D3 | 3 |
| m. 25, beat 4 to m. 26, beat 1 | Bb major | Gb5 | b6 | F3 | 5 |
| m. 26, beat 1 to m. 26, beat 3 | Bb major | Gb5 | b6 | F2 | 5 |
| m. 26, beat 3 to m. 26, beat 4 | Bb major | G5 | 6 | C3 | 2 |
| m. 26, beat 4 to m. 27, beat 1 | Bb major | C6 | 2 | F3 | 5 |
| m. 27, beat 1 to m. 27, beat 2 | Bb major | C6 | 2 | Bb2 | 1 |
| m. 27, beat 2 to m. 27, beat 3 | Bb major | C6 | 2 | F3 | 5 |
| m. 27, beat 3 to m. 27, beat 4 | Bb major | Bb5 | 1 | D3 | 3 |
| m. 27, beat 4 to m. 28, beat 1 | Bb major | Bb5 | 1 | F3 | 5 |
| m. 28, beat 1 to m. 28, beat 2 | F minor | F5 | 1 | Bb2 | 4 |
| m. 28, beat 2 to m. 28, beat 3 | F minor | F5 | 1 | Bb2 | 4 |
| m. 28, beat 3 to m. 28, beat 3.33 | F minor | F5 | 1 | Ab2 | 3 |
| m. 28, beat 3.33 to m. 28, beat 4 | F minor | F5 | 1 | F3 | 1 |
| m. 28, beat 4 to m. 29, beat 1 | F minor | G5 | 2 | G2 | 2 |
| m. 29, beat 1 to m. 29, beat 2 | F minor | Ab5 | 3 | F2 | 1 |
| m. 29, beat 2 to m. 29, beat 3 | F minor | Ab5 | 3 | rest (register guard) | - |
| m. 29, beat 3 to m. 29, beat 4 | F minor | Ab5 | 3 | F3 | 1 |
| m. 29, beat 4 to m. 30, beat 1 | F minor | Ab5 | 3 | Ab3 | 3 |
| m. 30, beat 1 to m. 30, beat 2 | F minor | G5 | 2 | C3 | 5 |
| m. 30, beat 2 to m. 30, beat 3 | F minor | G5 | 2 | Bb3 | 4 |
| m. 30, beat 3 to m. 30, beat 4 | F minor | G5 | 2 | C3 | 5 |
| m. 30, beat 4 to m. 31, beat 1 | F minor | Ab5 | 3 | Bb3 | 4 |
| m. 31, beat 1 to m. 31, beat 2 | F minor | Ab5 | 3 | F2 | 1 |
| m. 31, beat 2 to m. 31, beat 3 | F minor | G5 | 2 | rest (register guard) | - |
| m. 31, beat 3 to m. 31, beat 4 | F minor | rest (register guard) | - | F3 | 1 |
| m. 31, beat 4 to m. 32, beat 1 | F minor | F4 | 1 | Ab3 | 3 |
| m. 32, beat 1 to m. 32, beat 2 | F minor | D4 | #6 | Bb2 | 4 |
| m. 32, beat 2 to m. 32, beat 3 | F minor | D4 | #6 | Ab3 | 3 |
| m. 32, beat 3 to m. 32, beat 4 | F minor | G5 | 2 | Bb2 | 4 |
| m. 32, beat 4 to m. 33, beat 1 | F minor | G5 | 2 | Ab3 | 3 |
| m. 33, beat 1 to m. 33, beat 2 | Eb major | F5 | 2 | Eb3 | 1 |
| m. 33, beat 2 to m. 33, beat 3 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 33, beat 3 to m. 33, beat 4 | Eb major | Eb5 | 1 | Ab3 | 4 |
| m. 33, beat 4 to m. 34, beat 1 | Eb major | Eb5 | 1 | C4 | 6 |
| m. 34, beat 1 to m. 34, beat 2 | Eb major | D5 | 7 | Bb2 | 5 |
| m. 34, beat 2 to m. 34, beat 3 | Eb major | Eb5 | 1 | Ab3 | 4 |
| m. 34, beat 3 to m. 34, beat 4 | Eb major | F5 | 2 | Bb2 | 5 |
| m. 34, beat 4 to m. 35, beat 1 | Eb major | G5 | 3 | Ab3 | 4 |
| m. 35, beat 1 to m. 35, beat 2 | Eb major | G5 | 3 | Eb3 | 1 |
| m. 35, beat 2 to m. 35, beat 3 | Eb major | Eb5 | 1 | Bb2 | 5 |
| m. 35, beat 3 to m. 35, beat 4 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 35, beat 4 to m. 36, beat 1 | Eb major | Eb5 | 1 | Bb2 | 5 |
| m. 36, beat 1 to m. 36, beat 2 | Eb major | Ab5 | 4 | F3 | 2 |
| m. 36, beat 2 to m. 36, beat 3 | Eb major | Ab5 | 4 | Bb2 | 5 |
| m. 36, beat 3 to m. 36, beat 4 | Eb major | Ab5 | 4 | F3 | 2 |
| m. 36, beat 4 to m. 37, beat 1 | Eb major | Ab5 | 4 | Bb2 | 5 |
| m. 37, beat 1 to m. 37, beat 2 | Eb major | Eb6 | 1 | Eb3 | 1 |
| m. 37, beat 2 to m. 37, beat 3 | Eb major | Eb6 | 1 | Bb2 | 5 |
| m. 37, beat 3 to m. 37, beat 4 | Eb major | Eb6 | 1 | Eb3 | 1 |
| m. 37, beat 4 to m. 38, beat 1 | Eb major | Eb6 | 1 | Bb2 | 5 |
| m. 38, beat 1 to m. 38, beat 2 | Eb major | Ab6 | 4 | F3 | 2 |
| m. 38, beat 2 to m. 38, beat 3 | Eb major | F6 | 2 | Bb2 | 5 |
| m. 38, beat 3 to m. 38, beat 4 | Eb major | C6 | 6 | F3 | 2 |
| m. 38, beat 4 to m. 39, beat 1 | Eb major | A5 | #4 | Bb2 | 5 |
| m. 39, beat 1 to m. 39, beat 2 | Eb major | F5 | 2 | Eb3 | 1 |
| m. 39, beat 2 to m. 39, beat 3 | Eb major | G5 | 3 | Bb2 | 5 |
| m. 39, beat 3 to m. 39, beat 4 | Eb major | G5 | 3 | Eb3 | 1 |
| m. 39, beat 4 to m. 40, beat 1 | Eb major | F5 | 2 | Bb2 | 5 |
| m. 40, beat 1 to m. 40, beat 2 | Eb major | G5 | 3 | A2 | #4 |
| m. 40, beat 2 to m. 40, beat 3 | Eb major | F5 | 2 | A3 | #4 |
| m. 40, beat 3 to m. 40, beat 4 | Eb major | G5 | 3 | Ab3 | 4 |
| m. 40, beat 4 to m. 41, beat 1 | Eb major | C6 | 6 | Bb3 | 5 |
| m. 41, beat 1 to m. 41, beat 2 | Eb major | C6 | 6 | G3 | 3 |
| m. 41, beat 2 to m. 41, beat 3 | Eb major | Bb5 | 5 | Gb3 | b3 |
| m. 41, beat 3 to m. 41, beat 4 | Eb major | Bb5 | 5 | F3 | 2 |
| m. 41, beat 4 to m. 41, beat 4.33 | Eb major | Ab5 | 4 | C4 | 6 |
| m. 41, beat 4.33 to m. 42, beat 1 | Eb major | Ab5 | 4 | Ab3 | 4 |
| m. 42, beat 1 to m. 42, beat 2 | Eb major | Eb6 | 1 | Bb2 | 5 |
| m. 42, beat 2 to m. 42, beat 3 | Eb major | Eb6 | 1 | G3 | 3 |
| m. 42, beat 3 to m. 42, beat 4 | Eb major | C6 | 6 | Bb2 | 5 |
| m. 42, beat 4 to m. 43, beat 1 | Eb major | Ab5 | 4 | Ab3 | 4 |
| m. 43, beat 1 to m. 43, beat 2 | Eb major | G6 | 3 | Eb3 | 1 |
| m. 43, beat 2 to m. 43, beat 3 | Eb major | G6 | 3 | Bb3 | 5 |
| m. 43, beat 3 to m. 43, beat 4 | Eb major | Eb6 | 1 | Eb3 | 1 |
| m. 43, beat 4.07 to m. 44, beat 1 | Eb major | rest (register guard) | - | rest (register guard) | - |
| m. 44, beat 1 to m. 44, beat 2 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 44, beat 2 to m. 44, beat 3 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 44, beat 3 to m. 44, beat 4 | Eb major | F5 | 2 | C3 | 6 |
| m. 44, beat 4 to m. 45, beat 1 | Eb major | F5 | 2 | F3 | 2 |
| m. 45, beat 1 to m. 45, beat 2 | Eb major | D5 | 7 | Bb2 | 5 |
| m. 45, beat 2 to m. 45, beat 3 | Eb major | C5 | 6 | Ab3 | 4 |
| m. 45, beat 3 to m. 45, beat 4 | Eb major | D5 | 7 | F3 | 2 |
| m. 45, beat 4 to m. 45, beat 4.33 | Eb major | D5 | 7 | Ab3 | 4 |
| m. 45, beat 4.33 to m. 46, beat 1 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 46, beat 1 to m. 46, beat 2 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 46, beat 2 to m. 46, beat 4 | Eb major | G5 | 3 | Bb3 | 5 |
| m. 46, beat 4 to m. 47, beat 1 | Eb major | Bb5 | 5 | G3 | 3 |
| m. 47, beat 1 to m. 47, beat 2 | Eb major | Bb5 | 5 | Eb3 | 1 |
| m. 47, beat 2 to m. 47, beat 3 | Eb major | Bb5 | 5 | Bb3 | 5 |
| m. 47, beat 3 to m. 47, beat 4 | Eb major | Bb5 | 5 | G3 | 3 |
| m. 47, beat 4 to m. 48, beat 1 | Eb major | A5 | #4 | Bb3 | 5 |
| m. 48, beat 1 to m. 48, beat 2 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 48, beat 2 to m. 48, beat 3 | Eb major | Eb6 | 1 | Bb3 | 5 |
| m. 48, beat 3 to m. 48, beat 4 | Eb major | G6 | 3 | G3 | 3 |
| m. 48, beat 4 to m. 49, beat 1 | Eb major | D6 | 7 | Bb3 | 5 |
| m. 49, beat 1 to m. 49, beat 2 | Eb major | Eb5 | 1 | Bb2 | 5 |
| m. 49, beat 2 to m. 49, beat 3 | Eb major | Bb5 | 5 | Bb3 | 5 |
| m. 49, beat 3 to m. 49, beat 4 | Eb major | D6 | 7 | F3 | 2 |
| m. 49, beat 4 to m. 50, beat 1 | Eb major | Bb5 | 5 | Bb3 | 5 |
| m. 50, beat 1 to m. 50, beat 2 | Eb major | G5 | 3 | Eb3 | 1 |
| m. 50, beat 2 to m. 50, beat 3 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 50, beat 3 to m. 50, beat 4 | Eb major | G4 | 3 | G3 | 3 |
| m. 50, beat 4 to m. 51, beat 1 | Eb major | G4 | 3 | Bb3 | 5 |
| m. 51, beat 1 to m. 51, beat 3 | Eb major | F4 | 2 | G2 | 3 |
| m. 51, beat 3 to m. 51, beat 4 | Eb major | Eb5 | 1 | D3 | 7 |
| m. 51, beat 4 to m. 52, beat 1 | Eb major | Eb5 | 1 | G3 | 3 |
| m. 52, beat 1 to m. 52, beat 2 | Eb major | D5 | 7 | Ab2 | 4 |
| m. 52, beat 2 to m. 52, beat 3 | Eb major | C5 | 6 | Ab3 | 4 |
| m. 52, beat 3 to m. 52, beat 4 | Eb major | C5 | 6 | Eb3 | 1 |
| m. 52, beat 4 to m. 53, beat 1 | Eb major | B4 | b6 | Ab3 | 4 |
| m. 53, beat 1 to m. 53, beat 2 | Eb major | C5 | 6 | Bb2 | 5 |
| m. 53, beat 2 to m. 53, beat 3 | Eb major | Ab5 | 4 | Ab3 | 4 |
| m. 53, beat 3 to m. 53, beat 4 | Eb major | Ab5 | 4 | Bb2 | 5 |
| m. 53, beat 4 to m. 54, beat 1 | Eb major | G5 | 3 | Ab3 | 4 |
| m. 54, beat 1 to m. 54, beat 2 | Eb major | G5 | 3 | Eb3 | 1 |
| m. 54, beat 2 to m. 54, beat 3 | Eb major | F5 | 2 | Bb3 | 5 |
| m. 54, beat 3 to m. 54, beat 4 | Eb major | Eb5 | 1 | G3 | 3 |
| m. 54, beat 4 to m. 55, beat 1 | Eb major | G4 | 3 | Bb3 | 5 |
| m. 55, beat 1 to m. 55, beat 3 | Eb major | G5 | 3 | G2 | 3 |
| m. 55, beat 3 to m. 55, beat 4 | Eb major | F5 | 2 | D3 | 7 |
| m. 55, beat 4 to m. 56, beat 1 | Eb major | F5 | 2 | G3 | 3 |
| m. 56, beat 1 to m. 56, beat 2 | Eb major | D5 | 7 | Ab2 | 4 |
| m. 56, beat 2 to m. 56, beat 3 | Eb major | Ab5 | 4 | Ab3 | 4 |
| m. 56, beat 3 to m. 56, beat 4 | Eb major | C6 | 6 | Eb3 | 1 |
| m. 56, beat 4 to m. 57, beat 1 | Eb major | F5 | 2 | Ab3 | 4 |
| m. 57, beat 1 to m. 57, beat 1.33 | Eb major | B4 | b6 | Bb2 | 5 |
| m. 57, beat 1.33 to m. 57, beat 2 | Eb major | C5 | 6 | Bb3 | 5 |
| m. 57, beat 2 to m. 57, beat 3 | Eb major | C6 | 6 | Ab3 | 4 |
| m. 57, beat 3 to m. 57, beat 4 | Eb major | C6 | 6 | Bb2 | 5 |
| m. 57, beat 4 to m. 58, beat 1 | Eb major | C6 | 6 | Ab3 | 4 |
| m. 58, beat 1 to m. 58, beat 2 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 58, beat 2 to m. 58, beat 3 | Eb major | Eb5 | 1 | Bb3 | 5 |
| m. 58, beat 3 to m. 58, beat 4 | Eb major | G4 | 3 | G3 | 3 |
| m. 58, beat 4 to m. 59, beat 1 | Eb major | G4 | 3 | Bb3 | 5 |
| m. 59, beat 1 to m. 59, beat 2 | Eb major | C6 | 6 | Bb2 | 5 |
| m. 59, beat 2 to m. 59, beat 3 | Eb major | B5 | b6 | Bb3 | 5 |
| m. 59, beat 3 to m. 59, beat 4 | Eb major | Bb5 | 5 | F3 | 2 |
| m. 59, beat 4 to m. 60, beat 1 | Eb major | Ab5 | 4 | Bb3 | 5 |
| m. 60, beat 1 to m. 60, beat 2 | Eb major | G5 | 3 | G3 | 3 |
| m. 60, beat 2 to m. 60, beat 3 | Eb major | Eb6 | 1 | Bb3 | 5 |
| m. 60, beat 3 to m. 60, beat 4 | Eb major | G6 | 3 | G3 | 3 |
| m. 60, beat 4 to m. 61, beat 1 | Eb major | F6 | 2 | Bb3 | 5 |
| m. 61, beat 1 to m. 61, beat 2 | Eb major | C6 | 6 | Bb2 | 5 |
| m. 61, beat 2 to m. 61, beat 3 | Eb major | B5 | b6 | Bb3 | 5 |
| m. 61, beat 3 to m. 61, beat 4 | Eb major | Bb5 | 5 | F3 | 2 |
| m. 61, beat 4 to m. 62, beat 1 | Eb major | Ab5 | 4 | Bb3 | 5 |
| m. 62, beat 1 to m. 62, beat 2 | Eb major | F5 | 2 | Eb3 | 1 |
| m. 62, beat 2 to m. 62, beat 3 | Eb major | G5 | 3 | Bb3 | 5 |
| m. 62, beat 3 to m. 62, beat 4 | Eb major | G5 | 3 | G3 | 3 |
| m. 62, beat 4 to m. 63, beat 1 | Eb major | G4 | 3 | Bb3 | 5 |
| m. 63, beat 1 to m. 63, beat 2 | Eb major | F5 | 2 | A3 | #4 |
| m. 63, beat 2 to m. 63, beat 3 | Eb major | G5 | 3 | Ab3 | 4 |
| m. 63, beat 3 to m. 63, beat 4 | Eb major | Ab5 | 4 | Ab3 | 4 |
| m. 63, beat 4 to m. 64, beat 1 | Eb major | A5 | #4 | Bb3 | 5 |
| m. 64, beat 1 to m. 64, beat 2 | Eb major | Bb5 | 5 | G3 | 3 |
| m. 64, beat 2 to m. 64, beat 3 | Eb major | Db6 | b7 | G3 | 3 |
| m. 64, beat 3 to m. 64, beat 4 | Eb major | Db6 | b7 | Ab3 | 4 |
| m. 64, beat 4 to m. 65, beat 1 | Eb major | Bb5 | 5 | Ab3 | 4 |
| m. 65, beat 1 to m. 65, beat 1.33 | Eb major | Bb6 | 5 | Bb2 | 5 |
| m. 65, beat 1.33 to m. 65, beat 2 | Eb major | Bb6 | 5 | Bb3 | 5 |
| m. 65, beat 2 to m. 65, beat 3 | Eb major | Bb6 | 5 | G3 | 3 |
| m. 65, beat 3 to m. 65, beat 4 | Eb major | D6 | 7 | Bb2 | 5 |
| m. 65, beat 4 to m. 66, beat 1 | Eb major | Bb5 | 5 | F3 | 2 |
| m. 66, beat 1 to m. 66, beat 2 | Eb major | Eb5 | 1 | Eb3 | 1 |
| m. 66, beat 2 to m. 66, beat 3 | Eb major | G6 | 3 | Bb3 | 5 |
| m. 66, beat 3 to m. 66, beat 4 | Eb major | G6 | 3 | Eb3 | 1 |
| m. 66, beat 4 to m. 67, beat 1 | Eb major | G6 | 3 | Bb3 | 5 |
| m. 67, beat 1 to m. 67, beat 2 | Eb major | Eb6 | 1 | Eb3 | 1 |
| m. 67, beat 2 to m. 67, beat 3 | Eb major | Eb6 | 1 | Bb3 | 5 |
| m. 67, beat 3 to m. 67, beat 4 | Eb major | Eb6 | 1 | Eb3 | 1 |
## Non-chord tones

207 notes sound against a chord without being one of its tones; 131 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 2, beat 4 to m. 3, beat 1 | 2 | suspension | minor7 |
| m. 2, beat 4 to m. 3, beat 1 | 14 | unclassified | minor7 |
| m. 2, beat 4 to m. 3, beat 1 | 15 | unclassified | minor7 |
| m. 3, beat 1 to m. 3, beat 2 | 15 | suspension | V |
| m. 3, beat 1 to m. 3, beat 2 | 20 | unclassified | V |
| m. 3, beat 2 to m. 3, beat 3 | 23 | anticipation | half-diminished7 |
| m. 3, beat 3 to m. 3, beat 4 | 20 | suspension | V |
| m. 6, beat 4 to m. 7, beat 1 | 59 | suspension | minor7 |
| m. 6, beat 4 to m. 7, beat 1 | 72 | unclassified | minor7 |
| m. 7, beat 1 to m. 7, beat 2 | 77 | unclassified | V |
| m. 7, beat 2 to m. 7, beat 3 | 80 | anticipation | half-diminished7 |
| m. 7, beat 2 to m. 7, beat 3 | 81 | anticipation | half-diminished7 |
| m. 10, beat 1 to m. 10, beat 2 | 119 | anticipation | vii° |
| m. 10, beat 2 to m. 10, beat 3 | 117 | unclassified | IV |
| m. 11, beat 1 to m. 11, beat 2 | 128 | unclassified | dominant7 |
| m. 11, beat 1 to m. 11, beat 2 | 134 | unclassified | dominant7 |
| m. 11, beat 1 to m. 11, beat 2 | 137 | unclassified | dominant7 |
| m. 11, beat 2 to m. 11, beat 3 | 137 | suspension | dominant7 |
| m. 11, beat 3 to m. 11, beat 4 | 146 | anticipation | vii° |
| m. 11, beat 4 to m. 12, beat 1 | 148 | unclassified | iii |
| m. 12, beat 1 to m. 12, beat 2 | 151 | passing | I |
| m. 12, beat 2 to m. 12, beat 3 | 151 | unclassified | I |
| m. 13, beat 2 to m. 13, beat 3 | 171 | anticipation | minor7 |
| m. 13, beat 3 to m. 13, beat 4 | 173 | unclassified | I |
| m. 14, beat 1 to m. 14, beat 2 | 184 | anticipation | half-diminished7 |
| m. 14, beat 2 to m. 14, beat 3 | 182 | unclassified | IV |
| m. 15, beat 1 to m. 15, beat 2 | 199 | escape-tone | V |
| m. 15, beat 1 to m. 15, beat 2 | 202 | escape-tone | V |
| m. 15, beat 2 to m. 15, beat 3 | 203 | unclassified | half-diminished7 |
| m. 15, beat 2 to m. 15, beat 3 | 207 | unclassified | half-diminished7 |
| m. 15, beat 3 to m. 15, beat 4 | 210 | anticipation | half-diminished7 |
| m. 15, beat 4 to m. 16, beat 1 | 205 | unclassified | dominant7 |
| m. 16, beat 4 to m. 17, beat 1 | 227 | escape-tone | I |
| m. 17, beat 1 to m. 17, beat 2 | 229 | unclassified | half-diminished7 |
| m. 17, beat 2 to m. 17, beat 3 | 235 | unclassified | dominant7 |
| m. 17, beat 3 to m. 17, beat 4 | 243 | anticipation | minor7 |
| m. 17, beat 4 to m. 18, beat 1 | 242 | suspension | dominant7 |
| m. 18, beat 1 to m. 18, beat 2 | 247 | unclassified | major7 |
| m. 18, beat 1 to m. 18, beat 2 | 252 | unclassified | major7 |
| m. 18, beat 4 to m. 19, beat 1 | 268 | unclassified | I |
| m. 19, beat 1 to m. 19, beat 2 | 270 | unclassified | vii° |
| m. 19, beat 2 to m. 19, beat 3 | 275 | unclassified | half-diminished7 |
| m. 19, beat 3 to m. 19, beat 4 | 280 | anticipation | half-diminished7 |
| m. 19, beat 3 to m. 19, beat 4 | 282 | anticipation | half-diminished7 |
| m. 20, beat 1 to m. 20, beat 2 | 288 | unclassified | I |
| m. 20, beat 3 to m. 20, beat 4 | 300 | unclassified | I |
| m. 21, beat 1 to m. 21, beat 2 | 303 | unclassified | I |
| m. 22, beat 1 to m. 22, beat 2 | 321 | unclassified | dominant7 |
| m. 22, beat 1 to m. 22, beat 2 | 324 | escape-tone | dominant7 |
| m. 22, beat 3 to m. 22, beat 4 | 333 | unclassified | dominant7 |
| m. 22, beat 4 to m. 23, beat 1 | 335 | unclassified | minor7 |
| m. 23, beat 1 to m. 23, beat 2 | 333 | unclassified | minor7 |
| m. 23, beat 1 to m. 23, beat 2 | 339 | anticipation | minor7 |
| m. 23, beat 2 to m. 23, beat 3 | 337 | unclassified | I |
| m. 23, beat 3 to m. 23, beat 4 | 348 | unclassified | I |
| m. 23, beat 4 to m. 24, beat 1 | 348 | unclassified | major7 |
| m. 23, beat 4 to m. 24, beat 1 | 354 | anticipation | major7 |
| m. 24, beat 1 to m. 24, beat 2 | 356 | unclassified | minor7 |
| m. 24, beat 4 to m. 25, beat 1 | 371 | passing | minor7 |
| m. 25, beat 1 to m. 25, beat 2 | 371 | unclassified | minor7 |
| m. 25, beat 4 to m. 26, beat 1 | 391 | neighbor | I |
| m. 26, beat 1 to m. 26, beat 3 | 391 | unclassified | dominant7 |
| m. 26, beat 3 to m. 26, beat 4 | 400 | unclassified | dominant7 |
| m. 26, beat 3 to m. 26, beat 4 | 403 | unclassified | dominant7 |
| m. 27, beat 1 to m. 27, beat 2 | 413 | anticipation | minor7 |
| m. 27, beat 2 to m. 27, beat 3 | 411 | unclassified | I |
| m. 33, beat 1 to m. 33, beat 2 | 498 | unclassified | I |
| m. 33, beat 4 to m. 34, beat 1 | 512 | unclassified | vi |
| m. 34, beat 1 to m. 34, beat 2 | 511 | unclassified | V |
| m. 34, beat 2 to m. 34, beat 3 | 519 | passing | dominant7 |
| m. 34, beat 3 to m. 34, beat 4 | 519 | unclassified | V |
| m. 34, beat 4 to m. 35, beat 1 | 526 | passing | minor7 |
| m. 35, beat 4 to m. 36, beat 1 | 545 | unclassified | major7 |
| m. 36, beat 1 to m. 36, beat 2 | 545 | unclassified | dominant7 |
| m. 36, beat 4 to m. 37, beat 1 | 559 | escape-tone | dominant7 |
| m. 36, beat 4 to m. 37, beat 1 | 563 | anticipation | dominant7 |
| m. 37, beat 4 to m. 38, beat 1 | 579 | unclassified | major7 |
| m. 38, beat 1 to m. 38, beat 2 | 585 | unclassified | dominant7 |
| m. 38, beat 2 to m. 38, beat 3 | 588 | unclassified | dominant7 |
| m. 38, beat 2 to m. 38, beat 3 | 592 | unclassified | dominant7 |
| m. 38, beat 3 to m. 38, beat 4 | 594 | passing | half-diminished7 |
| m. 38, beat 3 to m. 38, beat 4 | 596 | unclassified | half-diminished7 |
| m. 38, beat 4 to m. 39, beat 1 | 596 | unclassified | dominant7 |
| m. 38, beat 4 to m. 39, beat 1 | 600 | passing | dominant7 |
| m. 38, beat 4 to m. 39, beat 1 | 604 | anticipation | dominant7 |
| m. 39, beat 1 to m. 39, beat 2 | 602 | unclassified | major7 |
| m. 39, beat 1 to m. 39, beat 2 | 607 | unclassified | major7 |
| m. 39, beat 4 to m. 40, beat 1 | 621 | unclassified | major7 |
| m. 40, beat 2 to m. 40, beat 3 | 632 | unclassified | minor7 |
| m. 40, beat 2 to m. 40, beat 3 | 634 | unclassified | minor7 |
| m. 40, beat 3 to m. 40, beat 4 | 637 | passing | vii° |
| m. 40, beat 3 to m. 40, beat 4 | 641 | neighbor | vii° |
| m. 40, beat 4 to m. 41, beat 1 | 644 | unclassified | V |
| m. 40, beat 4 to m. 41, beat 1 | 648 | anticipation | V |
| m. 41, beat 3 to m. 41, beat 4 | 658 | anticipation | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 660 | passing | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 663 | passing | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 664 | unclassified | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 665 | anticipation | dominant7 |
| m. 41, beat 4.33 to m. 42, beat 1 | 672 | passing | ii |
| m. 41, beat 4.33 to m. 42, beat 1 | 674 | anticipation | ii |
| m. 41, beat 4.33 to m. 42, beat 1 | 675 | anticipation | ii |
| m. 42, beat 2 to m. 42, beat 3 | 688 | unclassified | major7 |
| m. 42, beat 3 to m. 42, beat 4 | 691 | unclassified | iii |
| m. 42, beat 3 to m. 42, beat 4 | 695 | anticipation | iii |
| m. 42, beat 4 to m. 43, beat 1 | 693 | unclassified | dominant7 |
| m. 42, beat 4 to m. 43, beat 1 | 697 | unclassified | dominant7 |
| m. 44, beat 3 to m. 44, beat 4 | 724 | unclassified | vi |
| m. 44, beat 3 to m. 44, beat 4 | 725 | unclassified | vi |
| m. 44, beat 4 to m. 45, beat 1 | 716 | unclassified | vii° |
| m. 44, beat 4 to m. 45, beat 1 | 727 | unclassified | vii° |
| m. 44, beat 4 to m. 45, beat 1 | 728 | unclassified | vii° |
| m. 44, beat 4 to m. 45, beat 1 | 729 | unclassified | vii° |
| m. 45, beat 1 to m. 45, beat 2 | 734 | anticipation | V |
| m. 45, beat 2 to m. 45, beat 3 | 737 | anticipation | half-diminished7 |
| m. 45, beat 3 to m. 45, beat 4 | 734 | suspension | V |
| m. 45, beat 4.33 to m. 46, beat 1 | 746 | unclassified | V |
| m. 47, beat 3 to m. 47, beat 4 | 770 | passing | I |
| m. 47, beat 3 to m. 47, beat 4 | 772 | passing | I |
| m. 47, beat 4 to m. 48, beat 1 | 770 | unclassified | I |
| m. 47, beat 4 to m. 48, beat 1 | 772 | unclassified | I |
| m. 47, beat 4 to m. 48, beat 1 | 776 | unclassified | I |
| m. 48, beat 3 to m. 48, beat 4 | 790 | escape-tone | major7 |
| m. 48, beat 4 to m. 49, beat 1 | 793 | unclassified | minor7 |
| m. 48, beat 4 to m. 49, beat 1 | 797 | unclassified | minor7 |
| m. 48, beat 4 to m. 49, beat 1 | 799 | unclassified | minor7 |
| m. 48, beat 4 to m. 49, beat 1 | 802 | unclassified | minor7 |
| m. 49, beat 1 to m. 49, beat 2 | 805 | anticipation | major7 |
| m. 49, beat 1 to m. 49, beat 2 | 807 | appoggiatura | major7 |
| m. 49, beat 2 to m. 49, beat 3 | 810 | unclassified | dominant7 |
| m. 49, beat 2 to m. 49, beat 3 | 812 | unclassified | dominant7 |
| m. 49, beat 2 to m. 49, beat 3 | 815 | unclassified | dominant7 |
| m. 49, beat 3 to m. 49, beat 4 | 822 | neighbor | dominant7 |
| m. 49, beat 4 to m. 50, beat 1 | 826 | anticipation | dominant7 |
| m. 49, beat 4 to m. 50, beat 1 | 832 | anticipation | dominant7 |
| m. 49, beat 4 to m. 50, beat 1 | 835 | unclassified | dominant7 |
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
| m. 53, beat 4 to m. 54, beat 1 | 898 | unclassified | iii |
| m. 54, beat 1 to m. 54, beat 2 | 902 | unclassified | I |
| m. 54, beat 2 to m. 54, beat 3 | 902 | unclassified | I |
| m. 55, beat 1 to m. 55, beat 3 | 918 | unclassified | minor7 |
| m. 55, beat 1 to m. 55, beat 3 | 922 | unclassified | minor7 |
| m. 55, beat 3 to m. 55, beat 4 | 926 | unclassified | vii° |
| m. 55, beat 3 to m. 55, beat 4 | 927 | unclassified | vii° |
| m. 55, beat 4 to m. 56, beat 1 | 923 | unclassified | major7 |
| m. 55, beat 4 to m. 56, beat 1 | 929 | unclassified | major7 |
| m. 55, beat 4 to m. 56, beat 1 | 931 | unclassified | major7 |
| m. 56, beat 1 to m. 56, beat 2 | 933 | unclassified | IV |
| m. 56, beat 3 to m. 56, beat 4 | 945 | escape-tone | major7 |
| m. 56, beat 4 to m. 57, beat 1 | 952 | unclassified | minor7 |
| m. 56, beat 4 to m. 57, beat 1 | 956 | escape-tone | minor7 |
| m. 56, beat 4 to m. 57, beat 1 | 957 | unclassified | minor7 |
| m. 57, beat 1 to m. 57, beat 1.33 | 957 | unclassified | V |
| m. 57, beat 1.33 to m. 57, beat 2 | 961 | escape-tone | V |
| m. 57, beat 1.33 to m. 57, beat 2 | 964 | unclassified | V |
| m. 57, beat 2 to m. 57, beat 3 | 968 | unclassified | IV |
| m. 57, beat 2 to m. 57, beat 3 | 969 | anticipation | IV |
| m. 57, beat 3 to m. 57, beat 4 | 971 | unclassified | minor7 |
| m. 57, beat 4 to m. 58, beat 1 | 967 | unclassified | dominant7 |
| m. 59, beat 1 to m. 59, beat 2 | 991 | appoggiatura | dominant7 |
| m. 59, beat 1 to m. 59, beat 2 | 995 | unclassified | dominant7 |
| m. 59, beat 2 to m. 59, beat 3 | 997 | unclassified | dominant7 |
| m. 60, beat 1 to m. 60, beat 2 | 1012 | unclassified | I |
| m. 60, beat 4 to m. 61, beat 1 | 1035 | passing | major7 |
| m. 60, beat 4 to m. 61, beat 1 | 1039 | unclassified | major7 |
| m. 60, beat 4 to m. 61, beat 1 | 1043 | unclassified | major7 |
| m. 61, beat 1 to m. 61, beat 2 | 1043 | suspension | dominant7 |
| m. 61, beat 1 to m. 61, beat 2 | 1045 | unclassified | dominant7 |
| m. 61, beat 1 to m. 61, beat 2 | 1049 | passing | dominant7 |
| m. 61, beat 1 to m. 61, beat 2 | 1051 | passing | dominant7 |
| m. 61, beat 2 to m. 61, beat 3 | 1051 | suspension | dominant7 |
| m. 62, beat 1 to m. 62, beat 2 | 1067 | unclassified | major7 |
| m. 62, beat 1 to m. 62, beat 2 | 1072 | unclassified | major7 |
| m. 63, beat 2 to m. 63, beat 3 | 1098 | passing | minor7 |
| m. 63, beat 3 to m. 63, beat 4 | 1098 | unclassified | vii° |
| m. 63, beat 3 to m. 63, beat 4 | 1101 | passing | vii° |
| m. 63, beat 3 to m. 63, beat 4 | 1102 | passing | vii° |
| m. 63, beat 4 to m. 64, beat 1 | 1109 | passing | dominant7 |
| m. 64, beat 1 to m. 64, beat 2 | 1109 | unclassified | I |
| m. 64, beat 2 to m. 64, beat 3 | 1117 | unclassified | iii |
| m. 64, beat 2 to m. 64, beat 3 | 1118 | passing | iii |
| m. 64, beat 2 to m. 64, beat 3 | 1119 | appoggiatura | iii |
| m. 64, beat 3 to m. 64, beat 4 | 1119 | unclassified | ii |
| m. 64, beat 3 to m. 64, beat 4 | 1121 | neighbor | ii |
| m. 64, beat 3 to m. 64, beat 4 | 1125 | appoggiatura | ii |
| m. 64, beat 4 to m. 65, beat 1 | 1125 | unclassified | ii |
| m. 64, beat 4 to m. 65, beat 1 | 1127 | escape-tone | ii |
| m. 64, beat 4 to m. 65, beat 1 | 1132 | unclassified | ii |
| m. 65, beat 2 to m. 65, beat 3 | 1138 | unclassified | I |
| m. 65, beat 2 to m. 65, beat 3 | 1139 | unclassified | I |
| m. 65, beat 2 to m. 65, beat 3 | 1142 | unclassified | I |
| m. 65, beat 3 to m. 65, beat 4 | 1147 | unclassified | dominant7 |
| m. 65, beat 3 to m. 65, beat 4 | 1150 | unclassified | dominant7 |
| m. 65, beat 3 to m. 65, beat 4 | 1153 | appoggiatura | dominant7 |
| m. 65, beat 4 to m. 66, beat 1 | 1157 | unclassified | dominant7 |
| m. 65, beat 4 to m. 66, beat 1 | 1160 | unclassified | dominant7 |
