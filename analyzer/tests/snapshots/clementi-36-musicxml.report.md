# Analysis Report: clementi-36.musicxml

## Summary

- **File:** clementi-36.musicxml, 333 notes.
- **Input:** MusicXML score, 1 part merged into one texture. Meter: notated. Key signature: notated. Pitch spelling: notated.
- **Key:** C major, the key with the most time across 5 key regions.
- **Chords:** 205 chord events; 188 matched a chord (92%), 17 did not.
- **Most frequent chords:** I (47), V (33), iii (25).

## Meter

- **Time signature:** 4/4 (notated)
- **Measures:** 38 measures.
- **Pickup:** None.

## Key regions

Positions are measure and beat. Uncertainty is the library's, from 0 (clearly this key) to 1 (another key fits as well or better). The home key is the key with the most time across its regions.

- **Home key:** C major.
- **Opening key:** C major.
- **Closing key:** C major.
- **Notated key signatures:** C major or A minor at m. 1, beat 1.

| Position | Key | Uncertainty | Set by |
| --- | --- | --- | --- |
| m. 1, beat 1 to m. 8, beat 1 | C major | 0.80 | key signature |
| m. 8, beat 1 to m. 16, beat 1 | G major | 0.77 | profile |
| m. 16, beat 1 to m. 20, beat 1 | C minor | 0.82 | profile |
| m. 20, beat 1 to m. 24, beat 1 | G major | 0.89 | profile |
| m. 24, beat 1 to m. 39, beat 1 | C major | 0.70 | profile |

## Modulations

A modulation is a lasting new key. Position is where the new key arrives. Type is pivot when the library found a pivot chord and direct when it did not. A return goes back to the key before an excursion. The library gives the change's extent from the length of the new key and whether it returns; no cadence is looked for.

| Position | From | To | Extent | Type | Return |
| --- | --- | --- | --- | --- | --- |
| m. 8, beat 1 | C major | G major | modulation | pivot | no |
| m. 16, beat 1 | G major | C minor | weak-modulation | direct | no |
| m. 20, beat 1 | C minor | G major | weak-modulation | direct | yes |
| m. 24, beat 1 | G major | C major | modulation | pivot | no |

## Pivot chords

The chord at a modulation that is diatonic in both the old and the new key, named by root and quality, with no Roman numeral. Modulation is its row in the Modulations table, counting from 1.

| Position | Chord | Bass | Inversion | From | To | Modulation |
| --- | --- | --- | --- | --- | --- | --- |
| m. 8, beat 1 to m. 8, beat 2 | A minor7 | G | third | C major | G major | 1 |
| m. 24, beat 1 to m. 24, beat 2 | C major | C | root position | G major | C major | 4 |

## Chords

Positions are measure and beat. Numeral is the Roman numeral in the local key region, which is named where it changes. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Numeral | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 2 | I | I (in C major) | C | major | C | root position | 1 | 1.00 | - |
| m. 1, beat 2 to m. 1, beat 2.5 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 1, beat 2.5 to m. 1, beat 3 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 1, beat 3 to m. 2, beat 1 | V | V | G | major | G | root position | 2 | 1.00 | - |
| m. 2, beat 1 to m. 2, beat 2 | I | I | C | major | C | root position | 1 | 1.00 | - |
| m. 2, beat 2 to m. 2, beat 2.5 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 2, beat 2.5 to m. 2, beat 3 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 2, beat 3 to m. 3, beat 1 | V | V | G | major | G | root position | 2 | 1.00 | - |
| m. 3, beat 1 to m. 3, beat 2 | major7 | IV4/3 | F | major7 | C | second | 1 | 0.08 | I (C major, score 0.83) |
| m. 3, beat 2 to m. 3, beat 2.5 | ii | ii | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 3, beat 2.5 to m. 3, beat 3 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 3, beat 3 to m. 3, beat 4 | major7 | I7 | C | major7 | C | root position | 1 | 0.93 | - |
| m. 3, beat 4 to m. 3, beat 4.5 | vii° | vii° | B | diminished | B | root position | 0.5 | 1.00 | - |
| m. 3, beat 4.5 to m. 4, beat 1 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 4, beat 1 to m. 4, beat 2 | V | V | G | major | G | root position | 1 | 0.16 | - |
| m. 4, beat 2 to m. 4, beat 2.5 | vii° | vii° | B | diminished | B | root position | 0.5 | 1.00 | - |
| m. 4, beat 2.5 to m. 4, beat 3 | vi | vi | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 4, beat 3 to m. 4, beat 3.5 | V | V | G | major | G | root position | 0.5 | 1.00 | - |
| m. 4, beat 3.5 to m. 4, beat 4 | dominant7 | V4/2 | G | dominant7 | F | third | 0.5 | 0.56 | - |
| m. 4, beat 4 to m. 4, beat 4.5 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 4, beat 4.5 to m. 5, beat 1 | ii | ii | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 5, beat 1 to m. 5, beat 2 | I | I | C | major | C | root position | 1 | 1.00 | - |
| m. 5, beat 2 to m. 5, beat 2.5 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 5, beat 2.5 to m. 5, beat 3 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 5, beat 3 to m. 6, beat 1 | V | V | G | major | G | root position | 2 | 1.00 | - |
| m. 6, beat 1 to m. 6, beat 2 | I | I | C | major | C | root position | 1 | 0.76 | - |
| m. 6, beat 2 to m. 6, beat 2.5 | V | V | G | major | G | root position | 0.5 | 1.00 | - |
| m. 6, beat 2.5 to m. 6, beat 3 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 6, beat 3 to m. 6, beat 4 | I | I | C | major | C | root position | 1 | 1.00 | - |
| m. 6, beat 4 to m. 7, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - |
| m. 7, beat 1 to m. 7, beat 2 | V | V | G | major | G | root position | 1 | 0.71 | - |
| m. 7, beat 2 to m. 7, beat 3 | vi | vi6 | A | minor | C | first | 1 | 0.33 | - |
| m. 7, beat 3 to m. 7, beat 4 | V | V6/4 | G | major | D | second | 1 | 0.12 | - |
| m. 7, beat 4 to m. 8, beat 1 | ii | ii | D | minor | D | root position | 1 | 0.94 | - |
| m. 8, beat 1 to m. 8, beat 2 | minor7 | ii4/2 (in G major) | A | minor7 | G | third | 1 | 0.10 | I (G major, score 0.81) |
| m. 8, beat 2 to m. 8, beat 2.5 | iii | iii | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 8, beat 2.5 to m. 8, beat 3 | IV | IV | C | major | C | root position | 0.5 | 1.00 | - |
| m. 8, beat 3 to m. 8, beat 3.5 | V | V | D | major | D | root position | 0.5 | 1.00 | - |
| m. 8, beat 3.5 to m. 8, beat 4 | vi | vi | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 8, beat 4 to m. 8, beat 4.5 | vii° | vii° | F# | diminished | F# | root position | 0.5 | 1.00 | - |
| m. 8, beat 4.5 to m. 9, beat 1 | I | I | G | major | G | root position | 0.5 | 1.00 | - |
| m. 9, beat 1 to m. 9, beat 1.5 | vii° | vii° | F# | diminished | F# | root position | 0.5 | 0.74 | - |
| m. 9, beat 1.5 to m. 9, beat 2 | V | V | D | major | D | root position | 0.5 | 0.61 | - |
| m. 9, beat 2 to m. 9, beat 2.5 | ii | ii | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 9, beat 2.5 to m. 9, beat 3 | V | V | D | major | D | root position | 0.5 | 0.61 | - |
| m. 9, beat 3 to m. 9, beat 3.5 | vii° | vii° | F# | diminished | F# | root position | 0.5 | 0.74 | - |
| m. 9, beat 3.5 to m. 9, beat 4 | V | V | D | major | D | root position | 0.5 | 0.61 | - |
| m. 9, beat 4 to m. 9, beat 4.5 | ii | ii | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 9, beat 4.5 to m. 10, beat 1 | V | V | D | major | D | root position | 0.5 | 0.61 | - |
| m. 10, beat 1 to m. 10, beat 2 | I | I | G | major | G | root position | 1 | 0.06 | major7 (C major7, score 0.90) |
| m. 10, beat 2 to m. 10, beat 2.5 | V | V | D | major | D | root position | 0.5 | 1.00 | - |
| m. 10, beat 2.5 to m. 10, beat 3 | vi | vi | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 10, beat 3 to m. 10, beat 3.5 | vii° | vii° | F# | diminished | F# | root position | 0.5 | 1.00 | - |
| m. 10, beat 3.5 to m. 10, beat 4 | I | I | G | major | G | root position | 0.5 | 1.00 | - |
| m. 10, beat 4 to m. 10, beat 4.5 | ii | ii | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 10, beat 4.5 to m. 11, beat 1 | iii | iii | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 11, beat 1 to m. 11, beat 1.5 | ii | ii | A | minor | A | root position | 0.5 | 0.74 | - |
| m. 11, beat 1.5 to m. 11, beat 2 | dominant7 | V7 | D | dominant7 | D | root position | 0.5 | 0.61 | - |
| m. 11, beat 2 to m. 11, beat 2.5 | IV | IV | C | major | C | root position | 0.5 | 1.00 | - |
| m. 11, beat 2.5 to m. 11, beat 3 | dominant7 | V7 | D | dominant7 | D | root position | 0.5 | 0.61 | - |
| m. 11, beat 3 to m. 11, beat 3.5 | ii | ii | A | minor | A | root position | 0.5 | 0.74 | - |
| m. 11, beat 3.5 to m. 11, beat 4 | dominant7 | V7 | D | dominant7 | D | root position | 0.5 | 0.61 | - |
| m. 11, beat 4 to m. 11, beat 4.5 | IV | IV | C | major | C | root position | 0.5 | 1.00 | - |
| m. 11, beat 4.5 to m. 12, beat 1 | dominant7 | V7 | D | dominant7 | D | root position | 0.5 | 0.61 | - |
| m. 12, beat 1 to m. 12, beat 2 | I | I6 | G | major | B | first | 1 | 0.08 | iii (B minor, score 0.96) |
| m. 12, beat 2 to m. 12, beat 2.5 | iii | iii | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 12, beat 2.5 to m. 12, beat 3 | V | V | D | major | D | root position | 0.5 | 1.00 | - |
| m. 12, beat 3 to m. 12, beat 3.5 | IV | IV | C | major | C | root position | 0.5 | 1.00 | - |
| m. 12, beat 3.5 to m. 12, beat 4 | iii | iii | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 12, beat 4 to m. 12, beat 4.5 | ii | ii | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 12, beat 4.5 to m. 13, beat 1 | I | I | G | major | G | root position | 0.5 | 1.00 | - |
| m. 13, beat 1 to m. 13, beat 2 | half-diminished7 | vii∅4/3 | F# | half-diminished7 | C | second | 1 | 0.08 | IV (C major, score 0.83) |
| m. 13, beat 2 to m. 13, beat 2.5 | I | I | G | major | G | root position | 0.5 | 1.00 | - |
| m. 13, beat 2.5 to m. 13, beat 3 | vii° | vii° | F# | diminished | F# | root position | 0.5 | 1.00 | - |
| m. 13, beat 3 to m. 13, beat 4 | minor7 | ii6/5 | A | minor7 | C | first | 1 | 0.35 | - |
| m. 13, beat 4 to m. 13, beat 4.5 | vii° | vii° | F# | diminished | F# | root position | 0.5 | 1.00 | - |
| m. 13, beat 4.5 to m. 14, beat 1 | vi | vi | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 14, beat 1 to m. 14, beat 2 | minor7 | vi4/2 | E | minor7 | D | third | 1 | 0.24 | - |
| m. 14, beat 2 to m. 14, beat 2.5 | IV | IV | C | major | C | root position | 0.5 | 1.00 | - |
| m. 14, beat 2.5 to m. 14, beat 3 | iii | iii | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 14, beat 3 to m. 14, beat 4 | dominant7 | V7 | D | dominant7 | D | root position | 1 | 1.00 | - |
| m. 14, beat 4 to m. 14, beat 4.5 | iii | iii | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 14, beat 4.5 to m. 15, beat 1 | ii | ii | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 15, beat 1 to m. 15, beat 1.5 | I | I | G | major | G | root position | 0.5 | 1.00 | - |
| m. 15, beat 1.5 to m. 15, beat 2 | I | I6 | G | major | B | first | 0.5 | 0.64 | - |
| m. 15, beat 2 to m. 15, beat 3 | I | I6/4 | G | major | D | second | 1 | 0.82 | - |
| m. 15, beat 3 to m. 15, beat 4 | I | I | G | major | G | root position | 1 | 1.00 | - |
| m. 16, beat 1 to m. 16, beat 2 | no match | unlabelled: the library's chord identification names chords in major keys only | - | - | - | - | - | - | - |
| m. 16, beat 2 to m. 16, beat 3 | no match | unlabelled: the library's chord identification names chords in major keys only | - | - | - | - | - | - | - |
| m. 16, beat 3 to m. 17, beat 1 | no match | unlabelled: the library's chord identification names chords in major keys only | - | - | - | - | - | - | - |
| m. 17, beat 1 to m. 17, beat 3 | no match | unlabelled: the library's chord identification names chords in major keys only | - | - | - | - | - | - | - |
| m. 17, beat 3 to m. 18, beat 1 | no match | unlabelled: the library's chord identification names chords in major keys only | - | - | - | - | - | - | - |
| m. 18, beat 1 to m. 18, beat 2 | no match | unlabelled: the library's chord identification names chords in major keys only | - | - | - | - | - | - | - |
| m. 18, beat 2 to m. 18, beat 3 | no match | unlabelled: the library's chord identification names chords in major keys only | - | - | - | - | - | - | - |
| m. 18, beat 3 to m. 18, beat 4 | no match | unlabelled: the library's chord identification names chords in major keys only | - | - | - | - | - | - | - |
| m. 18, beat 4 to m. 19, beat 1 | no match | unlabelled: the library's chord identification names chords in major keys only | - | - | - | - | - | - | - |
| m. 19, beat 1 to m. 19, beat 2 | no match | unlabelled: the library's chord identification names chords in major keys only | - | - | - | - | - | - | - |
| m. 19, beat 2 to m. 19, beat 3 | no match | unlabelled: the library's chord identification names chords in major keys only | - | - | - | - | - | - | - |
| m. 19, beat 3 to m. 20, beat 1 | no match | unlabelled: the library's chord identification names chords in major keys only | - | - | - | - | - | - | - |
| m. 20, beat 1 to m. 20, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - |
| m. 20, beat 2 to m. 20, beat 3 | I | I (in G major) | G | major | G | root position | 1 | 1.00 | - |
| m. 20, beat 3 to m. 20, beat 4 | I | I6/4 | G | major | D | second | 1 | 0.37 | - |
| m. 20, beat 4 to m. 21, beat 1 | I | I | G | major | G | root position | 1 | 1.00 | - |
| m. 21, beat 1 to m. 21, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - |
| m. 21, beat 2 to m. 21, beat 3 | I | I | G | major | G | root position | 1 | 1.00 | - |
| m. 21, beat 3 to m. 21, beat 4 | IV | IV | C | major | C | root position | 1 | 0.80 | - |
| m. 21, beat 4 to m. 22, beat 1 | I | I | G | major | G | root position | 1 | 1.00 | - |
| m. 22, beat 1 to m. 22, beat 2 | iii | iii | B | minor | B | root position | 1 | 0.93 | - |
| m. 22, beat 2 to m. 22, beat 2.5 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - |
| m. 22, beat 2.5 to m. 22, beat 3 | V | V | D | major | D | root position | 0.5 | 1.00 | - |
| m. 22, beat 3 to m. 22, beat 4 | IV | IV | C | major | C | root position | 1 | 1.00 | - |
| m. 22, beat 4 to m. 22, beat 4.5 | V | V | D | major | D | root position | 0.5 | 1.00 | - |
| m. 22, beat 4.5 to m. 23, beat 1 | IV | IV | C | major | C | root position | 0.5 | 1.00 | - |
| m. 23, beat 1 to m. 23, beat 2 | I | I | G | major | G | root position | 1 | 0.87 | - |
| m. 23, beat 2 to m. 23, beat 3.5 | I | I | G | major | G | root position | 1.5 | 1.00 | - |
| m. 23, beat 3.5 to m. 23, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - |
| m. 23, beat 4 to m. 23, beat 4.5 | vi | vi | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 23, beat 4.5 to m. 24, beat 1 | V | V | D | major | D | root position | 0.5 | 1.00 | - |
| m. 24, beat 1 to m. 24, beat 2 | I | I (in C major) | C | major | C | root position | 1 | 1.00 | - |
| m. 24, beat 2 to m. 24, beat 2.5 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 24, beat 2.5 to m. 24, beat 3 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 24, beat 3 to m. 25, beat 1 | V | V | G | major | G | root position | 2 | 1.00 | - |
| m. 25, beat 1 to m. 25, beat 2 | I | I | C | major | C | root position | 1 | 1.00 | - |
| m. 25, beat 2 to m. 25, beat 2.5 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 25, beat 2.5 to m. 25, beat 3 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 25, beat 3 to m. 26, beat 1 | V | V | G | major | G | root position | 2 | 1.00 | - |
| m. 26, beat 1 to m. 26, beat 2 | major7 | IV4/3 | F | major7 | C | second | 1 | 0.08 | I (C major, score 0.83) |
| m. 26, beat 2 to m. 26, beat 2.5 | ii | ii | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 26, beat 2.5 to m. 26, beat 3 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 26, beat 3 to m. 26, beat 4 | major7 | I7 | C | major7 | C | root position | 1 | 0.93 | - |
| m. 26, beat 4 to m. 26, beat 4.5 | vii° | vii° | B | diminished | B | root position | 0.5 | 1.00 | - |
| m. 26, beat 4.5 to m. 27, beat 1 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 27, beat 1 to m. 27, beat 2 | V | V | G | major | G | root position | 1 | 0.16 | - |
| m. 27, beat 2 to m. 27, beat 2.5 | vii° | vii° | B | diminished | B | root position | 0.5 | 1.00 | - |
| m. 27, beat 2.5 to m. 27, beat 3 | vi | vi | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 27, beat 3 to m. 27, beat 3.5 | V | V | G | major | G | root position | 0.5 | 1.00 | - |
| m. 27, beat 3.5 to m. 27, beat 4 | dominant7 | V4/2 | G | dominant7 | F | third | 0.5 | 0.56 | - |
| m. 27, beat 4 to m. 27, beat 4.5 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 27, beat 4.5 to m. 28, beat 1 | ii | ii | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 28, beat 1 to m. 28, beat 2 | I | I | C | major | C | root position | 1 | 1.00 | - |
| m. 28, beat 2 to m. 28, beat 2.5 | V | V | G | major | G | root position | 0.5 | 1.00 | - |
| m. 28, beat 2.5 to m. 28, beat 3 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 28, beat 3 to m. 29, beat 1 | iii | iii | E | minor | E | root position | 2 | 1.00 | - |
| m. 29, beat 1 to m. 29, beat 2 | I | I | C | major | C | root position | 1 | 0.76 | - |
| m. 29, beat 2 to m. 29, beat 2.5 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 29, beat 2.5 to m. 29, beat 3 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 29, beat 3 to m. 29, beat 4 | V | V | G | major | G | root position | 1 | 1.00 | - |
| m. 29, beat 4 to m. 30, beat 1 | I | I | C | major | C | root position | 1 | 1.00 | - |
| m. 30, beat 1 to m. 30, beat 2 | iii | iii6 | E | minor | G | first | 1 | 0.41 | - |
| m. 30, beat 2 to m. 30, beat 3 | ii | ii | D | minor | D | root position | 1 | 0.76 | - |
| m. 30, beat 3 to m. 30, beat 4 | I | I6/4 | C | major | G | second | 1 | 0.27 | - |
| m. 30, beat 4 to m. 31, beat 1 | vii° | vii° | B | diminished | B | root position | 1 | 0.76 | - |
| m. 31, beat 1 to m. 31, beat 2 | minor7 | ii4/2 | D | minor7 | C | third | 1 | 0.10 | I (C major, score 0.81) |
| m. 31, beat 2 to m. 31, beat 2.5 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 31, beat 2.5 to m. 31, beat 3 | IV | IV | F | major | F | root position | 0.5 | 1.00 | - |
| m. 31, beat 3 to m. 31, beat 3.5 | V | V | G | major | G | root position | 0.5 | 1.00 | - |
| m. 31, beat 3.5 to m. 31, beat 4 | vi | vi | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 31, beat 4 to m. 31, beat 4.5 | vii° | vii° | B | diminished | B | root position | 0.5 | 1.00 | - |
| m. 31, beat 4.5 to m. 32, beat 1 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 32, beat 1 to m. 32, beat 1.5 | vii° | vii° | B | diminished | B | root position | 0.5 | 0.74 | - |
| m. 32, beat 1.5 to m. 32, beat 2 | V | V | G | major | G | root position | 0.5 | 0.61 | - |
| m. 32, beat 2 to m. 32, beat 2.5 | ii | ii | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 32, beat 2.5 to m. 32, beat 3 | V | V | G | major | G | root position | 0.5 | 0.61 | - |
| m. 32, beat 3 to m. 32, beat 3.5 | vii° | vii° | B | diminished | B | root position | 0.5 | 0.74 | - |
| m. 32, beat 3.5 to m. 32, beat 4 | V | V | G | major | G | root position | 0.5 | 0.61 | - |
| m. 32, beat 4 to m. 32, beat 4.5 | ii | ii | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 32, beat 4.5 to m. 33, beat 1 | V | V | G | major | G | root position | 0.5 | 0.61 | - |
| m. 33, beat 1 to m. 33, beat 2 | I | I | C | major | C | root position | 1 | 0.06 | major7 (F major7, score 0.90) |
| m. 33, beat 2 to m. 33, beat 2.5 | V | V | G | major | G | root position | 0.5 | 1.00 | - |
| m. 33, beat 2.5 to m. 33, beat 3 | vi | vi | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 33, beat 3 to m. 33, beat 3.5 | vii° | vii° | B | diminished | B | root position | 0.5 | 1.00 | - |
| m. 33, beat 3.5 to m. 33, beat 4 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 33, beat 4 to m. 33, beat 4.5 | ii | ii | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 33, beat 4.5 to m. 34, beat 1 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 34, beat 1 to m. 34, beat 1.5 | ii | ii | D | minor | D | root position | 0.5 | 0.74 | - |
| m. 34, beat 1.5 to m. 34, beat 2 | dominant7 | V7 | G | dominant7 | G | root position | 0.5 | 0.61 | - |
| m. 34, beat 2 to m. 34, beat 2.5 | IV | IV | F | major | F | root position | 0.5 | 1.00 | - |
| m. 34, beat 2.5 to m. 34, beat 3 | dominant7 | V7 | G | dominant7 | G | root position | 0.5 | 0.61 | - |
| m. 34, beat 3 to m. 34, beat 3.5 | ii | ii | D | minor | D | root position | 0.5 | 0.74 | - |
| m. 34, beat 3.5 to m. 34, beat 4 | dominant7 | V7 | G | dominant7 | G | root position | 0.5 | 0.61 | - |
| m. 34, beat 4 to m. 34, beat 4.5 | IV | IV | F | major | F | root position | 0.5 | 1.00 | - |
| m. 34, beat 4.5 to m. 35, beat 1 | dominant7 | V7 | G | dominant7 | G | root position | 0.5 | 0.61 | - |
| m. 35, beat 1 to m. 35, beat 2 | I | I6 | C | major | E | first | 1 | 0.08 | iii (E minor, score 0.96) |
| m. 35, beat 2 to m. 35, beat 2.5 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 35, beat 2.5 to m. 35, beat 3 | V | V | G | major | G | root position | 0.5 | 1.00 | - |
| m. 35, beat 3 to m. 35, beat 3.5 | IV | IV | F | major | F | root position | 0.5 | 1.00 | - |
| m. 35, beat 3.5 to m. 35, beat 4 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 35, beat 4 to m. 35, beat 4.5 | ii | ii | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 35, beat 4.5 to m. 36, beat 1 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 36, beat 1 to m. 36, beat 2 | IV | IV | F | major | F | root position | 1 | 0.29 | - |
| m. 36, beat 2 to m. 36, beat 2.5 | IV | IV | F | major | F | root position | 0.5 | 1.00 | - |
| m. 36, beat 2.5 to m. 36, beat 3 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 36, beat 3 to m. 36, beat 4 | minor7 | ii6/5 | D | minor7 | F | first | 1 | 0.35 | - |
| m. 36, beat 4 to m. 36, beat 4.5 | vii° | vii° | B | diminished | B | root position | 0.5 | 1.00 | - |
| m. 36, beat 4.5 to m. 37, beat 1 | vi | vi | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 37, beat 1 to m. 37, beat 2 | minor7 | vi4/2 | A | minor7 | G | third | 1 | 0.10 | V (G major, score 0.81) |
| m. 37, beat 2 to m. 37, beat 2.5 | IV | IV | F | major | F | root position | 0.5 | 1.00 | - |
| m. 37, beat 2.5 to m. 37, beat 3 | V | V | G | major | G | root position | 0.5 | 1.00 | - |
| m. 37, beat 3 to m. 37, beat 4 | iii | iii6 | E | minor | G | first | 1 | 0.26 | - |
| m. 37, beat 4 to m. 37, beat 4.5 | ii | ii | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 37, beat 4.5 to m. 38, beat 1 | iii | iii | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 38, beat 1 to m. 38, beat 1.5 | I | I | C | major | C | root position | 0.5 | 1.00 | - |
| m. 38, beat 1.5 to m. 38, beat 2 | I | I6 | C | major | E | first | 0.5 | 0.62 | - |
| m. 38, beat 2 to m. 38, beat 2.5 | V | V | G | major | G | root position | 0.5 | 1.00 | - |
| m. 38, beat 2.5 to m. 38, beat 4 | I | I | C | major | C | root position | 1.5 | 1.00 | - |
## Outer voices

The highest (soprano) and lowest (bass) sounding note of each chord's span, with its scale degree in that chord's local key. A flat or sharp sign marks a chromatic alteration. A degree is "-" where the span lies in no key region. A rest means the library found no note for that line in the span, a bridged one a short gap inside the line, and a register guard a note the library dropped rather than adopt an accompaniment note. Degrees carry the key region's uncertainty in the analysis document.

| Position | Key | Soprano | Degree | Bass | Degree |
| --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 2 | C major | C5 | 1 | C3 | 1 |
| m. 1, beat 2 to m. 1, beat 2.5 | C major | E5 | 3 | rest (register guard) | - |
| m. 1, beat 2.5 to m. 1, beat 3 | C major | C5 | 1 | rest (register guard) | - |
| m. 1, beat 3 to m. 2, beat 1 | C major | G4 | 5 | G4 | 5 |
| m. 2, beat 1 to m. 2, beat 2 | C major | C5 | 1 | C3 | 1 |
| m. 2, beat 2 to m. 2, beat 2.5 | C major | E5 | 3 | rest (register guard) | - |
| m. 2, beat 2.5 to m. 2, beat 3 | C major | C5 | 1 | rest (register guard) | - |
| m. 2, beat 3 to m. 3, beat 1 | C major | G5 | 5 | G4 | 5 |
| m. 3, beat 1 to m. 3, beat 2 | C major | F5 | 4 | C3 | 1 |
| m. 3, beat 2 to m. 3, beat 2.5 | C major | D5 | 2 | rest (register guard) | - |
| m. 3, beat 2.5 to m. 3, beat 3 | C major | C5 | 1 | rest (register guard) | - |
| m. 3, beat 3 to m. 3, beat 4 | C major | C5 | 1 | C3 | 1 |
| m. 3, beat 4 to m. 3, beat 4.5 | C major | B4 | 7 | rest (register guard) | - |
| m. 3, beat 4.5 to m. 4, beat 1 | C major | C5 | 1 | rest (register guard) | - |
| m. 4, beat 1 to m. 4, beat 2 | C major | D5 | 2 | G2 | 5 |
| m. 4, beat 2 to m. 4, beat 2.5 | C major | B4 | 7 | rest (register guard) | - |
| m. 4, beat 2.5 to m. 4, beat 3 | C major | A4 | 6 | rest (register guard) | - |
| m. 4, beat 3 to m. 4, beat 3.5 | C major | G4 | 5 | G3 | 5 |
| m. 4, beat 3.5 to m. 4, beat 4 | C major | G4 | 5 | F3 | 4 |
| m. 4, beat 4 to m. 4, beat 4.5 | C major | rest (register guard) | - | E3 | 3 |
| m. 4, beat 4.5 to m. 5, beat 1 | C major | rest (register guard) | - | D3 | 2 |
| m. 5, beat 1 to m. 5, beat 2 | C major | C5 | 1 | C3 | 1 |
| m. 5, beat 2 to m. 5, beat 2.5 | C major | E5 | 3 | rest (register guard) | - |
| m. 5, beat 2.5 to m. 5, beat 3 | C major | C5 | 1 | rest (register guard) | - |
| m. 5, beat 3 to m. 6, beat 1 | C major | G4 | 5 | G4 | 5 |
| m. 6, beat 1 to m. 6, beat 2 | C major | E5 | 3 | C4 | 1 |
| m. 6, beat 2 to m. 6, beat 2.5 | C major | G5 | 5 | rest (register guard) | - |
| m. 6, beat 2.5 to m. 6, beat 3 | C major | E5 | 3 | rest (register guard) | - |
| m. 6, beat 3 to m. 6, beat 4 | C major | C5 | 1 | C5 | 1 |
| m. 6, beat 4 to m. 7, beat 1 | C major | E5 | 3 | F#3 | #4 |
| m. 7, beat 1 to m. 7, beat 2 | C major | D5 | 2 | G3 | 5 |
| m. 7, beat 2 to m. 7, beat 3 | C major | C5 | 1 | C3 | 1 |
| m. 7, beat 3 to m. 7, beat 4 | C major | B4 | 7 | D3 | 2 |
| m. 7, beat 4 to m. 8, beat 1 | C major | A4 | 6 | D2 | 2 |
| m. 8, beat 1 to m. 8, beat 2 | G major | A4 | 2 | G2 | 1 |
| m. 8, beat 2 to m. 8, beat 2.5 | G major | B4 | 3 | rest (register guard) | - |
| m. 8, beat 2.5 to m. 8, beat 3 | G major | C5 | 4 | rest (register guard) | - |
| m. 8, beat 3 to m. 8, beat 3.5 | G major | D5 | 5 | D5 | 5 |
| m. 8, beat 3.5 to m. 8, beat 4 | G major | E5 | 6 | E5 | 6 |
| m. 8, beat 4 to m. 8, beat 4.5 | G major | F#5 | 7 | F#5 | 7 |
| m. 8, beat 4.5 to m. 9, beat 1 | G major | G5 | 1 | G5 | 1 |
| m. 9, beat 1 to m. 9, beat 1.5 | G major | A4 | 2 | F#3 | 7 |
| m. 9, beat 1.5 to m. 9, beat 2 | G major | A4 | 2 | D4 | 5 |
| m. 9, beat 2 to m. 9, beat 2.5 | G major | A5 | 2 | A3 | 2 |
| m. 9, beat 2.5 to m. 9, beat 3 | G major | A5 | 2 | D4 | 5 |
| m. 9, beat 3 to m. 9, beat 3.5 | G major | A5 | 2 | F#3 | 7 |
| m. 9, beat 3.5 to m. 9, beat 4 | G major | A5 | 2 | D4 | 5 |
| m. 9, beat 4 to m. 9, beat 4.5 | G major | A5 | 2 | A3 | 2 |
| m. 9, beat 4.5 to m. 10, beat 1 | G major | A5 | 2 | D4 | 5 |
| m. 10, beat 1 to m. 10, beat 2 | G major | C5 | 4 | G3 | 1 |
| m. 10, beat 2 to m. 10, beat 2.5 | G major | D5 | 5 | rest (register guard) | - |
| m. 10, beat 2.5 to m. 10, beat 3 | G major | E5 | 6 | rest (register guard) | - |
| m. 10, beat 3 to m. 10, beat 3.5 | G major | F#5 | 7 | F#5 | 7 |
| m. 10, beat 3.5 to m. 10, beat 4 | G major | G5 | 1 | G5 | 1 |
| m. 10, beat 4 to m. 10, beat 4.5 | G major | A5 | 2 | A5 | 2 |
| m. 10, beat 4.5 to m. 11, beat 1 | G major | B5 | 3 | B5 | 3 |
| m. 11, beat 1 to m. 11, beat 1.5 | G major | C5 | 4 | A3 | 2 |
| m. 11, beat 1.5 to m. 11, beat 2 | G major | C5 | 4 | D4 | 5 |
| m. 11, beat 2 to m. 11, beat 2.5 | G major | C6 | 4 | C4 | 4 |
| m. 11, beat 2.5 to m. 11, beat 3 | G major | C6 | 4 | D4 | 5 |
| m. 11, beat 3 to m. 11, beat 3.5 | G major | C6 | 4 | A3 | 2 |
| m. 11, beat 3.5 to m. 11, beat 4 | G major | C6 | 4 | D4 | 5 |
| m. 11, beat 4 to m. 11, beat 4.5 | G major | C6 | 4 | C4 | 4 |
| m. 11, beat 4.5 to m. 12, beat 1 | G major | C6 | 4 | D4 | 5 |
| m. 12, beat 1 to m. 12, beat 2 | G major | G5 | 1 | B3 | 3 |
| m. 12, beat 2 to m. 12, beat 2.5 | G major | B5 | 3 | rest (register guard) | - |
| m. 12, beat 2.5 to m. 12, beat 3 | G major | D6 | 5 | rest (register guard) | - |
| m. 12, beat 3 to m. 12, beat 3.5 | G major | C6 | 4 | C6 | 4 |
| m. 12, beat 3.5 to m. 12, beat 4 | G major | B5 | 3 | B5 | 3 |
| m. 12, beat 4 to m. 12, beat 4.5 | G major | A5 | 2 | A5 | 2 |
| m. 12, beat 4.5 to m. 13, beat 1 | G major | G5 | 1 | G5 | 1 |
| m. 13, beat 1 to m. 13, beat 2 | G major | F#5 | 7 | C4 | 4 |
| m. 13, beat 2 to m. 13, beat 2.5 | G major | G5 | 1 | rest (register guard) | - |
| m. 13, beat 2.5 to m. 13, beat 3 | G major | F#5 | 7 | rest (register guard) | - |
| m. 13, beat 3 to m. 13, beat 4 | G major | A5 | 2 | C3 | 4 |
| m. 13, beat 4 to m. 13, beat 4.5 | G major | F#5 | 7 | rest (register guard) | - |
| m. 13, beat 4.5 to m. 14, beat 1 | G major | E5 | 6 | rest (register guard) | - |
| m. 14, beat 1 to m. 14, beat 2 | G major | E5 | 6 | D3 | 5 |
| m. 14, beat 2 to m. 14, beat 2.5 | G major | C5 | 4 | rest (register guard) | - |
| m. 14, beat 2.5 to m. 14, beat 3 | G major | B4 | 3 | rest (register guard) | - |
| m. 14, beat 3 to m. 14, beat 4 | G major | D5 | 5 | D2 | 5 |
| m. 14, beat 4 to m. 14, beat 4.5 | G major | B4 | 3 | rest (register guard) | - |
| m. 14, beat 4.5 to m. 15, beat 1 | G major | A4 | 2 | rest (register guard) | - |
| m. 15, beat 1 to m. 15, beat 1.5 | G major | G4 | 1 | G2 | 1 |
| m. 15, beat 1.5 to m. 15, beat 2 | G major | G4 | 1 | B2 | 3 |
| m. 15, beat 2 to m. 15, beat 3 | G major | G4 | 1 | D3 | 5 |
| m. 15, beat 3 to m. 15, beat 4 | G major | rest (register guard) | - | G2 | 1 |
| m. 16, beat 1 to m. 16, beat 2 | C minor | B4 | #7 | F4 | 4 |
| m. 16, beat 2 to m. 16, beat 3 | C minor | D5 | 2 | F4 | 4 |
| m. 16, beat 3 to m. 17, beat 1 | C minor | G4 | 5 | F4 | 4 |
| m. 17, beat 1 to m. 17, beat 3 | C minor | D#5 | 3 | D#4 | 3 |
| m. 17, beat 3 to m. 18, beat 1 | C minor | G5 | 5 | D#4 | 3 |
| m. 18, beat 1 to m. 18, beat 2 | C minor | F5 | 4 | B3 | #7 |
| m. 18, beat 2 to m. 18, beat 3 | C minor | D5 | 2 | B3 | #7 |
| m. 18, beat 3 to m. 18, beat 4 | C minor | D#5 | 3 | C4 | 1 |
| m. 18, beat 4 to m. 19, beat 1 | C minor | C5 | 1 | C4 | 1 |
| m. 19, beat 1 to m. 19, beat 2 | C minor | C5 | 1 | G3 | 5 |
| m. 19, beat 2 to m. 19, beat 3 | C minor | D5 | 2 | G3 | 5 |
| m. 19, beat 3 to m. 20, beat 1 | C minor | G4 | 5 | G4 | 5 |
| m. 20, beat 1 to m. 20, beat 2 | G major | G5 | 1 | F4 | b7 |
| m. 20, beat 2 to m. 20, beat 3 | G major | G5 | 1 | G3 | 1 |
| m. 20, beat 3 to m. 20, beat 4 | G major | G5 | 1 | D4 | 5 |
| m. 20, beat 4 to m. 21, beat 1 | G major | G5 | 1 | G3 | 1 |
| m. 21, beat 1 to m. 21, beat 2 | G major | G5 | 1 | D#4 | b6 |
| m. 21, beat 2 to m. 21, beat 3 | G major | G5 | 1 | G3 | 1 |
| m. 21, beat 3 to m. 21, beat 4 | G major | G5 | 1 | C4 | 4 |
| m. 21, beat 4 to m. 22, beat 1 | G major | G5 | 1 | G3 | 1 |
| m. 22, beat 1 to m. 22, beat 2 | G major | D#5 | b6 | B3 | 3 |
| m. 22, beat 2 to m. 22, beat 2.5 | G major | F5 | b7 | rest (register guard) | - |
| m. 22, beat 2.5 to m. 22, beat 3 | G major | D5 | 5 | rest (register guard) | - |
| m. 22, beat 3 to m. 22, beat 4 | G major | F5 | b7 | C4 | 4 |
| m. 22, beat 4 to m. 22, beat 4.5 | G major | D5 | 5 | rest (bridged) | - |
| m. 22, beat 4.5 to m. 23, beat 1 | G major | C5 | 4 | C5 | 4 |
| m. 23, beat 1 to m. 23, beat 2 | G major | G5 | 1 | G3 | 1 |
| m. 23, beat 2 to m. 23, beat 3.5 | G major | rest (register guard) | - | G2 | 1 |
| m. 23, beat 3.5 to m. 23, beat 4 | G major | F3 | b7 | F3 | b7 |
| m. 23, beat 4 to m. 23, beat 4.5 | G major | E3 | 6 | E3 | 6 |
| m. 23, beat 4.5 to m. 24, beat 1 | G major | D3 | 5 | D3 | 5 |
| m. 24, beat 1 to m. 24, beat 2 | C major | C4 | 1 | C3 | 1 |
| m. 24, beat 2 to m. 24, beat 2.5 | C major | E4 | 3 | rest (bridged) | - |
| m. 24, beat 2.5 to m. 24, beat 3 | C major | C4 | 1 | C4 | 1 |
| m. 24, beat 3 to m. 25, beat 1 | C major | G3 | 5 | G3 | 5 |
| m. 25, beat 1 to m. 25, beat 2 | C major | C4 | 1 | C3 | 1 |
| m. 25, beat 2 to m. 25, beat 2.5 | C major | E4 | 3 | rest (bridged) | - |
| m. 25, beat 2.5 to m. 25, beat 3 | C major | C4 | 1 | C4 | 1 |
| m. 25, beat 3 to m. 26, beat 1 | C major | G4 | 5 | G3 | 5 |
| m. 26, beat 1 to m. 26, beat 2 | C major | F4 | 4 | C3 | 1 |
| m. 26, beat 2 to m. 26, beat 2.5 | C major | D4 | 2 | rest (bridged) | - |
| m. 26, beat 2.5 to m. 26, beat 3 | C major | C4 | 1 | C4 | 1 |
| m. 26, beat 3 to m. 26, beat 4 | C major | C4 | 1 | C3 | 1 |
| m. 26, beat 4 to m. 26, beat 4.5 | C major | B3 | 7 | B3 | 7 |
| m. 26, beat 4.5 to m. 27, beat 1 | C major | C4 | 1 | C4 | 1 |
| m. 27, beat 1 to m. 27, beat 2 | C major | D4 | 2 | G2 | 5 |
| m. 27, beat 2 to m. 27, beat 2.5 | C major | B3 | 7 | rest (register guard) | - |
| m. 27, beat 2.5 to m. 27, beat 3 | C major | A3 | 6 | rest (register guard) | - |
| m. 27, beat 3 to m. 27, beat 3.5 | C major | G3 | 5 | G2 | 5 |
| m. 27, beat 3.5 to m. 27, beat 4 | C major | G3 | 5 | F2 | 4 |
| m. 27, beat 4 to m. 27, beat 4.5 | C major | rest (register guard) | - | E2 | 3 |
| m. 27, beat 4.5 to m. 28, beat 1 | C major | rest (register guard) | - | D2 | 2 |
| m. 28, beat 1 to m. 28, beat 2 | C major | C4 | 1 | C2 | 1 |
| m. 28, beat 2 to m. 28, beat 2.5 | C major | G3 | 5 | rest (register guard) | - |
| m. 28, beat 2.5 to m. 28, beat 3 | C major | C4 | 1 | rest (register guard) | - |
| m. 28, beat 3 to m. 29, beat 1 | C major | E4 | 3 | E4 | 3 |
| m. 29, beat 1 to m. 29, beat 2 | C major | E4 | 3 | C3 | 1 |
| m. 29, beat 2 to m. 29, beat 2.5 | C major | C4 | 1 | C4 | 1 |
| m. 29, beat 2.5 to m. 29, beat 3 | C major | E4 | 3 | E4 | 3 |
| m. 29, beat 3 to m. 29, beat 4 | C major | G4 | 5 | G4 | 5 |
| m. 29, beat 4 to m. 30, beat 1 | C major | C5 | 1 | C5 | 1 |
| m. 30, beat 1 to m. 30, beat 2 | C major | G4 | 5 | G3 | 5 |
| m. 30, beat 2 to m. 30, beat 3 | C major | F4 | 4 | D4 | 2 |
| m. 30, beat 3 to m. 30, beat 4 | C major | E4 | 3 | G2 | 5 |
| m. 30, beat 4 to m. 31, beat 1 | C major | D4 | 2 | rest (register guard) | - |
| m. 31, beat 1 to m. 31, beat 2 | C major | D4 | 2 | C3 | 1 |
| m. 31, beat 2 to m. 31, beat 2.5 | C major | E4 | 3 | rest (register guard) | - |
| m. 31, beat 2.5 to m. 31, beat 3 | C major | F4 | 4 | rest (register guard) | - |
| m. 31, beat 3 to m. 31, beat 3.5 | C major | G4 | 5 | G4 | 5 |
| m. 31, beat 3.5 to m. 31, beat 4 | C major | A4 | 6 | A4 | 6 |
| m. 31, beat 4 to m. 31, beat 4.5 | C major | B4 | 7 | B4 | 7 |
| m. 31, beat 4.5 to m. 32, beat 1 | C major | C5 | 1 | C5 | 1 |
| m. 32, beat 1 to m. 32, beat 1.5 | C major | D4 | 2 | B2 | 7 |
| m. 32, beat 1.5 to m. 32, beat 2 | C major | D4 | 2 | G3 | 5 |
| m. 32, beat 2 to m. 32, beat 2.5 | C major | D5 | 2 | D3 | 2 |
| m. 32, beat 2.5 to m. 32, beat 3 | C major | D5 | 2 | G3 | 5 |
| m. 32, beat 3 to m. 32, beat 3.5 | C major | D5 | 2 | B2 | 7 |
| m. 32, beat 3.5 to m. 32, beat 4 | C major | D5 | 2 | G3 | 5 |
| m. 32, beat 4 to m. 32, beat 4.5 | C major | D5 | 2 | D3 | 2 |
| m. 32, beat 4.5 to m. 33, beat 1 | C major | D5 | 2 | G3 | 5 |
| m. 33, beat 1 to m. 33, beat 2 | C major | F4 | 4 | C3 | 1 |
| m. 33, beat 2 to m. 33, beat 2.5 | C major | G4 | 5 | rest (register guard) | - |
| m. 33, beat 2.5 to m. 33, beat 3 | C major | A4 | 6 | rest (register guard) | - |
| m. 33, beat 3 to m. 33, beat 3.5 | C major | B4 | 7 | B4 | 7 |
| m. 33, beat 3.5 to m. 33, beat 4 | C major | C5 | 1 | C5 | 1 |
| m. 33, beat 4 to m. 33, beat 4.5 | C major | D5 | 2 | D5 | 2 |
| m. 33, beat 4.5 to m. 34, beat 1 | C major | E5 | 3 | E5 | 3 |
| m. 34, beat 1 to m. 34, beat 1.5 | C major | F4 | 4 | D3 | 2 |
| m. 34, beat 1.5 to m. 34, beat 2 | C major | F4 | 4 | G3 | 5 |
| m. 34, beat 2 to m. 34, beat 2.5 | C major | F5 | 4 | F3 | 4 |
| m. 34, beat 2.5 to m. 34, beat 3 | C major | F5 | 4 | G3 | 5 |
| m. 34, beat 3 to m. 34, beat 3.5 | C major | F5 | 4 | D3 | 2 |
| m. 34, beat 3.5 to m. 34, beat 4 | C major | F5 | 4 | G3 | 5 |
| m. 34, beat 4 to m. 34, beat 4.5 | C major | F5 | 4 | F3 | 4 |
| m. 34, beat 4.5 to m. 35, beat 1 | C major | F5 | 4 | G3 | 5 |
| m. 35, beat 1 to m. 35, beat 2 | C major | C5 | 1 | E3 | 3 |
| m. 35, beat 2 to m. 35, beat 2.5 | C major | E5 | 3 | rest (register guard) | - |
| m. 35, beat 2.5 to m. 35, beat 3 | C major | G5 | 5 | rest (register guard) | - |
| m. 35, beat 3 to m. 35, beat 3.5 | C major | F5 | 4 | F5 | 4 |
| m. 35, beat 3.5 to m. 35, beat 4 | C major | E5 | 3 | E5 | 3 |
| m. 35, beat 4 to m. 35, beat 4.5 | C major | D5 | 2 | D5 | 2 |
| m. 35, beat 4.5 to m. 36, beat 1 | C major | C5 | 1 | C5 | 1 |
| m. 36, beat 1 to m. 36, beat 2 | C major | A5 | 6 | F3 | 4 |
| m. 36, beat 2 to m. 36, beat 2.5 | C major | F5 | 4 | rest (register guard) | - |
| m. 36, beat 2.5 to m. 36, beat 3 | C major | E5 | 3 | rest (register guard) | - |
| m. 36, beat 3 to m. 36, beat 4 | C major | D5 | 2 | F3 | 4 |
| m. 36, beat 4 to m. 36, beat 4.5 | C major | B4 | 7 | rest (register guard) | - |
| m. 36, beat 4.5 to m. 37, beat 1 | C major | A4 | 6 | rest (register guard) | - |
| m. 37, beat 1 to m. 37, beat 2 | C major | A4 | 6 | G3 | 5 |
| m. 37, beat 2 to m. 37, beat 2.5 | C major | F4 | 4 | F4 | 4 |
| m. 37, beat 2.5 to m. 37, beat 3 | C major | G4 | 5 | G4 | 5 |
| m. 37, beat 3 to m. 37, beat 4 | C major | F4 | 4 | G2 | 5 |
| m. 37, beat 4 to m. 37, beat 4.5 | C major | D4 | 2 | rest (register guard) | - |
| m. 37, beat 4.5 to m. 38, beat 1 | C major | E4 | 3 | rest (register guard) | - |
| m. 38, beat 1 to m. 38, beat 1.5 | C major | C4 | 1 | C2 | 1 |
| m. 38, beat 1.5 to m. 38, beat 2 | C major | C4 | 1 | E2 | 3 |
| m. 38, beat 2 to m. 38, beat 2.5 | C major | rest (bridged) | - | G2 | 5 |
| m. 38, beat 2.5 to m. 38, beat 4 | C major | C3 | 1 | C2 | 1 |

## Chord vocabulary

Each distinct Roman numeral of the chords the library numbered, for the whole piece and for each key region, sorted by count, then total length, then first appearance. Count is the number of chord rows with that numeral (every row in the Chords table is one). Runs counts a stretch of adjacent identical numerals in one key region once. Beats is the total length, Share its fraction of the scope's numbered chord length, and First where it first appears. Applied chords are listed on their own. Chords without a numeral are not counted.

### Overall

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 40 | 38 | 31 | 23.8% | m. 1, beat 1 |
| V | 32 | 32 | 25.5 | 19.6% | m. 1, beat 3 |
| iii | 23 | 23 | 13.5 | 10.4% | m. 1, beat 2 |
| ii | 20 | 20 | 11 | 8.5% | m. 3, beat 2 |
| vii° | 16 | 16 | 8.5 | 6.5% | m. 3, beat 4 |
| IV | 15 | 14 | 9 | 6.9% | m. 8, beat 2.5 |
| V7 | 9 | 9 | 5 | 3.8% | m. 11, beat 1.5 |
| vi | 9 | 9 | 4.5 | 3.5% | m. 4, beat 2.5 |
| I6 | 4 | 4 | 3 | 2.3% | m. 12, beat 1 |
| I6/4 | 3 | 3 | 3 | 2.3% | m. 15, beat 2 |
| IV4/3 | 2 | 2 | 2 | 1.5% | m. 3, beat 1 |
| I7 | 2 | 2 | 2 | 1.5% | m. 3, beat 3 |
| ii4/2 | 2 | 2 | 2 | 1.5% | m. 8, beat 1 |
| ii6/5 | 2 | 2 | 2 | 1.5% | m. 13, beat 3 |
| vi4/2 | 2 | 2 | 2 | 1.5% | m. 14, beat 1 |
| iii6 | 2 | 2 | 2 | 1.5% | m. 30, beat 1 |
| V4/2 | 2 | 2 | 1 | 0.8% | m. 4, beat 3.5 |
| vi6 | 1 | 1 | 1 | 0.8% | m. 7, beat 2 |
| V6/4 | 1 | 1 | 1 | 0.8% | m. 7, beat 3 |
| vii∅4/3 | 1 | 1 | 1 | 0.8% | m. 13, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 47 | 45 | 37 | 28.5% | m. 1, beat 1 |
| V | 33 | 33 | 26.5 | 20.4% | m. 1, beat 3 |
| iii | 25 | 25 | 15.5 | 11.9% | m. 1, beat 2 |
| ii | 20 | 20 | 11 | 8.5% | m. 3, beat 2 |
| vii° | 16 | 16 | 8.5 | 6.5% | m. 3, beat 4 |
| IV | 15 | 14 | 9 | 6.9% | m. 8, beat 2.5 |
| V7 | 11 | 11 | 6 | 4.6% | m. 4, beat 3.5 |
| vi | 10 | 10 | 5.5 | 4.2% | m. 4, beat 2.5 |
| ii7 | 4 | 4 | 4 | 3.1% | m. 8, beat 1 |
| IV7 | 2 | 2 | 2 | 1.5% | m. 3, beat 1 |
| I7 | 2 | 2 | 2 | 1.5% | m. 3, beat 3 |
| vi7 | 2 | 2 | 2 | 1.5% | m. 14, beat 1 |
| vii∅7 | 1 | 1 | 1 | 0.8% | m. 13, beat 1 |

### C major (region 0)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 10 | 10 | 7.5 | 27.8% | m. 1, beat 1 |
| V | 7 | 7 | 9 | 33.3% | m. 1, beat 3 |
| iii | 5 | 5 | 2.5 | 9.3% | m. 1, beat 2 |
| ii | 3 | 3 | 2 | 7.4% | m. 3, beat 2 |
| vii° | 2 | 2 | 1 | 3.7% | m. 3, beat 4 |
| IV4/3 | 1 | 1 | 1 | 3.7% | m. 3, beat 1 |
| I7 | 1 | 1 | 1 | 3.7% | m. 3, beat 3 |
| vi6 | 1 | 1 | 1 | 3.7% | m. 7, beat 2 |
| V6/4 | 1 | 1 | 1 | 3.7% | m. 7, beat 3 |
| vi | 1 | 1 | 0.5 | 1.9% | m. 4, beat 2.5 |
| V4/2 | 1 | 1 | 0.5 | 1.9% | m. 4, beat 3.5 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 10 | 10 | 7.5 | 27.8% | m. 1, beat 1 |
| V | 8 | 8 | 10 | 37.0% | m. 1, beat 3 |
| iii | 5 | 5 | 2.5 | 9.3% | m. 1, beat 2 |
| ii | 3 | 3 | 2 | 7.4% | m. 3, beat 2 |
| vi | 2 | 2 | 1.5 | 5.6% | m. 4, beat 2.5 |
| vii° | 2 | 2 | 1 | 3.7% | m. 3, beat 4 |
| IV7 | 1 | 1 | 1 | 3.7% | m. 3, beat 1 |
| I7 | 1 | 1 | 1 | 3.7% | m. 3, beat 3 |
| V7 | 1 | 1 | 0.5 | 1.9% | m. 4, beat 3.5 |

### G major (region 1)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 7 | 7 | 4.5 | 14.5% | m. 8, beat 4.5 |
| V | 7 | 7 | 3.5 | 11.3% | m. 8, beat 3 |
| ii | 7 | 7 | 3.5 | 11.3% | m. 9, beat 2 |
| iii | 6 | 6 | 3 | 9.7% | m. 8, beat 2 |
| vii° | 6 | 6 | 3 | 9.7% | m. 8, beat 4 |
| V7 | 5 | 5 | 3 | 9.7% | m. 11, beat 1.5 |
| IV | 5 | 5 | 2.5 | 8.1% | m. 8, beat 2.5 |
| vi | 3 | 3 | 1.5 | 4.8% | m. 8, beat 3.5 |
| I6 | 2 | 2 | 1.5 | 4.8% | m. 12, beat 1 |
| ii4/2 | 1 | 1 | 1 | 3.2% | m. 8, beat 1 |
| vii∅4/3 | 1 | 1 | 1 | 3.2% | m. 13, beat 1 |
| ii6/5 | 1 | 1 | 1 | 3.2% | m. 13, beat 3 |
| vi4/2 | 1 | 1 | 1 | 3.2% | m. 14, beat 1 |
| I6/4 | 1 | 1 | 1 | 3.2% | m. 15, beat 2 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 10 | 10 | 7 | 22.6% | m. 8, beat 4.5 |
| V | 7 | 7 | 3.5 | 11.3% | m. 8, beat 3 |
| ii | 7 | 7 | 3.5 | 11.3% | m. 9, beat 2 |
| iii | 6 | 6 | 3 | 9.7% | m. 8, beat 2 |
| vii° | 6 | 6 | 3 | 9.7% | m. 8, beat 4 |
| V7 | 5 | 5 | 3 | 9.7% | m. 11, beat 1.5 |
| IV | 5 | 5 | 2.5 | 8.1% | m. 8, beat 2.5 |
| vi | 3 | 3 | 1.5 | 4.8% | m. 8, beat 3.5 |
| ii7 | 2 | 2 | 2 | 6.5% | m. 8, beat 1 |
| vii∅7 | 1 | 1 | 1 | 3.2% | m. 13, beat 1 |
| vi7 | 1 | 1 | 1 | 3.2% | m. 14, beat 1 |

### G major (region 3)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 6 | 5 | 6.5 | 50.0% | m. 20, beat 2 |
| IV | 3 | 3 | 2.5 | 19.2% | m. 21, beat 3 |
| V | 3 | 3 | 1.5 | 11.5% | m. 22, beat 2.5 |
| I6/4 | 1 | 1 | 1 | 7.7% | m. 20, beat 3 |
| iii | 1 | 1 | 1 | 7.7% | m. 22, beat 1 |
| vi | 1 | 1 | 0.5 | 3.8% | m. 23, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 7 | 6 | 7.5 | 57.7% | m. 20, beat 2 |
| IV | 3 | 3 | 2.5 | 19.2% | m. 21, beat 3 |
| V | 3 | 3 | 1.5 | 11.5% | m. 22, beat 2.5 |
| iii | 1 | 1 | 1 | 7.7% | m. 22, beat 1 |
| vi | 1 | 1 | 0.5 | 3.8% | m. 23, beat 4 |

### C major (region 4)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 17 | 16 | 12.5 | 21.2% | m. 24, beat 1 |
| V | 15 | 15 | 11.5 | 19.5% | m. 24, beat 3 |
| iii | 11 | 11 | 7 | 11.9% | m. 24, beat 2 |
| ii | 10 | 10 | 5.5 | 9.3% | m. 26, beat 2 |
| vii° | 8 | 8 | 4.5 | 7.6% | m. 26, beat 4 |
| IV | 7 | 6 | 4 | 6.8% | m. 31, beat 2.5 |
| vi | 4 | 4 | 2 | 3.4% | m. 27, beat 2.5 |
| V7 | 4 | 4 | 2 | 3.4% | m. 34, beat 1.5 |
| iii6 | 2 | 2 | 2 | 3.4% | m. 30, beat 1 |
| I6 | 2 | 2 | 1.5 | 2.5% | m. 35, beat 1 |
| IV4/3 | 1 | 1 | 1 | 1.7% | m. 26, beat 1 |
| I7 | 1 | 1 | 1 | 1.7% | m. 26, beat 3 |
| I6/4 | 1 | 1 | 1 | 1.7% | m. 30, beat 3 |
| ii4/2 | 1 | 1 | 1 | 1.7% | m. 31, beat 1 |
| ii6/5 | 1 | 1 | 1 | 1.7% | m. 36, beat 3 |
| vi4/2 | 1 | 1 | 1 | 1.7% | m. 37, beat 1 |
| V4/2 | 1 | 1 | 0.5 | 0.8% | m. 27, beat 3.5 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 20 | 19 | 15 | 25.4% | m. 24, beat 1 |
| V | 15 | 15 | 11.5 | 19.5% | m. 24, beat 3 |
| iii | 13 | 13 | 9 | 15.3% | m. 24, beat 2 |
| ii | 10 | 10 | 5.5 | 9.3% | m. 26, beat 2 |
| vii° | 8 | 8 | 4.5 | 7.6% | m. 26, beat 4 |
| IV | 7 | 6 | 4 | 6.8% | m. 31, beat 2.5 |
| V7 | 5 | 5 | 2.5 | 4.2% | m. 27, beat 3.5 |
| vi | 4 | 4 | 2 | 3.4% | m. 27, beat 2.5 |
| ii7 | 2 | 2 | 2 | 3.4% | m. 31, beat 1 |
| IV7 | 1 | 1 | 1 | 1.7% | m. 26, beat 1 |
| I7 | 1 | 1 | 1 | 1.7% | m. 26, beat 3 |
| vi7 | 1 | 1 | 1 | 1.7% | m. 37, beat 1 |

## Non-chord tones

10 notes sound against a chord without being one of its tones; 1 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 4, beat 1 to m. 4, beat 2 | 24 | passing | V |
| m. 7, beat 4 to m. 8, beat 1 | 57 | unclassified | ii |
| m. 10, beat 1 to m. 10, beat 2 | 81 | passing | I |
| m. 22, beat 1 to m. 22, beat 2 | 186 | passing | iii |
| m. 22, beat 3 to m. 22, beat 4 | 189 | appoggiatura | IV |
| m. 22, beat 3 to m. 22, beat 4 | 191 | passing | IV |
| m. 27, beat 1 to m. 27, beat 2 | 226 | passing | V |
| m. 33, beat 1 to m. 33, beat 2 | 279 | passing | I |
| m. 36, beat 1 to m. 36, beat 2 | 309 | passing | IV |
| m. 37, beat 3 to m. 37, beat 4 | 324 | escape-tone | iii |
