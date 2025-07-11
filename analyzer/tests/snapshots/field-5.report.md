# Analysis Report: Field 5.mid

## Summary

- **File:** Field 5.mid, 1083 notes.
- **Input:** MIDI file. Meter: embedded in the file. Key signature: embedded in the file. Pitch spelling: by rule.
- **Key:** Bb major, the key with the most time across 5 key regions.
- **Chords:** 169 chord events; 157 matched a chord (93%), 12 did not.
- **Most frequent chords:** I (58), minor7 (28), dominant7 (23).

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
| m. 1, beat 1 to m. 1, beat 3 | I | I (in Bb major) | tonic | - | Bb | major | Bb | root position | 2 | 0.25 | - |
| m. 1, beat 3 to m. 1, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 1, beat 4 to m. 2, beat 1 | vi | vi6 | predominant (weak) | - | G | minor | Bb | first | 1 | 0.90 | - |
| m. 2, beat 1 to m. 2, beat 2 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | Bb | first | 1 | 0.01 | I (Bb major, score 1.04) |
| m. 2, beat 2 to m. 2, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.19 | - |
| m. 2, beat 3 to m. 2, beat 4 | I | I6/4 | tonic | arpeggiating | Bb | major | F | second | 1 | 0.10 | - |
| m. 2, beat 4 to m. 3, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.38 | - |
| m. 3, beat 1 to m. 3, beat 2 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.49 | - |
| m. 3, beat 2 to m. 3, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.19 | - |
| m. 3, beat 3 to m. 3, beat 4 | vii° | vii° | dominant | - | A | diminished | Gb | - | 1 | 0.13 | - |
| m. 3, beat 4 to m. 4, beat 1 | vii° | vii° | dominant | - | A | diminished | A | root position | 1 | 0.21 | - |
| m. 4, beat 1 to m. 4, beat 2 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.21 | - |
| m. 4, beat 2 to m. 4, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.42 | - |
| m. 4, beat 3 to m. 4, beat 4 | I | I6/4 | tonic | - | Bb | major | F | second | 1 | 0.50 | - |
| m. 4, beat 4 to m. 5, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.38 | - |
| m. 5, beat 1 to m. 5, beat 2 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.49 | - |
| m. 5, beat 2 to m. 5, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.19 | - |
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
| m. 8, beat 2 to m. 8, beat 3 | dominant7 | V4/2 | dominant | - | F | dominant7 | Eb | third | 1 | 0.38 | - |
| m. 8, beat 3 to m. 8, beat 4 | I | I6 | tonic | - | Bb | major | D | first | 1 | 0.32 | - |
| m. 8, beat 4 to m. 9, beat 1 | dominant7 | V4/3 | dominant | - | F | dominant7 | C | second | 1 | 0.19 | - |
| m. 9, beat 1 to m. 9, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 2 | 0.48 | - |
| m. 9, beat 3 to m. 9, beat 4 | minor7 | vi7 | predominant (weak) | - | G | minor7 | G | root position | 1 | 0.54 | - |
| m. 9, beat 4 to m. 10, beat 1 | vi | vi6 | predominant (weak) | - | G | minor | Bb | first | 1 | 0.56 | - |
| m. 10, beat 1 to m. 10, beat 2 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | Bb | first | 1 | 0.00 | I (Bb major, score 1.05) |
| m. 10, beat 2 to m. 10, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.54 | - |
| m. 10, beat 3 to m. 10, beat 4 | I | I6/4 | tonic | arpeggiating | Bb | major | F | second | 1 | 0.34 | - |
| m. 10, beat 4 to m. 11, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.38 | - |
| m. 11, beat 1 to m. 11, beat 2 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.49 | - |
| m. 11, beat 2 to m. 11, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.19 | - |
| m. 11, beat 3 to m. 11, beat 4 | vii° | vii° | dominant | - | A | diminished | Gb | - | 1 | 0.13 | - |
| m. 11, beat 4 to m. 12, beat 1 | vii° | vii° | dominant | - | A | diminished | A | root position | 1 | 0.21 | - |
| m. 12, beat 1 to m. 12, beat 2 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.21 | - |
| m. 12, beat 2 to m. 12, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.42 | - |
| m. 12, beat 3 to m. 12, beat 4 | I | I6/4 | tonic | - | Bb | major | F | second | 1 | 0.50 | - |
| m. 12, beat 4 to m. 13, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.38 | - |
| m. 13, beat 1 to m. 13, beat 2 | IV | IV6 | predominant (strong) | - | Eb | major | G | first | 1 | 0.66 | - |
| m. 13, beat 2 to m. 13, beat 3 | IV | IV6/4 | predominant (strong) | - | Eb | major | Bb | second | 1 | 0.69 | - |
| m. 13, beat 3 to m. 13, beat 4 | I | I6/4 | tonic | - | Bb | major | F | second | 1 | 0.11 | - |
| m. 13, beat 4 to m. 14, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.17 | - |
| m. 14, beat 1 to m. 14, beat 2 | I | I | tonic | - | Bb | major | E | - | 1 | 0.21 | - |
| m. 14, beat 2 to m. 14, beat 3 | minor7 | ii4/2 | predominant (strong) | - | C | minor7 | Bb | third | 1 | 0.43 | - |
| m. 14, beat 3 to m. 14, beat 4 | minor7 | V6/5/V | dominant of 5 | - | C | minor7 | E | - | 1 | 0.58 | - |
| m. 14, beat 4 to m. 15, beat 1 | dominant7 | V4/2 | dominant | - | F | dominant7 | Eb | third | 1 | 0.15 | - |
| m. 15, beat 1 to m. 15, beat 2 | I | I6 | tonic | - | Bb | major | D | first | 1 | 0.07 | minor7 (D minor7, score 0.97) |
| m. 15, beat 2 to m. 15, beat 3 | minor7 | ii6/5 | predominant (strong) | - | C | minor7 | Eb | first | 1 | 0.28 | - |
| m. 15, beat 3 to m. 15, beat 4 | minor7 | vi4/2 | predominant (weak) | - | G | minor7 | F | third | 1 | 0.00 | I (Bb major, score 1.04) |
| m. 15, beat 4 to m. 16, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 1 | 0.22 | - |
| m. 16, beat 1 to m. 16, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 16, beat 2 to m. 16, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 16, beat 3 to m. 16, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 16, beat 4 to m. 17, beat 1 | vii° | vii°6/4 | dominant | - | A | diminished | Eb | second | 1 | 0.17 | - |
| m. 17, beat 1 to m. 17, beat 3 | minor7 | iii7 | predominant (weak) | - | D | minor7 | D | root position | 2 | 0.00 | minor7 (G minor7, score 0.78) |
| m. 17, beat 3 to m. 17, beat 4 | minor7 | vi4/2 | predominant (weak) | - | G | minor7 | Eb | - | 1 | 0.00 | I (Bb major, score 0.99) |
| m. 17, beat 4 to m. 18, beat 1 | minor7 | iii6/5 | predominant (weak) | - | D | minor7 | F | first | 1 | 0.12 | - |
| m. 18, beat 1 to m. 18, beat 1.33 | major7 | I4/3 | tonic | - | Bb | major7 | F | second | 0.33 | 0.52 | - |
| m. 18, beat 1.33 to m. 18, beat 2 | I | I | tonic | - | Bb | major | Bb | root position | 0.67 | 0.50 | - |
| m. 18, beat 2 to m. 18, beat 3 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | Bb | first | 1 | 0.07 | I (Bb major, score 0.86) |
| m. 18, beat 3 to m. 18, beat 4 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 1 | 0.53 | - |
| m. 18, beat 4 to m. 19, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.44 | - |
| m. 19, beat 1 to m. 19, beat 2 | dominant7 | V7 | dominant | - | F | dominant7 | Bb | - | 1 | 0.05 | major7 (Bb major7, score 0.70) |
| m. 19, beat 2 to m. 19, beat 3 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 1 | 0.10 | dominant7 (F dominant7, score 0.65) |
| m. 19, beat 3 to m. 19, beat 4 | dominant7 | V7 | dominant | - | F | dominant7 | Bb | - | 1 | 0.03 | major7 (Bb major7, score 0.71) |
| m. 19, beat 4 to m. 20, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | Bb | - | 1 | 0.20 | - |
| m. 20, beat 1 to m. 20, beat 2 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.52 | - |
| m. 20, beat 2 to m. 20, beat 3 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | Bb | first | 1 | 0.07 | I (Bb major, score 0.86) |
| m. 20, beat 3 to m. 20, beat 4 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 1 | 0.53 | - |
| m. 20, beat 4 to m. 21, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.44 | - |
| m. 21, beat 1 to m. 21, beat 2 | dominant7 | V7 | dominant | - | F | dominant7 | Bb | - | 1 | 0.05 | major7 (Bb major7, score 0.70) |
| m. 21, beat 2 to m. 21, beat 3 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 1 | 0.10 | dominant7 (F dominant7, score 0.65) |
| m. 21, beat 3 to m. 21, beat 4 | dominant7 | V7 | dominant | - | F | dominant7 | Bb | - | 1 | 0.03 | major7 (Bb major7, score 0.71) |
| m. 21, beat 4 to m. 22, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | Bb | - | 1 | 0.20 | - |
| m. 22, beat 1 to m. 22, beat 4 | I | I | tonic | - | Bb | major | Bb | root position | 3 | 0.59 | - |
| m. 22, beat 4 to m. 23, beat 1 | I | I6/4 | tonic | - | Bb | major | F | second | 1 | 0.31 | - |
| m. 23, beat 1 to m. 23, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 2 | 0.25 | - |
| m. 23, beat 3 to m. 23, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 23, beat 4 to m. 24, beat 1 | vi | vi6 | predominant (weak) | - | G | minor | Bb | first | 1 | 0.90 | - |
| m. 24, beat 1 to m. 24, beat 2 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | Bb | first | 1 | 0.01 | I (Bb major, score 1.04) |
| m. 24, beat 2 to m. 24, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.19 | - |
| m. 24, beat 3 to m. 24, beat 4 | I | I6/4 | tonic | - | Bb | major | F | second | 1 | 0.07 | minor7 (D minor7, score 0.86) |
| m. 24, beat 4 to m. 25, beat 1 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 1 | 0.00 | minor7 (G minor7, score 0.82) |
| m. 25, beat 1 to m. 25, beat 2 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.14 | - |
| m. 25, beat 2 to m. 25, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.19 | - |
| m. 25, beat 3 to m. 25, beat 4 | vii° | vii° | dominant | - | A | diminished | Gb | - | 1 | 0.13 | - |
| m. 25, beat 4 to m. 26, beat 1 | vii° | vii° | dominant | - | A | diminished | A | root position | 1 | 0.22 | - |
| m. 26, beat 1 to m. 26, beat 2 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 1.00 | - |
| m. 26, beat 2 to m. 26, beat 3 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 1 | 0.48 | - |
| m. 26, beat 3 to m. 26, beat 4 | I | I6/4 | tonic | - | Bb | major | F | second | 1 | 0.50 | - |
| m. 26, beat 4 to m. 27, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.38 | - |
| m. 27, beat 1 to m. 27, beat 2 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.49 | - |
| m. 27, beat 2 to m. 27, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.18 | - |
| m. 27, beat 3 to m. 27, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 27, beat 4 to m. 28, beat 1 | half-diminished7 | V7/iii | dominant of 3 | - | A | half-diminished7 | A | root position | 1 | 0.99 | - |
| m. 28, beat 1 to m. 28, beat 2 | iii | iii | predominant (weak) | - | D | minor | D | root position | 1 | 0.35 | - |
| m. 28, beat 2 to m. 28, beat 3 | iii | iii6/4 | predominant (weak) | - | D | minor | A | second | 1 | 0.13 | - |
| m. 28, beat 3 to m. 28, beat 4 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | Bb | first | 1 | 0.04 | major7 (Bb major7, score 0.75) |
| m. 28, beat 4 to m. 29, beat 1 | minor7 | vi7 | predominant (weak) | - | G | minor7 | G | root position | 1 | 0.26 | - |
| m. 29, beat 1 to m. 29, beat 2 | iii | iii6 (in F major) | predominant (weak) | - | A | minor | C | first | 1 | 0.14 | - |
| m. 29, beat 2 to m. 29, beat 3 | I | I6 | tonic | - | F | major | A | first | 1 | 0.13 | - |
| m. 29, beat 3 to m. 29, beat 4 | minor7 | vii°4/2/V | dominant of 5 | - | D | minor7 | Ab | - | 1 | 0.81 | - |
| m. 29, beat 4 to m. 30, beat 1 | dominant7 | V4/3 | dominant | - | C | dominant7 | G | second | 1 | 0.15 | - |
| m. 30, beat 1 to m. 30, beat 2 | I | I | tonic | - | F | major | F | root position | 1 | 1.00 | - |
| m. 30, beat 2 to m. 30, beat 3 | I | V4/2/IV | dominant of 4 | - | F | major | Eb | - | 1 | 0.50 | - |
| m. 30, beat 3 to m. 30, beat 4 | IV | IV6 | predominant (strong) | - | Bb | major | D | first | 1 | 0.32 | - |
| m. 30, beat 4 to m. 31, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 31, beat 1 to m. 31, beat 2 | minor7 | ii4/2 | predominant (strong) | - | G | minor7 | B | - | 1 | 0.26 | - |
| m. 31, beat 2 to m. 31, beat 3 | minor7 | ii7 | predominant (strong) | - | G | minor7 | G | root position | 1 | 0.18 | - |
| m. 31, beat 3 to m. 31, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 31, beat 4 to m. 32, beat 1 | vi | vi6 | predominant (weak) | - | D | minor | F | first | 1 | 0.17 | - |
| m. 32, beat 1 to m. 32, beat 2 | V | V6/4 | dominant | - | C | major | G | second | 1 | 0.44 | - |
| m. 32, beat 2 to m. 32, beat 3 | minor7 | ii7 | predominant (strong) | - | G | minor7 | G | root position | 1 | 0.09 | minor7 (D minor7, score 0.66) |
| m. 32, beat 3 to m. 32, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 32, beat 4 to m. 33, beat 1 | I | I6 | tonic | - | F | major | A | first | 1 | 0.62 | - |
| m. 33, beat 1 to m. 33, beat 2 | I | I (in Bb major) | tonic | - | Bb | major | Bb | root position | 1 | 0.49 | - |
| m. 33, beat 2 to m. 33, beat 3 | I | I6/4 | tonic | - | Bb | major | F | second | 1 | 0.13 | - |
| m. 33, beat 3 to m. 33, beat 4 | minor7 | ii6/5 | predominant (strong) | - | C | minor7 | Eb | first | 1 | 0.79 | - |
| m. 33, beat 4 to m. 34, beat 1 | minor7 | ii4/2 | predominant (strong) | - | C | minor7 | E | - | 1 | 0.02 | vi (G minor, score 0.65) |
| m. 34, beat 1 to m. 34, beat 2 | I | I6/4 | tonic | - | Bb | major | F | second | 1 | 0.66 | - |
| m. 34, beat 2 to m. 34, beat 3 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.76 | - |
| m. 34, beat 3 to m. 34, beat 4 | V | V | dominant | - | F | major | F | root position | 1 | 0.23 | - |
| m. 34, beat 4 to m. 35, beat 1 | V | V6 | dominant | - | F | major | A | first | 1 | 1.00 | - |
| m. 35, beat 1 to m. 35, beat 2 | IV | IV6 | predominant (strong) | - | Eb | major | G | first | 1 | 0.66 | - |
| m. 35, beat 2 to m. 35, beat 3 | IV | IV6/4 | predominant (strong) | - | Eb | major | Gb | - | 1 | 1.00 | - |
| m. 35, beat 3 to m. 35, beat 4 | I | I6/4 | tonic | - | Bb | major | F | second | 1 | 0.11 | - |
| m. 35, beat 4 to m. 36, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 36, beat 1 to m. 36, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 36, beat 2 to m. 36, beat 3 | minor7 | ii4/2 | predominant (strong) | - | C | minor7 | Bb | third | 1 | 0.02 | vi (G minor, score 0.72) |
| m. 36, beat 3 to m. 36, beat 4 | minor7 | V6/5/V | dominant of 5 | - | C | minor7 | E | - | 1 | 0.06 | half-diminished7 (A half-diminished7, score 0.47) |
| m. 36, beat 4 to m. 37, beat 1 | dominant7 | V4/2 | dominant | - | F | dominant7 | Eb | third | 1 | 0.15 | - |
| m. 37, beat 1 to m. 37, beat 2 | iii | vii°6/5/ii | dominant of 2 | - | D | minor | D | root position | 1 | 0.30 | - |
| m. 37, beat 2 to m. 37, beat 3 | ii | ii6 | predominant (strong) | - | C | minor | Eb | first | 1 | 0.07 | dominant7 (F dominant7, score 0.75) |
| m. 37, beat 3 to m. 37, beat 4 | minor7 | vi4/2 | predominant (weak) | - | G | minor7 | F | third | 1 | 0.00 | I (Bb major, score 1.04) |
| m. 37, beat 4 to m. 38, beat 1 | minor7 | iii6/5 | predominant (weak) | - | D | minor7 | F | first | 1 | 0.17 | - |
| m. 38, beat 1 to m. 38, beat 2 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.56 | - |
| m. 38, beat 2 to m. 38, beat 3 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | Bb | first | 1 | 0.07 | I (Bb major, score 0.86) |
| m. 38, beat 3 to m. 38, beat 4 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 1 | 0.53 | - |
| m. 38, beat 4 to m. 39, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.44 | - |
| m. 39, beat 1 to m. 39, beat 2 | dominant7 | V7 | dominant | - | F | dominant7 | Bb | - | 1 | 0.05 | major7 (Bb major7, score 0.70) |
| m. 39, beat 2 to m. 39, beat 3 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 1 | 0.10 | dominant7 (F dominant7, score 0.65) |
| m. 39, beat 3 to m. 39, beat 4 | dominant7 | V7 | dominant | - | F | dominant7 | Bb | - | 1 | 0.03 | major7 (Bb major7, score 0.71) |
| m. 39, beat 4 to m. 40, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | Bb | - | 1 | 0.20 | - |
| m. 40, beat 1 to m. 40, beat 2 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.52 | - |
| m. 40, beat 2 to m. 40, beat 3 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | Bb | first | 1 | 0.07 | I (Bb major, score 0.86) |
| m. 40, beat 3 to m. 40, beat 4 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 1 | 0.53 | - |
| m. 40, beat 4 to m. 41, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 1 | 0.44 | - |
| m. 41, beat 1 to m. 41, beat 2 | dominant7 | V7 | dominant | - | F | dominant7 | Bb | - | 1 | 0.05 | major7 (Bb major7, score 0.70) |
| m. 41, beat 2 to m. 41, beat 3 | major7 | I7 | tonic | - | Bb | major7 | Bb | root position | 1 | 0.10 | dominant7 (F dominant7, score 0.65) |
| m. 41, beat 3 to m. 41, beat 4 | dominant7 | V7 | dominant | - | F | dominant7 | Bb | - | 1 | 0.03 | major7 (Bb major7, score 0.71) |
| m. 41, beat 4 to m. 42, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | Bb | - | 1 | 0.20 | - |
| m. 42, beat 1 to m. 42, beat 1.33 | I | I | tonic | - | Bb | major | Bb | root position | 0.33 | 0.55 | - |
| m. 42, beat 1.33 to m. 42, beat 2 | major7 | IV4/2 | predominant (strong) | - | Eb | major7 | D | third | 0.67 | 0.52 | - |
| m. 42, beat 2 to m. 42, beat 3 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 1 | 0.00 | minor7 (D minor7, score 0.68) |
| m. 42, beat 3 to m. 42, beat 3.33 | major7 | I4/3 | tonic | - | Bb | major7 | F | second | 0.33 | 0.20 | - |
| m. 42, beat 3.33 to m. 42, beat 4 | major7 | IV4/2 | predominant (strong) | - | Eb | major7 | D | third | 0.67 | 0.52 | - |
| m. 42, beat 4 to m. 43, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 1 | 0.00 | minor7 (D minor7, score 0.81) |
| m. 43, beat 1 to m. 43, beat 1.33 | major7 | I4/3 | tonic | - | Bb | major7 | F | second | 0.33 | 0.20 | - |
| m. 43, beat 1.33 to m. 43, beat 2 | major7 | IV4/2 | predominant (strong) | - | Eb | major7 | D | third | 0.67 | 0.52 | - |
| m. 43, beat 2 to m. 43, beat 3 | dominant7 | V4/2 | dominant | - | F | dominant7 | Eb | third | 1 | 0.45 | - |
| m. 43, beat 3 to m. 44, beat 1 | I | I | tonic | - | Bb | major | Bb | root position | 2 | 0.65 | - |

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
| m. 1, beat 1 to m. 1, beat 3 | Bb major | F5 | 5 | Bb2 | 1 |
| m. 1, beat 3 to m. 1, beat 4 | Bb major | F5 | 5 | G3 | 6 |
| m. 1, beat 4 to m. 2, beat 1 | Bb major | G5 | 6 | Bb3 | 1 |
| m. 2, beat 1 to m. 2, beat 2 | Bb major | G5 | 6 | Bb2 | 1 |
| m. 2, beat 2 to m. 2, beat 3 | Bb major | F5 | 5 | Bb3 | 1 |
| m. 2, beat 3 to m. 2, beat 4 | Bb major | F5 | 5 | F3 | 5 |
| m. 2, beat 4 to m. 3, beat 1 | Bb major | F4 | 5 | Bb3 | 1 |
| m. 3, beat 1 to m. 3, beat 2 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 3, beat 2 to m. 3, beat 3 | Bb major | D5 | 3 | Bb3 | 1 |
| m. 3, beat 3 to m. 3, beat 4 | Bb major | Eb5 | 4 | Gb3 | b6 |
| m. 3, beat 4 to m. 4, beat 1 | Bb major | Eb5 | 4 | A3 | 7 |
| m. 4, beat 1 to m. 4, beat 2 | Bb major | C5 | 2 | Bb2 | 1 |
| m. 4, beat 2 to m. 4, beat 3 | Bb major | D5 | 3 | Bb3 | 1 |
| m. 4, beat 3 to m. 4, beat 4 | Bb major | C5 | 2 | F3 | 5 |
| m. 4, beat 4 to m. 5, beat 1 | Bb major | F4 | 5 | Bb3 | 1 |
| m. 5, beat 1 to m. 5, beat 2 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 5, beat 2 to m. 5, beat 3 | Bb major | D5 | 3 | Bb3 | 1 |
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
| m. 8, beat 2 to m. 8, beat 3 | Bb major | F6 | 5 | Eb3 | 4 |
| m. 8, beat 3 to m. 8, beat 4 | Bb major | Bb5 | 1 | D3 | 3 |
| m. 8, beat 4 to m. 9, beat 1 | Bb major | Eb5 | 4 | C3 | 2 |
| m. 9, beat 1 to m. 9, beat 3 | Bb major | F5 | 5 | Bb2 | 1 |
| m. 9, beat 3 to m. 9, beat 4 | Bb major | F5 | 5 | G3 | 6 |
| m. 9, beat 4 to m. 10, beat 1 | Bb major | G5 | 6 | Bb3 | 1 |
| m. 10, beat 1 to m. 10, beat 2 | Bb major | G5 | 6 | Bb2 | 1 |
| m. 10, beat 2 to m. 10, beat 3 | Bb major | F5 | 5 | Bb3 | 1 |
| m. 10, beat 3 to m. 10, beat 4 | Bb major | F5 | 5 | F3 | 5 |
| m. 10, beat 4 to m. 11, beat 1 | Bb major | F4 | 5 | Bb3 | 1 |
| m. 11, beat 1 to m. 11, beat 2 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 11, beat 2 to m. 11, beat 3 | Bb major | D5 | 3 | Bb3 | 1 |
| m. 11, beat 3 to m. 11, beat 4 | Bb major | Eb5 | 4 | Gb3 | b6 |
| m. 11, beat 4 to m. 12, beat 1 | Bb major | Eb5 | 4 | A3 | 7 |
| m. 12, beat 1 to m. 12, beat 2 | Bb major | C5 | 2 | Bb2 | 1 |
| m. 12, beat 2 to m. 12, beat 3 | Bb major | D5 | 3 | Bb3 | 1 |
| m. 12, beat 3 to m. 12, beat 4 | Bb major | C5 | 2 | F3 | 5 |
| m. 12, beat 4 to m. 13, beat 1 | Bb major | F4 | 5 | Bb3 | 1 |
| m. 13, beat 1 to m. 13, beat 2 | Bb major | Eb5 | 4 | G3 | 6 |
| m. 13, beat 2 to m. 13, beat 3 | Bb major | Eb5 | 4 | Bb3 | 1 |
| m. 13, beat 3 to m. 13, beat 4 | Bb major | Eb5 | 4 | F3 | 5 |
| m. 13, beat 4 to m. 14, beat 1 | Bb major | Eb5 | 4 | Bb3 | 1 |
| m. 14, beat 1 to m. 14, beat 2 | Bb major | Eb5 | 4 | E3 | #4 |
| m. 14, beat 2 to m. 14, beat 3 | Bb major | D5 | 3 | Bb3 | 1 |
| m. 14, beat 3 to m. 14, beat 4 | Bb major | C5 | 2 | E3 | #4 |
| m. 14, beat 4 to m. 15, beat 1 | Bb major | D5 | 3 | Eb3 | 4 |
| m. 15, beat 1 to m. 15, beat 2 | Bb major | F5 | 5 | D3 | 3 |
| m. 15, beat 2 to m. 15, beat 3 | Bb major | F5 | 5 | Eb3 | 4 |
| m. 15, beat 3 to m. 15, beat 4 | Bb major | C5 | 2 | F3 | 5 |
| m. 15, beat 4 to m. 16, beat 1 | Bb major | C5 | 2 | F3 | 5 |
| m. 16, beat 1 to m. 16, beat 2 | Bb major | Db5 | b3 | E3 | #4 |
| m. 16, beat 2 to m. 16, beat 3 | Bb major | Db5 | b3 | Bb3 | 1 |
| m. 16, beat 3 to m. 16, beat 4 | Bb major | Db5 | b3 | E3 | #4 |
| m. 16, beat 4 to m. 17, beat 1 | Bb major | Db5 | b3 | Eb3 | 4 |
| m. 17, beat 1 to m. 17, beat 3 | Bb major | F5 | 5 | D3 | 3 |
| m. 17, beat 3 to m. 17, beat 4 | Bb major | C5 | 2 | Eb3 | 4 |
| m. 17, beat 4 to m. 18, beat 1 | Bb major | D5 | 3 | F3 | 5 |
| m. 18, beat 1 to m. 18, beat 1.33 | Bb major | C5 | 2 | F3 | 5 |
| m. 18, beat 1.33 to m. 18, beat 2 | Bb major | F4 | 5 | Bb2 | 1 |
| m. 18, beat 2 to m. 18, beat 3 | Bb major | Ab4 | b7 | Bb2 | 1 |
| m. 18, beat 3 to m. 18, beat 4 | Bb major | B4 | b2 | Bb2 | 1 |
| m. 18, beat 4 to m. 19, beat 1 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 19, beat 1 to m. 19, beat 2 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 19, beat 2 to m. 19, beat 3 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 19, beat 3 to m. 19, beat 4 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 19, beat 4 to m. 20, beat 1 | Bb major | F5 | 5 | Bb2 | 1 |
| m. 20, beat 1 to m. 20, beat 2 | Bb major | Bb4 | 1 | Bb2 | 1 |
| m. 20, beat 2 to m. 20, beat 3 | Bb major | Ab4 | b7 | Bb2 | 1 |
| m. 20, beat 3 to m. 20, beat 4 | Bb major | B4 | b2 | Bb2 | 1 |
| m. 20, beat 4 to m. 21, beat 1 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 21, beat 1 to m. 21, beat 2 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 21, beat 2 to m. 21, beat 3 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 21, beat 3 to m. 21, beat 4 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 21, beat 4 to m. 22, beat 1 | Bb major | F5 | 5 | Bb2 | 1 |
| m. 22, beat 1 to m. 22, beat 4 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 22, beat 4 to m. 23, beat 1 | Bb major | D5 | 3 | rest (register guard) | - |
| m. 23, beat 1 to m. 23, beat 3 | Bb major | F5 | 5 | Bb2 | 1 |
| m. 23, beat 3 to m. 23, beat 4 | Bb major | F5 | 5 | G3 | 6 |
| m. 23, beat 4 to m. 24, beat 1 | Bb major | G5 | 6 | Bb3 | 1 |
| m. 24, beat 1 to m. 24, beat 2 | Bb major | G5 | 6 | Bb2 | 1 |
| m. 24, beat 2 to m. 24, beat 3 | Bb major | F5 | 5 | Bb3 | 1 |
| m. 24, beat 3 to m. 24, beat 4 | Bb major | C6 | 2 | F3 | 5 |
| m. 24, beat 4 to m. 25, beat 1 | Bb major | Bb5 | 1 | Bb3 | 1 |
| m. 25, beat 1 to m. 25, beat 2 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 25, beat 2 to m. 25, beat 3 | Bb major | D5 | 3 | Bb3 | 1 |
| m. 25, beat 3 to m. 25, beat 4 | Bb major | Eb5 | 4 | Gb3 | b6 |
| m. 25, beat 4 to m. 26, beat 1 | Bb major | Eb5 | 4 | A3 | 7 |
| m. 26, beat 1 to m. 26, beat 2 | Bb major | Bb4 | 1 | Bb2 | 1 |
| m. 26, beat 2 to m. 26, beat 3 | Bb major | D5 | 3 | Bb3 | 1 |
| m. 26, beat 3 to m. 26, beat 4 | Bb major | C5 | 2 | F3 | 5 |
| m. 26, beat 4 to m. 27, beat 1 | Bb major | F4 | 5 | Bb3 | 1 |
| m. 27, beat 1 to m. 27, beat 2 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 27, beat 2 to m. 27, beat 3 | Bb major | D5 | 3 | Bb3 | 1 |
| m. 27, beat 3 to m. 27, beat 4 | Bb major | E5 | #4 | A2 | 7 |
| m. 27, beat 4 to m. 28, beat 1 | Bb major | E5 | #4 | A3 | 7 |
| m. 28, beat 1 to m. 28, beat 2 | Bb major | F5 | 5 | D3 | 3 |
| m. 28, beat 2 to m. 28, beat 3 | Bb major | F5 | 5 | A3 | 7 |
| m. 28, beat 3 to m. 28, beat 4 | Bb major | Bb5 | 1 | Bb2 | 1 |
| m. 28, beat 4 to m. 29, beat 1 | Bb major | A5 | 7 | G3 | 6 |
| m. 29, beat 1 to m. 29, beat 2 | F major | D5 | 6 | C3 | 5 |
| m. 29, beat 2 to m. 29, beat 3 | F major | C5 | 5 | A3 | 3 |
| m. 29, beat 3 to m. 29, beat 4 | F major | D5 | 6 | Ab3 | b3 |
| m. 29, beat 4 to m. 30, beat 1 | F major | E5 | 7 | G3 | 2 |
| m. 30, beat 1 to m. 30, beat 2 | F major | F5 | 1 | F3 | 1 |
| m. 30, beat 2 to m. 30, beat 3 | F major | F5 | 1 | Eb3 | b7 |
| m. 30, beat 3 to m. 30, beat 4 | F major | F5 | 1 | D3 | 6 |
| m. 30, beat 4 to m. 31, beat 1 | F major | F5 | 1 | C3 | 5 |
| m. 31, beat 1 to m. 31, beat 2 | F major | F5 | 1 | B2 | #4 |
| m. 31, beat 2 to m. 31, beat 3 | F major | G5 | 2 | G3 | 2 |
| m. 31, beat 3 to m. 31, beat 4 | F major | F5 | 1 | C3 | 5 |
| m. 31, beat 4 to m. 32, beat 1 | F major | Eb5 | b7 | F3 | 1 |
| m. 32, beat 1 to m. 32, beat 2 | F major | D5 | 6 | G3 | 2 |
| m. 32, beat 2 to m. 32, beat 3 | F major | D5 | 6 | G3 | 2 |
| m. 32, beat 3 to m. 32, beat 4 | F major | Eb5 | b7 | C4 | 5 |
| m. 32, beat 4 to m. 33, beat 1 | F major | F5 | 1 | A3 | 3 |
| m. 33, beat 1 to m. 33, beat 2 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 33, beat 2 to m. 33, beat 3 | Bb major | D5 | 3 | F3 | 5 |
| m. 33, beat 3 to m. 33, beat 4 | Bb major | C5 | 2 | Eb3 | 4 |
| m. 33, beat 4 to m. 34, beat 1 | Bb major | G4 | 6 | E3 | #4 |
| m. 34, beat 1 to m. 34, beat 2 | Bb major | Bb4 | 1 | F3 | 5 |
| m. 34, beat 2 to m. 34, beat 3 | Bb major | Bb4 | 1 | Bb3 | 1 |
| m. 34, beat 3 to m. 34, beat 4 | Bb major | Bb4 | 1 | F3 | 5 |
| m. 34, beat 4 to m. 35, beat 1 | Bb major | F5 | 5 | A3 | 7 |
| m. 35, beat 1 to m. 35, beat 2 | Bb major | E5 | #4 | G3 | 6 |
| m. 35, beat 2 to m. 35, beat 3 | Bb major | Eb5 | 4 | Gb3 | b6 |
| m. 35, beat 3 to m. 35, beat 4 | Bb major | Eb5 | 4 | F3 | 5 |
| m. 35, beat 4 to m. 36, beat 1 | Bb major | D5 | 3 | Bb3 | 1 |
| m. 36, beat 1 to m. 36, beat 2 | Bb major | Bb5 | 1 | E3 | #4 |
| m. 36, beat 2 to m. 36, beat 3 | Bb major | Bb6 | 1 | Bb3 | 1 |
| m. 36, beat 3 to m. 36, beat 4 | Bb major | A6 | 7 | E3 | #4 |
| m. 36, beat 4 to m. 37, beat 1 | Bb major | rest (register guard) | - | Eb3 | 4 |
| m. 37, beat 1 to m. 37, beat 2 | Bb major | F5 | 5 | D3 | 3 |
| m. 37, beat 2 to m. 37, beat 3 | Bb major | F5 | 5 | Eb3 | 4 |
| m. 37, beat 3 to m. 37, beat 4 | Bb major | C5 | 2 | F3 | 5 |
| m. 37, beat 4 to m. 38, beat 1 | Bb major | D5 | 3 | F3 | 5 |
| m. 38, beat 1 to m. 38, beat 2 | Bb major | C5 | 2 | Bb2 | 1 |
| m. 38, beat 2 to m. 38, beat 3 | Bb major | Ab4 | b7 | Bb2 | 1 |
| m. 38, beat 3 to m. 38, beat 4 | Bb major | B4 | b2 | Bb2 | 1 |
| m. 38, beat 4 to m. 39, beat 1 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 39, beat 1 to m. 39, beat 2 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 39, beat 2 to m. 39, beat 3 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 39, beat 3 to m. 39, beat 4 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 39, beat 4 to m. 40, beat 1 | Bb major | F5 | 5 | Bb2 | 1 |
| m. 40, beat 1 to m. 40, beat 2 | Bb major | Bb4 | 1 | Bb2 | 1 |
| m. 40, beat 2 to m. 40, beat 3 | Bb major | Ab4 | b7 | Bb2 | 1 |
| m. 40, beat 3 to m. 40, beat 4 | Bb major | B4 | b2 | Bb2 | 1 |
| m. 40, beat 4 to m. 41, beat 1 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 41, beat 1 to m. 41, beat 2 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 41, beat 2 to m. 41, beat 3 | Bb major | D5 | 3 | Bb2 | 1 |
| m. 41, beat 3 to m. 41, beat 4 | Bb major | Eb5 | 4 | Bb2 | 1 |
| m. 41, beat 4 to m. 42, beat 1 | Bb major | F5 | 5 | Bb2 | 1 |
| m. 42, beat 1 to m. 42, beat 1.33 | Bb major | Bb4 | 1 | Bb2 | 1 |
| m. 42, beat 1.33 to m. 42, beat 2 | Bb major | Eb5 | 4 | rest (register guard) | - |
| m. 42, beat 2 to m. 42, beat 3 | Bb major | A5 | 7 | rest (register guard) | - |
| m. 42, beat 3 to m. 42, beat 3.33 | Bb major | Bb5 | 1 | F4 | 5 |
| m. 42, beat 3.33 to m. 42, beat 4 | Bb major | rest (bridged) | - | D3 | 3 |
| m. 42, beat 4 to m. 43, beat 1 | Bb major | D5 | 3 | F2 | 5 |
| m. 43, beat 1 to m. 43, beat 1.33 | Bb major | Bb4 | 1 | F2 | 5 |
| m. 43, beat 1.33 to m. 43, beat 2 | Bb major | rest (register guard) | - | D2 | 3 |
| m. 43, beat 2 to m. 43, beat 3 | Bb major | rest (register guard) | - | Eb2 | 4 |
| m. 43, beat 3 to m. 44, beat 1 | Bb major | Bb3 | 1 | Bb1 | 1 |

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

225 notes sound against a chord without being one of its tones; 147 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

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
| m. 17, beat 1 to m. 17, beat 3 | 258 | anticipation | minor7 |
| m. 17, beat 1 to m. 17, beat 3 | 259 | unclassified | minor7 |
| m. 17, beat 1 to m. 17, beat 3 | 260 | unclassified | minor7 |
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
| m. 18, beat 4 to m. 19, beat 1 | 316 | unclassified | I |
| m. 18, beat 4 to m. 19, beat 1 | 321 | passing | I |
| m. 19, beat 1 to m. 19, beat 2 | 334 | anticipation | dominant7 |
| m. 19, beat 1 to m. 19, beat 2 | 336 | unclassified | dominant7 |
| m. 19, beat 1 to m. 19, beat 2 | 339 | anticipation | dominant7 |
| m. 19, beat 1 to m. 19, beat 2 | 344 | anticipation | dominant7 |
| m. 19, beat 2 to m. 19, beat 3 | 346 | neighbor | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 347 | anticipation | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 351 | unclassified | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 352 | anticipation | major7 |
| m. 19, beat 2 to m. 19, beat 3 | 357 | anticipation | major7 |
| m. 19, beat 3 to m. 19, beat 4 | 364 | unclassified | dominant7 |
| m. 19, beat 3 to m. 19, beat 4 | 366 | passing | dominant7 |
| m. 19, beat 3 to m. 19, beat 4 | 369 | unclassified | dominant7 |
| m. 19, beat 3 to m. 19, beat 4 | 374 | unclassified | dominant7 |
| m. 19, beat 4 to m. 20, beat 1 | 376 | neighbor | dominant7 |
| m. 19, beat 4 to m. 20, beat 1 | 379 | unclassified | dominant7 |
| m. 19, beat 4 to m. 20, beat 1 | 384 | unclassified | dominant7 |
| m. 19, beat 4 to m. 20, beat 1 | 389 | unclassified | dominant7 |
| m. 20, beat 1 to m. 20, beat 2 | 395 | appoggiatura | I |
| m. 20, beat 2 to m. 20, beat 3 | 405 | passing | minor7 |
| m. 20, beat 2 to m. 20, beat 3 | 415 | passing | minor7 |
| m. 20, beat 3 to m. 20, beat 4 | 430 | passing | major7 |
| m. 20, beat 4 to m. 21, beat 1 | 435 | unclassified | I |
| m. 20, beat 4 to m. 21, beat 1 | 440 | passing | I |
| m. 21, beat 1 to m. 21, beat 2 | 453 | anticipation | dominant7 |
| m. 21, beat 1 to m. 21, beat 2 | 455 | unclassified | dominant7 |
| m. 21, beat 1 to m. 21, beat 2 | 458 | anticipation | dominant7 |
| m. 21, beat 1 to m. 21, beat 2 | 463 | anticipation | dominant7 |
| m. 21, beat 2 to m. 21, beat 3 | 465 | neighbor | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 466 | anticipation | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 470 | unclassified | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 471 | anticipation | major7 |
| m. 21, beat 2 to m. 21, beat 3 | 476 | anticipation | major7 |
| m. 21, beat 3 to m. 21, beat 4 | 483 | unclassified | dominant7 |
| m. 21, beat 3 to m. 21, beat 4 | 485 | passing | dominant7 |
| m. 21, beat 3 to m. 21, beat 4 | 488 | unclassified | dominant7 |
| m. 21, beat 3 to m. 21, beat 4 | 493 | unclassified | dominant7 |
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
| m. 37, beat 4 to m. 38, beat 1 | 786 | unclassified | minor7 |
| m. 37, beat 4 to m. 38, beat 1 | 789 | unclassified | minor7 |
| m. 37, beat 4 to m. 38, beat 1 | 792 | unclassified | minor7 |
| m. 38, beat 1 to m. 38, beat 2 | 793 | unclassified | I |
| m. 38, beat 1 to m. 38, beat 2 | 798 | appoggiatura | I |
| m. 38, beat 2 to m. 38, beat 3 | 808 | passing | minor7 |
| m. 38, beat 2 to m. 38, beat 3 | 818 | passing | minor7 |
| m. 38, beat 3 to m. 38, beat 4 | 833 | passing | major7 |
| m. 38, beat 4 to m. 39, beat 1 | 838 | unclassified | I |
| m. 38, beat 4 to m. 39, beat 1 | 843 | passing | I |
| m. 39, beat 1 to m. 39, beat 2 | 856 | anticipation | dominant7 |
| m. 39, beat 1 to m. 39, beat 2 | 858 | unclassified | dominant7 |
| m. 39, beat 1 to m. 39, beat 2 | 861 | anticipation | dominant7 |
| m. 39, beat 1 to m. 39, beat 2 | 866 | anticipation | dominant7 |
| m. 39, beat 2 to m. 39, beat 3 | 868 | neighbor | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 869 | anticipation | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 873 | unclassified | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 874 | anticipation | major7 |
| m. 39, beat 2 to m. 39, beat 3 | 879 | anticipation | major7 |
| m. 39, beat 3 to m. 39, beat 4 | 886 | unclassified | dominant7 |
| m. 39, beat 3 to m. 39, beat 4 | 888 | passing | dominant7 |
| m. 39, beat 3 to m. 39, beat 4 | 891 | unclassified | dominant7 |
| m. 39, beat 3 to m. 39, beat 4 | 896 | unclassified | dominant7 |
| m. 39, beat 4 to m. 40, beat 1 | 898 | neighbor | dominant7 |
| m. 39, beat 4 to m. 40, beat 1 | 901 | unclassified | dominant7 |
| m. 39, beat 4 to m. 40, beat 1 | 906 | unclassified | dominant7 |
| m. 39, beat 4 to m. 40, beat 1 | 911 | unclassified | dominant7 |
| m. 40, beat 1 to m. 40, beat 2 | 917 | appoggiatura | I |
| m. 40, beat 2 to m. 40, beat 3 | 927 | passing | minor7 |
| m. 40, beat 2 to m. 40, beat 3 | 937 | passing | minor7 |
| m. 40, beat 3 to m. 40, beat 4 | 952 | passing | major7 |
| m. 40, beat 4 to m. 41, beat 1 | 957 | unclassified | I |
| m. 40, beat 4 to m. 41, beat 1 | 962 | passing | I |
| m. 41, beat 1 to m. 41, beat 2 | 975 | anticipation | dominant7 |
| m. 41, beat 1 to m. 41, beat 2 | 977 | unclassified | dominant7 |
| m. 41, beat 1 to m. 41, beat 2 | 980 | anticipation | dominant7 |
| m. 41, beat 1 to m. 41, beat 2 | 985 | anticipation | dominant7 |
| m. 41, beat 2 to m. 41, beat 3 | 987 | neighbor | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 988 | anticipation | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 992 | unclassified | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 993 | anticipation | major7 |
| m. 41, beat 2 to m. 41, beat 3 | 998 | anticipation | major7 |
| m. 41, beat 3 to m. 41, beat 4 | 1005 | unclassified | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 1007 | passing | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 1010 | unclassified | dominant7 |
| m. 41, beat 3 to m. 41, beat 4 | 1015 | unclassified | dominant7 |
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

## Cadences

The library's standard-harmony analysis found 12 cadences from the chords, the meter and the soprano. Position is where the arrival begins. Key is the key region of the arrival. Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added before a section start.

| Position | Type | Key | Soprano | Beat strength | Boundary |
| --- | --- | --- | --- | --- | --- |
| m. 8, beat 1 | half | Bb major | 5 | strong | no |
| m. 19, beat 1 | half | Bb major | 4 | strong | no |
| m. 20, beat 1 | perfect-authentic | Bb major | 1 | strong | no |
| m. 21, beat 1 | half | Bb major | 4 | strong | no |
| m. 22, beat 1 | imperfect-authentic | Bb major | 3 | strong | no |
| m. 34, beat 3 | half | Bb major | 1 | medium | no |
| m. 39, beat 1 | half | Bb major | 4 | strong | no |
| m. 40, beat 1 | perfect-authentic | Bb major | 1 | strong | no |
| m. 41, beat 1 | half | Bb major | 4 | strong | no |
| m. 41, beat 3 | half | Bb major | 4 | medium | no |
| m. 42, beat 3 | subverted | Bb major | 1 | medium | no |
| m. 43, beat 1 | subverted | Bb major | 1 | strong | no |

### Cadence plan

1. m. 8, beat 1: half in Bb major
2. m. 19, beat 1: half in Bb major
3. m. 20, beat 1: perfect-authentic in Bb major
4. m. 21, beat 1: half in Bb major
5. m. 22, beat 1: imperfect-authentic in Bb major
6. m. 34, beat 3: half in Bb major
7. m. 39, beat 1: half in Bb major
8. m. 40, beat 1: perfect-authentic in Bb major
9. m. 41, beat 1: half in Bb major
10. m. 41, beat 3: half in Bb major
11. m. 42, beat 3: subverted in Bb major
12. m. 43, beat 1: subverted in Bb major

**Counts:** half 7, perfect-authentic 2, imperfect-authentic 1, subverted 2.

## Phrases

The library's standard-form analysis found 9 phrases from the soprano, the meter and the cadences. Length counts the measures of the phrase proper; Ends is what closed it: its cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in the piece; Expansion is a lengthening or shortening of the phrase's ending, or an extension after its cadence, with the measures it adds or removes.

| Phrase | Measures | Length | Cadence | Ends | Role | Expansion |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 8 | 8 | half (m. 8, beat 1) | cadence | beginning | - |
| 2 | mm. 9 to 19 | 11 | half (m. 19, beat 1) | cadence | middle | internal +3 (mm. 13 to 19) |
| 3 | m. 20 | 1 | perfect-authentic (m. 20, beat 1) | cadence | middle | - |
| 4 | m. 21 | 1 | half (m. 21, beat 1) | cadence | middle | - |
| 5 | m. 22 | 1 | imperfect-authentic (m. 22, beat 1) | cadence | middle | - |
| 6 | mm. 23 to 34 | 12 | half (m. 34, beat 3) | cadence | middle | internal +4 (mm. 27 to 34) |
| 7 | mm. 35 to 39 | 5 | half (m. 39, beat 1) | cadence | middle | - |
| 8 | m. 40 | 1 | perfect-authentic (m. 40, beat 1) | cadence | middle | contraction -4 (m. 40) |
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
| 4 | hybrid | slightly-less-common | mm. 35 to 40 | 6 | antecedent (mm. 35 to 39, half (m. 39, beat 1)), continuation (m. 40, perfect-authentic (m. 40, beat 1)) | fragmentation |

## Form plan

The kernel's form structure detection grouped the phrases into 4 sections, closing a section at each phrase that ends on a perfect or imperfect authentic cadence; the phrases after the last such cadence form the last section. Sections are lettered by first appearance, and a section whose opening theme returns an earlier section's material takes that letter with a prime. Key is the key the section opens in.

| Section | Measures | Key | Phrases | Closing cadence | Returns |
| --- | --- | --- | --- | --- | --- |
| A | mm. 1 to 20 | Bb major | 1 to 3 | perfect-authentic (m. 20, beat 1) | - |
| B | mm. 21 to 22 | Bb major | 4 to 5 | imperfect-authentic (m. 22, beat 1) | - |
| A' | mm. 23 to 40 | Bb major | 6 to 8 | perfect-authentic (m. 40, beat 1) | returns A (similarity 0.83) |
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
| 11 | m. 42 | 3.5 | 3.5 | no |
| 12 | m. 43 | 3.5 | 4.5 | yes |
