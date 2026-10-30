# Analysis Report: field-5.musicxml

## Summary

- **File:** field-5.musicxml, 1083 notes.
- **Input:** MusicXML score, 1 part merged into one texture. Meter: notated. Key signature: notated. Pitch spelling: notated.
- **Key:** Bb major, the key with the most time across 5 key regions.
- **Chords:** 169 chord events; 157 matched a chord (93%), 12 did not.
- **Most frequent chords:** I (58), minor7 (28), dominant7 (23).

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
| m. 1, beat 1 to m. 6, beat 1 | Bb major | 0.89 | key signature |
| m. 6, beat 1 to m. 8, beat 1 | D minor | 1.00 | profile |
| m. 8, beat 1 to m. 29, beat 1 | Bb major | 0.68 | profile |
| m. 29, beat 1 to m. 33, beat 1 | F major | 0.87 | profile |
| m. 33, beat 1 to m. 44, beat 1 | Bb major | 0.62 | profile |

## Tonicizations

2 brief excursions of another key that return to the key before. Span is the stretch in the target key. The library tells a tonicization from a modulation by how long the new key lasts, whether it returns, and whether a cadence arrives in it; without a cadence it is a tonicization however long.

| Span | In | Target |
| --- | --- | --- |
| m. 6, beat 1 to m. 8, beat 1 | Bb major | D minor |
| m. 29, beat 1 to m. 33, beat 1 | Bb major | F major |

## Chords

Positions are measure and beat. Numeral is the Roman numeral in the local key region, which is named where it changes. Function is the library's harmonic function of the chord: an applied chord is a dominant toward its target degree, and a predominant is strong (IV, ii) or weak (iii, vi). Six-four is the library's type for a chord in second inversion. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Numeral | Function | Six-four | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 3 | I | I (in Bb major) | tonic | - | A# | major | A# | root position | 2 | 0.25 | - |
| m. 1, beat 3 to m. 1, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 1, beat 4 to m. 2, beat 1 | vi | vi6 | predominant (weak) | - | G | minor | A# | first | 1 | 0.90 | - |
| m. 2, beat 1 to m. 2, beat 2 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | A# | first | 1 | 0.01 | I (A# major, score 1.04) |
| m. 2, beat 2 to m. 2, beat 3 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.19 | - |
| m. 2, beat 3 to m. 2, beat 4 | I | I6/4 | tonic | arpeggiating | A# | major | F | second | 1 | 0.10 | - |
| m. 2, beat 4 to m. 3, beat 1 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.38 | - |
| m. 3, beat 1 to m. 3, beat 2 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.49 | - |
| m. 3, beat 2 to m. 3, beat 3 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.19 | - |
| m. 3, beat 3 to m. 3, beat 4 | vii° | vii° | dominant | - | A | diminished | F# | - | 1 | 0.13 | - |
| m. 3, beat 4 to m. 4, beat 1 | vii° | vii° | dominant | - | A | diminished | A | root position | 1 | 0.21 | - |
| m. 4, beat 1 to m. 4, beat 2 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.21 | - |
| m. 4, beat 2 to m. 4, beat 3 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.42 | - |
| m. 4, beat 3 to m. 4, beat 4 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.50 | - |
| m. 4, beat 4 to m. 5, beat 1 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.38 | - |
| m. 5, beat 1 to m. 5, beat 2 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.49 | - |
| m. 5, beat 2 to m. 5, beat 3 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.19 | - |
| m. 5, beat 3 to m. 5, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 5, beat 4 to m. 6, beat 1 | half-diminished7 | V7/iii | dominant of 3 | - | A | half-diminished7 | A | root position | 1 | 0.99 | - |
| m. 6, beat 1 to m. 6, beat 2 | minor | i (in D minor) | tonic | - | D | minor | D | root position | 1 | 0.35 | - |
| m. 6, beat 2 to m. 6, beat 3 | minor | i6/4 | tonic | - | D | minor | A | second | 1 | 0.13 | - |
| m. 6, beat 3 to m. 6, beat 4 | minor7 | ii4/3 | predominant (strong) | - | E | minor7 | B | second | 1 | 0.53 | - |
| m. 6, beat 4 to m. 7, beat 1 | minor | i | tonic | - | D | minor | G# | - | 1 | 0.98 | - |
| m. 7, beat 1 to m. 7, beat 2 | dominant7 | VII7 | predominant (weak) | - | C | dominant7 | C | root position | 1 | 0.83 | - |
| m. 7, beat 2 to m. 7, beat 3 | dominant7 | VII4/3 | predominant (weak) | - | C | dominant7 | G | second | 1 | 0.56 | - |
| m. 7, beat 3 to m. 7, beat 4 | minor | iv | predominant (strong) | - | G | minor | C | - | 1 | 0.11 | minor7 (D minor7, score 0.68) |
| m. 7, beat 4 to m. 8, beat 1 | half-diminished7 | ii/o6/5 | predominant (strong) | - | E | half-diminished7 | G | first | 1 | 0.61 | - |
| m. 8, beat 1 to m. 8, beat 2 | V | V (in Bb major) | dominant | - | F | major | F | root position | 1 | 0.22 | - |
| m. 8, beat 2 to m. 8, beat 3 | dominant7 | V4/2 | dominant | - | F | dominant7 | D# | third | 1 | 0.38 | - |
| m. 8, beat 3 to m. 8, beat 4 | I | I6 | tonic | - | A# | major | D | first | 1 | 0.32 | - |
| m. 8, beat 4 to m. 9, beat 1 | dominant7 | V4/3 | dominant | - | F | dominant7 | C | second | 1 | 0.19 | - |
| m. 9, beat 1 to m. 9, beat 3 | I | I | tonic | - | A# | major | A# | root position | 2 | 0.48 | - |
| m. 9, beat 3 to m. 9, beat 4 | minor7 | vi7 | predominant (weak) | - | G | minor7 | G | root position | 1 | 0.54 | - |
| m. 9, beat 4 to m. 10, beat 1 | vi | vi6 | predominant (weak) | - | G | minor | A# | first | 1 | 0.56 | - |
| m. 10, beat 1 to m. 10, beat 2 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | A# | first | 1 | 0.00 | I (A# major, score 1.05) |
| m. 10, beat 2 to m. 10, beat 3 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.54 | - |
| m. 10, beat 3 to m. 10, beat 4 | I | I6/4 | tonic | arpeggiating | A# | major | F | second | 1 | 0.34 | - |
| m. 10, beat 4 to m. 11, beat 1 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.38 | - |
| m. 11, beat 1 to m. 11, beat 2 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.49 | - |
| m. 11, beat 2 to m. 11, beat 3 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.19 | - |
| m. 11, beat 3 to m. 11, beat 4 | vii° | vii° | dominant | - | A | diminished | F# | - | 1 | 0.13 | - |
| m. 11, beat 4 to m. 12, beat 1 | vii° | vii° | dominant | - | A | diminished | A | root position | 1 | 0.21 | - |
| m. 12, beat 1 to m. 12, beat 2 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.21 | - |
| m. 12, beat 2 to m. 12, beat 3 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.42 | - |
| m. 12, beat 3 to m. 12, beat 4 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.50 | - |
| m. 12, beat 4 to m. 13, beat 1 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.38 | - |
| m. 13, beat 1 to m. 13, beat 2 | IV | IV6 | predominant (strong) | - | D# | major | G | first | 1 | 0.66 | - |
| m. 13, beat 2 to m. 13, beat 3 | IV | IV6/4 | predominant (strong) | - | D# | major | A# | second | 1 | 0.69 | - |
| m. 13, beat 3 to m. 13, beat 4 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.11 | - |
| m. 13, beat 4 to m. 14, beat 1 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.17 | - |
| m. 14, beat 1 to m. 14, beat 2 | I | I | tonic | - | A# | major | E | - | 1 | 0.21 | - |
| m. 14, beat 2 to m. 14, beat 3 | minor7 | ii4/2 | predominant (strong) | - | C | minor7 | A# | third | 1 | 0.43 | - |
| m. 14, beat 3 to m. 14, beat 4 | minor7 | V6/5/V | dominant of 5 | - | C | minor7 | E | - | 1 | 0.58 | - |
| m. 14, beat 4 to m. 15, beat 1 | dominant7 | V4/2 | dominant | - | F | dominant7 | D# | third | 1 | 0.15 | - |
| m. 15, beat 1 to m. 15, beat 2 | I | I6 | tonic | - | A# | major | D | first | 1 | 0.07 | minor7 (D minor7, score 0.97) |
| m. 15, beat 2 to m. 15, beat 3 | minor7 | ii6/5 | predominant (strong) | - | C | minor7 | D# | first | 1 | 0.28 | - |
| m. 15, beat 3 to m. 15, beat 4 | minor7 | vi4/2 | predominant (weak) | - | G | minor7 | F | third | 1 | 0.00 | I (A# major, score 1.04) |
| m. 15, beat 4 to m. 16, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 1 | 0.22 | - |
| m. 16, beat 1 to m. 16, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 16, beat 2 to m. 16, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 16, beat 3 to m. 16, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 16, beat 4 to m. 17, beat 1 | vii° | vii°6/4 | dominant | - | A | diminished | D# | second | 1 | 0.17 | - |
| m. 17, beat 1 to m. 17, beat 3 | minor7 | iii7 | predominant (weak) | - | D | minor7 | D | root position | 2 | 0.00 | minor7 (G minor7, score 0.78) |
| m. 17, beat 3 to m. 17, beat 4 | minor7 | vi4/2 | predominant (weak) | - | G | minor7 | D# | - | 1 | 0.00 | I (A# major, score 0.99) |
| m. 17, beat 4 to m. 18, beat 1 | minor7 | iii6/5 | predominant (weak) | - | D | minor7 | F | first | 1 | 0.12 | - |
| m. 18, beat 1 to m. 18, beat 1.33 | major7 | I4/3 | tonic | - | A# | major7 | F | second | 0.33 | 0.52 | - |
| m. 18, beat 1.33 to m. 18, beat 2 | I | I | tonic | - | A# | major | A# | root position | 0.67 | 0.50 | - |
| m. 18, beat 2 to m. 18, beat 3 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | A# | first | 1 | 0.07 | I (A# major, score 0.86) |
| m. 18, beat 3 to m. 18, beat 4 | major7 | I7 | tonic | - | A# | major7 | A# | root position | 1 | 0.53 | - |
| m. 18, beat 4 to m. 19, beat 1 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.44 | - |
| m. 19, beat 1 to m. 19, beat 2 | dominant7 | V7 | dominant | - | F | dominant7 | A# | - | 1 | 0.05 | major7 (A# major7, score 0.70) |
| m. 19, beat 2 to m. 19, beat 3 | major7 | I7 | tonic | - | A# | major7 | A# | root position | 1 | 0.10 | dominant7 (F dominant7, score 0.65) |
| m. 19, beat 3 to m. 19, beat 4 | dominant7 | V7 | dominant | - | F | dominant7 | A# | - | 1 | 0.03 | major7 (A# major7, score 0.71) |
| m. 19, beat 4 to m. 20, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | A# | - | 1 | 0.20 | - |
| m. 20, beat 1 to m. 20, beat 2 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.52 | - |
| m. 20, beat 2 to m. 20, beat 3 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | A# | first | 1 | 0.07 | I (A# major, score 0.86) |
| m. 20, beat 3 to m. 20, beat 4 | major7 | I7 | tonic | - | A# | major7 | A# | root position | 1 | 0.53 | - |
| m. 20, beat 4 to m. 21, beat 1 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.44 | - |
| m. 21, beat 1 to m. 21, beat 2 | dominant7 | V7 | dominant | - | F | dominant7 | A# | - | 1 | 0.05 | major7 (A# major7, score 0.70) |
| m. 21, beat 2 to m. 21, beat 3 | major7 | I7 | tonic | - | A# | major7 | A# | root position | 1 | 0.10 | dominant7 (F dominant7, score 0.65) |
| m. 21, beat 3 to m. 21, beat 4 | dominant7 | V7 | dominant | - | F | dominant7 | A# | - | 1 | 0.03 | major7 (A# major7, score 0.71) |
| m. 21, beat 4 to m. 22, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | A# | - | 1 | 0.20 | - |
| m. 22, beat 1 to m. 22, beat 4 | I | I | tonic | - | A# | major | A# | root position | 3 | 0.59 | - |
| m. 22, beat 4 to m. 23, beat 1 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.31 | - |
| m. 23, beat 1 to m. 23, beat 3 | I | I | tonic | - | A# | major | A# | root position | 2 | 0.25 | - |
| m. 23, beat 3 to m. 23, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 23, beat 4 to m. 24, beat 1 | vi | vi6 | predominant (weak) | - | G | minor | A# | first | 1 | 0.90 | - |
| m. 24, beat 1 to m. 24, beat 2 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | A# | first | 1 | 0.01 | I (A# major, score 1.04) |
| m. 24, beat 2 to m. 24, beat 3 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.19 | - |
| m. 24, beat 3 to m. 24, beat 4 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.07 | minor7 (D minor7, score 0.86) |
| m. 24, beat 4 to m. 25, beat 1 | major7 | I7 | tonic | - | A# | major7 | A# | root position | 1 | 0.00 | minor7 (G minor7, score 0.82) |
| m. 25, beat 1 to m. 25, beat 2 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.14 | - |
| m. 25, beat 2 to m. 25, beat 3 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.19 | - |
| m. 25, beat 3 to m. 25, beat 4 | vii° | vii° | dominant | - | A | diminished | F# | - | 1 | 0.13 | - |
| m. 25, beat 4 to m. 26, beat 1 | vii° | vii° | dominant | - | A | diminished | A | root position | 1 | 0.22 | - |
| m. 26, beat 1 to m. 26, beat 2 | I | I | tonic | - | A# | major | A# | root position | 1 | 1.00 | - |
| m. 26, beat 2 to m. 26, beat 3 | major7 | I7 | tonic | - | A# | major7 | A# | root position | 1 | 0.48 | - |
| m. 26, beat 3 to m. 26, beat 4 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.50 | - |
| m. 26, beat 4 to m. 27, beat 1 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.38 | - |
| m. 27, beat 1 to m. 27, beat 2 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.49 | - |
| m. 27, beat 2 to m. 27, beat 3 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.18 | - |
| m. 27, beat 3 to m. 27, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 27, beat 4 to m. 28, beat 1 | half-diminished7 | V7/iii | dominant of 3 | - | A | half-diminished7 | A | root position | 1 | 0.99 | - |
| m. 28, beat 1 to m. 28, beat 2 | iii | iii | predominant (weak) | - | D | minor | D | root position | 1 | 0.35 | - |
| m. 28, beat 2 to m. 28, beat 3 | iii | iii6/4 | predominant (weak) | - | D | minor | A | second | 1 | 0.13 | - |
| m. 28, beat 3 to m. 28, beat 4 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | A# | first | 1 | 0.04 | major7 (A# major7, score 0.75) |
| m. 28, beat 4 to m. 29, beat 1 | minor7 | vi7 | predominant (weak) | - | G | minor7 | G | root position | 1 | 0.26 | - |
| m. 29, beat 1 to m. 29, beat 2 | iii | iii6 (in F major) | predominant (weak) | - | A | minor | C | first | 1 | 0.14 | - |
| m. 29, beat 2 to m. 29, beat 3 | I | I6 | tonic | - | F | major | A | first | 1 | 0.13 | - |
| m. 29, beat 3 to m. 29, beat 4 | minor7 | vii°4/2/V | dominant of 5 | - | D | minor7 | G# | - | 1 | 0.81 | - |
| m. 29, beat 4 to m. 30, beat 1 | dominant7 | V4/3 | dominant | - | C | dominant7 | G | second | 1 | 0.15 | - |
| m. 30, beat 1 to m. 30, beat 2 | I | I | tonic | - | F | major | F | root position | 1 | 1.00 | - |
| m. 30, beat 2 to m. 30, beat 3 | I | V4/2/IV | dominant of 4 | - | F | major | D# | - | 1 | 0.50 | - |
| m. 30, beat 3 to m. 30, beat 4 | IV | IV6 | predominant (strong) | - | A# | major | D | first | 1 | 0.32 | - |
| m. 30, beat 4 to m. 31, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 31, beat 1 to m. 31, beat 2 | minor7 | ii4/2 | predominant (strong) | - | G | minor7 | B | - | 1 | 0.26 | - |
| m. 31, beat 2 to m. 31, beat 3 | minor7 | ii7 | predominant (strong) | - | G | minor7 | G | root position | 1 | 0.18 | - |
| m. 31, beat 3 to m. 31, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 31, beat 4 to m. 32, beat 1 | vi | vi6 | predominant (weak) | - | D | minor | F | first | 1 | 0.17 | - |
| m. 32, beat 1 to m. 32, beat 2 | V | V6/4 | dominant | - | C | major | G | second | 1 | 0.44 | - |
| m. 32, beat 2 to m. 32, beat 3 | minor7 | ii7 | predominant (strong) | - | G | minor7 | G | root position | 1 | 0.09 | minor7 (D minor7, score 0.66) |
| m. 32, beat 3 to m. 32, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 32, beat 4 to m. 33, beat 1 | I | I6 | tonic | - | F | major | A | first | 1 | 0.62 | - |
| m. 33, beat 1 to m. 33, beat 2 | I | I (in Bb major) | tonic | - | A# | major | A# | root position | 1 | 0.49 | - |
| m. 33, beat 2 to m. 33, beat 3 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.13 | - |
| m. 33, beat 3 to m. 33, beat 4 | minor7 | ii6/5 | predominant (strong) | - | C | minor7 | D# | first | 1 | 0.79 | - |
| m. 33, beat 4 to m. 34, beat 1 | minor7 | ii4/2 | predominant (strong) | - | C | minor7 | E | - | 1 | 0.02 | vi (G minor, score 0.65) |
| m. 34, beat 1 to m. 34, beat 2 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.66 | - |
| m. 34, beat 2 to m. 34, beat 3 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.76 | - |
| m. 34, beat 3 to m. 34, beat 4 | V | V | dominant | - | F | major | F | root position | 1 | 0.23 | - |
| m. 34, beat 4 to m. 35, beat 1 | V | V6 | dominant | - | F | major | A | first | 1 | 1.00 | - |
| m. 35, beat 1 to m. 35, beat 2 | IV | IV6 | predominant (strong) | - | D# | major | G | first | 1 | 0.66 | - |
| m. 35, beat 2 to m. 35, beat 3 | IV | IV6/4 | predominant (strong) | - | D# | major | F# | - | 1 | 1.00 | - |
| m. 35, beat 3 to m. 35, beat 4 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.11 | - |
| m. 35, beat 4 to m. 36, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 36, beat 1 to m. 36, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 36, beat 2 to m. 36, beat 3 | minor7 | ii4/2 | predominant (strong) | - | C | minor7 | A# | third | 1 | 0.02 | vi (G minor, score 0.72) |
| m. 36, beat 3 to m. 36, beat 4 | minor7 | V6/5/V | dominant of 5 | - | C | minor7 | E | - | 1 | 0.06 | half-diminished7 (A half-diminished7, score 0.47) |
| m. 36, beat 4 to m. 37, beat 1 | dominant7 | V4/2 | dominant | - | F | dominant7 | D# | third | 1 | 0.15 | - |
| m. 37, beat 1 to m. 37, beat 2 | iii | vii°6/5/ii | dominant of 2 | - | D | minor | D | root position | 1 | 0.30 | - |
| m. 37, beat 2 to m. 37, beat 3 | ii | ii6 | predominant (strong) | - | C | minor | D# | first | 1 | 0.07 | dominant7 (F dominant7, score 0.75) |
| m. 37, beat 3 to m. 37, beat 4 | minor7 | vi4/2 | predominant (weak) | - | G | minor7 | F | third | 1 | 0.00 | I (A# major, score 1.04) |
| m. 37, beat 4 to m. 38, beat 1 | minor7 | iii6/5 | predominant (weak) | - | D | minor7 | F | first | 1 | 0.17 | - |
| m. 38, beat 1 to m. 38, beat 2 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.56 | - |
| m. 38, beat 2 to m. 38, beat 3 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | A# | first | 1 | 0.07 | I (A# major, score 0.86) |
| m. 38, beat 3 to m. 38, beat 4 | major7 | I7 | tonic | - | A# | major7 | A# | root position | 1 | 0.53 | - |
| m. 38, beat 4 to m. 39, beat 1 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.44 | - |
| m. 39, beat 1 to m. 39, beat 2 | dominant7 | V7 | dominant | - | F | dominant7 | A# | - | 1 | 0.05 | major7 (A# major7, score 0.70) |
| m. 39, beat 2 to m. 39, beat 3 | major7 | I7 | tonic | - | A# | major7 | A# | root position | 1 | 0.10 | dominant7 (F dominant7, score 0.65) |
| m. 39, beat 3 to m. 39, beat 4 | dominant7 | V7 | dominant | - | F | dominant7 | A# | - | 1 | 0.03 | major7 (A# major7, score 0.71) |
| m. 39, beat 4 to m. 40, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | A# | - | 1 | 0.20 | - |
| m. 40, beat 1 to m. 40, beat 2 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.52 | - |
| m. 40, beat 2 to m. 40, beat 3 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | A# | first | 1 | 0.07 | I (A# major, score 0.86) |
| m. 40, beat 3 to m. 40, beat 4 | major7 | I7 | tonic | - | A# | major7 | A# | root position | 1 | 0.53 | - |
| m. 40, beat 4 to m. 41, beat 1 | I | I | tonic | - | A# | major | A# | root position | 1 | 0.44 | - |
| m. 41, beat 1 to m. 41, beat 2 | dominant7 | V7 | dominant | - | F | dominant7 | A# | - | 1 | 0.05 | major7 (A# major7, score 0.70) |
| m. 41, beat 2 to m. 41, beat 3 | major7 | I7 | tonic | - | A# | major7 | A# | root position | 1 | 0.10 | dominant7 (F dominant7, score 0.65) |
| m. 41, beat 3 to m. 41, beat 4 | dominant7 | V7 | dominant | - | F | dominant7 | A# | - | 1 | 0.03 | major7 (A# major7, score 0.71) |
| m. 41, beat 4 to m. 42, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | A# | - | 1 | 0.20 | - |
| m. 42, beat 1 to m. 42, beat 1.33 | I | I | tonic | - | A# | major | A# | root position | 0.33 | 0.55 | - |
| m. 42, beat 1.33 to m. 42, beat 2 | major7 | IV4/2 | predominant (strong) | - | D# | major7 | D | third | 0.67 | 0.52 | - |
| m. 42, beat 2 to m. 42, beat 3 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 1 | 0.00 | minor7 (D minor7, score 0.68) |
| m. 42, beat 3 to m. 42, beat 3.33 | major7 | I4/3 | tonic | - | A# | major7 | F | second | 0.33 | 0.20 | - |
| m. 42, beat 3.33 to m. 42, beat 4 | major7 | IV4/2 | predominant (strong) | - | D# | major7 | D | third | 0.67 | 0.52 | - |
| m. 42, beat 4 to m. 43, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 1 | 0.00 | minor7 (D minor7, score 0.81) |
| m. 43, beat 1 to m. 43, beat 1.33 | major7 | I4/3 | tonic | - | A# | major7 | F | second | 0.33 | 0.20 | - |
| m. 43, beat 1.33 to m. 43, beat 2 | major7 | IV4/2 | predominant (strong) | - | D# | major7 | D | third | 0.67 | 0.52 | - |
| m. 43, beat 2 to m. 43, beat 3 | dominant7 | V4/2 | dominant | - | F | dominant7 | D# | third | 1 | 0.45 | - |
| m. 43, beat 3 to m. 44, beat 1 | I | I | tonic | - | A# | major | A# | root position | 2 | 0.65 | - |

## Applied chords

Chords the library reads as applied (secondary) chords: the chord as it reads in the tonicized key, the chord it tonicizes and that chord's tonic. The last column checks each against the tonicization claims above for the same span; where they disagree both are shown and neither is changed.

| Position | Applied chord | Target | Resolution | Tonicization |
| --- | --- | --- | --- | --- |
| m. 5, beat 4 to m. 6, beat 1 | V7 (secondary-dominant) | iii (D) | to the target | agrees: tonicization of D minor |
| m. 14, beat 3 to m. 14, beat 4 | V6/5 (secondary-dominant) | V (F) | to the target | DISAGREES: no tonicization is reported here |
| m. 27, beat 4 to m. 28, beat 1 | V7 (secondary-dominant) | iii (D) | to the target | DISAGREES: no tonicization is reported here |
| m. 29, beat 3 to m. 29, beat 4 | vii°4/2 (secondary-leading-tone) | V (C) | to the target | DISAGREES: the tonicization is of F major |
| m. 30, beat 2 to m. 30, beat 3 | V4/2 (secondary-dominant) | IV (Bb) | to the target | DISAGREES: the tonicization is of F major |
| m. 36, beat 3 to m. 36, beat 4 | V6/5 (secondary-dominant) | V (F) | to the target | DISAGREES: no tonicization is reported here |
| m. 37, beat 1 to m. 37, beat 2 | vii°6/5 (secondary-leading-tone) | ii (C) | to the target | DISAGREES: no tonicization is reported here |
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
| m. 5, beat 1 to m. 5, beat 2 | Bb major | D5 | 3 | A#2 | 1 |
| m. 5, beat 2 to m. 5, beat 3 | Bb major | D5 | 3 | A#3 | 1 |
| m. 5, beat 3 to m. 5, beat 4 | Bb major | E5 | #4 | A2 | 7 |
| m. 5, beat 4 to m. 6, beat 1 | Bb major | A4 | 7 | A3 | 7 |
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
| m. 17, beat 1 to m. 17, beat 3 | Bb major | F5 | 5 | D3 | 3 |
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

## Chord vocabulary

Each distinct Roman numeral of the chords the library numbered, for the whole piece and for each key region, sorted by count, then total length, then first appearance. Count is the number of chord rows with that numeral (every row in the Chords table is one). Runs counts a stretch of adjacent identical numerals in one key region once. Beats is the total length, Share its fraction of the scope's numbered chord length, and First where it first appears. Applied chords are listed on their own. Chords without a numeral are not counted.

### Overall

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 42 | 30 | 47 | 29.4% | m. 1, beat 1 |
| V7 | 15 | 11 | 15 | 9.4% | m. 15, beat 4 |
| I6/4 | 11 | 11 | 11 | 6.9% | m. 2, beat 3 |
| I7 | 10 | 10 | 10 | 6.2% | m. 18, beat 3 |
| vi6/5 | 8 | 8 | 8 | 5.0% | m. 2, beat 1 |
| vii° | 6 | 3 | 6 | 3.8% | m. 3, beat 3 |
| vi6 | 4 | 4 | 4 | 2.5% | m. 1, beat 4 |
| V4/2 | 4 | 4 | 4 | 2.5% | m. 8, beat 2 |
| I6 | 4 | 4 | 4 | 2.5% | m. 8, beat 3 |
| ii4/2 | 4 | 4 | 4 | 2.5% | m. 14, beat 2 |
| IV6 | 3 | 3 | 3 | 1.9% | m. 13, beat 1 |
| vi4/2 | 3 | 3 | 3 | 1.9% | m. 15, beat 3 |
| IV4/2 | 3 | 3 | 2 | 1.2% | m. 42, beat 1.33 |
| I4/3 | 3 | 3 | 1 | 0.6% | m. 18, beat 1 |
| i | 2 | 2 | 2 | 1.2% | m. 6, beat 1 |
| V | 2 | 2 | 2 | 1.2% | m. 8, beat 1 |
| V4/3 | 2 | 2 | 2 | 1.2% | m. 8, beat 4 |
| vi7 | 2 | 2 | 2 | 1.2% | m. 9, beat 3 |
| IV6/4 | 2 | 2 | 2 | 1.2% | m. 13, beat 2 |
| ii6/5 | 2 | 2 | 2 | 1.2% | m. 15, beat 2 |
| iii6/5 | 2 | 2 | 2 | 1.2% | m. 17, beat 4 |
| ii7 | 2 | 2 | 2 | 1.2% | m. 31, beat 2 |
| iii7 | 1 | 1 | 2 | 1.2% | m. 17, beat 1 |
| i6/4 | 1 | 1 | 1 | 0.6% | m. 6, beat 2 |
| ii4/3 | 1 | 1 | 1 | 0.6% | m. 6, beat 3 |
| VII7 | 1 | 1 | 1 | 0.6% | m. 7, beat 1 |
| VII4/3 | 1 | 1 | 1 | 0.6% | m. 7, beat 2 |
| iv | 1 | 1 | 1 | 0.6% | m. 7, beat 3 |
| ii/o6/5 | 1 | 1 | 1 | 0.6% | m. 7, beat 4 |
| vii°6/4 | 1 | 1 | 1 | 0.6% | m. 16, beat 4 |
| iii | 1 | 1 | 1 | 0.6% | m. 28, beat 1 |
| iii6/4 | 1 | 1 | 1 | 0.6% | m. 28, beat 2 |
| iii6 | 1 | 1 | 1 | 0.6% | m. 29, beat 1 |
| V6/4 | 1 | 1 | 1 | 0.6% | m. 32, beat 1 |
| V6 | 1 | 1 | 1 | 0.6% | m. 34, beat 4 |
| ii6 | 1 | 1 | 1 | 0.6% | m. 37, beat 2 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 57 | 45 | 62 | 38.8% | m. 1, beat 1 |
| V7 | 21 | 17 | 21 | 13.1% | m. 8, beat 2 |
| vi7 | 13 | 13 | 13 | 8.1% | m. 2, beat 1 |
| I7 | 13 | 13 | 11 | 6.9% | m. 18, beat 1 |
| ii7 | 9 | 9 | 9 | 5.6% | m. 6, beat 3 |
| vii° | 7 | 4 | 7 | 4.4% | m. 3, beat 3 |
| IV | 5 | 5 | 5 | 3.1% | m. 13, beat 1 |
| vi | 4 | 4 | 4 | 2.5% | m. 1, beat 4 |
| V | 4 | 4 | 4 | 2.5% | m. 8, beat 1 |
| iii7 | 3 | 3 | 4 | 2.5% | m. 17, beat 1 |
| i | 3 | 3 | 3 | 1.9% | m. 6, beat 1 |
| iii | 3 | 3 | 3 | 1.9% | m. 28, beat 1 |
| IV7 | 3 | 3 | 2 | 1.2% | m. 42, beat 1.33 |
| VII7 | 2 | 2 | 2 | 1.2% | m. 7, beat 1 |
| iv | 1 | 1 | 1 | 0.6% | m. 7, beat 3 |
| ii/o7 | 1 | 1 | 1 | 0.6% | m. 7, beat 4 |
| ii | 1 | 1 | 1 | 0.6% | m. 37, beat 2 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/iii | 2 | 2 | 2 | 1.2% | m. 5, beat 4 |
| V6/5/V | 2 | 2 | 2 | 1.2% | m. 14, beat 3 |
| vii°4/2/V | 1 | 1 | 1 | 0.6% | m. 29, beat 3 |
| V4/2/IV | 1 | 1 | 1 | 0.6% | m. 30, beat 2 |
| vii°6/5/ii | 1 | 1 | 1 | 0.6% | m. 37, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/iii | 2 | 2 | 2 | 1.2% | m. 5, beat 4 |
| V7/V | 2 | 2 | 2 | 1.2% | m. 14, beat 3 |
| vii°7/V | 1 | 1 | 1 | 0.6% | m. 29, beat 3 |
| V7/IV | 1 | 1 | 1 | 0.6% | m. 30, beat 2 |
| vii°7/ii | 1 | 1 | 1 | 0.6% | m. 37, beat 1 |

### Bb major (region 0)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 10 | 5 | 11 | 61.1% | m. 1, beat 1 |
| I6/4 | 2 | 2 | 2 | 11.1% | m. 2, beat 3 |
| vii° | 2 | 1 | 2 | 11.1% | m. 3, beat 3 |
| vi6 | 1 | 1 | 1 | 5.6% | m. 1, beat 4 |
| vi6/5 | 1 | 1 | 1 | 5.6% | m. 2, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 12 | 7 | 13 | 72.2% | m. 1, beat 1 |
| vii° | 2 | 1 | 2 | 11.1% | m. 3, beat 3 |
| vi | 1 | 1 | 1 | 5.6% | m. 1, beat 4 |
| vi7 | 1 | 1 | 1 | 5.6% | m. 2, beat 1 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/iii | 1 | 1 | 1 | 5.6% | m. 5, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/iii | 1 | 1 | 1 | 5.6% | m. 5, beat 4 |

### D minor (region 1)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 2 | 2 | 2 | 25.0% | m. 6, beat 1 |
| i6/4 | 1 | 1 | 1 | 12.5% | m. 6, beat 2 |
| ii4/3 | 1 | 1 | 1 | 12.5% | m. 6, beat 3 |
| VII7 | 1 | 1 | 1 | 12.5% | m. 7, beat 1 |
| VII4/3 | 1 | 1 | 1 | 12.5% | m. 7, beat 2 |
| iv | 1 | 1 | 1 | 12.5% | m. 7, beat 3 |
| ii/o6/5 | 1 | 1 | 1 | 12.5% | m. 7, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 3 | 3 | 3 | 37.5% | m. 6, beat 1 |
| VII7 | 2 | 2 | 2 | 25.0% | m. 7, beat 1 |
| ii7 | 1 | 1 | 1 | 12.5% | m. 6, beat 3 |
| iv | 1 | 1 | 1 | 12.5% | m. 7, beat 3 |
| ii/o7 | 1 | 1 | 1 | 12.5% | m. 7, beat 4 |

### Bb major (region 2)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 23 | 16 | 26.67 | 33.8% | m. 9, beat 1 |
| V7 | 7 | 5 | 7 | 8.9% | m. 15, beat 4 |
| I6/4 | 6 | 6 | 6 | 7.6% | m. 10, beat 3 |
| I7 | 6 | 6 | 6 | 7.6% | m. 18, beat 3 |
| vi6/5 | 5 | 5 | 5 | 6.3% | m. 10, beat 1 |
| vii° | 4 | 2 | 4 | 5.1% | m. 11, beat 3 |
| V4/2 | 2 | 2 | 2 | 2.5% | m. 8, beat 2 |
| I6 | 2 | 2 | 2 | 2.5% | m. 8, beat 3 |
| vi7 | 2 | 2 | 2 | 2.5% | m. 9, beat 3 |
| vi6 | 2 | 2 | 2 | 2.5% | m. 9, beat 4 |
| vi4/2 | 2 | 2 | 2 | 2.5% | m. 15, beat 3 |
| iii7 | 1 | 1 | 2 | 2.5% | m. 17, beat 1 |
| V | 1 | 1 | 1 | 1.3% | m. 8, beat 1 |
| V4/3 | 1 | 1 | 1 | 1.3% | m. 8, beat 4 |
| IV6 | 1 | 1 | 1 | 1.3% | m. 13, beat 1 |
| IV6/4 | 1 | 1 | 1 | 1.3% | m. 13, beat 2 |
| ii4/2 | 1 | 1 | 1 | 1.3% | m. 14, beat 2 |
| ii6/5 | 1 | 1 | 1 | 1.3% | m. 15, beat 2 |
| vii°6/4 | 1 | 1 | 1 | 1.3% | m. 16, beat 4 |
| iii6/5 | 1 | 1 | 1 | 1.3% | m. 17, beat 4 |
| iii | 1 | 1 | 1 | 1.3% | m. 28, beat 1 |
| iii6/4 | 1 | 1 | 1 | 1.3% | m. 28, beat 2 |
| I4/3 | 1 | 1 | 0.33 | 0.4% | m. 18, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 31 | 24 | 34.67 | 43.9% | m. 8, beat 3 |
| V7 | 10 | 8 | 10 | 12.7% | m. 8, beat 2 |
| vi7 | 9 | 9 | 9 | 11.4% | m. 9, beat 3 |
| I7 | 7 | 7 | 6.33 | 8.0% | m. 18, beat 1 |
| vii° | 5 | 3 | 5 | 6.3% | m. 11, beat 3 |
| iii7 | 2 | 2 | 3 | 3.8% | m. 17, beat 1 |
| vi | 2 | 2 | 2 | 2.5% | m. 9, beat 4 |
| IV | 2 | 2 | 2 | 2.5% | m. 13, beat 1 |
| ii7 | 2 | 2 | 2 | 2.5% | m. 14, beat 2 |
| iii | 2 | 2 | 2 | 2.5% | m. 28, beat 1 |
| V | 1 | 1 | 1 | 1.3% | m. 8, beat 1 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V6/5/V | 1 | 1 | 1 | 1.3% | m. 14, beat 3 |
| V7/iii | 1 | 1 | 1 | 1.3% | m. 27, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/V | 1 | 1 | 1 | 1.3% | m. 14, beat 3 |
| V7/iii | 1 | 1 | 1 | 1.3% | m. 27, beat 4 |

### F major (region 3)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I6 | 2 | 2 | 2 | 15.4% | m. 29, beat 2 |
| ii7 | 2 | 2 | 2 | 15.4% | m. 31, beat 2 |
| iii6 | 1 | 1 | 1 | 7.7% | m. 29, beat 1 |
| V4/3 | 1 | 1 | 1 | 7.7% | m. 29, beat 4 |
| I | 1 | 1 | 1 | 7.7% | m. 30, beat 1 |
| IV6 | 1 | 1 | 1 | 7.7% | m. 30, beat 3 |
| ii4/2 | 1 | 1 | 1 | 7.7% | m. 31, beat 1 |
| vi6 | 1 | 1 | 1 | 7.7% | m. 31, beat 4 |
| V6/4 | 1 | 1 | 1 | 7.7% | m. 32, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 3 | 23.1% | m. 29, beat 2 |
| ii7 | 3 | 3 | 3 | 23.1% | m. 31, beat 1 |
| iii | 1 | 1 | 1 | 7.7% | m. 29, beat 1 |
| V7 | 1 | 1 | 1 | 7.7% | m. 29, beat 4 |
| IV | 1 | 1 | 1 | 7.7% | m. 30, beat 3 |
| vi | 1 | 1 | 1 | 7.7% | m. 31, beat 4 |
| V | 1 | 1 | 1 | 7.7% | m. 32, beat 1 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°4/2/V | 1 | 1 | 1 | 7.7% | m. 29, beat 3 |
| V4/2/IV | 1 | 1 | 1 | 7.7% | m. 30, beat 2 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°7/V | 1 | 1 | 1 | 7.7% | m. 29, beat 3 |
| V7/IV | 1 | 1 | 1 | 7.7% | m. 30, beat 2 |

### Bb major (region 4)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 8 | 8 | 8.33 | 19.8% | m. 33, beat 1 |
| V7 | 8 | 6 | 8 | 19.0% | m. 39, beat 1 |
| I7 | 4 | 4 | 4 | 9.5% | m. 38, beat 3 |
| I6/4 | 3 | 3 | 3 | 7.1% | m. 33, beat 2 |
| IV4/2 | 3 | 3 | 2 | 4.8% | m. 42, beat 1.33 |
| ii4/2 | 2 | 2 | 2 | 4.8% | m. 33, beat 4 |
| V4/2 | 2 | 2 | 2 | 4.8% | m. 36, beat 4 |
| vi6/5 | 2 | 2 | 2 | 4.8% | m. 38, beat 2 |
| I4/3 | 2 | 2 | 0.67 | 1.6% | m. 42, beat 3 |
| ii6/5 | 1 | 1 | 1 | 2.4% | m. 33, beat 3 |
| V | 1 | 1 | 1 | 2.4% | m. 34, beat 3 |
| V6 | 1 | 1 | 1 | 2.4% | m. 34, beat 4 |
| IV6 | 1 | 1 | 1 | 2.4% | m. 35, beat 1 |
| IV6/4 | 1 | 1 | 1 | 2.4% | m. 35, beat 2 |
| ii6 | 1 | 1 | 1 | 2.4% | m. 37, beat 2 |
| vi4/2 | 1 | 1 | 1 | 2.4% | m. 37, beat 3 |
| iii6/5 | 1 | 1 | 1 | 2.4% | m. 37, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 11 | 11 | 11.33 | 27.0% | m. 33, beat 1 |
| V7 | 10 | 8 | 10 | 23.8% | m. 36, beat 4 |
| I7 | 6 | 6 | 4.67 | 11.1% | m. 38, beat 3 |
| ii7 | 3 | 3 | 3 | 7.1% | m. 33, beat 3 |
| vi7 | 3 | 3 | 3 | 7.1% | m. 37, beat 3 |
| IV7 | 3 | 3 | 2 | 4.8% | m. 42, beat 1.33 |
| V | 2 | 2 | 2 | 4.8% | m. 34, beat 3 |
| IV | 2 | 2 | 2 | 4.8% | m. 35, beat 1 |
| ii | 1 | 1 | 1 | 2.4% | m. 37, beat 2 |
| iii7 | 1 | 1 | 1 | 2.4% | m. 37, beat 4 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V6/5/V | 1 | 1 | 1 | 2.4% | m. 36, beat 3 |
| vii°6/5/ii | 1 | 1 | 1 | 2.4% | m. 37, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/V | 1 | 1 | 1 | 2.4% | m. 36, beat 3 |
| vii°7/ii | 1 | 1 | 1 | 2.4% | m. 37, beat 1 |

## Non-chord tones

225 notes sound against a chord without being one of its tones; 115 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 1, beat 4 to m. 2, beat 1 | 7 | unclassified | vi |
| m. 1, beat 4 to m. 2, beat 1 | 12 | unclassified | vi |
| m. 1, beat 4 to m. 2, beat 1 | 13 | unclassified | vi |
| m. 3, beat 3 to m. 3, beat 4 | 30 | unclassified | vii° |
| m. 3, beat 3 to m. 3, beat 4 | 36 | suspension | vii° |
| m. 3, beat 3 to m. 3, beat 4 | 38 | unclassified | vii° |
| m. 4, beat 1 to m. 4, beat 2 | 41 | unclassified | I |
| m. 4, beat 2 to m. 4, beat 3 | 53 | unclassified | I |
| m. 4, beat 3 to m. 4, beat 4 | 53 | unclassified | I |
| m. 5, beat 4 to m. 6, beat 1 | 74 | unclassified | half-diminished7 |
| m. 6, beat 3 to m. 6, beat 4 | 86 | unclassified | minor7 |
| m. 6, beat 4 to m. 7, beat 1 | 86 | unclassified | minor |
| m. 6, beat 4 to m. 7, beat 1 | 90 | unclassified | minor |
| m. 7, beat 3 to m. 7, beat 4 | 91 | unclassified | minor |
| m. 7, beat 3 to m. 7, beat 4 | 99 | unclassified | minor |
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
| m. 12, beat 2 to m. 12, beat 3 | 182 | unclassified | I |
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
| m. 17, beat 1 to m. 17, beat 3 | 258 | unclassified | minor7 |
| m. 17, beat 1 to m. 17, beat 3 | 260 | unclassified | minor7 |
| m. 17, beat 1 to m. 17, beat 3 | 259 | unclassified | minor7 |
| m. 17, beat 1 to m. 17, beat 3 | 263 | anticipation | minor7 |
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
| m. 19, beat 1 to m. 19, beat 2 | 334 | pedal | dominant7 |
| m. 19, beat 1 to m. 19, beat 2 | 339 | pedal | dominant7 |
| m. 19, beat 1 to m. 19, beat 2 | 336 | passing | dominant7 |
| m. 19, beat 1 to m. 19, beat 2 | 344 | anticipation | dominant7 |
| m. 19, beat 2 to m. 19, beat 3 | 347 | pedal | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 346 | neighbor | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 352 | pedal | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 351 | passing | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 357 | anticipation | major7 |
| m. 19, beat 3 to m. 19, beat 4 | 364 | pedal | dominant7 |
| m. 19, beat 3 to m. 19, beat 4 | 369 | pedal | dominant7 |
| m. 19, beat 3 to m. 19, beat 4 | 366 | passing | dominant7 |
| m. 19, beat 3 to m. 19, beat 4 | 374 | pedal | dominant7 |
| m. 19, beat 4 to m. 20, beat 1 | 379 | pedal | dominant7 |
| m. 19, beat 4 to m. 20, beat 1 | 376 | neighbor | dominant7 |
| m. 19, beat 4 to m. 20, beat 1 | 384 | pedal | dominant7 |
| m. 19, beat 4 to m. 20, beat 1 | 389 | pedal | dominant7 |
| m. 20, beat 1 to m. 20, beat 2 | 395 | appoggiatura | I |
| m. 20, beat 2 to m. 20, beat 3 | 405 | passing | minor7 |
| m. 20, beat 2 to m. 20, beat 3 | 415 | passing | minor7 |
| m. 20, beat 3 to m. 20, beat 4 | 430 | passing | major7 |
| m. 20, beat 4 to m. 21, beat 1 | 435 | passing | I |
| m. 20, beat 4 to m. 21, beat 1 | 440 | passing | I |
| m. 21, beat 1 to m. 21, beat 2 | 453 | pedal | dominant7 |
| m. 21, beat 1 to m. 21, beat 2 | 458 | pedal | dominant7 |
| m. 21, beat 1 to m. 21, beat 2 | 455 | passing | dominant7 |
| m. 21, beat 1 to m. 21, beat 2 | 463 | anticipation | dominant7 |
| m. 21, beat 2 to m. 21, beat 3 | 466 | pedal | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 465 | neighbor | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 471 | pedal | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 470 | passing | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 476 | anticipation | major7 |
| m. 21, beat 3 to m. 21, beat 4 | 483 | pedal | dominant7 |
| m. 21, beat 3 to m. 21, beat 4 | 488 | pedal | dominant7 |
| m. 21, beat 3 to m. 21, beat 4 | 485 | passing | dominant7 |
| m. 21, beat 3 to m. 21, beat 4 | 493 | pedal | dominant7 |
| m. 21, beat 4 to m. 22, beat 1 | 498 | pedal | dominant7 |
| m. 21, beat 4 to m. 22, beat 1 | 495 | neighbor | dominant7 |
| m. 21, beat 4 to m. 22, beat 1 | 503 | pedal | dominant7 |
| m. 21, beat 4 to m. 22, beat 1 | 508 | pedal | dominant7 |
| m. 23, beat 4 to m. 24, beat 1 | 534 | unclassified | vi |
| m. 23, beat 4 to m. 24, beat 1 | 539 | unclassified | vi |
| m. 23, beat 4 to m. 24, beat 1 | 540 | unclassified | vi |
| m. 24, beat 3 to m. 24, beat 4 | 554 | unclassified | I |
| m. 24, beat 4 to m. 25, beat 1 | 559 | unclassified | major7 |
| m. 24, beat 4 to m. 25, beat 1 | 562 | unclassified | major7 |
| m. 24, beat 4 to m. 25, beat 1 | 564 | unclassified | major7 |
| m. 25, beat 1 to m. 25, beat 2 | 564 | unclassified | I |
| m. 25, beat 3 to m. 25, beat 4 | 565 | unclassified | vii° |
| m. 25, beat 3 to m. 25, beat 4 | 571 | retardation | vii° |
| m. 25, beat 3 to m. 25, beat 4 | 573 | unclassified | vii° |
| m. 26, beat 2 to m. 26, beat 3 | 590 | unclassified | major7 |
| m. 26, beat 3 to m. 26, beat 4 | 590 | unclassified | I |
| m. 27, beat 4 to m. 28, beat 1 | 605 | unclassified | half-diminished7 |
| m. 27, beat 4 to m. 28, beat 1 | 611 | unclassified | half-diminished7 |
| m. 28, beat 3 to m. 28, beat 4 | 620 | unclassified | minor7 |
| m. 28, beat 3 to m. 28, beat 4 | 626 | unclassified | minor7 |
| m. 28, beat 4 to m. 29, beat 1 | 626 | unclassified | minor7 |
| m. 28, beat 4 to m. 29, beat 1 | 631 | unclassified | minor7 |
| m. 29, beat 1 to m. 29, beat 2 | 633 | unclassified | iii |
| m. 29, beat 3 to m. 29, beat 4 | 642 | unclassified | minor7 |
| m. 29, beat 3 to m. 29, beat 4 | 644 | unclassified | minor7 |
| m. 29, beat 4 to m. 30, beat 1 | 641 | unclassified | dominant7 |
| m. 29, beat 4 to m. 30, beat 1 | 644 | retardation | dominant7 |
| m. 30, beat 2 to m. 30, beat 3 | 653 | unclassified | I |
| m. 30, beat 3 to m. 30, beat 4 | 657 | suspension | IV |
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
| m. 32, beat 4 to m. 33, beat 1 | 698 | retardation | I |
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
| m. 36, beat 3 to m. 36, beat 4 | 764 | unclassified | minor7 |
| m. 36, beat 3 to m. 36, beat 4 | 766 | unclassified | minor7 |
| m. 36, beat 4 to m. 37, beat 1 | 768 | unclassified | dominant7 |
| m. 36, beat 4 to m. 37, beat 1 | 772 | unclassified | dominant7 |
| m. 36, beat 4 to m. 37, beat 1 | 775 | neighbor | dominant7 |
| m. 37, beat 1 to m. 37, beat 2 | 779 | unclassified | iii |
| m. 37, beat 1 to m. 37, beat 2 | 780 | unclassified | iii |
| m. 37, beat 2 to m. 37, beat 3 | 777 | unclassified | ii |
| m. 37, beat 2 to m. 37, beat 3 | 780 | unclassified | ii |
| m. 37, beat 3 to m. 37, beat 4 | 784 | unclassified | minor7 |
| m. 37, beat 4 to m. 38, beat 1 | 786 | retardation | minor7 |
| m. 37, beat 4 to m. 38, beat 1 | 789 | unclassified | minor7 |
| m. 37, beat 4 to m. 38, beat 1 | 792 | unclassified | minor7 |
| m. 38, beat 1 to m. 38, beat 2 | 793 | unclassified | I |
| m. 38, beat 1 to m. 38, beat 2 | 798 | appoggiatura | I |
| m. 38, beat 2 to m. 38, beat 3 | 808 | passing | minor7 |
| m. 38, beat 2 to m. 38, beat 3 | 818 | passing | minor7 |
| m. 38, beat 3 to m. 38, beat 4 | 833 | passing | major7 |
| m. 38, beat 4 to m. 39, beat 1 | 838 | passing | I |
| m. 38, beat 4 to m. 39, beat 1 | 843 | passing | I |
| m. 39, beat 1 to m. 39, beat 2 | 856 | pedal | dominant7 |
| m. 39, beat 1 to m. 39, beat 2 | 861 | pedal | dominant7 |
| m. 39, beat 1 to m. 39, beat 2 | 858 | passing | dominant7 |
| m. 39, beat 1 to m. 39, beat 2 | 866 | anticipation | dominant7 |
| m. 39, beat 2 to m. 39, beat 3 | 869 | pedal | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 868 | neighbor | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 874 | pedal | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 873 | passing | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 879 | anticipation | major7 |
| m. 39, beat 3 to m. 39, beat 4 | 886 | pedal | dominant7 |
| m. 39, beat 3 to m. 39, beat 4 | 891 | pedal | dominant7 |
| m. 39, beat 3 to m. 39, beat 4 | 888 | passing | dominant7 |
| m. 39, beat 3 to m. 39, beat 4 | 896 | pedal | dominant7 |
| m. 39, beat 4 to m. 40, beat 1 | 901 | pedal | dominant7 |
| m. 39, beat 4 to m. 40, beat 1 | 898 | neighbor | dominant7 |
| m. 39, beat 4 to m. 40, beat 1 | 906 | pedal | dominant7 |
| m. 39, beat 4 to m. 40, beat 1 | 911 | pedal | dominant7 |
| m. 40, beat 1 to m. 40, beat 2 | 917 | appoggiatura | I |
| m. 40, beat 2 to m. 40, beat 3 | 927 | passing | minor7 |
| m. 40, beat 2 to m. 40, beat 3 | 937 | passing | minor7 |
| m. 40, beat 3 to m. 40, beat 4 | 952 | passing | major7 |
| m. 40, beat 4 to m. 41, beat 1 | 957 | passing | I |
| m. 40, beat 4 to m. 41, beat 1 | 962 | passing | I |
| m. 41, beat 1 to m. 41, beat 2 | 975 | pedal | dominant7 |
| m. 41, beat 1 to m. 41, beat 2 | 980 | pedal | dominant7 |
| m. 41, beat 1 to m. 41, beat 2 | 977 | passing | dominant7 |
| m. 41, beat 1 to m. 41, beat 2 | 985 | anticipation | dominant7 |
| m. 41, beat 2 to m. 41, beat 3 | 988 | pedal | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 987 | neighbor | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 993 | pedal | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 992 | passing | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 998 | anticipation | major7 |
| m. 41, beat 3 to m. 41, beat 4 | 1005 | pedal | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 1010 | pedal | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 1007 | passing | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 1015 | pedal | dominant7 |
| m. 41, beat 4 to m. 42, beat 1 | 1020 | pedal | dominant7 |
| m. 41, beat 4 to m. 42, beat 1 | 1017 | neighbor | dominant7 |
| m. 41, beat 4 to m. 42, beat 1 | 1025 | pedal | dominant7 |
| m. 41, beat 4 to m. 42, beat 1 | 1030 | anticipation | dominant7 |
| m. 42, beat 2 to m. 42, beat 3 | 1039 | unclassified | dominant7 |
| m. 42, beat 2 to m. 42, beat 3 | 1040 | neighbor | dominant7 |
| m. 42, beat 2 to m. 42, beat 3 | 1041 | unclassified | dominant7 |
| m. 42, beat 3 to m. 42, beat 3.33 | 1043 | unclassified | major7 |
| m. 42, beat 4 to m. 43, beat 1 | 1051 | unclassified | dominant7 |
| m. 42, beat 4 to m. 43, beat 1 | 1057 | unclassified | dominant7 |
| m. 42, beat 4 to m. 43, beat 1 | 1056 | escape-tone | dominant7 |
| m. 43, beat 1 to m. 43, beat 1.33 | 1059 | unclassified | major7 |
| m. 43, beat 2 to m. 43, beat 3 | 1069 | unclassified | dominant7 |
| m. 43, beat 2 to m. 43, beat 3 | 1071 | unclassified | dominant7 |

## Cadences

The library's standard-harmony analysis found 10 cadences from the chords, the meter and the soprano. Position is where the arrival begins. Key is the key region of the arrival. Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added before a section start.

| Position | Type | Key | Soprano | Beat strength | Boundary |
| --- | --- | --- | --- | --- | --- |
| m. 8, beat 1 | half | Bb major | 5 | strong | no |
| m. 19, beat 1 | half | Bb major | 4 | strong | no |
| m. 20, beat 1 | imperfect-authentic | Bb major | 1 | strong | no |
| m. 21, beat 1 | half | Bb major | 4 | strong | no |
| m. 22, beat 1 | imperfect-authentic | Bb major | 3 | strong | no |
| m. 34, beat 3 | half | Bb major | 1 | medium | no |
| m. 39, beat 1 | half | Bb major | 4 | strong | no |
| m. 40, beat 1 | imperfect-authentic | Bb major | 1 | strong | no |
| m. 41, beat 1 | half | Bb major | 4 | strong | no |
| m. 41, beat 3 | half | Bb major | 4 | medium | no |

### Cadence plan

1. m. 8, beat 1: half in Bb major
2. m. 19, beat 1: half in Bb major
3. m. 20, beat 1: imperfect-authentic in Bb major
4. m. 21, beat 1: half in Bb major
5. m. 22, beat 1: imperfect-authentic in Bb major
6. m. 34, beat 3: half in Bb major
7. m. 39, beat 1: half in Bb major
8. m. 40, beat 1: imperfect-authentic in Bb major
9. m. 41, beat 1: half in Bb major
10. m. 41, beat 3: half in Bb major

**Counts:** half 7, imperfect-authentic 3.

## Phrases

The library's standard-form analysis found 9 phrases from the soprano, the meter and the cadences. Length counts the measures of the phrase proper; Ends is what closed it: its cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in the piece; Expansion is a lengthening or shortening of the phrase's ending, or an extension after its cadence, with the measures it adds or removes.

| Phrase | Measures | Length | Cadence | Ends | Role | Expansion |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 8 | 8 | half (m. 8, beat 1) | cadence | beginning | - |
| 2 | mm. 9 to 19 | 11 | half (m. 19, beat 1) | cadence | middle | internal +3 (mm. 13 to 19) |
| 3 | m. 20 | 1 | imperfect-authentic (m. 20, beat 1) | cadence | middle | - |
| 4 | m. 21 | 1 | half (m. 21, beat 1) | cadence | middle | - |
| 5 | m. 22 | 1 | imperfect-authentic (m. 22, beat 1) | cadence | middle | - |
| 6 | mm. 23 to 34 | 12 | half (m. 34, beat 3) | cadence | middle | internal +4 (mm. 27 to 34) |
| 7 | mm. 35 to 39 | 5 | half (m. 39, beat 1) | cadence | middle | - |
| 8 | m. 40 | 1 | imperfect-authentic (m. 40, beat 1) | cadence | middle | contraction -4 (m. 40) |
| 9 | m. 41 | 1 | half (m. 41, beat 1) | cadence | ending | external post-cadential-extension +2 (mm. 42 to 43) |

- **Phrase 1:** ideas basic-idea (mm. 1 to 2), contrasting-idea (mm. 3 to 4), contrasting-idea (mm. 5 to 6), contrasting-idea (mm. 7 to 8); subphrases mm. 1 to 4, mm. 5 to 8; phrase model Tb (m. 1, beat 1), PD (m. 7, beat 3), D (m. 8, beat 1)
- **Phrase 2:** ideas basic-idea (mm. 9 to 10), contrasting-idea (mm. 11 to 12), contrasting-idea (mm. 13 to 14), contrasting-idea (mm. 15 to 16), contrasting-idea (mm. 17 to 19); subphrases mm. 9 to 12, mm. 13 to 19; phrase model Tb (m. 9, beat 1), D (m. 19, beat 1)
- **Phrase 3:** ideas basic-idea (m. 20)
- **Phrase 4:** ideas basic-idea (m. 21); phrase model D (m. 21, beat 1)
- **Phrase 5:** ideas basic-idea (m. 22)
- **Phrase 6:** ideas basic-idea (mm. 23 to 24), contrasting-idea (mm. 25 to 26), contrasting-idea (mm. 27 to 28), contrasting-idea (mm. 29 to 30), contrasting-idea (mm. 31 to 32), contrasting-idea (mm. 33 to 34); subphrases mm. 23 to 26, mm. 27 to 34; phrase model Tb (m. 23, beat 1), D (m. 34, beat 3)
- **Phrase 7:** ideas basic-idea (mm. 35 to 36), contrasting-idea (mm. 37 to 39); phrase model D (m. 39, beat 1)
- **Phrase 8:** ideas basic-idea (m. 40)
- **Phrase 9:** ideas basic-idea (m. 41); phrase model D (m. 41, beat 1)

## Themes

The library's standard-form analysis found 4 themes from the phrases, their ideas and the cadences. Form is the library's name for the pairing of the two parts; Commonality is how common that pairing is; Traits are what the library finds in the continuation.

| Theme | Form | Commonality | Measures | Length | Parts | Traits |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | hybrid | slightly-less-common | mm. 1 to 8 | 8 | compound-basic-idea (mm. 1 to 4), continuation (mm. 5 to 8, half (m. 8, beat 1)) | - |
| 2 | hybrid | slightly-less-common | mm. 9 to 19 | 11 | compound-basic-idea (mm. 9 to 12), continuation (mm. 13 to 19, half (m. 19, beat 1)) | - |
| 3 | hybrid | slightly-less-common | mm. 23 to 34 | 12 | compound-basic-idea (mm. 23 to 26), continuation (mm. 27 to 34, half (m. 34, beat 3)) | - |
| 4 | hybrid | slightly-less-common | mm. 35 to 40 | 6 | antecedent (mm. 35 to 39, half (m. 39, beat 1)), continuation (m. 40, imperfect-authentic (m. 40, beat 1)) | fragmentation |

## Form plan

The kernel's form structure detection grouped the phrases into 4 sections, closing a section at each phrase that ends on a perfect or imperfect authentic cadence; the phrases after the last such cadence form the last section. Sections are lettered by first appearance, and a section whose opening theme returns an earlier section's material takes that letter with a prime. Key is the key the section opens in.

| Section | Measures | Key | Phrases | Closing cadence | Returns |
| --- | --- | --- | --- | --- | --- |
| A | mm. 1 to 20 | Bb major | 1 to 3 | imperfect-authentic (m. 20, beat 1) | - |
| B | mm. 21 to 22 | Bb major | 4 to 5 | imperfect-authentic (m. 22, beat 1) | - |
| A' | mm. 23 to 40 | Bb major | 6 to 8 | imperfect-authentic (m. 40, beat 1) | returns A (similarity 0.83) |
| C | mm. 41 to 43 | Bb major | 9 | half (m. 41, beat 1) | - |

### Thematic returns

A later theme recalls an earlier one when the kernel's thematic return detection finds the same contour and rhythm in a stretch of both. Similarity is the lower of the two scores. Themes that share a phrase are not compared.

| Later theme | Measures | Section | Earlier theme | Measures | Section | Similarity |
| --- | --- | --- | --- | --- | --- | --- |
| 2 | mm. 9 to 19 | A | 1 | mm. 1 to 8 | A | 1.00 |
| 3 | mm. 23 to 34 | A' | 1 | mm. 1 to 8 | A | 0.83 |
| 3 | mm. 23 to 34 | A' | 2 | mm. 9 to 19 | A | 0.83 |
| 4 | mm. 35 to 40 | A' | 2 | mm. 9 to 19 | A | 1.00 |

### Outline

- form (mm. 1 to 43)
  - section A (mm. 1 to 20, Bb major)
    - phrase 1 (mm. 1 to 8)
      - subphrase 1 (mm. 1 to 4)
        - idea basic-idea (mm. 1 to 2)
        - idea contrasting-idea (mm. 3 to 4)
      - subphrase 2 (mm. 5 to 8)
        - idea contrasting-idea (mm. 5 to 6)
        - idea contrasting-idea (mm. 7 to 8)
    - phrase 2 (mm. 9 to 19)
      - subphrase 1 (mm. 9 to 12)
        - idea basic-idea (mm. 9 to 10)
        - idea contrasting-idea (mm. 11 to 12)
      - subphrase 2 (mm. 13 to 19)
        - idea contrasting-idea (mm. 13 to 14)
        - idea contrasting-idea (mm. 15 to 16)
        - idea contrasting-idea (mm. 17 to 19)
    - phrase 3 (m. 20)
      - idea basic-idea (m. 20)
  - section B (mm. 21 to 22, Bb major)
    - phrase 4 (m. 21)
      - idea basic-idea (m. 21)
    - phrase 5 (m. 22)
      - idea basic-idea (m. 22)
  - section A' (mm. 23 to 40, Bb major)
    - phrase 6 (mm. 23 to 34)
      - subphrase 1 (mm. 23 to 26)
        - idea basic-idea (mm. 23 to 24)
        - idea contrasting-idea (mm. 25 to 26)
      - subphrase 2 (mm. 27 to 34)
        - idea contrasting-idea (mm. 27 to 28)
        - idea contrasting-idea (mm. 29 to 30)
        - idea contrasting-idea (mm. 31 to 32)
        - idea contrasting-idea (mm. 33 to 34)
    - phrase 7 (mm. 35 to 39)
      - idea basic-idea (mm. 35 to 36)
      - idea contrasting-idea (mm. 37 to 39)
    - phrase 8 (m. 40)
      - idea basic-idea (m. 40)
  - section C (mm. 41 to 43, Bb major)
    - phrase 9 (m. 41)
      - idea basic-idea (m. 41)

## Motives

The library's motive analysis found 10 motive families in the soprano, the highest sounding note at each attack. A family is a short figure that recurs; Notes is the length of the figure, Pattern whether its occurrences match by scale steps or by semitones, Measures the measures the occurrences start in, Phrases the phrases they lie in (an occurrence across a phrase boundary lies in none), and Transformations how many occurrences are of each kind the library names against the first occurrence (exact, real-transposed, tonal-transposed, inverted, retrograde, augmented, diminished, fragmented, varied).

| Family | Notes | Degrees | Pattern | Occurrences | Measures | Phrases | Transformations |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 5 | 2 3 2 2 5 | scale steps | 2 | 14, 36 | 2, 7 | exact 2 |
| 2 | 8 | b6 6 b7 7 1 b2 2 b3 | scale steps | 4 | 18, 20, 38, 40 | 2, 7 | exact 4 |
| 3 | 8 | 7 1 b2 2 b3 3 4 3 | scale steps | 4 | 18, 20, 38, 40 | 2, 7 | exact 4 |
| 4 | 3 | 7 1 b2 | scale steps | 6 | 18, 20, 38, 40, 42, 43 | 2, 7, 9 | exact 4, tonal-transposed 2 |
| 5 | 8 | 2 b3 3 4 3 2 b2 2 | scale steps | 8 | 18, 19, 20, 21, 38, 39, 40, 41 | 9 | exact 4, fragmented 4 |
| 6 | 8 | 4 3 2 b2 2 3 4 3 | scale steps | 4 | 19, 21, 39, 41 | 9 | exact 4 |
| 7 | 8 | b2 2 3 4 3 2 b2 2 | scale steps | 4 | 19, 21, 39, 41 | 9 | exact 4 |
| 8 | 8 | 4 3 2 b2 2 5 1 #4 | scale steps | 4 | 19, 21, 39, 41 | 9 | exact 2, fragmented 2 |
| 9 | 8 | b2 2 5 1 #4 5 b6 6 | scale steps | 2 | 19, 39 | - | exact 2 |
| 10 | 8 | 1 #4 5 b6 6 b7 7 1 | scale steps | 2 | 20, 40 | - | exact 2 |

## Melody

The library's melody adapter read the soprano of each phrase as a line of notes: the soprano notes that start in the phrase. Range is the lowest and highest note and the semitones between them; Climax is the first note at the highest pitch, with how many notes sound it when it is not one; Contour counts the intervals between consecutive notes that go up, go down and repeat, and how many of them are steps and leaps.

| Phrase | Measures | Notes | Range | Climax | Up | Down | Same | Steps | Leaps |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 8 | 21 | F4 to G5 (14) | G5, m. 1, beat 4 | 10 | 9 | 1 | 12 | 7 |
| 2 | mm. 9 to 19 | 28 | F4 to G5 (14) | G5, m. 9, beat 4 | 13 | 11 | 3 | 12 | 12 |
| 3 | m. 20 | 1 | F5 to F5 (0) | F5, m. 19, beat 4.67 | 0 | 0 | 0 | 0 | 0 |
| 4 | m. 21 | 1 | D5 to D5 (0) | D5, m. 20, beat 4.67 | 0 | 0 | 0 | 0 | 0 |
| 5 | m. 22 | 1 | F5 to F5 (0) | F5, m. 21, beat 4.67 | 0 | 0 | 0 | 0 | 0 |
| 6 | mm. 23 to 34 | 38 | F4 to C6 (19) | C6, m. 24, beat 3.5 | 16 | 16 | 5 | 20 | 12 |
| 7 | mm. 35 to 39 | 14 | G#4 to A#6 (26) | A#6, m. 36, beat 2.67 | 5 | 7 | 0 | 6 | 5 |
| 8 | m. 40 | 1 | F5 to F5 (0) | F5, m. 39, beat 4.67 | 0 | 0 | 0 | 0 | 0 |
| 9 | mm. 41 to 43 | 11 | A#4 to A#5 (12) | A#5, m. 42, beat 3 | 6 | 3 | 0 | 5 | 4 |

### Leaps

A leap is an interval of a third or larger. The library's rule asks a leap of a fourth or larger to be followed by a step in the opposite direction: such a leap is Recovered when one follows, Unrecovered when a step the same way, a repeated note or another leap does, and Not judged when nothing follows it; a third asks nothing (Not required). Consecutive counts the pairs of leaps in a row the library's rule finds fault with.

| Phrase | Leaps | Recovered | Unrecovered | Not required | Not judged | Consecutive |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 7 | 1 | 5 | 1 | 0 | 0 |
| 2 | 12 | 1 | 6 | 5 | 0 | 2 |
| 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| 4 | 0 | 0 | 0 | 0 | 0 | 0 |
| 5 | 0 | 0 | 0 | 0 | 0 | 0 |
| 6 | 12 | 2 | 6 | 4 | 0 | 0 |
| 7 | 5 | 2 | 1 | 2 | 0 | 1 |
| 8 | 0 | 0 | 0 | 0 | 0 | 0 |
| 9 | 4 | 0 | 3 | 1 | 0 | 2 |

### Non-chord tones

Each melody note is set against the chord of the segment it starts in, as the library's chord identification reads it: a Chord tone is one of the chord's tones, a Non-chord tone is one it left over and named, and Unclassified notes are in a segment with no chord or were left unnamed. By type counts the non-chord tones under the library's names.

| Phrase | Notes | Chord tones | Non-chord tones | Unclassified | By type |
| --- | --- | --- | --- | --- | --- |
| 1 | 21 | 18 | 1 | 2 | appoggiatura 1 |
| 2 | 28 | 17 | 3 | 8 | passing 1, anticipation 1, appoggiatura 1 |
| 3 | 1 | 1 | 0 | 0 | - |
| 4 | 1 | 0 | 0 | 1 | - |
| 5 | 1 | 1 | 0 | 0 | - |
| 6 | 38 | 23 | 6 | 9 | passing 4, neighbor 1, appoggiatura 1 |
| 7 | 14 | 5 | 2 | 7 | passing 1, appoggiatura 1 |
| 8 | 1 | 1 | 0 | 0 | - |
| 9 | 11 | 8 | 2 | 1 | neighbor 2 |

### Tendencies

The library's melodic tendencies are soft preferences a well-formed line usually shows: Pitch proximity (steps outnumber leaps, small leaps outnumber large ones), Step declination (descending steps outnumber ascending ones), Step inertia (the line continues in its direction at least as often as it reverses), Melodic regression (extreme notes turn back toward the middle) and Melodic arch (the line rises to a climax in its middle half and ends below it); a phrase either shows a tendency or does not. The seventh degree column counts the notes the library labels the seventh degree that another note follows, judged by its leading tone rule: a leading tone goes up to the tonic by step, and a seventh a whole step below the tonic does not. Followed is the count that did, Broken the count that did not, with where each broken one stands.

| Phrase | Pitch proximity | Step declination | Step inertia | Melodic regression | Melodic arch | Seventh degree followed | Seventh degree broken |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | does not hold | holds | does not hold | does not hold | does not hold | 0 | 1 (m. 5, beat 4) |
| 2 | does not hold | holds | does not hold | does not hold | does not hold | 1 | 0 |
| 3 | holds | holds | holds | holds | does not hold | 0 | 0 |
| 4 | holds | holds | holds | holds | does not hold | 0 | 0 |
| 5 | holds | holds | holds | holds | does not hold | 0 | 0 |
| 6 | does not hold | does not hold | holds | holds | does not hold | 0 | 1 (m. 28, beat 3.75) |
| 7 | does not hold | holds | holds | does not hold | holds | 1 | 0 |
| 8 | holds | holds | holds | holds | does not hold | 0 | 0 |
| 9 | does not hold | holds | does not hold | does not hold | holds | 0 | 1 (m. 42, beat 2.67) |

### Development

Each family's occurrences in time order, as the measure they start in and the kind the library names against the family's prototype, which is the original.

- **Family 1:** m14 original > m36 exact
- **Family 2:** m18 original > m20 exact > m38 exact > m40 exact
- **Family 3:** m18 original > m20 exact > m38 exact > m40 exact
- **Family 4:** m18 original > m20 exact > m38 exact > m40 exact > m42 tonal-transposed > m43 tonal-transposed
- **Family 5:** m18 original > m19 fragmented > m20 exact > m21 fragmented > m38 exact > m39 fragmented > m40 exact > m41 fragmented
- **Family 6:** m19 original > m21 exact > m39 exact > m41 exact
- **Family 7:** m19 original > m21 exact > m39 exact > m41 exact
- **Family 8:** m19 original > m21 fragmented > m39 exact > m41 fragmented
- **Family 9:** m19 original > m39 exact
- **Family 10:** m20 original > m40 exact

## Progression patterns

23 recurring progressions of two to six chords, counted over the library's Roman numerals: a held chord counts once, and a progression does not cross an unlabelled chord or a change of key. Ranked by count, then length, then first occurrence; the top 10 are shown. Measures are where each occurrence begins and ends.

| Rank | Progression | Count | Measures |
| --- | --- | --- | --- |
| 1 | V7 > I | 9 | m. 8, mm. 8 to 9, mm. 14 to 15, mm. 19 to 20, mm. 21 to 23, mm. 29 to 30, mm. 39 to 40, mm. 41 to 42, m. 43 |
| 2 | I7 > I | 7 | m. 18, m. 18, m. 20, mm. 24 to 25, mm. 26 to 27, m. 38, m. 40 |
| 3 | V7 > I7 | 6 | m. 19, m. 21, m. 39, m. 41, m. 42, mm. 42 to 43 |
| 4 | I > V7 | 5 | m. 8, mm. 18 to 19, mm. 20 to 21, mm. 38 to 39, mm. 40 to 41 |
| 5 | I > vi7 | 5 | m. 9, m. 18, m. 20, m. 38, m. 40 |
| 6 | I > vi7 > I7 > I > V7 > I7 | 4 | mm. 18 to 19, mm. 20 to 21, mm. 38 to 39, mm. 40 to 41 |
| 7 | vi7 > I7 > I > V7 > I7 > V7 | 4 | mm. 18 to 19, mm. 20 to 21, mm. 38 to 39, mm. 40 to 41 |
| 8 | I7 > I > V7 > I7 > V7 > I | 4 | mm. 18 to 20, mm. 20 to 23, mm. 38 to 40, mm. 40 to 42 |
| 9 | vi > vi7 > I | 3 | mm. 1 to 3, mm. 9 to 11, mm. 23 to 24 |
| 10 | I > vii° > I | 3 | mm. 2 to 5, mm. 10 to 12, mm. 25 to 26 |

## Harmonic rhythm

How often the harmony changes. A change is a chord whose base Roman numeral or key differs from the chord before it, counted in the measure it starts in. Changes lists the measures of each row in order; Mean is changes per measure over the row.

| Measures | Changes | Mean |
| --- | --- | --- |
| mm. 1 to 4 | 3, 2, 1, 1 | 1.75 |
| mm. 5 to 8 | 2, 3, 3, 4 | 3 |
| mm. 9 to 12 | 3, 2, 1, 1 | 1.75 |
| mm. 13 to 16 | 2, 3, 4, 2 | 2.75 |
| mm. 17 to 20 | 3, 5, 3, 4 | 3.75 |
| mm. 21 to 24 | 3, 1, 2, 3 | 2.25 |
| mm. 25 to 28 | 2, 3, 2, 2 | 2.25 |
| mm. 29 to 32 | 4, 4, 3, 4 | 3.75 |
| mm. 33 to 36 | 2, 2, 3, 3 | 2.5 |
| mm. 37 to 40 | 4, 4, 3, 4 | 3.75 |
| mm. 41 to 43 | 3, 6, 4 | 4.33 |

**Mean:** 2.86 changes per measure over 43 measures.

### Approach to cadences

Changes per measure over the two measures before each cadence's arrival (Approach), against the two measures before those (Before). Faster is yes when the approach changes harmony more often.

| Cadence | Arrival | Before | Approach | Faster |
| --- | --- | --- | --- | --- |
| 1 | m. 8 | 1.5 | 3 | yes |
| 2 | m. 19 | 3 | 4 | yes |
| 3 | m. 20 | 2.5 | 4 | yes |
| 4 | m. 21 | 4 | 3.5 | no |
| 5 | m. 22 | 4 | 3.5 | no |
| 6 | m. 34 | 3.5 | 3 | no |
| 7 | m. 39 | 3 | 4 | yes |
| 8 | m. 40 | 3.5 | 3.5 | no |
| 9 | m. 41 | 4 | 3.5 | no |
| 10 | m. 41 | 4 | 3.5 | no |

## Rhythm and meter

**Meter fit:** 12/8, scale 1, no pickup (fit 0.80); no better reading found

### Density

The library's density profile of each phrase, for all its notes and for each line that has a note in it (the soprano and bass of the outer voices, and Other for the rest). A note is in a phrase when its onset is; a chord is one attack. Attacks per beat is read measure by measure over the beats the phrase spans in it, as a mean and the largest; Mean gap is the mean distance between attacks in whole notes; On the beat is the share of attacks exactly on a beat. Durations lists the notes' lengths in whole notes, shortest first, with how many notes have each. The patterns that recur are the motive families' rhythms (see Motives).

| Phrase | Measures | Line | Notes | Attacks | Per beat | Max per beat | Mean gap | On the beat | Durations |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 8 | all | 110 | 88 | 3.03 | 3.25 | 0.12 | 33.0% | 1/16 (2), 1/8 (88), 11/32 (3), 3/8 (10), 5/8 (2), 3/4 (5) |
| 1 | mm. 1 to 8 | soprano | 21 | 21 | 0.75 | 1 | 0.53 | 90.5% | 1/16 (2), 1/8 (2), 11/32 (2), 3/8 (9), 5/8 (1), 3/4 (5) |
| 1 | mm. 1 to 8 | bass | 28 | 28 | 0.97 | 1 | 0.39 | 53.6% | 1/8 (28) |
| 1 | mm. 1 to 8 | other | 61 | 61 | 2.09 | 2.5 | 0.18 | 24.6% | 1/8 (58), 11/32 (1), 3/8 (1), 5/8 (1) |
| 2 | mm. 9 to 19 | all | 220 | 127 | 3.09 | 3.75 | 0.12 | 32.3% | 1/32 (3), 1/16 (2), 3/32 (1), 1/8 (182), 1/4 (3), 5/16 (1), 11/32 (1), 3/8 (18), 19/32 (1), 5/8 (2), 23/32 (1), 3/4 (3), 17/16 (1), 37/32 (1) |
| 2 | mm. 9 to 19 | soprano | 29 | 29 | 0.73 | 1 | 0.54 | 62.1% | 1/32 (1), 1/16 (2), 1/8 (11), 1/4 (1), 3/8 (8), 19/32 (1), 23/32 (1), 3/4 (3), 37/32 (1) |
| 2 | mm. 9 to 19 | bass | 39 | 39 | 0.95 | 1 | 0.4 | 59.0% | 1/8 (36), 3/8 (3) |
| 2 | mm. 9 to 19 | other | 152 | 101 | 2.5 | 3 | 0.15 | 25.7% | 1/32 (2), 3/32 (1), 1/8 (135), 1/4 (2), 5/16 (1), 11/32 (1), 3/8 (7), 5/8 (2), 17/16 (1) |
| 3 | m. 20 | all | 14 | 3 | 3 | 3 | 0.12 | 33.3% | 1/8 (14) |
| 3 | m. 20 | soprano | 1 | 1 | 1 | 1 | 0 | 100.0% | 1/8 (1) |
| 3 | m. 20 | bass | 1 | 1 | 1 | 1 | 0 | 0.0% | 1/8 (1) |
| 3 | m. 20 | other | 12 | 3 | 3 | 3 | 0.12 | 33.3% | 1/8 (12) |
| 4 | m. 21 | all | 15 | 3 | 3 | 3 | 0.12 | 33.3% | 1/8 (15) |
| 4 | m. 21 | soprano | 1 | 1 | 1 | 1 | 0 | 100.0% | 1/8 (1) |
| 4 | m. 21 | bass | 1 | 1 | 1 | 1 | 0 | 0.0% | 1/8 (1) |
| 4 | m. 21 | other | 13 | 3 | 3 | 3 | 0.12 | 33.3% | 1/8 (13) |
| 5 | m. 22 | all | 14 | 5 | 1.67 | 1.67 | 0.19 | 60.0% | 35/512 (1), 3/32 (1), 1/8 (7), 3/8 (5) |
| 5 | m. 22 | soprano | 1 | 1 | 0.33 | 0.33 | 0 | 100.0% | 1/8 (1) |
| 5 | m. 22 | bass | 1 | 1 | 0.33 | 0.33 | 0 | 100.0% | 3/8 (1) |
| 5 | m. 22 | other | 12 | 5 | 1.67 | 1.67 | 0.19 | 60.0% | 35/512 (1), 3/32 (1), 1/8 (6), 3/8 (4) |
| 6 | mm. 23 to 34 | all | 203 | 157 | 3.33 | 4.5 | 0.11 | 29.9% | 1/16 (13), 3/32 (9), 1/8 (151), 11/32 (5), 3/8 (14), 201/512 (1), 1/2 (1), 9/16 (1), 5/8 (1), 3/4 (7) |
| 6 | mm. 23 to 34 | soprano | 37 | 37 | 0.78 | 1.25 | 0.46 | 64.9% | 1/16 (5), 3/32 (3), 1/8 (6), 11/32 (2), 3/8 (11), 1/2 (1), 9/16 (1), 5/8 (1), 3/4 (7) |
| 6 | mm. 23 to 34 | bass | 47 | 47 | 1.01 | 1.33 | 0.38 | 68.1% | 1/8 (47) |
| 6 | mm. 23 to 34 | other | 119 | 110 | 2.33 | 3.25 | 0.16 | 19.1% | 1/16 (8), 3/32 (6), 1/8 (98), 11/32 (3), 3/8 (3), 201/512 (1) |
| 7 | mm. 35 to 39 | all | 135 | 58 | 3.35 | 4.75 | 0.11 | 29.3% | 1/32 (3), 1/16 (8), 3/32 (1), 1/8 (116), 1/4 (1), 5/16 (1), 3/8 (3), 1/2 (1), 3/4 (1) |
| 7 | mm. 35 to 39 | soprano | 13 | 13 | 0.8 | 1 | 0.5 | 38.5% | 1/16 (3), 1/8 (6), 1/4 (1), 3/8 (1), 1/2 (1), 3/4 (1) |
| 7 | mm. 35 to 39 | bass | 17 | 17 | 1 | 1 | 0.39 | 58.8% | 1/8 (17) |
| 7 | mm. 35 to 39 | other | 105 | 50 | 2.95 | 4.25 | 0.12 | 24.0% | 1/32 (3), 1/16 (5), 3/32 (1), 1/8 (93), 5/16 (1), 3/8 (2) |
| 8 | m. 40 | all | 14 | 3 | 3 | 3 | 0.12 | 33.3% | 1/8 (14) |
| 8 | m. 40 | soprano | 1 | 1 | 1 | 1 | 0 | 100.0% | 1/8 (1) |
| 8 | m. 40 | bass | 1 | 1 | 1 | 1 | 0 | 0.0% | 1/8 (1) |
| 8 | m. 40 | other | 12 | 3 | 3 | 3 | 0.12 | 33.3% | 1/8 (12) |
| 9 | mm. 41 to 43 | all | 111 | 34 | 2.83 | 3 | 0.12 | 32.4% | 1/8 (103), 3/16 (1), 3/8 (1), 7/16 (1), 59/128 (1), 123/256 (2), 1/2 (2) |
| 9 | mm. 41 to 43 | soprano | 11 | 11 | 0.92 | 1.25 | 0.38 | 54.5% | 1/8 (10), 7/16 (1) |
| 9 | mm. 41 to 43 | bass | 11 | 11 | 0.92 | 1 | 0.35 | 27.3% | 1/8 (9), 3/8 (1), 1/2 (1) |
| 9 | mm. 41 to 43 | other | 89 | 29 | 2.42 | 3 | 0.14 | 37.9% | 1/8 (84), 3/16 (1), 59/128 (1), 123/256 (2), 1/2 (1) |

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
| 1 | mm. 1 to 8 | 4 | 1, 5 | 2 regular | m. 8 | 1 |
| 2 | mm. 9 to 19 | 4 | 9, 13 | 1 regular, 1 expanded | m. 16 | 1 |
| 3 | m. 20 | 4 | 20 | 1 short | m. 20 | 0.5 |
| 4 | m. 21 | 4 | 21 | 1 short | m. 21 | 0.5 |
| 5 | m. 22 | 4 | 22 | 1 short | m. 22 | 0.5 |
| 6 | mm. 23 to 34 | 4 | 23, 27 | 1 regular, 1 expanded | m. 30 | 1 |
| 7 | mm. 35 to 39 | 4 | 35, 39 | 1 regular, 1 short | m. 39 | 0.9 |
| 8 | m. 40 | 4 | 40, 44 | 1 regular, 1 short | m. 40 | 0.4 |
| 9 | mm. 41 to 43 | 4 | 41, 42 | 1 short, 1 extension | m. 41 | 0.5 |

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

4 voices over 169 chord segments, 531 motions between consecutive segments: 96 contrary, 80 similar, 4 parallel, 266 oblique, 85 static.

| Voices | Contrary | Similar | Parallel | Oblique | Static |
| --- | --- | --- | --- | --- | --- |
| 0 and 1 | 15 | 12 | 0 | 51 | 4 |
| 0 and 2 | 14 | 12 | 2 | 59 | 2 |
| 0 and 3 | 9 | 14 | 1 | 58 | 2 |
| 1 and 2 | 18 | 9 | 0 | 42 | 22 |
| 1 and 3 | 17 | 15 | 1 | 32 | 23 |
| 2 and 3 | 23 | 18 | 0 | 24 | 32 |

0 parallel fifths, 0 parallel octaves, 2 direct fifths or octaves in the outer voices, 28 unresolved sevenths, 2 unresolved leading tones.

| Location | Fault | Voices |
| --- | --- | --- |
| m. 7, beat 3 to m. 8, beat 1 | direct-perfects | 0 and 3 |
| m. 37, beat 2 to m. 37, beat 4 | direct-perfects | 0 and 3 |
| m. 2, beat 1 to m. 2, beat 3 | seventh-resolves-down | 2 |
| m. 8, beat 2 to m. 8, beat 4 | leading-tone-resolves | 2 |
| m. 8, beat 2 to m. 8, beat 4 | seventh-resolves-down | 3 |
| m. 11, beat 4 to m. 12, beat 2 | leading-tone-resolves | 3 |
| m. 14, beat 2 to m. 14, beat 4 | seventh-resolves-down | 3 |
| m. 14, beat 4 to m. 15, beat 2 | seventh-resolves-down | 3 |
| m. 15, beat 2 to m. 15, beat 4 | seventh-resolves-down | 2 |
| m. 15, beat 3 to m. 16, beat 1 | seventh-resolves-down | 3 |
| m. 17, beat 3 to m. 18, beat 1 | seventh-resolves-down | 3 |
| m. 18, beat 2 to m. 18, beat 4 | seventh-resolves-down | 2 |
| m. 18, beat 3 to m. 19, beat 1 | seventh-resolves-down | 0 |
| m. 19, beat 2 to m. 19, beat 4 | seventh-resolves-down | 0 |
| m. 20, beat 2 to m. 20, beat 4 | seventh-resolves-down | 2 |
| m. 20, beat 3 to m. 21, beat 1 | seventh-resolves-down | 0 |
| m. 21, beat 2 to m. 21, beat 4 | seventh-resolves-down | 0 |
| m. 28, beat 3 to m. 29, beat 1 | seventh-resolves-down | 1 |
| m. 28, beat 4 to m. 29, beat 2 | seventh-resolves-down | 2 |
| m. 31, beat 1 to m. 31, beat 3 | seventh-resolves-down | 0 |
| m. 31, beat 1 to m. 31, beat 3 | seventh-resolves-down | 3 |
| m. 36, beat 2 to m. 36, beat 4 | seventh-resolves-down | 3 |
| m. 36, beat 4 to m. 37, beat 2 | seventh-resolves-down | 3 |
| m. 37, beat 3 to m. 38, beat 1 | seventh-resolves-down | 3 |
| m. 37, beat 4 to m. 38, beat 2 | seventh-resolves-down | 2 |
| m. 38, beat 2 to m. 38, beat 4 | seventh-resolves-down | 2 |
| m. 38, beat 3 to m. 39, beat 1 | seventh-resolves-down | 0 |
| m. 39, beat 2 to m. 39, beat 4 | seventh-resolves-down | 0 |
| m. 40, beat 2 to m. 40, beat 4 | seventh-resolves-down | 2 |
| m. 40, beat 3 to m. 41, beat 1 | seventh-resolves-down | 0 |
| m. 41, beat 2 to m. 41, beat 4 | seventh-resolves-down | 0 |
| m. 42, beat 4 to m. 43, beat 1.33 | seventh-resolves-down | 0 |

127 chords with three or more voices sounding: the root is doubled in 36, the third in 10, the fifth in 10, the seventh in 0, no member doubled in 71. Doubling is counted, not judged. Spacing is judged by the library for the 83 chords with four voices all sounding: 44 too wide.

| Location | Chord | Gap too wide |
| --- | --- | --- |
| m. 1, beat 4 to m. 2, beat 1 | 2 | voices 1 and 2 |
| m. 2, beat 3 to m. 2, beat 4 | 5 | voices 0 and 1 |
| m. 3, beat 3 to m. 3, beat 4 | 9 | voices 1 and 2 |
| m. 4, beat 3 to m. 4, beat 4 | 13 | voices 1 and 2 |
| m. 7, beat 3 to m. 7, beat 4 | 25 | voices 1 and 2 |
| m. 7, beat 4 to m. 8, beat 1 | 26 | voices 1 and 2 |
| m. 8, beat 1 to m. 8, beat 2 | 27 | voices 1 and 2 |
| m. 9, beat 1 to m. 9, beat 3 | 31 | voices 1 and 2 |
| m. 9, beat 3 to m. 9, beat 4 | 32 | voices 1 and 2 |
| m. 9, beat 4 to m. 10, beat 1 | 33 | voices 1 and 2 |
| m. 10, beat 1 to m. 10, beat 2 | 34 | voices 0 and 1 |
| m. 11, beat 3 to m. 11, beat 4 | 40 | voices 1 and 2 |
| m. 12, beat 3 to m. 12, beat 4 | 44 | voices 1 and 2 |
| m. 13, beat 3 to m. 13, beat 4 | 48 | voices 1 and 2 |
| m. 14, beat 2 to m. 14, beat 3 | 51 | voices 1 and 2 |
| m. 15, beat 2 to m. 15, beat 3 | 55 | voices 0 and 1 |
| m. 15, beat 3 to m. 15, beat 4 | 56 | voices 1 and 2 |
| m. 15, beat 4 to m. 16, beat 1 | 57 | voices 1 and 2 |
| m. 16, beat 4 to m. 17, beat 1 | 61 | voices 1 and 2 |
| m. 17, beat 3 to m. 17, beat 4 | 63 | voices 0 and 1 |
| m. 17, beat 4 to m. 18, beat 1 | 64 | voices 0 and 1 |
| m. 18, beat 4 to m. 19, beat 1 | 69 | voices 0 and 1 |
| m. 20, beat 4 to m. 21, beat 1 | 77 | voices 0 and 1 |
| m. 24, beat 3 to m. 24, beat 4 | 89 | voices 1 and 2 |
| m. 25, beat 3 to m. 25, beat 4 | 93 | voices 0 and 1 |
| m. 26, beat 3 to m. 26, beat 4 | 97 | voices 1 and 2 |
| m. 27, beat 4 to m. 28, beat 1 | 102 | voices 1 and 2 |
| m. 28, beat 4 to m. 29, beat 1 | 106 | voices 1 and 2 |
| m. 29, beat 3 to m. 29, beat 4 | 109 | voices 0 and 1 |
| m. 29, beat 4 to m. 30, beat 1 | 110 | voices 1 and 2 |
| m. 31, beat 1 to m. 31, beat 2 | 115 | voices 1 and 2 |
| m. 31, beat 4 to m. 32, beat 1 | 118 | voices 1 and 2 |
| m. 32, beat 1 to m. 32, beat 2 | 119 | voices 1 and 2 |
| m. 32, beat 4 to m. 33, beat 1 | 122 | voices 0 and 1 |
| m. 35, beat 2 to m. 35, beat 3 | 132 | voices 1 and 2 |
| m. 35, beat 3 to m. 35, beat 4 | 133 | voices 0 and 1 |
| m. 36, beat 2 to m. 36, beat 3 | 136 | voices 1 and 2 |
| m. 36, beat 3 to m. 36, beat 4 | 137 | voices 0 and 1 |
| m. 37, beat 3 to m. 37, beat 4 | 141 | voices 1 and 2 |
| m. 37, beat 4 to m. 38, beat 1 | 142 | voices 1 and 2 |
| m. 38, beat 4 to m. 39, beat 1 | 146 | voices 0 and 1 |
| m. 40, beat 4 to m. 41, beat 1 | 154 | voices 0 and 1 |
| m. 42, beat 3 to m. 42, beat 3.33 | 162 | voices 1 and 2 |
| m. 42, beat 4 to m. 43, beat 1 | 164 | voices 0 and 1 |

## Markings

The tempo and dynamics markings the score notates, read with the library's standard-markings rules. A tempo holds until the next one, a dynamic until the next in its part. Group and the loudness comparison are the library's; a text it does not recognise has none. Wedges are not read, and articulations are not imported.

The score has no tempo marking.

The score has no dynamics marking.
