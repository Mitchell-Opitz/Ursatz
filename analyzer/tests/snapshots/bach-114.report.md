# Analysis Report: Bach 114.mid

## Summary

- **File:** Bach 114.mid, 430 notes.
- **Input:** MIDI file. Meter: embedded in the file. Key signature: inferred. Pitch spelling: by rule.
- **Key:** G major, the key with the most time across 4 key regions.
- **Chords:** 155 chord events; 151 matched a chord (97%), 4 did not.
- **Most frequent chords:** I (42), minor7 (24), major7 (19).

## Meter

- **Time signature:** 4/4
- **Measures:** 24 measures.
- **Pickup:** None.

## Key regions

Positions are measure and beat. Uncertainty is the library's, from 0 (clearly this key) to 1 (another key fits as well or better). The home key is the key with the most time across its regions.

- **Home key:** G major.
- **Opening key:** G major.
- **Closing key:** D major.

| Position | Key | Uncertainty | Set by |
| --- | --- | --- | --- |
| m. 1, beat 1 to m. 14, beat 1 | G major | 0.66 | profile |
| m. 14, beat 1 to m. 16, beat 1 | D major | 1.00 | profile |
| m. 16, beat 1 to m. 20, beat 1 | G major | 0.76 | profile |
| m. 20, beat 1 to m. 25, beat 1 | D major | 0.91 | profile |

## Modulations

A modulation is a lasting new key. Position is where the new key arrives. Type is pivot when the library found a pivot chord and direct when it did not. A return goes back to the key before an excursion. The library gives the change's extent from the length of the new key, whether it returns, and whether a cadence arrives in it.

| Position | From | To | Extent | Type | Return |
| --- | --- | --- | --- | --- | --- |
| m. 14, beat 1 | G major | D major | weak-modulation | direct | no |
| m. 16, beat 1 | D major | G major | weak-modulation | direct | yes |
| m. 20, beat 1 | G major | D major | weak-modulation | direct | yes |

## Chords

Positions are measure and beat. Numeral is the Roman numeral in the local key region, which is named where it changes. Function is the library's harmonic function of the chord: an applied chord is a dominant toward its target degree, and a predominant is strong (IV, ii) or weak (iii, vi). Six-four is the library's type for a chord in second inversion. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Numeral | Function | Six-four | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 2 | I | I (in G major) | tonic | - | G | major | G | root position | 1 | 0.33 | - |
| m. 1, beat 2 to m. 1, beat 2.5 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 0.18 | - |
| m. 1, beat 2.5 to m. 1, beat 3 | iii | iii | predominant (weak) | - | B | minor | B | root position | 0.5 | 0.69 | - |
| m. 1, beat 3 to m. 1, beat 3.5 | I | I6 | tonic | - | G | major | B | first | 0.5 | 0.63 | - |
| m. 1, beat 3.5 to m. 1, beat 4 | major7 | IV4/3 | predominant (strong) | - | C | major7 | G | second | 0.5 | 0.31 | - |
| m. 1, beat 4 to m. 2, beat 1 | major7 | IV7 | predominant (strong) | - | C | major7 | C | root position | 1 | 0.31 | - |
| m. 2, beat 1 to m. 2, beat 1.5 | half-diminished7 | vii∅4/2 | dominant | - | F# | half-diminished7 | E | third | 0.5 | 0.30 | - |
| m. 2, beat 1.5 to m. 2, beat 2.5 | I | I6 | tonic | - | G | major | B | first | 1 | 0.06 | major7 (C major7, score 0.90) |
| m. 2, beat 2.5 to m. 2, beat 3 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.06 | major7 (C major7, score 0.90) |
| m. 2, beat 3 to m. 2, beat 4 | ii | ii | predominant (strong) | - | A | minor | A | root position | 1 | 0.08 | dominant7 (D dominant7, score 0.90) |
| m. 2, beat 4 to m. 2, beat 4.5 | minor7 | iii4/2 | predominant (weak) | - | B | minor7 | A | third | 0.5 | 0.53 | - |
| m. 2, beat 4.5 to m. 3, beat 1 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.69 | - |
| m. 3, beat 1 to m. 3, beat 2 | minor7 | ii4/2 | predominant (strong) | - | A | minor7 | G | third | 1 | 0.16 | - |
| m. 3, beat 2 to m. 3, beat 2.5 | V | V | dominant | - | D | major | D | root position | 0.5 | 0.69 | - |
| m. 3, beat 2.5 to m. 3, beat 3 | I | I6 | tonic | - | G | major | B | first | 0.5 | 0.18 | - |
| m. 3, beat 3 to m. 3, beat 3.5 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.89 | - |
| m. 3, beat 3.5 to m. 3, beat 4 | I | I6/4 | tonic | - | G | major | D | second | 0.5 | 0.89 | - |
| m. 3, beat 4 to m. 4, beat 1 | minor7 | iii6/5 | predominant (weak) | - | B | minor7 | D | first | 1 | 0.16 | - |
| m. 4, beat 1 to m. 4, beat 2 | minor7 | iii7 | predominant (weak) | - | B | minor7 | B | root position | 1 | 0.00 | I (G major, score 0.99) |
| m. 4, beat 2 to m. 4, beat 2.5 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 0.18 | - |
| m. 4, beat 2.5 to m. 4, beat 3 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.64 | - |
| m. 4, beat 3 to m. 4, beat 3.5 | I | I6 | tonic | - | G | major | B | first | 0.5 | 0.64 | - |
| m. 4, beat 3.5 to m. 4, beat 4 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.64 | - |
| m. 4, beat 4 to m. 5, beat 1 | major7 | IV7 | predominant (strong) | - | C | major7 | C | root position | 1 | 0.31 | - |
| m. 5, beat 1 to m. 5, beat 1.5 | vi | vi | predominant (weak) | - | E | minor | E | root position | 0.5 | 0.21 | - |
| m. 5, beat 1.5 to m. 5, beat 2 | vi | vi6/4 | predominant (weak) | - | E | minor | B | second | 0.5 | 0.21 | - |
| m. 5, beat 2 to m. 5, beat 3 | minor7 | ii4/2 | predominant (strong) | - | A | minor7 | G | third | 1 | 0.12 | - |
| m. 5, beat 3 to m. 5, beat 4 | ii | ii | predominant (strong) | - | A | minor | A | root position | 1 | 0.08 | dominant7 (D dominant7, score 0.90) |
| m. 5, beat 4 to m. 5, beat 4.5 | minor7 | iii4/3 | predominant (weak) | - | B | minor7 | F# | second | 0.5 | 0.10 | vii° (F# diminished, score 0.81) |
| m. 5, beat 4.5 to m. 6, beat 1 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.69 | - |
| m. 6, beat 1 to m. 6, beat 1.25 | major7 | IV7 | predominant (strong) | - | C | major7 | C | root position | 0.25 | 0.22 | - |
| m. 6, beat 1.25 to m. 6, beat 2 | major7 | IV4/2 | predominant (strong) | - | C | major7 | B | third | 0.75 | 0.22 | - |
| m. 6, beat 2 to m. 6, beat 2.5 | ii | ii6 | predominant (strong) | - | A | minor | C | first | 0.5 | 0.60 | - |
| m. 6, beat 2.5 to m. 6, beat 3 | minor7 | iii6/5 | predominant (weak) | - | B | minor7 | D | first | 0.5 | 0.37 | - |
| m. 6, beat 3 to m. 6, beat 3.5 | major7 | I4/3 | tonic | - | G | major7 | D | second | 0.5 | 0.13 | - |
| m. 6, beat 3.5 to m. 7, beat 1 | major7 | I7 | tonic | - | G | major7 | G | root position | 1.5 | 0.13 | - |
| m. 7, beat 1 to m. 7, beat 2 | I | I | tonic | - | G | major | G | root position | 1 | 0.33 | - |
| m. 7, beat 2 to m. 7, beat 2.5 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 0.18 | - |
| m. 7, beat 2.5 to m. 7, beat 3 | iii | iii | predominant (weak) | - | B | minor | B | root position | 0.5 | 0.69 | - |
| m. 7, beat 3 to m. 7, beat 4 | major7 | IV4/3 | predominant (strong) | - | C | major7 | G | second | 1 | 0.31 | - |
| m. 7, beat 4 to m. 8, beat 1 | major7 | IV7 | predominant (strong) | - | C | major7 | C | root position | 1 | 0.31 | - |
| m. 8, beat 1 to m. 8, beat 1.5 | half-diminished7 | vii∅4/2 | dominant | - | F# | half-diminished7 | E | third | 0.5 | 0.30 | - |
| m. 8, beat 1.5 to m. 8, beat 2.5 | I | I6 | tonic | - | G | major | B | first | 1 | 0.06 | major7 (C major7, score 0.90) |
| m. 8, beat 2.5 to m. 8, beat 3 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.06 | major7 (C major7, score 0.90) |
| m. 8, beat 3 to m. 8, beat 4 | ii | ii | predominant (strong) | - | A | minor | A | root position | 1 | 0.08 | dominant7 (D dominant7, score 0.90) |
| m. 8, beat 4 to m. 8, beat 4.5 | minor7 | iii4/2 | predominant (weak) | - | B | minor7 | A | third | 0.5 | 0.53 | - |
| m. 8, beat 4.5 to m. 9, beat 1 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.69 | - |
| m. 9, beat 1 to m. 9, beat 2 | minor7 | ii4/2 | predominant (strong) | - | A | minor7 | G | third | 1 | 0.16 | - |
| m. 9, beat 2 to m. 9, beat 2.5 | V | V | dominant | - | D | major | D | root position | 0.5 | 0.69 | - |
| m. 9, beat 2.5 to m. 9, beat 3 | I | I6 | tonic | - | G | major | B | first | 0.5 | 0.18 | - |
| m. 9, beat 3 to m. 9, beat 3.5 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.89 | - |
| m. 9, beat 3.5 to m. 9, beat 4 | I | I6/4 | tonic | - | G | major | D | second | 0.5 | 0.89 | - |
| m. 9, beat 4 to m. 10, beat 1 | minor7 | iii6/5 | predominant (weak) | - | B | minor7 | D | first | 1 | 0.16 | - |
| m. 10, beat 1 to m. 10, beat 2 | minor7 | iii7 | predominant (weak) | - | B | minor7 | B | root position | 1 | 0.00 | I (G major, score 0.99) |
| m. 10, beat 2 to m. 10, beat 2.5 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 0.18 | - |
| m. 10, beat 2.5 to m. 10, beat 3 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.64 | - |
| m. 10, beat 3 to m. 10, beat 3.5 | I | I6 | tonic | - | G | major | B | first | 0.5 | 0.64 | - |
| m. 10, beat 3.5 to m. 10, beat 4 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.64 | - |
| m. 10, beat 4 to m. 11, beat 1 | major7 | IV7 | predominant (strong) | - | C | major7 | C | root position | 1 | 0.31 | - |
| m. 11, beat 1 to m. 11, beat 1.5 | vi | vi | predominant (weak) | - | E | minor | E | root position | 0.5 | 0.21 | - |
| m. 11, beat 1.5 to m. 11, beat 2 | vi | vi6/4 | predominant (weak) | - | E | minor | B | second | 0.5 | 0.21 | - |
| m. 11, beat 2 to m. 11, beat 3 | minor7 | ii4/2 | predominant (strong) | - | A | minor7 | G | third | 1 | 0.12 | - |
| m. 11, beat 3 to m. 11, beat 4 | ii | ii | predominant (strong) | - | A | minor | A | root position | 1 | 0.08 | dominant7 (D dominant7, score 0.90) |
| m. 11, beat 4 to m. 11, beat 4.5 | minor7 | iii4/3 | predominant (weak) | - | B | minor7 | F# | second | 0.5 | 0.10 | vii° (F# diminished, score 0.81) |
| m. 11, beat 4.5 to m. 12, beat 1 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.69 | - |
| m. 12, beat 1 to m. 12, beat 1.25 | major7 | IV7 | predominant (strong) | - | C | major7 | C | root position | 0.25 | 0.22 | - |
| m. 12, beat 1.25 to m. 12, beat 2 | major7 | IV4/2 | predominant (strong) | - | C | major7 | B | third | 0.75 | 0.22 | - |
| m. 12, beat 2 to m. 12, beat 2.5 | ii | ii6 | predominant (strong) | - | A | minor | C | first | 0.5 | 0.60 | - |
| m. 12, beat 2.5 to m. 12, beat 3 | minor7 | iii6/5 | predominant (weak) | - | B | minor7 | D | first | 0.5 | 0.37 | - |
| m. 12, beat 3 to m. 12, beat 3.5 | major7 | I4/3 | tonic | - | G | major7 | D | second | 0.5 | 0.13 | - |
| m. 12, beat 3.5 to m. 13, beat 1 | major7 | I7 | tonic | - | G | major7 | G | root position | 1.5 | 0.13 | - |
| m. 13, beat 1 to m. 13, beat 2 | I | I | tonic | - | G | major | G | root position | 1 | 0.37 | - |
| m. 13, beat 2 to m. 13, beat 2.5 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.97 | - |
| m. 13, beat 2.5 to m. 13, beat 3 | vii° | vii° | dominant | - | F# | diminished | F# | root position | 0.5 | 0.69 | - |
| m. 13, beat 3 to m. 13, beat 4 | V | V6 | dominant | - | D | major | F# | first | 1 | 0.31 | - |
| m. 13, beat 4 to m. 13, beat 4.5 | vi | vi | predominant (weak) | - | E | minor | E | root position | 0.5 | 0.69 | - |
| m. 13, beat 4.5 to m. 14, beat 1 | vi | vi6 | predominant (weak) | - | E | minor | G | first | 0.5 | 0.18 | - |
| m. 14, beat 1 to m. 14, beat 1.5 | minor7 | ii7 (in D major) | predominant (strong) | - | E | minor7 | E | root position | 0.5 | 0.77 | - |
| m. 14, beat 1.5 to m. 14, beat 2 | V | V | dominant | - | A | major | A | root position | 0.5 | 0.69 | - |
| m. 14, beat 2 to m. 14, beat 3 | V | V | dominant | - | A | major | A | root position | 1 | 0.16 | - |
| m. 14, beat 3 to m. 14, beat 4 | minor7 | vi4/2 | predominant (weak) | - | B | minor7 | A | third | 1 | 0.08 | V (A major, score 0.89) |
| m. 14, beat 4 to m. 14, beat 4.5 | minor7 | iii6/5 | predominant (weak) | - | F# | minor7 | A | first | 0.5 | 0.32 | - |
| m. 14, beat 4.5 to m. 15, beat 1 | IV | IV6 | predominant (strong) | - | G | major | B | first | 0.5 | 0.60 | - |
| m. 15, beat 1 to m. 15, beat 1.5 | I | I | tonic | - | D | major | D | root position | 0.5 | 0.69 | - |
| m. 15, beat 1.5 to m. 15, beat 2 | vii° | vii° | dominant | - | C# | diminished | C# | root position | 0.5 | 0.69 | - |
| m. 15, beat 2 to m. 15, beat 2.5 | I | I | tonic | - | D | major | D | root position | 0.5 | 0.69 | - |
| m. 15, beat 2.5 to m. 15, beat 3 | iii | iii | predominant (weak) | - | F# | minor | F# | root position | 0.5 | 0.69 | - |
| m. 15, beat 3 to m. 15, beat 3.5 | V | V | dominant | - | A | major | A | root position | 0.5 | 0.69 | - |
| m. 15, beat 3.5 to m. 15, beat 4.5 | I | I | tonic | - | D | major | D | root position | 1 | 1.00 | - |
| m. 15, beat 4.5 to m. 16, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 16, beat 1 to m. 16, beat 1.5 | iii | iii (in G major) | predominant (weak) | - | B | minor | B | root position | 0.5 | 0.69 | - |
| m. 16, beat 1.5 to m. 16, beat 2 | major7 | I4/3 | tonic | - | G | major7 | D | second | 0.5 | 0.10 | V (D major, score 0.81) |
| m. 16, beat 2 to m. 16, beat 2.5 | major7 | I6/5 | tonic | - | G | major7 | B | first | 0.5 | 0.10 | V (D major, score 0.81) |
| m. 16, beat 2.5 to m. 16, beat 3 | IV | IV | predominant (strong) | - | C | major | C | root position | 0.5 | 0.69 | - |
| m. 16, beat 3 to m. 16, beat 3.5 | vi | vi | predominant (weak) | - | E | minor | E | root position | 0.5 | 0.29 | - |
| m. 16, beat 3.5 to m. 16, beat 4 | IV | IV | predominant (strong) | - | C | major | C | root position | 0.5 | 0.64 | - |
| m. 16, beat 4 to m. 16, beat 4.5 | iii | iii | predominant (weak) | - | B | minor | B | root position | 0.5 | 0.69 | - |
| m. 16, beat 4.5 to m. 17, beat 1 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 0.69 | - |
| m. 17, beat 1 to m. 17, beat 1.5 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.69 | - |
| m. 17, beat 1.5 to m. 17, beat 2 | V | V | dominant | - | D | major | D | root position | 0.5 | 0.00 | I (G major, score 0.66) |
| m. 17, beat 2 to m. 17, beat 2.5 | major7 | I4/3 | tonic | - | G | major7 | D | second | 0.5 | 0.04 | V (D major, score 0.86) |
| m. 17, beat 2.5 to m. 17, beat 3 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 17, beat 3 to m. 17, beat 4 | minor7 | vi4/2 | predominant (weak) | - | E | minor7 | D | third | 1 | 0.08 | V (D major, score 0.89) |
| m. 17, beat 4 to m. 17, beat 4.5 | minor7 | iii4/3 | predominant (weak) | - | B | minor7 | F# | second | 0.5 | 0.10 | vii° (F# diminished, score 0.81) |
| m. 17, beat 4.5 to m. 18, beat 1 | IV | IV6 | predominant (strong) | - | C | major | E | first | 0.5 | 0.44 | - |
| m. 18, beat 1 to m. 18, beat 1.5 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.04 | major7 (C major7, score 0.90) |
| m. 18, beat 1.5 to m. 18, beat 2 | vii° | vii° | dominant | - | F# | diminished | F# | root position | 0.5 | 0.69 | - |
| m. 18, beat 2 to m. 18, beat 2.5 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.55 | - |
| m. 18, beat 2.5 to m. 18, beat 3 | I | I6 | tonic | - | G | major | B | first | 0.5 | 0.55 | - |
| m. 18, beat 3 to m. 18, beat 3.5 | V | V | dominant | - | D | major | D | root position | 0.5 | 0.69 | - |
| m. 18, beat 3.5 to m. 18, beat 4 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.55 | - |
| m. 18, beat 4 to m. 18, beat 4.5 | I | I6/4 | tonic | arpeggiating | G | major | D | second | 0.5 | 0.39 | - |
| m. 18, beat 4.5 to m. 19, beat 1 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.39 | - |
| m. 19, beat 1 to m. 19, beat 2 | I | I | tonic | - | G | major | G | root position | 1 | 0.37 | - |
| m. 19, beat 2 to m. 19, beat 2.5 | I | I | tonic | - | G | major | G | root position | 0.5 | 0.97 | - |
| m. 19, beat 2.5 to m. 19, beat 3 | vii° | vii° | dominant | - | F# | diminished | F# | root position | 0.5 | 0.69 | - |
| m. 19, beat 3 to m. 19, beat 4 | V | V6 | dominant | - | D | major | F# | first | 1 | 0.31 | - |
| m. 19, beat 4 to m. 19, beat 4.5 | vi | vi | predominant (weak) | - | E | minor | E | root position | 0.5 | 0.69 | - |
| m. 19, beat 4.5 to m. 20, beat 1 | vi | vi6 | predominant (weak) | - | E | minor | G | first | 0.5 | 0.18 | - |
| m. 20, beat 1 to m. 20, beat 1.5 | minor7 | ii7 (in D major) | predominant (strong) | - | E | minor7 | E | root position | 0.5 | 0.77 | - |
| m. 20, beat 1.5 to m. 20, beat 2 | V | V | dominant | - | A | major | A | root position | 0.5 | 0.69 | - |
| m. 20, beat 2 to m. 20, beat 3 | V | V | dominant | - | A | major | A | root position | 1 | 0.16 | - |
| m. 20, beat 3 to m. 20, beat 4 | minor7 | vi4/2 | predominant (weak) | - | B | minor7 | A | third | 1 | 0.08 | V (A major, score 0.89) |
| m. 20, beat 4 to m. 20, beat 4.5 | minor7 | iii6/5 | predominant (weak) | - | F# | minor7 | A | first | 0.5 | 0.32 | - |
| m. 20, beat 4.5 to m. 21, beat 1 | IV | IV6 | predominant (strong) | - | G | major | B | first | 0.5 | 0.60 | - |
| m. 21, beat 1 to m. 21, beat 1.5 | I | I | tonic | - | D | major | D | root position | 0.5 | 0.69 | - |
| m. 21, beat 1.5 to m. 21, beat 2 | vii° | vii° | dominant | - | C# | diminished | C# | root position | 0.5 | 0.69 | - |
| m. 21, beat 2 to m. 21, beat 2.5 | I | I | tonic | - | D | major | D | root position | 0.5 | 0.69 | - |
| m. 21, beat 2.5 to m. 21, beat 3 | iii | iii | predominant (weak) | - | F# | minor | F# | root position | 0.5 | 0.69 | - |
| m. 21, beat 3 to m. 21, beat 3.5 | V | V | dominant | - | A | major | A | root position | 0.5 | 0.69 | - |
| m. 21, beat 3.5 to m. 21, beat 4.5 | I | I | tonic | - | D | major | D | root position | 1 | 1.00 | - |
| m. 21, beat 4.5 to m. 22, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 22, beat 1 to m. 22, beat 1.5 | vi | vi | predominant (weak) | - | B | minor | B | root position | 0.5 | 0.69 | - |
| m. 22, beat 1.5 to m. 22, beat 2 | major7 | IV4/3 | predominant (strong) | - | G | major7 | D | second | 0.5 | 0.10 | I (D major, score 0.81) |
| m. 22, beat 2 to m. 22, beat 2.5 | ii | ii6/4 | predominant (strong) | - | E | minor | B | second | 0.5 | 0.30 | - |
| m. 22, beat 2.5 to m. 22, beat 3 | ii | ii6/4 | predominant (strong) | - | E | minor | C | - | 0.5 | 0.30 | - |
| m. 22, beat 3 to m. 22, beat 3.5 | ii | ii | predominant (strong) | - | E | minor | E | root position | 0.5 | 0.29 | - |
| m. 22, beat 3.5 to m. 22, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 22, beat 4 to m. 22, beat 4.5 | vi | vi | predominant (weak) | - | B | minor | B | root position | 0.5 | 0.69 | - |
| m. 22, beat 4.5 to m. 23, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 23, beat 1 to m. 23, beat 1.5 | IV | IV | predominant (strong) | - | G | major | G | root position | 0.5 | 0.69 | - |
| m. 23, beat 1.5 to m. 23, beat 2 | I | I | tonic | - | D | major | D | root position | 0.5 | 0.00 | IV (G major, score 0.66) |
| m. 23, beat 2 to m. 23, beat 2.5 | major7 | IV4/3 | predominant (strong) | - | G | major7 | D | second | 0.5 | 0.04 | I (D major, score 0.86) |
| m. 23, beat 2.5 to m. 23, beat 3 | V | V | dominant | - | A | major | A | root position | 0.5 | 1.00 | - |
| m. 23, beat 3 to m. 23, beat 4 | minor7 | ii4/2 | predominant (strong) | - | E | minor7 | D | third | 1 | 0.08 | I (D major, score 0.89) |
| m. 23, beat 4 to m. 23, beat 4.5 | minor7 | vi4/3 | predominant (weak) | - | B | minor7 | F# | second | 0.5 | 0.10 | iii (F# minor, score 0.81) |
| m. 23, beat 4.5 to m. 24, beat 1 | ii | ii | predominant (strong) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 24, beat 1 to m. 24, beat 1.5 | IV | IV | predominant (strong) | - | G | major | G | root position | 0.5 | 0.99 | - |
| m. 24, beat 1.5 to m. 24, beat 2 | iii | iii | predominant (weak) | - | F# | minor | F# | root position | 0.5 | 0.69 | - |
| m. 24, beat 2 to m. 24, beat 2.5 | IV | IV | predominant (strong) | - | G | major | G | root position | 0.5 | 0.55 | - |
| m. 24, beat 2.5 to m. 24, beat 3 | IV | IV6 | predominant (strong) | - | G | major | B | first | 0.5 | 0.55 | - |
| m. 24, beat 3 to m. 24, beat 3.5 | I | I | tonic | - | D | major | D | root position | 0.5 | 0.69 | - |
| m. 24, beat 3.5 to m. 24, beat 4 | IV | IV | predominant (strong) | - | G | major | G | root position | 0.5 | 0.55 | - |
| m. 24, beat 4 to m. 24, beat 4.5 | IV | IV6/4 | predominant (strong) | arpeggiating | G | major | D | second | 0.5 | 0.39 | - |
| m. 24, beat 4.5 to m. 25, beat 1 | IV | IV | predominant (strong) | - | G | major | G | root position | 0.5 | 0.39 | - |
## Outer voices

The highest (soprano) and lowest (bass) sounding note of each chord's span, with its scale degree in that chord's local key. A flat or sharp sign marks a chromatic alteration. A degree is "-" where the span lies in no key region. A rest means the library found no note for that line in the span, a bridged one a short gap inside the line, and a register guard a note the library dropped rather than adopt an accompaniment note. Degrees carry the key region's uncertainty in the analysis document.

| Position | Key | Soprano | Degree | Bass | Degree |
| --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 2 | G major | D5 | 5 | G3 | 1 |
| m. 1, beat 2 to m. 1, beat 2.5 | G major | C5 | 4 | A3 | 2 |
| m. 1, beat 2.5 to m. 1, beat 3 | G major | D5 | 5 | B3 | 3 |
| m. 1, beat 3 to m. 1, beat 3.5 | G major | G4 | 1 | B3 | 3 |
| m. 1, beat 3.5 to m. 1, beat 4 | G major | G4 | 1 | G4 | 1 |
| m. 1, beat 4 to m. 2, beat 1 | G major | E5 | 6 | C4 | 4 |
| m. 2, beat 1 to m. 2, beat 1.5 | G major | F#5 | 7 | rest (bridged) | - |
| m. 2, beat 1.5 to m. 2, beat 2.5 | G major | G5 | 1 | B3 | 3 |
| m. 2, beat 2.5 to m. 2, beat 3 | G major | C5 | 4 | G4 | 1 |
| m. 2, beat 3 to m. 2, beat 4 | G major | D5 | 5 | A3 | 2 |
| m. 2, beat 4 to m. 2, beat 4.5 | G major | B4 | 3 | A4 | 2 |
| m. 2, beat 4.5 to m. 3, beat 1 | G major | B4 | 3 | G3 | 1 |
| m. 3, beat 1 to m. 3, beat 2 | G major | C5 | 4 | G3 | 1 |
| m. 3, beat 2 to m. 3, beat 2.5 | G major | F#4 | 7 | D4 | 5 |
| m. 3, beat 2.5 to m. 3, beat 3 | G major | A4 | 2 | B3 | 3 |
| m. 3, beat 3 to m. 3, beat 3.5 | G major | B4 | 3 | G3 | 1 |
| m. 3, beat 3.5 to m. 3, beat 4 | G major | B4 | 3 | D4 | 5 |
| m. 3, beat 4 to m. 4, beat 1 | G major | A4 | 2 | D3 | 5 |
| m. 4, beat 1 to m. 4, beat 2 | G major | D5 | 5 | B3 | 3 |
| m. 4, beat 2 to m. 4, beat 2.5 | G major | C5 | 4 | A3 | 2 |
| m. 4, beat 2.5 to m. 4, beat 3 | G major | D5 | 5 | G3 | 1 |
| m. 4, beat 3 to m. 4, beat 3.5 | G major | G4 | 1 | B3 | 3 |
| m. 4, beat 3.5 to m. 4, beat 4 | G major | G4 | 1 | G3 | 1 |
| m. 4, beat 4 to m. 5, beat 1 | G major | E5 | 6 | C4 | 4 |
| m. 5, beat 1 to m. 5, beat 1.5 | G major | F#5 | 7 | rest (bridged) | - |
| m. 5, beat 1.5 to m. 5, beat 2 | G major | G5 | 1 | B3 | 3 |
| m. 5, beat 2 to m. 5, beat 3 | G major | C5 | 4 | G3 | 1 |
| m. 5, beat 3 to m. 5, beat 4 | G major | D5 | 5 | A3 | 2 |
| m. 5, beat 4 to m. 5, beat 4.5 | G major | B4 | 3 | F#3 | 7 |
| m. 5, beat 4.5 to m. 6, beat 1 | G major | B4 | 3 | G3 | 1 |
| m. 6, beat 1 to m. 6, beat 1.25 | G major | C5 | 4 | rest (bridged) | - |
| m. 6, beat 1.25 to m. 6, beat 2 | G major | B4 | 3 | B3 | 3 |
| m. 6, beat 2 to m. 6, beat 2.5 | G major | A4 | 2 | C4 | 4 |
| m. 6, beat 2.5 to m. 6, beat 3 | G major | B4 | 3 | D4 | 5 |
| m. 6, beat 3 to m. 6, beat 3.5 | G major | G4 | 1 | D3 | 5 |
| m. 6, beat 3.5 to m. 7, beat 1 | G major | G4 | 1 | G2 | 1 |
| m. 7, beat 1 to m. 7, beat 2 | G major | D5 | 5 | G3 | 1 |
| m. 7, beat 2 to m. 7, beat 2.5 | G major | C5 | 4 | A3 | 2 |
| m. 7, beat 2.5 to m. 7, beat 3 | G major | D5 | 5 | B3 | 3 |
| m. 7, beat 3 to m. 7, beat 4 | G major | G4 | 1 | G4 | 1 |
| m. 7, beat 4 to m. 8, beat 1 | G major | E5 | 6 | C4 | 4 |
| m. 8, beat 1 to m. 8, beat 1.5 | G major | F#5 | 7 | rest (bridged) | - |
| m. 8, beat 1.5 to m. 8, beat 2.5 | G major | G5 | 1 | B3 | 3 |
| m. 8, beat 2.5 to m. 8, beat 3 | G major | C5 | 4 | G4 | 1 |
| m. 8, beat 3 to m. 8, beat 4 | G major | D5 | 5 | A3 | 2 |
| m. 8, beat 4 to m. 8, beat 4.5 | G major | B4 | 3 | A4 | 2 |
| m. 8, beat 4.5 to m. 9, beat 1 | G major | B4 | 3 | G3 | 1 |
| m. 9, beat 1 to m. 9, beat 2 | G major | C5 | 4 | G3 | 1 |
| m. 9, beat 2 to m. 9, beat 2.5 | G major | F#4 | 7 | D4 | 5 |
| m. 9, beat 2.5 to m. 9, beat 3 | G major | A4 | 2 | B3 | 3 |
| m. 9, beat 3 to m. 9, beat 3.5 | G major | B4 | 3 | G3 | 1 |
| m. 9, beat 3.5 to m. 9, beat 4 | G major | B4 | 3 | D4 | 5 |
| m. 9, beat 4 to m. 10, beat 1 | G major | A4 | 2 | D3 | 5 |
| m. 10, beat 1 to m. 10, beat 2 | G major | D5 | 5 | B3 | 3 |
| m. 10, beat 2 to m. 10, beat 2.5 | G major | C5 | 4 | A3 | 2 |
| m. 10, beat 2.5 to m. 10, beat 3 | G major | D5 | 5 | G3 | 1 |
| m. 10, beat 3 to m. 10, beat 3.5 | G major | G4 | 1 | B3 | 3 |
| m. 10, beat 3.5 to m. 10, beat 4 | G major | G4 | 1 | G3 | 1 |
| m. 10, beat 4 to m. 11, beat 1 | G major | E5 | 6 | C4 | 4 |
| m. 11, beat 1 to m. 11, beat 1.5 | G major | F#5 | 7 | rest (bridged) | - |
| m. 11, beat 1.5 to m. 11, beat 2 | G major | G5 | 1 | B3 | 3 |
| m. 11, beat 2 to m. 11, beat 3 | G major | C5 | 4 | G3 | 1 |
| m. 11, beat 3 to m. 11, beat 4 | G major | D5 | 5 | A3 | 2 |
| m. 11, beat 4 to m. 11, beat 4.5 | G major | B4 | 3 | F#3 | 7 |
| m. 11, beat 4.5 to m. 12, beat 1 | G major | B4 | 3 | G3 | 1 |
| m. 12, beat 1 to m. 12, beat 1.25 | G major | C5 | 4 | rest (bridged) | - |
| m. 12, beat 1.25 to m. 12, beat 2 | G major | B4 | 3 | B3 | 3 |
| m. 12, beat 2 to m. 12, beat 2.5 | G major | A4 | 2 | C4 | 4 |
| m. 12, beat 2.5 to m. 12, beat 3 | G major | B4 | 3 | D4 | 5 |
| m. 12, beat 3 to m. 12, beat 3.5 | G major | G4 | 1 | D3 | 5 |
| m. 12, beat 3.5 to m. 13, beat 1 | G major | G4 | 1 | G2 | 1 |
| m. 13, beat 1 to m. 13, beat 2 | G major | B5 | 3 | G3 | 1 |
| m. 13, beat 2 to m. 13, beat 2.5 | G major | B5 | 3 | G3 | 1 |
| m. 13, beat 2.5 to m. 13, beat 3 | G major | A5 | 2 | F#3 | 7 |
| m. 13, beat 3 to m. 13, beat 4 | G major | F#5 | 7 | F#3 | 7 |
| m. 13, beat 4 to m. 13, beat 4.5 | G major | G5 | 1 | E3 | 6 |
| m. 13, beat 4.5 to m. 14, beat 1 | G major | F#5 | 7 | G3 | 1 |
| m. 14, beat 1 to m. 14, beat 1.5 | D major | G5 | 4 | E3 | 2 |
| m. 14, beat 1.5 to m. 14, beat 2 | D major | C#5 | 7 | A3 | 5 |
| m. 14, beat 2 to m. 14, beat 3 | D major | C#5 | 7 | A2 | 5 |
| m. 14, beat 3 to m. 14, beat 4 | D major | D5 | 1 | A3 | 5 |
| m. 14, beat 4 to m. 14, beat 4.5 | D major | F#5 | 3 | A3 | 5 |
| m. 14, beat 4.5 to m. 15, beat 1 | D major | G5 | 4 | B3 | 6 |
| m. 15, beat 1 to m. 15, beat 1.5 | D major | F#5 | 3 | D4 | 1 |
| m. 15, beat 1.5 to m. 15, beat 2 | D major | E5 | 2 | C#4 | 7 |
| m. 15, beat 2 to m. 15, beat 2.5 | D major | F#5 | 3 | D4 | 1 |
| m. 15, beat 2.5 to m. 15, beat 3 | D major | A4 | 5 | F#3 | 3 |
| m. 15, beat 3 to m. 15, beat 3.5 | D major | C#5 | 7 | A3 | 5 |
| m. 15, beat 3.5 to m. 15, beat 4.5 | D major | D5 | 1 | D3 | 1 |
| m. 15, beat 4.5 to m. 16, beat 1 | D major | rest (register guard) | - | C4 | b7 |
| m. 16, beat 1 to m. 16, beat 1.5 | G major | D5 | 5 | B3 | 3 |
| m. 16, beat 1.5 to m. 16, beat 2 | G major | G4 | 1 | D4 | 5 |
| m. 16, beat 2 to m. 16, beat 2.5 | G major | G4 | 1 | B3 | 3 |
| m. 16, beat 2.5 to m. 16, beat 3 | G major | E5 | 6 | C4 | 4 |
| m. 16, beat 3 to m. 16, beat 3.5 | G major | G4 | 1 | E4 | 6 |
| m. 16, beat 3.5 to m. 16, beat 4 | G major | G4 | 1 | C4 | 4 |
| m. 16, beat 4 to m. 16, beat 4.5 | G major | D5 | 5 | B3 | 3 |
| m. 16, beat 4.5 to m. 17, beat 1 | G major | C5 | 4 | A3 | 2 |
| m. 17, beat 1 to m. 17, beat 1.5 | G major | B4 | 3 | G3 | 1 |
| m. 17, beat 1.5 to m. 17, beat 2 | G major | A4 | 2 | D4 | 5 |
| m. 17, beat 2 to m. 17, beat 2.5 | G major | G4 | 1 | D4 | 5 |
| m. 17, beat 2.5 to m. 17, beat 3 | G major | A4 | 2 | A4 | 2 |
| m. 17, beat 3 to m. 17, beat 4 | G major | G4 | 1 | D3 | 5 |
| m. 17, beat 4 to m. 17, beat 4.5 | G major | B4 | 3 | F#3 | 7 |
| m. 17, beat 4.5 to m. 18, beat 1 | G major | C5 | 4 | E3 | 6 |
| m. 18, beat 1 to m. 18, beat 1.5 | G major | C5 | 4 | G3 | 1 |
| m. 18, beat 1.5 to m. 18, beat 2 | G major | A4 | 2 | F#3 | 7 |
| m. 18, beat 2 to m. 18, beat 2.5 | G major | D5 | 5 | G3 | 1 |
| m. 18, beat 2.5 to m. 18, beat 3 | G major | G4 | 1 | B2 | 3 |
| m. 18, beat 3 to m. 18, beat 3.5 | G major | F#4 | 7 | D3 | 5 |
| m. 18, beat 3.5 to m. 18, beat 4 | G major | G4 | 1 | G3 | 1 |
| m. 18, beat 4 to m. 18, beat 4.5 | G major | G4 | 1 | D3 | 5 |
| m. 18, beat 4.5 to m. 19, beat 1 | G major | rest (bridged) | - | G2 | 1 |
| m. 19, beat 1 to m. 19, beat 2 | G major | B5 | 3 | G3 | 1 |
| m. 19, beat 2 to m. 19, beat 2.5 | G major | B5 | 3 | G3 | 1 |
| m. 19, beat 2.5 to m. 19, beat 3 | G major | A5 | 2 | F#3 | 7 |
| m. 19, beat 3 to m. 19, beat 4 | G major | F#5 | 7 | F#3 | 7 |
| m. 19, beat 4 to m. 19, beat 4.5 | G major | G5 | 1 | E3 | 6 |
| m. 19, beat 4.5 to m. 20, beat 1 | G major | F#5 | 7 | G3 | 1 |
| m. 20, beat 1 to m. 20, beat 1.5 | D major | G5 | 4 | E3 | 2 |
| m. 20, beat 1.5 to m. 20, beat 2 | D major | C#5 | 7 | A3 | 5 |
| m. 20, beat 2 to m. 20, beat 3 | D major | C#5 | 7 | A2 | 5 |
| m. 20, beat 3 to m. 20, beat 4 | D major | D5 | 1 | A3 | 5 |
| m. 20, beat 4 to m. 20, beat 4.5 | D major | F#5 | 3 | A3 | 5 |
| m. 20, beat 4.5 to m. 21, beat 1 | D major | G5 | 4 | B3 | 6 |
| m. 21, beat 1 to m. 21, beat 1.5 | D major | F#5 | 3 | D4 | 1 |
| m. 21, beat 1.5 to m. 21, beat 2 | D major | E5 | 2 | C#4 | 7 |
| m. 21, beat 2 to m. 21, beat 2.5 | D major | F#5 | 3 | D4 | 1 |
| m. 21, beat 2.5 to m. 21, beat 3 | D major | A4 | 5 | F#3 | 3 |
| m. 21, beat 3 to m. 21, beat 3.5 | D major | C#5 | 7 | A3 | 5 |
| m. 21, beat 3.5 to m. 21, beat 4.5 | D major | D5 | 1 | D3 | 1 |
| m. 21, beat 4.5 to m. 22, beat 1 | D major | rest (register guard) | - | C4 | b7 |
| m. 22, beat 1 to m. 22, beat 1.5 | D major | D5 | 1 | B3 | 6 |
| m. 22, beat 1.5 to m. 22, beat 2 | D major | G4 | 4 | D4 | 1 |
| m. 22, beat 2 to m. 22, beat 2.5 | D major | G4 | 4 | B3 | 6 |
| m. 22, beat 2.5 to m. 22, beat 3 | D major | E5 | 2 | C4 | b7 |
| m. 22, beat 3 to m. 22, beat 3.5 | D major | G4 | 4 | E4 | 2 |
| m. 22, beat 3.5 to m. 22, beat 4 | D major | G4 | 4 | C4 | b7 |
| m. 22, beat 4 to m. 22, beat 4.5 | D major | D5 | 1 | B3 | 6 |
| m. 22, beat 4.5 to m. 23, beat 1 | D major | C5 | b7 | A3 | 5 |
| m. 23, beat 1 to m. 23, beat 1.5 | D major | B4 | 6 | G3 | 4 |
| m. 23, beat 1.5 to m. 23, beat 2 | D major | A4 | 5 | D4 | 1 |
| m. 23, beat 2 to m. 23, beat 2.5 | D major | G4 | 4 | D4 | 1 |
| m. 23, beat 2.5 to m. 23, beat 3 | D major | A4 | 5 | A4 | 5 |
| m. 23, beat 3 to m. 23, beat 4 | D major | G4 | 4 | D3 | 1 |
| m. 23, beat 4 to m. 23, beat 4.5 | D major | B4 | 6 | F#3 | 3 |
| m. 23, beat 4.5 to m. 24, beat 1 | D major | C5 | b7 | E3 | 2 |
| m. 24, beat 1 to m. 24, beat 1.5 | D major | C5 | b7 | G3 | 4 |
| m. 24, beat 1.5 to m. 24, beat 2 | D major | A4 | 5 | F#3 | 3 |
| m. 24, beat 2 to m. 24, beat 2.5 | D major | D5 | 1 | G3 | 4 |
| m. 24, beat 2.5 to m. 24, beat 3 | D major | G4 | 4 | B2 | 6 |
| m. 24, beat 3 to m. 24, beat 3.5 | D major | F#4 | 3 | D3 | 1 |
| m. 24, beat 3.5 to m. 24, beat 4 | D major | G4 | 4 | G3 | 4 |
| m. 24, beat 4 to m. 24, beat 4.5 | D major | G4 | 4 | D3 | 1 |
| m. 24, beat 4.5 to m. 25, beat 1 | D major | rest (register guard) | - | G2 | 4 |

## Chord vocabulary

Each distinct Roman numeral of the chords the library numbered, for the whole piece and for each key region, sorted by count, then total length, then first appearance. Count is the number of chord rows with that numeral (every row in the Chords table is one). Runs counts a stretch of adjacent identical numerals in one key region once. Beats is the total length, Share its fraction of the scope's numbered chord length, and First where it first appears. Applied chords are listed on their own. Chords without a numeral are not counted.

### Overall

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 31 | 28 | 18.5 | 19.7% | m. 1, beat 1 |
| ii | 12 | 12 | 8 | 8.5% | m. 1, beat 2 |
| V | 11 | 9 | 6.5 | 6.9% | m. 3, beat 2 |
| I6 | 8 | 8 | 5 | 5.3% | m. 1, beat 3 |
| iii | 7 | 7 | 3.5 | 3.7% | m. 1, beat 2.5 |
| vi | 7 | 7 | 3.5 | 3.7% | m. 5, beat 1 |
| IV | 7 | 7 | 3.5 | 3.7% | m. 16, beat 2.5 |
| IV7 | 6 | 6 | 4.5 | 4.8% | m. 1, beat 4 |
| iii6/5 | 6 | 6 | 4 | 4.3% | m. 3, beat 4 |
| ii4/2 | 5 | 5 | 5 | 5.3% | m. 3, beat 1 |
| vii° | 5 | 5 | 2.5 | 2.7% | m. 13, beat 2.5 |
| IV4/3 | 4 | 4 | 2.5 | 2.7% | m. 1, beat 3.5 |
| I4/3 | 4 | 4 | 2 | 2.1% | m. 6, beat 3 |
| IV6 | 4 | 4 | 2 | 2.1% | m. 14, beat 4.5 |
| vi4/2 | 3 | 3 | 3 | 3.2% | m. 14, beat 3 |
| I6/4 | 3 | 3 | 1.5 | 1.6% | m. 3, beat 3.5 |
| iii4/3 | 3 | 3 | 1.5 | 1.6% | m. 5, beat 4 |
| I7 | 2 | 2 | 3 | 3.2% | m. 6, beat 3.5 |
| iii7 | 2 | 2 | 2 | 2.1% | m. 4, beat 1 |
| V6 | 2 | 2 | 2 | 2.1% | m. 13, beat 3 |
| IV4/2 | 2 | 2 | 1.5 | 1.6% | m. 6, beat 1.25 |
| vii∅4/2 | 2 | 2 | 1 | 1.1% | m. 2, beat 1 |
| iii4/2 | 2 | 2 | 1 | 1.1% | m. 2, beat 4 |
| vi6/4 | 2 | 2 | 1 | 1.1% | m. 5, beat 1.5 |
| ii6 | 2 | 2 | 1 | 1.1% | m. 6, beat 2 |
| vi6 | 2 | 2 | 1 | 1.1% | m. 13, beat 4.5 |
| ii7 | 2 | 2 | 1 | 1.1% | m. 14, beat 1 |
| ii6/4 | 2 | 1 | 1 | 1.1% | m. 22, beat 2 |
| I6/5 | 1 | 1 | 0.5 | 0.5% | m. 16, beat 2 |
| vi4/3 | 1 | 1 | 0.5 | 0.5% | m. 23, beat 4 |
| IV6/4 | 1 | 1 | 0.5 | 0.5% | m. 24, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 42 | 39 | 25 | 26.6% | m. 1, beat 1 |
| ii | 16 | 15 | 10 | 10.6% | m. 1, beat 2 |
| iii7 | 13 | 13 | 8.5 | 9.0% | m. 2, beat 4 |
| V | 13 | 11 | 8.5 | 9.0% | m. 3, beat 2 |
| IV7 | 12 | 12 | 8.5 | 9.0% | m. 1, beat 3.5 |
| IV | 12 | 12 | 6 | 6.4% | m. 14, beat 4.5 |
| vi | 11 | 11 | 5.5 | 5.9% | m. 5, beat 1 |
| ii7 | 7 | 7 | 6 | 6.4% | m. 3, beat 1 |
| I7 | 7 | 7 | 5.5 | 5.9% | m. 6, beat 3 |
| iii | 7 | 7 | 3.5 | 3.7% | m. 1, beat 2.5 |
| vii° | 5 | 5 | 2.5 | 2.7% | m. 13, beat 2.5 |
| vi7 | 4 | 4 | 3.5 | 3.7% | m. 14, beat 3 |
| vii∅7 | 2 | 2 | 1 | 1.1% | m. 2, beat 1 |

### G major (region 0)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 16 | 15 | 9.5 | 18.3% | m. 1, beat 1 |
| ii | 8 | 8 | 6 | 11.5% | m. 1, beat 2 |
| I6 | 7 | 7 | 4.5 | 8.7% | m. 1, beat 3 |
| IV7 | 6 | 6 | 4.5 | 8.7% | m. 1, beat 4 |
| ii4/2 | 4 | 4 | 4 | 7.7% | m. 3, beat 1 |
| iii6/5 | 4 | 4 | 3 | 5.8% | m. 3, beat 4 |
| vi | 3 | 3 | 1.5 | 2.9% | m. 5, beat 1 |
| I7 | 2 | 2 | 3 | 5.8% | m. 6, beat 3.5 |
| iii7 | 2 | 2 | 2 | 3.8% | m. 4, beat 1 |
| IV4/3 | 2 | 2 | 1.5 | 2.9% | m. 1, beat 3.5 |
| IV4/2 | 2 | 2 | 1.5 | 2.9% | m. 6, beat 1.25 |
| iii | 2 | 2 | 1 | 1.9% | m. 1, beat 2.5 |
| vii∅4/2 | 2 | 2 | 1 | 1.9% | m. 2, beat 1 |
| iii4/2 | 2 | 2 | 1 | 1.9% | m. 2, beat 4 |
| V | 2 | 2 | 1 | 1.9% | m. 3, beat 2 |
| I6/4 | 2 | 2 | 1 | 1.9% | m. 3, beat 3.5 |
| vi6/4 | 2 | 2 | 1 | 1.9% | m. 5, beat 1.5 |
| iii4/3 | 2 | 2 | 1 | 1.9% | m. 5, beat 4 |
| ii6 | 2 | 2 | 1 | 1.9% | m. 6, beat 2 |
| I4/3 | 2 | 2 | 1 | 1.9% | m. 6, beat 3 |
| V6 | 1 | 1 | 1 | 1.9% | m. 13, beat 3 |
| vii° | 1 | 1 | 0.5 | 1.0% | m. 13, beat 2.5 |
| vi6 | 1 | 1 | 0.5 | 1.0% | m. 13, beat 4.5 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 25 | 24 | 15 | 28.8% | m. 1, beat 1 |
| IV7 | 10 | 10 | 7.5 | 14.4% | m. 1, beat 3.5 |
| ii | 10 | 10 | 7 | 13.5% | m. 1, beat 2 |
| iii7 | 10 | 10 | 7 | 13.5% | m. 2, beat 4 |
| vi | 6 | 6 | 3 | 5.8% | m. 5, beat 1 |
| ii7 | 4 | 4 | 4 | 7.7% | m. 3, beat 1 |
| I7 | 4 | 4 | 4 | 7.7% | m. 6, beat 3 |
| V | 3 | 3 | 2 | 3.8% | m. 3, beat 2 |
| iii | 2 | 2 | 1 | 1.9% | m. 1, beat 2.5 |
| vii∅7 | 2 | 2 | 1 | 1.9% | m. 2, beat 1 |
| vii° | 1 | 1 | 0.5 | 1.0% | m. 13, beat 2.5 |

### D major (region 1)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V | 3 | 2 | 2 | 26.7% | m. 14, beat 1.5 |
| I | 3 | 3 | 2 | 26.7% | m. 15, beat 1 |
| vi4/2 | 1 | 1 | 1 | 13.3% | m. 14, beat 3 |
| ii7 | 1 | 1 | 0.5 | 6.7% | m. 14, beat 1 |
| iii6/5 | 1 | 1 | 0.5 | 6.7% | m. 14, beat 4 |
| IV6 | 1 | 1 | 0.5 | 6.7% | m. 14, beat 4.5 |
| vii° | 1 | 1 | 0.5 | 6.7% | m. 15, beat 1.5 |
| iii | 1 | 1 | 0.5 | 6.7% | m. 15, beat 2.5 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V | 3 | 2 | 2 | 26.7% | m. 14, beat 1.5 |
| I | 3 | 3 | 2 | 26.7% | m. 15, beat 1 |
| vi7 | 1 | 1 | 1 | 13.3% | m. 14, beat 3 |
| ii7 | 1 | 1 | 0.5 | 6.7% | m. 14, beat 1 |
| iii7 | 1 | 1 | 0.5 | 6.7% | m. 14, beat 4 |
| IV | 1 | 1 | 0.5 | 6.7% | m. 14, beat 4.5 |
| vii° | 1 | 1 | 0.5 | 6.7% | m. 15, beat 1.5 |
| iii | 1 | 1 | 0.5 | 6.7% | m. 15, beat 2.5 |

### G major (region 2)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 7 | 5 | 4 | 25.0% | m. 17, beat 1 |
| iii | 2 | 2 | 1 | 6.2% | m. 16, beat 1 |
| I4/3 | 2 | 2 | 1 | 6.2% | m. 16, beat 1.5 |
| IV | 2 | 2 | 1 | 6.2% | m. 16, beat 2.5 |
| vi | 2 | 2 | 1 | 6.2% | m. 16, beat 3 |
| ii | 2 | 2 | 1 | 6.2% | m. 16, beat 4.5 |
| V | 2 | 2 | 1 | 6.2% | m. 17, beat 1.5 |
| vii° | 2 | 2 | 1 | 6.2% | m. 18, beat 1.5 |
| vi4/2 | 1 | 1 | 1 | 6.2% | m. 17, beat 3 |
| V6 | 1 | 1 | 1 | 6.2% | m. 19, beat 3 |
| I6/5 | 1 | 1 | 0.5 | 3.1% | m. 16, beat 2 |
| iii4/3 | 1 | 1 | 0.5 | 3.1% | m. 17, beat 4 |
| IV6 | 1 | 1 | 0.5 | 3.1% | m. 17, beat 4.5 |
| I6 | 1 | 1 | 0.5 | 3.1% | m. 18, beat 2.5 |
| I6/4 | 1 | 1 | 0.5 | 3.1% | m. 18, beat 4 |
| vi6 | 1 | 1 | 0.5 | 3.1% | m. 19, beat 4.5 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 9 | 7 | 5 | 31.2% | m. 17, beat 1 |
| V | 3 | 3 | 2 | 12.5% | m. 17, beat 1.5 |
| I7 | 3 | 3 | 1.5 | 9.4% | m. 16, beat 1.5 |
| IV | 3 | 3 | 1.5 | 9.4% | m. 16, beat 2.5 |
| vi | 3 | 3 | 1.5 | 9.4% | m. 16, beat 3 |
| iii | 2 | 2 | 1 | 6.2% | m. 16, beat 1 |
| ii | 2 | 2 | 1 | 6.2% | m. 16, beat 4.5 |
| vii° | 2 | 2 | 1 | 6.2% | m. 18, beat 1.5 |
| vi7 | 1 | 1 | 1 | 6.2% | m. 17, beat 3 |
| iii7 | 1 | 1 | 0.5 | 3.1% | m. 17, beat 4 |

### D major (region 3)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 5 | 5 | 3 | 16.2% | m. 21, beat 1 |
| IV | 5 | 5 | 2.5 | 13.5% | m. 23, beat 1 |
| V | 4 | 3 | 2.5 | 13.5% | m. 20, beat 1.5 |
| IV6 | 2 | 2 | 1 | 5.4% | m. 20, beat 4.5 |
| iii | 2 | 2 | 1 | 5.4% | m. 21, beat 2.5 |
| vi | 2 | 2 | 1 | 5.4% | m. 22, beat 1 |
| IV4/3 | 2 | 2 | 1 | 5.4% | m. 22, beat 1.5 |
| ii6/4 | 2 | 1 | 1 | 5.4% | m. 22, beat 2 |
| ii | 2 | 2 | 1 | 5.4% | m. 22, beat 3 |
| vi4/2 | 1 | 1 | 1 | 5.4% | m. 20, beat 3 |
| ii4/2 | 1 | 1 | 1 | 5.4% | m. 23, beat 3 |
| ii7 | 1 | 1 | 0.5 | 2.7% | m. 20, beat 1 |
| iii6/5 | 1 | 1 | 0.5 | 2.7% | m. 20, beat 4 |
| vii° | 1 | 1 | 0.5 | 2.7% | m. 21, beat 1.5 |
| vi4/3 | 1 | 1 | 0.5 | 2.7% | m. 23, beat 4 |
| IV6/4 | 1 | 1 | 0.5 | 2.7% | m. 24, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| IV | 8 | 8 | 4 | 21.6% | m. 20, beat 4.5 |
| I | 5 | 5 | 3 | 16.2% | m. 21, beat 1 |
| V | 4 | 3 | 2.5 | 13.5% | m. 20, beat 1.5 |
| ii | 4 | 3 | 2 | 10.8% | m. 22, beat 2 |
| ii7 | 2 | 2 | 1.5 | 8.1% | m. 20, beat 1 |
| vi7 | 2 | 2 | 1.5 | 8.1% | m. 20, beat 3 |
| iii | 2 | 2 | 1 | 5.4% | m. 21, beat 2.5 |
| vi | 2 | 2 | 1 | 5.4% | m. 22, beat 1 |
| IV7 | 2 | 2 | 1 | 5.4% | m. 22, beat 1.5 |
| iii7 | 1 | 1 | 0.5 | 2.7% | m. 20, beat 4 |
| vii° | 1 | 1 | 0.5 | 2.7% | m. 21, beat 1.5 |

## Non-chord tones

54 notes sound against a chord without being one of its tones; 19 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 2 | 5 | unclassified | I |
| m. 1, beat 2 to m. 1, beat 2.5 | 6 | unclassified | ii |
| m. 1, beat 4 to m. 2, beat 1 | 18 | escape-tone | major7 |
| m. 2, beat 2.5 to m. 2, beat 3 | 25 | unclassified | I |
| m. 2, beat 3 to m. 2, beat 4 | 29 | neighbor | ii |
| m. 3, beat 1 to m. 3, beat 2 | 36 | passing | minor7 |
| m. 3, beat 2.5 to m. 3, beat 3 | 43 | passing | I |
| m. 3, beat 4 to m. 4, beat 1 | 51 | appoggiatura | minor7 |
| m. 4, beat 1 to m. 4, beat 2 | 56 | appoggiatura | minor7 |
| m. 4, beat 2 to m. 4, beat 2.5 | 58 | unclassified | ii |
| m. 4, beat 4 to m. 5, beat 1 | 72 | escape-tone | major7 |
| m. 5, beat 1 to m. 5, beat 1.5 | 74 | passing | vi |
| m. 5, beat 2 to m. 5, beat 3 | 79 | passing | minor7 |
| m. 5, beat 2 to m. 5, beat 3 | 84 | unclassified | minor7 |
| m. 5, beat 3 to m. 5, beat 4 | 87 | neighbor | ii |
| m. 6, beat 1.25 to m. 6, beat 2 | 96 | unclassified | major7 |
| m. 7, beat 1 to m. 7, beat 2 | 115 | unclassified | I |
| m. 7, beat 2 to m. 7, beat 2.5 | 116 | unclassified | ii |
| m. 7, beat 4 to m. 8, beat 1 | 128 | escape-tone | major7 |
| m. 8, beat 2.5 to m. 8, beat 3 | 135 | unclassified | I |
| m. 8, beat 3 to m. 8, beat 4 | 139 | neighbor | ii |
| m. 9, beat 1 to m. 9, beat 2 | 146 | passing | minor7 |
| m. 9, beat 2.5 to m. 9, beat 3 | 153 | passing | I |
| m. 9, beat 4 to m. 10, beat 1 | 161 | appoggiatura | minor7 |
| m. 10, beat 1 to m. 10, beat 2 | 166 | appoggiatura | minor7 |
| m. 10, beat 2 to m. 10, beat 2.5 | 168 | unclassified | ii |
| m. 10, beat 4 to m. 11, beat 1 | 182 | escape-tone | major7 |
| m. 11, beat 1 to m. 11, beat 1.5 | 184 | passing | vi |
| m. 11, beat 2 to m. 11, beat 3 | 189 | passing | minor7 |
| m. 11, beat 2 to m. 11, beat 3 | 194 | unclassified | minor7 |
| m. 11, beat 3 to m. 11, beat 4 | 197 | neighbor | ii |
| m. 12, beat 1.25 to m. 12, beat 2 | 206 | unclassified | major7 |
| m. 13, beat 1 to m. 13, beat 2 | 223 | passing | I |
| m. 13, beat 3 to m. 13, beat 4 | 229 | passing | V |
| m. 13, beat 4.5 to m. 14, beat 1 | 236 | unclassified | vi |
| m. 14, beat 2 to m. 14, beat 3 | 242 | unclassified | V |
| m. 14, beat 3 to m. 14, beat 4 | 249 | passing | minor7 |
| m. 16, beat 3 to m. 16, beat 3.5 | 280 | neighbor | vi |
| m. 17, beat 1.5 to m. 17, beat 2 | 291 | unclassified | V |
| m. 17, beat 3 to m. 17, beat 4 | 298 | passing | minor7 |
| m. 18, beat 1 to m. 18, beat 1.5 | 306 | appoggiatura | I |
| m. 18, beat 1 to m. 18, beat 1.5 | 308 | neighbor | I |
| m. 19, beat 1 to m. 19, beat 2 | 328 | passing | I |
| m. 19, beat 3 to m. 19, beat 4 | 334 | passing | V |
| m. 19, beat 4.5 to m. 20, beat 1 | 341 | unclassified | vi |
| m. 20, beat 2 to m. 20, beat 3 | 347 | unclassified | V |
| m. 20, beat 3 to m. 20, beat 4 | 354 | passing | minor7 |
| m. 22, beat 2.5 to m. 22, beat 3 | 382 | unclassified | ii |
| m. 22, beat 3 to m. 22, beat 3.5 | 385 | neighbor | ii |
| m. 23, beat 1.5 to m. 23, beat 2 | 396 | unclassified | I |
| m. 23, beat 3 to m. 23, beat 4 | 403 | passing | minor7 |
| m. 23, beat 4.5 to m. 24, beat 1 | 408 | escape-tone | ii |
| m. 24, beat 1 to m. 24, beat 1.5 | 411 | appoggiatura | IV |
| m. 24, beat 1 to m. 24, beat 1.5 | 413 | neighbor | IV |

## Cadences

The library's standard-harmony analysis found 5 cadences from the chords, the meter and the soprano. Position is where the arrival begins. Key is the key region of the arrival. Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added before a section start.

| Position | Type | Key | Soprano | Beat strength | Boundary |
| --- | --- | --- | --- | --- | --- |
| m. 14, beat 3 | subverted | D major | 1 | medium | no |
| m. 15, beat 3.5 | perfect-authentic | D major | 1 | weak | no |
| m. 20, beat 3 | subverted | D major | 1 | medium | no |
| m. 21, beat 3.5 | perfect-authentic | D major | 1 | weak | no |
| m. 23, beat 3 | subverted | D major | 4 | medium | no |

### Cadence plan

1. m. 14, beat 3: subverted in D major
2. m. 15, beat 3.5: perfect-authentic in D major
3. m. 20, beat 3: subverted in D major
4. m. 21, beat 3.5: perfect-authentic in D major
5. m. 23, beat 3: subverted in D major

**Counts:** subverted 3, perfect-authentic 2.

## Phrases

The library's standard-form analysis found 2 phrases from the soprano, the meter and the cadences. Length counts the measures of the phrase proper; Ends is what closed it: its cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in the piece; Expansion is a lengthening or shortening of the phrase's ending, or an extension after its cadence, with the measures it adds or removes.

| Phrase | Measures | Length | Cadence | Ends | Role | Expansion |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 15 | 15 | perfect-authentic (m. 15, beat 3.5) | cadence | beginning | internal +7 (mm. 5 to 15) |
| 2 | mm. 16 to 21 | 6 | perfect-authentic (m. 21, beat 3.5) | cadence | ending | contraction -2 (mm. 20 to 21); external codetta +3 (mm. 22 to 24) |

- **Phrase 1:** ideas basic-idea (mm. 1 to 2), contrasting-idea (mm. 3 to 4), contrasting-idea (mm. 5 to 6), contrasting-idea (mm. 7 to 8), contrasting-idea (mm. 9 to 10), contrasting-idea (mm. 11 to 12), contrasting-idea (mm. 13 to 15); subphrases mm. 1 to 4, mm. 5 to 15
- **Phrase 2:** ideas basic-idea (mm. 16 to 17), contrasting-idea (mm. 18 to 19), contrasting-idea (mm. 20 to 21); subphrases mm. 16 to 19, mm. 20 to 21; phrase model D (m. 21, beat 3), Te (m. 21, beat 3.5)

## Themes

The library's standard-form analysis found 2 themes from the phrases, their ideas and the cadences. Form is the library's name for the pairing of the two parts; Commonality is how common that pairing is; Traits are what the library finds in the continuation.

| Theme | Form | Commonality | Measures | Length | Parts | Traits |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | hybrid | slightly-less-common | mm. 1 to 15 | 15 | compound-basic-idea (mm. 1 to 4), continuation (mm. 5 to 15, perfect-authentic (m. 15, beat 3.5)) | - |
| 2 | hybrid | slightly-less-common | mm. 16 to 21 | 6 | compound-basic-idea (mm. 16 to 19), continuation (mm. 20 to 21, perfect-authentic (m. 21, beat 3.5)) | - |

## Form plan

The kernel's form structure detection grouped the phrases into 2 sections, closing a section at each phrase that ends on a perfect or imperfect authentic cadence; the phrases after the last such cadence form the last section. Sections are lettered by first appearance, and a section whose opening theme returns an earlier section's material takes that letter with a prime. Key is the key the section opens in.

| Section | Measures | Key | Phrases | Closing cadence | Returns |
| --- | --- | --- | --- | --- | --- |
| A | mm. 1 to 15 | G major | 1 | perfect-authentic (m. 15, beat 3.5) | - |
| A' | mm. 16 to 24 | G major | 2 | perfect-authentic (m. 21, beat 3.5) | returns A (similarity 1.00) |

### Thematic returns

A later theme recalls an earlier one when the kernel's thematic return detection finds the same contour and rhythm in a stretch of both. Similarity is the lower of the two scores. Themes that share a phrase are not compared.

| Later theme | Measures | Section | Earlier theme | Measures | Section | Similarity |
| --- | --- | --- | --- | --- | --- | --- |
| 2 | mm. 16 to 21 | A' | 1 | mm. 1 to 15 | A | 1.00 |

### Outline

- form (mm. 1 to 24)
  - section A (mm. 1 to 15, G major)
    - phrase 1 (mm. 1 to 15)
      - subphrase 1 (mm. 1 to 4)
        - idea basic-idea (mm. 1 to 2)
        - idea contrasting-idea (mm. 3 to 4)
      - subphrase 2 (mm. 5 to 15)
        - idea contrasting-idea (mm. 5 to 6)
        - idea contrasting-idea (mm. 7 to 8)
        - idea contrasting-idea (mm. 9 to 10)
        - idea contrasting-idea (mm. 11 to 12)
        - idea contrasting-idea (mm. 13 to 15)
  - section A' (mm. 16 to 24, G major)
    - phrase 2 (mm. 16 to 21)
      - subphrase 1 (mm. 16 to 19)
        - idea basic-idea (mm. 16 to 17)
        - idea contrasting-idea (mm. 18 to 19)
      - subphrase 2 (mm. 20 to 21)
        - idea contrasting-idea (mm. 20 to 21)

## Motives

The library's motive analysis found 32 motive families in the soprano, the highest sounding note at each attack. A family is a short figure that recurs; Notes is the length of the figure, Pattern whether its occurrences match by scale steps or by semitones, Measures the measures the occurrences start in, Phrases the phrases they lie in (an occurrence across a phrase boundary lies in none), and Transformations how many occurrences are of each kind the library names against the first occurrence (exact, real-transposed, tonal-transposed, inverted, retrograde, augmented, diminished, fragmented, varied).

| Family | Notes | Pattern | Occurrences | Measures | Phrases | Transformations |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 8 | scale steps | 4 | 1, 4, 7, 10 | 1 | exact 4 |
| 2 | 8 | scale steps | 4 | 1, 4, 7, 10 | 1 | exact 4 |
| 3 | 8 | scale steps | 8 | 1, 4, 7, 10, 14, 17, 20, 23 | 1, 2 | exact 4, fragmented 4 |
| 4 | 8 | scale steps | 4 | 1, 4, 7, 10 | 1 | exact 4 |
| 5 | 8 | scale steps | 4 | 2, 5, 8, 11 | 1 | exact 4 |
| 6 | 8 | scale steps | 4 | 2, 5, 8, 11 | 1 | exact 4 |
| 7 | 8 | scale steps | 6 | 2, 5, 6, 8, 11, 12 | 1 | exact 4, fragmented 2 |
| 8 | 8 | scale steps | 4 | 2, 5, 8, 11 | 1 | exact 2, fragmented 2 |
| 9 | 8 | scale steps | 2 | 3, 9 | 1 | exact 2 |
| 10 | 8 | scale steps | 2 | 3, 9 | 1 | exact 2 |
| 11 | 8 | scale steps | 2 | 3, 9 | 1 | exact 2 |
| 12 | 8 | scale steps | 2 | 3, 9 | 1 | exact 2 |
| 13 | 8 | scale steps | 2 | 5, 11 | 1 | exact 2 |
| 14 | 8 | scale steps | 2 | 6, 12 | 1 | exact 2 |
| 15 | 7 | scale steps | 6 | 6, 2, 5, 8, 11, 12 | 1 | exact 2, fragmented 4 |
| 16 | 8 | scale steps | 2 | 13, 19 | 1, 2 | exact 2 |
| 17 | 8 | scale steps | 2 | 13, 19 | 1, 2 | exact 2 |
| 18 | 8 | scale steps | 2 | 13, 19 | 1, 2 | exact 2 |
| 19 | 8 | scale steps | 2 | 13, 19 | 1, 2 | exact 2 |
| 20 | 8 | scale steps | 2 | 14, 20 | 1, 2 | exact 2 |
| 21 | 8 | scale steps | 2 | 14, 20 | 1, 2 | exact 2 |
| 22 | 8 | scale steps | 4 | 14, 17, 20, 23 | 1, 2 | exact 2, fragmented 2 |
| 23 | 8 | scale steps | 2 | 14, 20 | 1, 2 | exact 2 |
| 24 | 8 | scale steps | 2 | 15, 21 | 1, 2 | exact 2 |
| 25 | 8 | scale steps | 2 | 16, 22 | 2 | exact 2 |
| 26 | 8 | scale steps | 2 | 16, 22 | 2 | exact 2 |
| 27 | 8 | scale steps | 2 | 16, 22 | 2 | exact 2 |
| 28 | 8 | scale steps | 2 | 16, 22 | 2 | exact 2 |
| 29 | 8 | scale steps | 2 | 17, 23 | 2 | exact 2 |
| 30 | 8 | scale steps | 2 | 17, 23 | 2 | exact 2 |
| 31 | 4 | scale steps | 2 | 18, 24 | 2 | exact 2 |
| 32 | 6 | scale steps | 2 | 18, 24 | 2 | exact 2 |

## Melody

The library's melody adapter read the soprano of each phrase as a line of notes: the soprano notes that start in the phrase. Range is the lowest and highest note and the semitones between them; Climax is the first note at the highest pitch, with how many notes sound it when it is not one; Contour counts the intervals between consecutive notes that go up, go down and repeat, and how many of them are steps and leaps.

| Phrase | Measures | Notes | Range | Climax | Up | Down | Same | Steps | Leaps |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 15 | 89 | F#4 to B5 (17) | B5, m. 13, beat 1, 2 notes | 42 | 33 | 13 | 43 | 32 |
| 2 | mm. 16 to 24 | 60 | F#4 to B5 (17) | B5, m. 19, beat 1, 2 notes | 22 | 27 | 8 | 27 | 22 |

### Leaps

A leap is an interval of a third or larger. The library's rule asks a leap of a fourth or larger to be followed by a step in the opposite direction: such a leap is Recovered when one follows, Unrecovered when a step the same way, a repeated note or another leap does, and Not judged when nothing follows it; a third asks nothing (Not required). Consecutive counts the pairs of leaps in a row the library's rule finds fault with.

| Phrase | Leaps | Recovered | Unrecovered | Not required | Not judged | Consecutive |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 32 | 7 | 14 | 11 | 0 | 0 |
| 2 | 22 | 2 | 13 | 7 | 0 | 2 |

### Non-chord tones

Each melody note is set against the chord of the segment it starts in, as the library's chord identification reads it: a Chord tone is one of the chord's tones, a Non-chord tone is one it left over and named, and Unclassified notes are in a segment with no chord or were left unnamed. By type counts the non-chord tones under the library's names.

| Phrase | Notes | Chord tones | Non-chord tones | Unclassified | By type |
| --- | --- | --- | --- | --- | --- |
| 1 | 89 | 78 | 8 | 3 | passing 4, neighbor 4 |
| 2 | 60 | 54 | 3 | 3 | neighbor 2, escape-tone 1 |

### Tendencies

The library's melodic tendencies are soft preferences a well-formed line usually shows: Pitch proximity (steps outnumber leaps, small leaps outnumber large ones), Step declination (descending steps outnumber ascending ones), Step inertia (the line continues in its direction at least as often as it reverses), Melodic regression (extreme notes turn back toward the middle) and Melodic arch (the line rises to a climax in its middle half and ends below it); a phrase either shows a tendency or does not. The seventh degree column counts the notes the library labels the seventh degree that another note follows, judged by its leading tone rule: a leading tone goes up to the tonic by step, and a seventh a whole step below the tonic does not. Followed is the count that did, Broken the count that did not, with where each broken one stands.

| Phrase | Pitch proximity | Step declination | Step inertia | Melodic regression | Melodic arch | Seventh degree followed | Seventh degree broken |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | does not hold | does not hold | does not hold | holds | does not hold | 8 | 3 (m. 3, beat 2; m. 9, beat 2; m. 14, beat 1.5) |
| 2 | does not hold | holds | does not hold | does not hold | holds | 8 | 1 (m. 20, beat 1.5) |

### Development

Each family's occurrences in time order, as the measure they start in and the kind the library names against the family's prototype, which is the original.

- **Family 1:** m1 original > m4 exact > m7 exact > m10 exact
- **Family 2:** m1 original > m4 exact > m7 exact > m10 exact
- **Family 3:** m1 original > m4 exact > m7 exact > m10 exact > m14 fragmented > m17 fragmented > m20 fragmented > m23 fragmented
- **Family 4:** m1 original > m4 exact > m7 exact > m10 exact
- **Family 5:** m2 original > m5 exact > m8 exact > m11 exact
- **Family 6:** m2 original > m5 exact > m8 exact > m11 exact
- **Family 7:** m2 original > m5 exact > m6 fragmented > m8 exact > m11 exact > m12 fragmented
- **Family 8:** m2 original > m5 fragmented > m8 exact > m11 fragmented
- **Family 9:** m3 original > m9 exact
- **Family 10:** m3 original > m9 exact
- **Family 11:** m3 original > m9 exact
- **Family 12:** m3 original > m9 exact
- **Family 13:** m5 original > m11 exact
- **Family 14:** m6 original > m12 exact
- **Family 15:** m2 fragmented > m5 fragmented > m6 original > m8 fragmented > m11 fragmented > m12 exact
- **Family 16:** m13 original > m19 exact
- **Family 17:** m13 original > m19 exact
- **Family 18:** m13 original > m19 exact
- **Family 19:** m13 original > m19 exact
- **Family 20:** m14 original > m20 exact
- **Family 21:** m14 original > m20 exact
- **Family 22:** m14 original > m17 fragmented > m20 exact > m23 fragmented
- **Family 23:** m14 original > m20 exact
- **Family 24:** m15 original > m21 exact
- **Family 25:** m16 original > m22 exact
- **Family 26:** m16 original > m22 exact
- **Family 27:** m16 original > m22 exact
- **Family 28:** m16 original > m22 exact
- **Family 29:** m17 original > m23 exact
- **Family 30:** m17 original > m23 exact
- **Family 31:** m18 original > m24 exact
- **Family 32:** m18 original > m24 exact

## Progression patterns

38 recurring progressions of two to six chords, counted over the library's Roman numerals: a held chord counts once, and a progression does not cross an unlabelled chord or a change of key. Ranked by count, then length, then first occurrence; the top 10 are shown. Measures are where each occurrence begins and ends.

| Rank | Progression | Count | Measures |
| --- | --- | --- | --- |
| 1 | I > IV7 | 6 | m. 1, m. 4, mm. 5 to 6, m. 10, mm. 11 to 12, m. 23 |
| 2 | ii > iii7 | 6 | m. 2, m. 5, m. 6, m. 8, m. 11, m. 12 |
| 3 | V > I | 5 | m. 3, m. 9, m. 15, mm. 18 to 19, m. 21 |
| 4 | I > vii° | 5 | m. 13, m. 15, m. 18, mm. 18 to 19, m. 21 |
| 5 | IV > I | 5 | mm. 14 to 15, mm. 17 to 18, mm. 20 to 21, m. 23, m. 24 |
| 6 | ii > iii7 > I | 4 | m. 2, m. 5, m. 8, m. 11 |
| 7 | I > ii | 4 | m. 1, m. 2, m. 7, m. 8 |
| 8 | ii7 > V | 4 | m. 3, m. 9, m. 14, m. 20 |
| 9 | vi7 > iii7 > IV > I > vii° > I | 3 | mm. 14 to 15, mm. 17 to 18, mm. 20 to 21 |
| 10 | ii > I | 3 | m. 4, m. 10, mm. 16 to 17 |

## Harmonic rhythm

How often the harmony changes. A change is a chord whose base Roman numeral or key differs from the chord before it, counted in the measure it starts in. Changes lists the measures of each row in order; Mean is changes per measure over the row.

| Measures | Changes | Mean |
| --- | --- | --- |
| mm. 1 to 4 | 5, 5, 4, 3 | 4.25 |
| mm. 5 to 8 | 5, 4, 4, 5 | 4.5 |
| mm. 9 to 12 | 4, 3, 5, 4 | 4 |
| mm. 13 to 16 | 4, 5, 7, 7 | 5.75 |
| mm. 17 to 20 | 7, 5, 3, 5 | 5 |
| mm. 21 to 24 | 7, 6, 7, 5 | 6.25 |

**Mean:** 4.96 changes per measure over 24 measures.

### Approach to cadences

Changes per measure over the two measures before each cadence's arrival (Approach), against the two measures before those (Before). Faster is yes when the approach changes harmony more often.

| Cadence | Arrival | Before | Approach | Faster |
| --- | --- | --- | --- | --- |
| 1 | m. 14 | 4 | 4 | no |
| 2 | m. 15 | 4.5 | 4.5 | no |
| 3 | m. 20 | 7 | 4 | no |
| 4 | m. 21 | 6 | 4 | no |
| 5 | m. 23 | 4 | 6.5 | yes |

## Rhythm and meter

### Density

The library's density profile of each phrase, for all its notes and for each line that has a note in it (the soprano and bass of the outer voices, and Other for the rest). A note is in a phrase when its onset is; a chord is one attack. Attacks per beat is read measure by measure over the beats the phrase spans in it, as a mean and the largest; Mean gap is the mean distance between attacks in whole notes; On the beat is the share of attacks exactly on a beat. Durations lists the notes' lengths in whole notes, shortest first, with how many notes have each. The patterns that recur are the motive families' rhythms (see Motives).

| Phrase | Measures | Line | Notes | Attacks | Per beat | Max per beat | Mean gap | On the beat | Durations |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 15 | all | 269 | 192 | 3.2 | 3.75 | 0.08 | 30.2% | 1/64 (8), 1/32 (8), 1/16 (116), 1/8 (106), 1/4 (28), 3/8 (3) |
| 1 | mm. 1 to 15 | soprano | 89 | 89 | 1.48 | 1.5 | 0.17 | 40.4% | 1/32 (4), 1/16 (37), 1/8 (44), 1/4 (4) |
| 1 | mm. 1 to 15 | bass | 76 | 76 | 1.27 | 1.75 | 0.2 | 50.0% | 1/16 (6), 1/8 (49), 1/4 (18), 3/8 (3) |
| 1 | mm. 1 to 15 | other | 104 | 98 | 1.63 | 2.25 | 0.15 | 23.5% | 1/64 (8), 1/32 (4), 1/16 (73), 1/8 (13), 1/4 (6) |
| 2 | mm. 16 to 24 | all | 161 | 104 | 2.89 | 3.25 | 0.09 | 34.6% | 1/64 (2), 1/32 (4), 1/16 (54), 5/64 (2), 1/8 (83), 1/4 (13), 3/8 (3) |
| 2 | mm. 16 to 24 | soprano | 60 | 60 | 1.67 | 2 | 0.15 | 31.7% | 1/32 (2), 1/16 (23), 1/8 (33), 1/4 (2) |
| 2 | mm. 16 to 24 | bass | 58 | 58 | 1.61 | 2 | 0.16 | 51.7% | 1/8 (48), 1/4 (7), 3/8 (3) |
| 2 | mm. 16 to 24 | other | 43 | 41 | 1.14 | 1.75 | 0.21 | 34.1% | 1/64 (2), 1/32 (2), 1/16 (31), 5/64 (2), 1/8 (2), 1/4 (4) |

### Syncopation

A note is syncopated when it is attacked at a weaker metric level and held through a stronger one that no note of its line attacks. Each line (the soprano and bass of the outer voices, and Other for the rest) is read on its own. Strength is the difference of the two levels, 1 to 4: an off-beat note held over the beat is 1, a note on a beat held over the barline is 2, an off-beat note held over the barline is 3.

| Line | Measure | Beat | Attacked at | Held through | Strength |
| --- | --- | --- | --- | --- | --- |
| soprano | 6 | 3.5 | offbeat | beat (m. 6, beat 4) | 1 |
| soprano | 12 | 3.5 | offbeat | beat (m. 12, beat 4) | 1 |
| soprano | 18 | 3.5 | offbeat | beat (m. 18, beat 4) | 1 |
| soprano | 24 | 3.5 | offbeat | beat (m. 24, beat 4) | 1 |
| bass | 1 | 2.5 | offbeat | secondary-beat (m. 1, beat 3) | 2 |
| bass | 2 | 1.5 | offbeat | beat (m. 2, beat 2) | 1 |
| bass | 2 | 4.5 | offbeat | downbeat (m. 3, beat 1) | 3 |
| bass | 8 | 1.5 | offbeat | beat (m. 8, beat 2) | 1 |
| bass | 8 | 4.5 | offbeat | downbeat (m. 9, beat 1) | 3 |
| bass | 13 | 2.5 | offbeat | secondary-beat (m. 13, beat 3) | 2 |
| bass | 14 | 1.5 | offbeat | beat (m. 14, beat 2) | 1 |
| bass | 17 | 1.5 | offbeat | beat (m. 17, beat 2) | 1 |
| bass | 19 | 2.5 | offbeat | secondary-beat (m. 19, beat 3) | 2 |
| bass | 20 | 1.5 | offbeat | beat (m. 20, beat 2) | 1 |
| bass | 23 | 1.5 | offbeat | beat (m. 23, beat 2) | 1 |
| other | 6 | 3.5 | offbeat | beat (m. 6, beat 4) | 1 |
| other | 12 | 3.5 | offbeat | beat (m. 12, beat 4) | 1 |
| other | 18 | 3.5 | offbeat | beat (m. 18, beat 4) | 1 |
| other | 18 | 3.5 | offbeat | beat (m. 18, beat 4) | 1 |
| other | 24 | 3.5 | offbeat | beat (m. 24, beat 4) | 1 |
| other | 24 | 3.5 | offbeat | beat (m. 24, beat 4) | 1 |

### Hemiola

The library found no hemiola.

### Hypermeter

The library reads each phrase in hypermeasures of two or four measures: the length that fills the phrase and holds the cadence's arrival in the last one. The first measure of each hypermeasure is a hyperdownbeat. A phrase that does not fill its hypermeasures ends in a short one, an internal expansion makes the last one longer, an external expansion adds one of its own and a contraction shortens the last. Score is the library's confidence in the reading. The form adapter does not report elided cadences, so no phrase is read as elided.

| Phrase | Measures | Length | Hyperdownbeats | Hypermeasures | Cadence arrives | Score |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 15 | 4 | 1, 5 | 1 regular, 1 expanded | m. 8 | 1 |
| 2 | mm. 16 to 24 | 2 | 16, 18, 20, 22, 24, 26 | 5 regular, 1 short | m. 21 | 0.45 |

### Tuplets and borrowed divisions

No tuplets were found.

## Texture

The library's texture adapter read every measure of the piece from the voices the kernel separated in each track or part, classified it, and merged consecutive measures of the same texture into passages. Category is the main texture (homorhythm and melody and accompaniment are homophony). Voices is the most sounding at once, Synchrony the share of attack times where the voices attack together, Pattern the score of a repeated cell (0 to 1: evidence for an accompaniment figure, not a name for it). Confidence is the texture's margin from its thresholds times the voice separation's.

| Measures | Texture | Category | Voices | Synchrony | Pattern | Confidence |
| --- | --- | --- | --- | --- | --- | --- |
| mm. 1 to 2 | melody and accompaniment | homophony | 4 | 0.27 | 0.12 | 0.51 |
| mm. 3 to 4 | polyphony | polyphony | 2 | 0.41 | 0.13 | 0.24 |
| mm. 5 to 8 | melody and accompaniment | homophony | 4 | 0.35 | 0.17 | 0.41 |
| mm. 9 to 10 | polyphony | polyphony | 2 | 0.41 | 0.13 | 0.24 |
| mm. 11 to 14 | melody and accompaniment | homophony | 2 | 0.39 | 0.24 | 0.4 |
| mm. 15 to 16 | homorhythm | homophony | 2 | 0.78 | 0.11 | 0.31 |
| mm. 17 to 20 | melody and accompaniment | homophony | 4 | 0.39 | 0.2 | 0.41 |
| mm. 21 to 22 | homorhythm | homophony | 2 | 0.78 | 0.11 | 0.26 |
| mm. 23 to 24 | melody and accompaniment | homophony | 4 | 0.44 | 0.16 | 0.35 |

### Changes

| Measure | From | To | Category changes |
| --- | --- | --- | --- |
| 3 | melody and accompaniment | polyphony | yes |
| 5 | polyphony | melody and accompaniment | yes |
| 9 | melody and accompaniment | polyphony | yes |
| 11 | polyphony | melody and accompaniment | yes |
| 15 | melody and accompaniment | homorhythm | no |
| 17 | homorhythm | melody and accompaniment | no |
| 21 | melody and accompaniment | homorhythm | no |
| 23 | homorhythm | melody and accompaniment | no |
