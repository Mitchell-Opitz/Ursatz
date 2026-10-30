# Analysis Report: Clementi 36.mid

## Summary

- **File:** Clementi 36.mid, 333 notes.
- **Input:** MIDI file. Meter: embedded in the file. Key signature: embedded in the file. Pitch spelling: by rule.
- **Key:** C major, the key with the most time across 5 key regions.
- **Chords:** 205 chord events; 199 matched a chord (97%), 6 did not.
- **Most frequent chords:** I (49), V (32), iii (19).

## Meter

- **Time signature:** 4/4
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
| m. 8, beat 1 to m. 15, beat 1 | G major | 0.89 | profile |
| m. 15, beat 1 to m. 20, beat 1 | C minor | 0.92 | profile |
| m. 20, beat 1 to m. 24, beat 1 | G major | 0.90 | profile |
| m. 24, beat 1 to m. 39, beat 1 | C major | 0.69 | profile |

## Modulations

A modulation is a lasting new key. Position is where the new key arrives. Type is pivot when the library found a pivot chord and direct when it did not. A return goes back to the key before an excursion. The library gives the change's extent from the length of the new key, whether it returns, and whether a cadence arrives in it.

| Position | From | To | Extent | Type | Return |
| --- | --- | --- | --- | --- | --- |
| m. 8, beat 1 | C major | G major | modulation | pivot | no |
| m. 15, beat 1 | G major | C minor | weak-modulation | direct | no |
| m. 20, beat 1 | C minor | G major | weak-modulation | pivot | yes |
| m. 24, beat 1 | G major | C major | modulation | pivot | no |

## Pivot chords

The chord at a modulation that is diatonic in both the old and the new key, named by root and quality, with no Roman numeral. Modulation is its row in the Modulations table, counting from 1.

| Position | Chord | Bass | Inversion | From | To | Modulation |
| --- | --- | --- | --- | --- | --- | --- |
| m. 8, beat 1 to m. 8, beat 2 | A minor7 | G | third | C major | G major | 1 |
| m. 19, beat 3 to m. 20, beat 1 | G major | G | root position | C minor | G major | 3 |
| m. 24, beat 1 to m. 24, beat 2 | C major | C | root position | G major | C major | 4 |

## Chords

Positions are measure and beat. Numeral is the Roman numeral in the local key region, which is named where it changes. Function is the library's harmonic function of the chord: an applied chord is a dominant toward its target degree, and a predominant is strong (IV, ii) or weak (iii, vi). Six-four is the library's type for a chord in second inversion. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Numeral | Function | Six-four | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 2 | I | I (in C major) | tonic | - | C | major | C | root position | 1 | 1.00 | - |
| m. 1, beat 2 to m. 1, beat 2.5 | iii | iii | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 1, beat 2.5 to m. 1, beat 3 | I | I | tonic | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 1, beat 3 to m. 2, beat 1 | V | V | dominant | - | G | major | G | root position | 2 | 1.00 | - |
| m. 2, beat 1 to m. 2, beat 2 | I | I | tonic | - | C | major | C | root position | 1 | 1.00 | - |
| m. 2, beat 2 to m. 2, beat 2.5 | iii | iii | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 2, beat 2.5 to m. 2, beat 3 | I | I | tonic | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 2, beat 3 to m. 3, beat 1 | V | V | dominant | - | G | major | G | root position | 2 | 1.00 | - |
| m. 3, beat 1 to m. 3, beat 2 | major7 | IV4/3 | predominant (strong) | - | F | major7 | C | second | 1 | 0.13 | - |
| m. 3, beat 2 to m. 3, beat 2.5 | ii | ii | predominant (strong) | - | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 3, beat 2.5 to m. 3, beat 3 | major7 | I7 | tonic | - | C | major7 | C | root position | 0.5 | 0.88 | - |
| m. 3, beat 3 to m. 3, beat 4 | major7 | I7 | tonic | - | C | major7 | C | root position | 1 | 0.88 | - |
| m. 3, beat 4 to m. 3, beat 4.5 | major7 | I4/2 | tonic | - | C | major7 | B | third | 0.5 | 0.88 | - |
| m. 3, beat 4.5 to m. 4, beat 1 | major7 | I7 | tonic | - | C | major7 | C | root position | 0.5 | 0.88 | - |
| m. 4, beat 1 to m. 4, beat 2 | V | V | dominant | - | G | major | G | root position | 1 | 0.20 | - |
| m. 4, beat 2 to m. 4, beat 2.5 | V | V6 | dominant | - | G | major | B | first | 0.5 | 0.20 | - |
| m. 4, beat 2.5 to m. 4, beat 3 | vi | vi | predominant (weak) | - | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 4, beat 3 to m. 4, beat 3.5 | V | V | dominant | - | G | major | G | root position | 0.5 | 1.00 | - |
| m. 4, beat 3.5 to m. 4, beat 4 | dominant7 | V4/2 | dominant | - | G | dominant7 | F | third | 0.5 | 0.64 | - |
| m. 4, beat 4 to m. 4, beat 4.5 | iii | iii | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 4, beat 4.5 to m. 5, beat 1 | ii | ii | predominant (strong) | - | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 5, beat 1 to m. 5, beat 2 | I | I | tonic | - | C | major | C | root position | 1 | 1.00 | - |
| m. 5, beat 2 to m. 5, beat 2.5 | iii | iii | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 5, beat 2.5 to m. 5, beat 3 | I | I | tonic | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 5, beat 3 to m. 6, beat 1 | I | I6/4 | tonic | - | C | major | G | second | 2 | 0.69 | - |
| m. 6, beat 1 to m. 6, beat 2 | I | I | tonic | - | C | major | C | root position | 1 | 0.69 | - |
| m. 6, beat 2 to m. 6, beat 2.5 | I | I6/4 | tonic | - | C | major | G | second | 0.5 | 0.69 | - |
| m. 6, beat 2.5 to m. 6, beat 3 | I | I6 | tonic | - | C | major | E | first | 0.5 | 0.69 | - |
| m. 6, beat 3 to m. 6, beat 4 | I | I | tonic | - | C | major | C | root position | 1 | 0.69 | - |
| m. 6, beat 4 to m. 7, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 7, beat 1 to m. 7, beat 2 | V | V | dominant | - | G | major | G | root position | 1 | 0.64 | - |
| m. 7, beat 2 to m. 7, beat 3 | vi | vi6 | predominant (weak) | - | A | minor | C | first | 1 | 0.36 | - |
| m. 7, beat 3 to m. 7, beat 4 | V | V6/4 | dominant | - | G | major | D | second | 1 | 0.14 | - |
| m. 7, beat 4 to m. 8, beat 1 | ii | ii | predominant (strong) | - | D | minor | D | root position | 1 | 0.88 | - |
| m. 8, beat 1 to m. 8, beat 2 | minor7 | ii4/2 (in G major) | predominant (strong) | - | A | minor7 | G | third | 1 | 0.12 | - |
| m. 8, beat 2 to m. 8, beat 2.5 | iii | iii | predominant (weak) | - | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 8, beat 2.5 to m. 8, beat 3 | IV | IV | predominant (strong) | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 8, beat 3 to m. 8, beat 3.5 | V | V | dominant | - | D | major | D | root position | 0.5 | 1.00 | - |
| m. 8, beat 3.5 to m. 8, beat 4 | vi | vi | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 8, beat 4 to m. 8, beat 4.5 | vii° | vii° | dominant | - | F# | diminished | F# | root position | 0.5 | 1.00 | - |
| m. 8, beat 4.5 to m. 9, beat 1 | I | I | tonic | - | G | major | G | root position | 0.5 | 1.00 | - |
| m. 9, beat 1 to m. 9, beat 1.5 | vii° | vii° | dominant | - | F# | diminished | F# | root position | 0.5 | 0.67 | - |
| m. 9, beat 1.5 to m. 9, beat 2 | V | V | dominant | - | D | major | D | root position | 0.5 | 0.53 | - |
| m. 9, beat 2 to m. 9, beat 2.5 | V | V6/4 | dominant | arpeggiating | D | major | A | second | 0.5 | 0.53 | - |
| m. 9, beat 2.5 to m. 9, beat 3 | V | V | dominant | - | D | major | D | root position | 0.5 | 0.53 | - |
| m. 9, beat 3 to m. 9, beat 3.5 | vii° | vii° | dominant | - | F# | diminished | F# | root position | 0.5 | 0.67 | - |
| m. 9, beat 3.5 to m. 9, beat 4 | V | V | dominant | - | D | major | D | root position | 0.5 | 0.53 | - |
| m. 9, beat 4 to m. 9, beat 4.5 | V | V6/4 | dominant | arpeggiating | D | major | A | second | 0.5 | 0.53 | - |
| m. 9, beat 4.5 to m. 10, beat 1 | V | V | dominant | - | D | major | D | root position | 0.5 | 0.53 | - |
| m. 10, beat 1 to m. 10, beat 2 | I | I | tonic | - | G | major | G | root position | 1 | 0.04 | major7 (C major7, score 0.90) |
| m. 10, beat 2 to m. 10, beat 2.5 | I | I6/4 | tonic | - | G | major | D | second | 0.5 | 0.04 | major7 (C major7, score 0.90) |
| m. 10, beat 2.5 to m. 10, beat 3 | vi | vi | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 10, beat 3 to m. 10, beat 3.5 | vii° | vii° | dominant | - | F# | diminished | F# | root position | 0.5 | 1.00 | - |
| m. 10, beat 3.5 to m. 10, beat 4 | I | I | tonic | - | G | major | G | root position | 0.5 | 1.00 | - |
| m. 10, beat 4 to m. 10, beat 4.5 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 10, beat 4.5 to m. 11, beat 1 | iii | iii | predominant (weak) | - | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 11, beat 1 to m. 11, beat 1.5 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 0.67 | - |
| m. 11, beat 1.5 to m. 11, beat 2 | dominant7 | V7 | dominant | - | D | dominant7 | D | root position | 0.5 | 0.53 | - |
| m. 11, beat 2 to m. 11, beat 2.5 | dominant7 | V4/2 | dominant | - | D | dominant7 | C | third | 0.5 | 0.53 | - |
| m. 11, beat 2.5 to m. 11, beat 3 | dominant7 | V7 | dominant | - | D | dominant7 | D | root position | 0.5 | 0.53 | - |
| m. 11, beat 3 to m. 11, beat 3.5 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 0.67 | - |
| m. 11, beat 3.5 to m. 11, beat 4 | dominant7 | V7 | dominant | - | D | dominant7 | D | root position | 0.5 | 0.53 | - |
| m. 11, beat 4 to m. 11, beat 4.5 | dominant7 | V4/2 | dominant | - | D | dominant7 | C | third | 0.5 | 0.53 | - |
| m. 11, beat 4.5 to m. 12, beat 1 | dominant7 | V7 | dominant | - | D | dominant7 | D | root position | 0.5 | 0.53 | - |
| m. 12, beat 1 to m. 12, beat 2 | I | I6 | tonic | - | G | major | B | first | 1 | 0.10 | - |
| m. 12, beat 2 to m. 12, beat 2.5 | I | I6 | tonic | - | G | major | B | first | 0.5 | 0.10 | - |
| m. 12, beat 2.5 to m. 12, beat 3 | I | I6/4 | tonic | - | G | major | D | second | 0.5 | 0.10 | - |
| m. 12, beat 3 to m. 12, beat 3.5 | IV | IV | predominant (strong) | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 12, beat 3.5 to m. 12, beat 4 | iii | iii | predominant (weak) | - | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 12, beat 4 to m. 12, beat 4.5 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 12, beat 4.5 to m. 13, beat 1 | I | I | tonic | - | G | major | G | root position | 0.5 | 1.00 | - |
| m. 13, beat 1 to m. 13, beat 2 | half-diminished7 | vii∅4/3 | dominant | - | F# | half-diminished7 | C | second | 1 | 0.13 | - |
| m. 13, beat 2 to m. 13, beat 2.5 | I | I | tonic | - | G | major | G | root position | 0.5 | 1.00 | - |
| m. 13, beat 2.5 to m. 13, beat 3 | vii° | vii° | dominant | - | F# | diminished | F# | root position | 0.5 | 1.00 | - |
| m. 13, beat 3 to m. 13, beat 4 | minor7 | ii6/5 | predominant (strong) | - | A | minor7 | C | first | 1 | 0.39 | - |
| m. 13, beat 4 to m. 13, beat 4.5 | vii° | vii° | dominant | - | F# | diminished | F# | root position | 0.5 | 1.00 | - |
| m. 13, beat 4.5 to m. 14, beat 1 | vi | vi | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 14, beat 1 to m. 14, beat 2 | minor7 | vi4/2 | predominant (weak) | - | E | minor7 | D | third | 1 | 0.30 | - |
| m. 14, beat 2 to m. 14, beat 2.5 | IV | IV | predominant (strong) | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 14, beat 2.5 to m. 14, beat 3 | iii | iii | predominant (weak) | - | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 14, beat 3 to m. 14, beat 4 | dominant7 | V7 | dominant | - | D | dominant7 | D | root position | 1 | 1.00 | - |
| m. 14, beat 4 to m. 14, beat 4.5 | iii | iii | predominant (weak) | - | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 14, beat 4.5 to m. 15, beat 1 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 15, beat 1 to m. 15, beat 1.5 | major | V (in C minor) | dominant | - | G | major | G | root position | 0.5 | 1.00 | - |
| m. 15, beat 1.5 to m. 15, beat 2 | major | V6 | dominant | - | G | major | B | first | 0.5 | 0.71 | - |
| m. 15, beat 2 to m. 15, beat 3 | major | V6/4 | dominant | - | G | major | D | second | 1 | 0.88 | - |
| m. 15, beat 3 to m. 15, beat 4 | major | V | dominant | - | G | major | G | root position | 1 | 1.00 | - |
| m. 16, beat 1 to m. 16, beat 2 | diminished | viio6/4 | dominant | - | B | diminished | F | second | 1 | 0.17 | - |
| m. 16, beat 2 to m. 16, beat 3 | diminished | viio6/4 | dominant | - | B | diminished | F | second | 1 | 0.17 | - |
| m. 16, beat 3 to m. 17, beat 1 | dominant7 | V4/2 | dominant | - | G | dominant7 | F | third | 2 | 0.50 | - |
| m. 17, beat 1 to m. 17, beat 3 | minor | i6 | tonic | - | C | minor | Eb | first | 2 | 0.45 | - |
| m. 17, beat 3 to m. 18, beat 1 | major | III | predominant (weak) | - | Eb | major | Eb | root position | 2 | 0.72 | - |
| m. 18, beat 1 to m. 18, beat 2 | diminished | viio | dominant | - | B | diminished | B | root position | 1 | 0.65 | - |
| m. 18, beat 2 to m. 18, beat 3 | diminished | viio | dominant | - | B | diminished | B | root position | 1 | 0.78 | - |
| m. 18, beat 3 to m. 18, beat 4 | minor | i | tonic | - | C | minor | C | root position | 1 | 0.70 | - |
| m. 18, beat 4 to m. 19, beat 1 | minor | i | tonic | - | C | minor | C | root position | 1 | 0.70 | - |
| m. 19, beat 1 to m. 19, beat 2 | major | V | dominant | - | G | major | G | root position | 1 | 0.33 | - |
| m. 19, beat 2 to m. 19, beat 3 | major | V | dominant | - | G | major | G | root position | 1 | 0.65 | - |
| m. 19, beat 3 to m. 20, beat 1 | major | V | dominant | - | G | major | G | root position | 2 | 0.65 | - |
| m. 20, beat 1 to m. 20, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 20, beat 2 to m. 20, beat 3 | I | I (in G major) | tonic | - | G | major | G | root position | 1 | 1.00 | - |
| m. 20, beat 3 to m. 20, beat 4 | I | I6/4 | tonic | arpeggiating | G | major | D | second | 1 | 0.45 | - |
| m. 20, beat 4 to m. 21, beat 1 | I | I | tonic | - | G | major | G | root position | 1 | 1.00 | - |
| m. 21, beat 1 to m. 21, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 21, beat 2 to m. 21, beat 3 | IV | IV6/4 | predominant (strong) | - | C | major | G | second | 1 | 0.72 | - |
| m. 21, beat 3 to m. 21, beat 4 | IV | IV | predominant (strong) | - | C | major | C | root position | 1 | 0.72 | - |
| m. 21, beat 4 to m. 22, beat 1 | IV | IV6/4 | predominant (strong) | - | C | major | G | second | 1 | 0.72 | - |
| m. 22, beat 1 to m. 22, beat 2 | iii | iii | predominant (weak) | - | B | minor | B | root position | 1 | 0.87 | - |
| m. 22, beat 2 to m. 22, beat 2.5 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 22, beat 2.5 to m. 22, beat 3 | V | V | dominant | - | D | major | D | root position | 0.5 | 1.00 | - |
| m. 22, beat 3 to m. 22, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 22, beat 4 to m. 22, beat 4.5 | V | V | dominant | - | D | major | D | root position | 0.5 | 1.00 | - |
| m. 22, beat 4.5 to m. 23, beat 1 | IV | IV | predominant (strong) | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 23, beat 1 to m. 23, beat 2 | I | I | tonic | - | G | major | G | root position | 1 | 0.85 | - |
| m. 23, beat 2 to m. 23, beat 3.5 | I | I | tonic | - | G | major | G | root position | 1.5 | 0.85 | - |
| m. 23, beat 3.5 to m. 23, beat 4 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 23, beat 4 to m. 23, beat 4.5 | vi | vi | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 23, beat 4.5 to m. 24, beat 1 | V | V | dominant | - | D | major | D | root position | 0.5 | 1.00 | - |
| m. 24, beat 1 to m. 24, beat 2 | I | I (in C major) | tonic | - | C | major | C | root position | 1 | 1.00 | - |
| m. 24, beat 2 to m. 24, beat 2.5 | iii | iii | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 24, beat 2.5 to m. 24, beat 3 | I | I | tonic | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 24, beat 3 to m. 25, beat 1 | V | V | dominant | - | G | major | G | root position | 2 | 1.00 | - |
| m. 25, beat 1 to m. 25, beat 2 | I | I | tonic | - | C | major | C | root position | 1 | 1.00 | - |
| m. 25, beat 2 to m. 25, beat 2.5 | iii | iii | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 25, beat 2.5 to m. 25, beat 3 | I | I | tonic | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 25, beat 3 to m. 26, beat 1 | V | V | dominant | - | G | major | G | root position | 2 | 1.00 | - |
| m. 26, beat 1 to m. 26, beat 2 | major7 | IV4/3 | predominant (strong) | - | F | major7 | C | second | 1 | 0.13 | - |
| m. 26, beat 2 to m. 26, beat 2.5 | ii | ii | predominant (strong) | - | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 26, beat 2.5 to m. 26, beat 3 | major7 | I7 | tonic | - | C | major7 | C | root position | 0.5 | 0.88 | - |
| m. 26, beat 3 to m. 26, beat 4 | major7 | I7 | tonic | - | C | major7 | C | root position | 1 | 0.88 | - |
| m. 26, beat 4 to m. 26, beat 4.5 | major7 | I4/2 | tonic | - | C | major7 | B | third | 0.5 | 0.88 | - |
| m. 26, beat 4.5 to m. 27, beat 1 | major7 | I7 | tonic | - | C | major7 | C | root position | 0.5 | 0.88 | - |
| m. 27, beat 1 to m. 27, beat 2 | V | V | dominant | - | G | major | G | root position | 1 | 0.20 | - |
| m. 27, beat 2 to m. 27, beat 2.5 | V | V6 | dominant | - | G | major | B | first | 0.5 | 0.20 | - |
| m. 27, beat 2.5 to m. 27, beat 3 | vi | vi | predominant (weak) | - | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 27, beat 3 to m. 27, beat 3.5 | V | V | dominant | - | G | major | G | root position | 0.5 | 1.00 | - |
| m. 27, beat 3.5 to m. 27, beat 4 | dominant7 | V4/2 | dominant | - | G | dominant7 | F | third | 0.5 | 0.64 | - |
| m. 27, beat 4 to m. 27, beat 4.5 | iii | iii | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 27, beat 4.5 to m. 28, beat 1 | ii | ii | predominant (strong) | - | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 28, beat 1 to m. 28, beat 2 | I | I | tonic | - | C | major | C | root position | 1 | 1.00 | - |
| m. 28, beat 2 to m. 28, beat 2.5 | V | V | dominant | - | G | major | G | root position | 0.5 | 1.00 | - |
| m. 28, beat 2.5 to m. 28, beat 3 | I | I | tonic | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 28, beat 3 to m. 29, beat 1 | I | I6 | tonic | - | C | major | E | first | 2 | 0.69 | - |
| m. 29, beat 1 to m. 29, beat 2 | I | I | tonic | - | C | major | C | root position | 1 | 0.69 | - |
| m. 29, beat 2 to m. 29, beat 2.5 | I | I | tonic | - | C | major | C | root position | 0.5 | 0.69 | - |
| m. 29, beat 2.5 to m. 29, beat 3 | I | I6 | tonic | - | C | major | E | first | 0.5 | 0.69 | - |
| m. 29, beat 3 to m. 29, beat 4 | I | I6/4 | tonic | - | C | major | G | second | 1 | 0.69 | - |
| m. 29, beat 4 to m. 30, beat 1 | I | I | tonic | - | C | major | C | root position | 1 | 0.69 | - |
| m. 30, beat 1 to m. 30, beat 2 | I | I6/4 | tonic | - | C | major | G | second | 1 | 0.69 | - |
| m. 30, beat 2 to m. 30, beat 3 | ii | ii | predominant (strong) | - | D | minor | D | root position | 1 | 0.69 | - |
| m. 30, beat 3 to m. 30, beat 4 | I | I6/4 | tonic | - | C | major | G | second | 1 | 0.31 | - |
| m. 30, beat 4 to m. 31, beat 1 | vii° | vii° | dominant | - | B | diminished | B | root position | 1 | 0.69 | - |
| m. 31, beat 1 to m. 31, beat 2 | minor7 | ii4/2 | predominant (strong) | - | D | minor7 | C | third | 1 | 0.12 | - |
| m. 31, beat 2 to m. 31, beat 2.5 | iii | iii | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 31, beat 2.5 to m. 31, beat 3 | IV | IV | predominant (strong) | - | F | major | F | root position | 0.5 | 1.00 | - |
| m. 31, beat 3 to m. 31, beat 3.5 | V | V | dominant | - | G | major | G | root position | 0.5 | 1.00 | - |
| m. 31, beat 3.5 to m. 31, beat 4 | vi | vi | predominant (weak) | - | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 31, beat 4 to m. 31, beat 4.5 | vii° | vii° | dominant | - | B | diminished | B | root position | 0.5 | 1.00 | - |
| m. 31, beat 4.5 to m. 32, beat 1 | I | I | tonic | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 32, beat 1 to m. 32, beat 1.5 | vii° | vii° | dominant | - | B | diminished | B | root position | 0.5 | 0.67 | - |
| m. 32, beat 1.5 to m. 32, beat 2 | V | V | dominant | - | G | major | G | root position | 0.5 | 0.53 | - |
| m. 32, beat 2 to m. 32, beat 2.5 | V | V6/4 | dominant | arpeggiating | G | major | D | second | 0.5 | 0.53 | - |
| m. 32, beat 2.5 to m. 32, beat 3 | V | V | dominant | - | G | major | G | root position | 0.5 | 0.53 | - |
| m. 32, beat 3 to m. 32, beat 3.5 | vii° | vii° | dominant | - | B | diminished | B | root position | 0.5 | 0.67 | - |
| m. 32, beat 3.5 to m. 32, beat 4 | V | V | dominant | - | G | major | G | root position | 0.5 | 0.53 | - |
| m. 32, beat 4 to m. 32, beat 4.5 | V | V6/4 | dominant | arpeggiating | G | major | D | second | 0.5 | 0.53 | - |
| m. 32, beat 4.5 to m. 33, beat 1 | V | V | dominant | - | G | major | G | root position | 0.5 | 0.53 | - |
| m. 33, beat 1 to m. 33, beat 2 | I | I | tonic | - | C | major | C | root position | 1 | 0.04 | major7 (F major7, score 0.90) |
| m. 33, beat 2 to m. 33, beat 2.5 | I | I6/4 | tonic | - | C | major | G | second | 0.5 | 0.04 | major7 (F major7, score 0.90) |
| m. 33, beat 2.5 to m. 33, beat 3 | vi | vi | predominant (weak) | - | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 33, beat 3 to m. 33, beat 3.5 | vii° | vii° | dominant | - | B | diminished | B | root position | 0.5 | 1.00 | - |
| m. 33, beat 3.5 to m. 33, beat 4 | I | I | tonic | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 33, beat 4 to m. 33, beat 4.5 | ii | ii | predominant (strong) | - | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 33, beat 4.5 to m. 34, beat 1 | iii | iii | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 34, beat 1 to m. 34, beat 1.5 | ii | ii | predominant (strong) | - | D | minor | D | root position | 0.5 | 0.67 | - |
| m. 34, beat 1.5 to m. 34, beat 2 | dominant7 | V7 | dominant | - | G | dominant7 | G | root position | 0.5 | 0.53 | - |
| m. 34, beat 2 to m. 34, beat 2.5 | dominant7 | V4/2 | dominant | - | G | dominant7 | F | third | 0.5 | 0.53 | - |
| m. 34, beat 2.5 to m. 34, beat 3 | dominant7 | V7 | dominant | - | G | dominant7 | G | root position | 0.5 | 0.53 | - |
| m. 34, beat 3 to m. 34, beat 3.5 | ii | ii | predominant (strong) | - | D | minor | D | root position | 0.5 | 0.67 | - |
| m. 34, beat 3.5 to m. 34, beat 4 | dominant7 | V7 | dominant | - | G | dominant7 | G | root position | 0.5 | 0.53 | - |
| m. 34, beat 4 to m. 34, beat 4.5 | dominant7 | V4/2 | dominant | - | G | dominant7 | F | third | 0.5 | 0.53 | - |
| m. 34, beat 4.5 to m. 35, beat 1 | dominant7 | V7 | dominant | - | G | dominant7 | G | root position | 0.5 | 0.53 | - |
| m. 35, beat 1 to m. 35, beat 2 | I | I6 | tonic | - | C | major | E | first | 1 | 0.10 | - |
| m. 35, beat 2 to m. 35, beat 2.5 | I | I6 | tonic | - | C | major | E | first | 0.5 | 0.10 | - |
| m. 35, beat 2.5 to m. 35, beat 3 | I | I6/4 | tonic | - | C | major | G | second | 0.5 | 0.10 | - |
| m. 35, beat 3 to m. 35, beat 3.5 | IV | IV | predominant (strong) | - | F | major | F | root position | 0.5 | 1.00 | - |
| m. 35, beat 3.5 to m. 35, beat 4 | iii | iii | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 35, beat 4 to m. 35, beat 4.5 | ii | ii | predominant (strong) | - | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 35, beat 4.5 to m. 36, beat 1 | IV | IV6/4 | predominant (strong) | - | F | major | C | second | 0.5 | 0.33 | - |
| m. 36, beat 1 to m. 36, beat 2 | IV | IV | predominant (strong) | - | F | major | F | root position | 1 | 0.33 | - |
| m. 36, beat 2 to m. 36, beat 2.5 | IV | IV | predominant (strong) | - | F | major | F | root position | 0.5 | 0.33 | - |
| m. 36, beat 2.5 to m. 36, beat 3 | iii | iii | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 36, beat 3 to m. 36, beat 4 | minor7 | ii6/5 | predominant (strong) | - | D | minor7 | F | first | 1 | 0.39 | - |
| m. 36, beat 4 to m. 36, beat 4.5 | vii° | vii° | dominant | - | B | diminished | B | root position | 0.5 | 1.00 | - |
| m. 36, beat 4.5 to m. 37, beat 1 | vi | vi | predominant (weak) | - | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 37, beat 1 to m. 37, beat 2 | minor7 | vi4/2 | predominant (weak) | - | A | minor7 | G | third | 1 | 0.12 | - |
| m. 37, beat 2 to m. 37, beat 2.5 | IV | IV | predominant (strong) | - | F | major | F | root position | 0.5 | 1.00 | - |
| m. 37, beat 2.5 to m. 37, beat 3 | V | V | dominant | - | G | major | G | root position | 0.5 | 1.00 | - |
| m. 37, beat 3 to m. 37, beat 4 | iii | iii6 | predominant (weak) | - | E | minor | G | first | 1 | 0.29 | - |
| m. 37, beat 4 to m. 37, beat 4.5 | ii | ii | predominant (strong) | - | D | minor | D | root position | 0.5 | 1.00 | - |
| m. 37, beat 4.5 to m. 38, beat 1 | iii | iii | predominant (weak) | - | E | minor | E | root position | 0.5 | 1.00 | - |
| m. 38, beat 1 to m. 38, beat 1.5 | I | I | tonic | - | C | major | C | root position | 0.5 | 1.00 | - |
| m. 38, beat 1.5 to m. 38, beat 2 | I | I6 | tonic | - | C | major | E | first | 0.5 | 0.69 | - |
| m. 38, beat 2 to m. 38, beat 2.5 | V | V | dominant | - | G | major | G | root position | 0.5 | 1.00 | - |
| m. 38, beat 2.5 to m. 38, beat 4 | I | I | tonic | - | C | major | C | root position | 1.5 | 1.00 | - |
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
| m. 15, beat 1 to m. 15, beat 1.5 | C minor | G4 | 5 | G2 | 5 |
| m. 15, beat 1.5 to m. 15, beat 2 | C minor | G4 | 5 | B2 | #7 |
| m. 15, beat 2 to m. 15, beat 3 | C minor | G4 | 5 | D3 | 2 |
| m. 15, beat 3 to m. 15, beat 4 | C minor | rest (register guard) | - | G2 | 5 |
| m. 16, beat 1 to m. 16, beat 2 | C minor | B4 | #7 | F4 | 4 |
| m. 16, beat 2 to m. 16, beat 3 | C minor | D5 | 2 | F4 | 4 |
| m. 16, beat 3 to m. 17, beat 1 | C minor | G4 | 5 | F4 | 4 |
| m. 17, beat 1 to m. 17, beat 3 | C minor | Eb5 | 3 | Eb4 | 3 |
| m. 17, beat 3 to m. 18, beat 1 | C minor | G5 | 5 | Eb4 | 3 |
| m. 18, beat 1 to m. 18, beat 2 | C minor | F5 | 4 | B3 | #7 |
| m. 18, beat 2 to m. 18, beat 3 | C minor | D5 | 2 | B3 | #7 |
| m. 18, beat 3 to m. 18, beat 4 | C minor | Eb5 | 3 | C4 | 1 |
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
| I | 31 | 29 | 25 | 17.2% | m. 1, beat 1 |
| V | 30 | 28 | 25.5 | 17.6% | m. 1, beat 3 |
| iii | 18 | 18 | 9.5 | 6.6% | m. 1, beat 2 |
| ii | 16 | 16 | 9 | 6.2% | m. 3, beat 2 |
| vii° | 12 | 12 | 6.5 | 4.5% | m. 8, beat 4 |
| I6/4 | 10 | 10 | 8.5 | 5.9% | m. 5, beat 3 |
| IV | 10 | 9 | 6 | 4.1% | m. 8, beat 2.5 |
| V7 | 9 | 9 | 5 | 3.4% | m. 11, beat 1.5 |
| vi | 9 | 9 | 4.5 | 3.1% | m. 4, beat 2.5 |
| I6 | 8 | 6 | 6.5 | 4.5% | m. 6, beat 2.5 |
| V4/2 | 7 | 7 | 5 | 3.4% | m. 4, beat 3.5 |
| I7 | 6 | 4 | 4 | 2.8% | m. 3, beat 2.5 |
| V6/4 | 6 | 6 | 4 | 2.8% | m. 7, beat 3 |
| IV6/4 | 3 | 3 | 2.5 | 1.7% | m. 21, beat 2 |
| V6 | 3 | 3 | 1.5 | 1.0% | m. 4, beat 2 |
| IV4/3 | 2 | 2 | 2 | 1.4% | m. 3, beat 1 |
| ii4/2 | 2 | 2 | 2 | 1.4% | m. 8, beat 1 |
| ii6/5 | 2 | 2 | 2 | 1.4% | m. 13, beat 3 |
| vi4/2 | 2 | 2 | 2 | 1.4% | m. 14, beat 1 |
| viio6/4 | 2 | 1 | 2 | 1.4% | m. 16, beat 1 |
| viio | 2 | 1 | 2 | 1.4% | m. 18, beat 1 |
| i | 2 | 1 | 2 | 1.4% | m. 18, beat 3 |
| I4/2 | 2 | 2 | 1 | 0.7% | m. 3, beat 4 |
| i6 | 1 | 1 | 2 | 1.4% | m. 17, beat 1 |
| III | 1 | 1 | 2 | 1.4% | m. 17, beat 3 |
| vi6 | 1 | 1 | 1 | 0.7% | m. 7, beat 2 |
| vii∅4/3 | 1 | 1 | 1 | 0.7% | m. 13, beat 1 |
| iii6 | 1 | 1 | 1 | 0.7% | m. 37, beat 3 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 49 | 45 | 40 | 27.6% | m. 1, beat 1 |
| V | 39 | 37 | 31 | 21.4% | m. 1, beat 3 |
| iii | 19 | 19 | 10.5 | 7.2% | m. 1, beat 2 |
| V7 | 16 | 16 | 10 | 6.9% | m. 4, beat 3.5 |
| ii | 16 | 16 | 9 | 6.2% | m. 3, beat 2 |
| IV | 13 | 12 | 8.5 | 5.9% | m. 8, beat 2.5 |
| vii° | 12 | 12 | 6.5 | 4.5% | m. 8, beat 4 |
| vi | 10 | 10 | 5.5 | 3.8% | m. 4, beat 2.5 |
| I7 | 8 | 6 | 5 | 3.4% | m. 3, beat 2.5 |
| ii7 | 4 | 4 | 4 | 2.8% | m. 8, beat 1 |
| viio | 4 | 2 | 4 | 2.8% | m. 16, beat 1 |
| i | 3 | 2 | 4 | 2.8% | m. 17, beat 1 |
| IV7 | 2 | 2 | 2 | 1.4% | m. 3, beat 1 |
| vi7 | 2 | 2 | 2 | 1.4% | m. 14, beat 1 |
| III | 1 | 1 | 2 | 1.4% | m. 17, beat 3 |
| vii∅7 | 1 | 1 | 1 | 0.7% | m. 13, beat 1 |

### C major (region 0)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 8 | 8 | 6.5 | 24.1% | m. 1, beat 1 |
| V | 5 | 5 | 6.5 | 24.1% | m. 1, beat 3 |
| iii | 4 | 4 | 2 | 7.4% | m. 1, beat 2 |
| ii | 3 | 3 | 2 | 7.4% | m. 3, beat 2 |
| I7 | 3 | 2 | 2 | 7.4% | m. 3, beat 2.5 |
| I6/4 | 2 | 2 | 2.5 | 9.3% | m. 5, beat 3 |
| IV4/3 | 1 | 1 | 1 | 3.7% | m. 3, beat 1 |
| vi6 | 1 | 1 | 1 | 3.7% | m. 7, beat 2 |
| V6/4 | 1 | 1 | 1 | 3.7% | m. 7, beat 3 |
| I4/2 | 1 | 1 | 0.5 | 1.9% | m. 3, beat 4 |
| V6 | 1 | 1 | 0.5 | 1.9% | m. 4, beat 2 |
| vi | 1 | 1 | 0.5 | 1.9% | m. 4, beat 2.5 |
| V4/2 | 1 | 1 | 0.5 | 1.9% | m. 4, beat 3.5 |
| I6 | 1 | 1 | 0.5 | 1.9% | m. 6, beat 2.5 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 11 | 11 | 9.5 | 35.2% | m. 1, beat 1 |
| V | 7 | 7 | 8 | 29.6% | m. 1, beat 3 |
| I7 | 4 | 3 | 2.5 | 9.3% | m. 3, beat 2.5 |
| iii | 4 | 4 | 2 | 7.4% | m. 1, beat 2 |
| ii | 3 | 3 | 2 | 7.4% | m. 3, beat 2 |
| vi | 2 | 2 | 1.5 | 5.6% | m. 4, beat 2.5 |
| IV7 | 1 | 1 | 1 | 3.7% | m. 3, beat 1 |
| V7 | 1 | 1 | 0.5 | 1.9% | m. 4, beat 3.5 |

### G major (region 1)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii° | 6 | 6 | 3 | 10.7% | m. 8, beat 4 |
| I | 5 | 5 | 3 | 10.7% | m. 8, beat 4.5 |
| V7 | 5 | 5 | 3 | 10.7% | m. 11, beat 1.5 |
| iii | 5 | 5 | 2.5 | 8.9% | m. 8, beat 2 |
| V | 5 | 5 | 2.5 | 8.9% | m. 8, beat 3 |
| ii | 5 | 5 | 2.5 | 8.9% | m. 10, beat 4 |
| IV | 3 | 3 | 1.5 | 5.4% | m. 8, beat 2.5 |
| vi | 3 | 3 | 1.5 | 5.4% | m. 8, beat 3.5 |
| I6 | 2 | 1 | 1.5 | 5.4% | m. 12, beat 1 |
| V6/4 | 2 | 2 | 1 | 3.6% | m. 9, beat 2 |
| I6/4 | 2 | 2 | 1 | 3.6% | m. 10, beat 2 |
| V4/2 | 2 | 2 | 1 | 3.6% | m. 11, beat 2 |
| ii4/2 | 1 | 1 | 1 | 3.6% | m. 8, beat 1 |
| vii∅4/3 | 1 | 1 | 1 | 3.6% | m. 13, beat 1 |
| ii6/5 | 1 | 1 | 1 | 3.6% | m. 13, beat 3 |
| vi4/2 | 1 | 1 | 1 | 3.6% | m. 14, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 9 | 8 | 5.5 | 19.6% | m. 8, beat 4.5 |
| V7 | 7 | 7 | 4 | 14.3% | m. 11, beat 1.5 |
| V | 7 | 7 | 3.5 | 12.5% | m. 8, beat 3 |
| vii° | 6 | 6 | 3 | 10.7% | m. 8, beat 4 |
| iii | 5 | 5 | 2.5 | 8.9% | m. 8, beat 2 |
| ii | 5 | 5 | 2.5 | 8.9% | m. 10, beat 4 |
| IV | 3 | 3 | 1.5 | 5.4% | m. 8, beat 2.5 |
| vi | 3 | 3 | 1.5 | 5.4% | m. 8, beat 3.5 |
| ii7 | 2 | 2 | 2 | 7.1% | m. 8, beat 1 |
| vii∅7 | 1 | 1 | 1 | 3.6% | m. 13, beat 1 |
| vi7 | 1 | 1 | 1 | 3.6% | m. 14, beat 1 |

### C minor (region 2)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V | 5 | 3 | 5.5 | 28.9% | m. 15, beat 1 |
| viio6/4 | 2 | 1 | 2 | 10.5% | m. 16, beat 1 |
| viio | 2 | 1 | 2 | 10.5% | m. 18, beat 1 |
| i | 2 | 1 | 2 | 10.5% | m. 18, beat 3 |
| V4/2 | 1 | 1 | 2 | 10.5% | m. 16, beat 3 |
| i6 | 1 | 1 | 2 | 10.5% | m. 17, beat 1 |
| III | 1 | 1 | 2 | 10.5% | m. 17, beat 3 |
| V6/4 | 1 | 1 | 1 | 5.3% | m. 15, beat 2 |
| V6 | 1 | 1 | 0.5 | 2.6% | m. 15, beat 1.5 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V | 7 | 5 | 7 | 36.8% | m. 15, beat 1 |
| viio | 4 | 2 | 4 | 21.1% | m. 16, beat 1 |
| i | 3 | 2 | 4 | 21.1% | m. 17, beat 1 |
| V7 | 1 | 1 | 2 | 10.5% | m. 16, beat 3 |
| III | 1 | 1 | 2 | 10.5% | m. 17, beat 3 |

### G major (region 3)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 4 | 3 | 4.5 | 37.5% | m. 20, beat 2 |
| V | 3 | 3 | 1.5 | 12.5% | m. 22, beat 2.5 |
| IV6/4 | 2 | 2 | 2 | 16.7% | m. 21, beat 2 |
| IV | 2 | 2 | 1.5 | 12.5% | m. 21, beat 3 |
| I6/4 | 1 | 1 | 1 | 8.3% | m. 20, beat 3 |
| iii | 1 | 1 | 1 | 8.3% | m. 22, beat 1 |
| vi | 1 | 1 | 0.5 | 4.2% | m. 23, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 5 | 4 | 5.5 | 45.8% | m. 20, beat 2 |
| IV | 4 | 4 | 3.5 | 29.2% | m. 21, beat 2 |
| V | 3 | 3 | 1.5 | 12.5% | m. 22, beat 2.5 |
| iii | 1 | 1 | 1 | 8.3% | m. 22, beat 1 |
| vi | 1 | 1 | 0.5 | 4.2% | m. 23, beat 4 |

### C major (region 4)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 14 | 13 | 11 | 18.6% | m. 24, beat 1 |
| V | 12 | 12 | 9.5 | 16.1% | m. 24, beat 3 |
| ii | 8 | 8 | 4.5 | 7.6% | m. 26, beat 2 |
| iii | 8 | 8 | 4 | 6.8% | m. 24, beat 2 |
| vii° | 6 | 6 | 3.5 | 5.9% | m. 30, beat 4 |
| I6 | 5 | 4 | 4.5 | 7.6% | m. 28, beat 3 |
| I6/4 | 5 | 5 | 4 | 6.8% | m. 29, beat 3 |
| IV | 5 | 4 | 3 | 5.1% | m. 31, beat 2.5 |
| vi | 4 | 4 | 2 | 3.4% | m. 27, beat 2.5 |
| V7 | 4 | 4 | 2 | 3.4% | m. 34, beat 1.5 |
| I7 | 3 | 2 | 2 | 3.4% | m. 26, beat 2.5 |
| V4/2 | 3 | 3 | 1.5 | 2.5% | m. 27, beat 3.5 |
| V6/4 | 2 | 2 | 1 | 1.7% | m. 32, beat 2 |
| IV4/3 | 1 | 1 | 1 | 1.7% | m. 26, beat 1 |
| ii4/2 | 1 | 1 | 1 | 1.7% | m. 31, beat 1 |
| ii6/5 | 1 | 1 | 1 | 1.7% | m. 36, beat 3 |
| vi4/2 | 1 | 1 | 1 | 1.7% | m. 37, beat 1 |
| iii6 | 1 | 1 | 1 | 1.7% | m. 37, beat 3 |
| I4/2 | 1 | 1 | 0.5 | 0.8% | m. 26, beat 4 |
| V6 | 1 | 1 | 0.5 | 0.8% | m. 27, beat 2 |
| IV6/4 | 1 | 1 | 0.5 | 0.8% | m. 35, beat 4.5 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 24 | 22 | 19.5 | 33.1% | m. 24, beat 1 |
| V | 15 | 15 | 11 | 18.6% | m. 24, beat 3 |
| iii | 9 | 9 | 5 | 8.5% | m. 24, beat 2 |
| ii | 8 | 8 | 4.5 | 7.6% | m. 26, beat 2 |
| V7 | 7 | 7 | 3.5 | 5.9% | m. 27, beat 3.5 |
| vii° | 6 | 6 | 3.5 | 5.9% | m. 30, beat 4 |
| IV | 6 | 5 | 3.5 | 5.9% | m. 31, beat 2.5 |
| I7 | 4 | 3 | 2.5 | 4.2% | m. 26, beat 2.5 |
| vi | 4 | 4 | 2 | 3.4% | m. 27, beat 2.5 |
| ii7 | 2 | 2 | 2 | 3.4% | m. 31, beat 1 |
| IV7 | 1 | 1 | 1 | 1.7% | m. 26, beat 1 |
| vi7 | 1 | 1 | 1 | 1.7% | m. 37, beat 1 |

## Non-chord tones

9 notes sound against a chord without being one of its tones; 5 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 4, beat 1 to m. 4, beat 2 | 24 | unclassified | V |
| m. 7, beat 4 to m. 8, beat 1 | 57 | unclassified | ii |
| m. 10, beat 1 to m. 10, beat 2 | 81 | appoggiatura | I |
| m. 19, beat 1 to m. 19, beat 2 | 155 | unclassified | major |
| m. 22, beat 1 to m. 22, beat 2 | 186 | appoggiatura | iii |
| m. 27, beat 1 to m. 27, beat 2 | 226 | unclassified | V |
| m. 33, beat 1 to m. 33, beat 2 | 279 | appoggiatura | I |
| m. 36, beat 1 to m. 36, beat 2 | 309 | appoggiatura | IV |
| m. 37, beat 3 to m. 37, beat 4 | 324 | unclassified | iii |

## Cadences

The library's standard-harmony analysis found 15 cadences from the chords, the meter and the soprano. Position is where the arrival begins. Key is the key region of the arrival. Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added before a section start.

| Position | Type | Key | Soprano | Beat strength | Boundary |
| --- | --- | --- | --- | --- | --- |
| m. 2, beat 1 | perfect-authentic | C major | 1 | strong | no |
| m. 2, beat 3 | half | C major | 5 | medium | no |
| m. 4, beat 1 | half | C major | 2 | strong | no |
| m. 10, beat 1 | imperfect-authentic | G major | 4 | strong | no |
| m. 12, beat 1 | evaded | G major | 1 | strong | no |
| m. 14, beat 3 | half | G major | 5 | medium | no |
| m. 19, beat 1 | half | C minor | 1 | strong | no |
| m. 24, beat 1 | perfect-authentic | C major | 1 | strong | no |
| m. 25, beat 1 | perfect-authentic | C major | 1 | strong | no |
| m. 25, beat 3 | half | C major | 5 | medium | no |
| m. 27, beat 1 | half | C major | 2 | strong | no |
| m. 33, beat 1 | imperfect-authentic | C major | 4 | strong | no |
| m. 35, beat 1 | evaded | C major | 1 | strong | no |
| m. 37, beat 3 | evaded | C major | 4 | medium | no |
| m. 38, beat 2.5 | perfect-authentic | C major | 1 | weak | no |

### Competing readings

A cadence the evidence does not settle has two readings, the likelier first, each with its share of the evidence.

- m12 b1 evaded cadence (50%), or tonic prolongation (50%)
- m24 b1 perfect authentic cadence (75%), or half cadence at m23 b4 (25%)
- m35 b1 evaded cadence (50%), or tonic prolongation (50%)

### Cadence plan

1. m. 2, beat 1: perfect-authentic in C major
2. m. 2, beat 3: half in C major
3. m. 4, beat 1: half in C major
4. m. 10, beat 1: imperfect-authentic in G major
5. m. 12, beat 1: evaded in G major
6. m. 14, beat 3: half in G major
7. m. 19, beat 1: half in C minor
8. m. 24, beat 1: perfect-authentic in C major
9. m. 25, beat 1: perfect-authentic in C major
10. m. 25, beat 3: half in C major
11. m. 27, beat 1: half in C major
12. m. 33, beat 1: imperfect-authentic in C major
13. m. 35, beat 1: evaded in C major
14. m. 37, beat 3: evaded in C major
15. m. 38, beat 2.5: perfect-authentic in C major

**Counts:** perfect-authentic 4, half 6, imperfect-authentic 2, evaded 3.

## Phrases

The library's standard-form analysis found 10 phrases from the soprano, the meter and the cadences. Length counts the measures of the phrase proper; Ends is what closed it: its cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in the piece; Expansion is a lengthening or shortening of the phrase's ending, or an extension after its cadence, with the measures it adds or removes.

| Phrase | Measures | Length | Cadence | Ends | Role | Expansion |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 2 | 2 | perfect-authentic (m. 2, beat 1) | cadence | beginning | - |
| 2 | mm. 3 to 4 | 2 | half (m. 4, beat 1) | cadence | middle | - |
| 3 | mm. 5 to 10 | 6 | imperfect-authentic (m. 10, beat 1) | cadence | middle | contraction -2 (mm. 9 to 10) |
| 4 | mm. 11 to 14 | 4 | half (m. 14, beat 3) | cadence | middle | - |
| 5 | mm. 15 to 19 | 5 | half (m. 19, beat 1) | cadence | middle | - |
| 6 | mm. 20 to 24 | 5 | perfect-authentic (m. 24, beat 1) | cadence | middle | - |
| 7 | m. 25 | 1 | perfect-authentic (m. 25, beat 1) | cadence | middle | - |
| 8 | mm. 26 to 27 | 2 | half (m. 27, beat 1) | cadence | middle | - |
| 9 | mm. 28 to 33 | 6 | imperfect-authentic (m. 33, beat 1) | cadence | middle | contraction -2 (mm. 32 to 33) |
| 10 | mm. 34 to 38 | 5 | perfect-authentic (m. 38, beat 2.5) | cadence | ending | - |

- **Phrase 1:** ideas basic-idea (mm. 1 to 2); phrase model Tb (m. 1, beat 1), D (m. 1, beat 3), Te (m. 2, beat 1)
- **Phrase 2:** ideas basic-idea (mm. 3 to 4); phrase model D (m. 4, beat 1)
- **Phrase 3:** ideas basic-idea (mm. 5 to 6), contrasting-idea (mm. 7 to 8), contrasting-idea (mm. 9 to 10); subphrases mm. 5 to 8, mm. 9 to 10; phrase model Tb (m. 5, beat 1), D (m. 9, beat 3.5), D (m. 9, beat 4.5), Te (m. 10, beat 1)
- **Phrase 4:** ideas basic-idea (mm. 11 to 12), contrasting-idea (mm. 13 to 14); phrase model PD (m. 14, beat 2), D (m. 14, beat 3)
- **Phrase 5:** ideas basic-idea (mm. 15 to 16), contrasting-idea (mm. 17 to 19); phrase model D (m. 19, beat 1), D (m. 19, beat 2), D (m. 19, beat 3)
- **Phrase 6:** ideas basic-idea (mm. 20 to 21), contrasting-idea (mm. 22 to 24); phrase model Tb (m. 20, beat 2), Tb (m. 20, beat 4), D (m. 23, beat 4.5), Te (m. 24, beat 1)
- **Phrase 7:** ideas basic-idea (m. 25)
- **Phrase 8:** ideas basic-idea (mm. 26 to 27); phrase model D (m. 27, beat 1)
- **Phrase 9:** ideas basic-idea (mm. 28 to 29), contrasting-idea (mm. 30 to 31), contrasting-idea (mm. 32 to 33); subphrases mm. 28 to 31, mm. 32 to 33; phrase model Tb (m. 28, beat 1), D (m. 32, beat 3.5), D (m. 32, beat 4.5), Te (m. 33, beat 1)
- **Phrase 10:** ideas basic-idea (mm. 34 to 35), contrasting-idea (mm. 36 to 38); phrase model D (m. 38, beat 2), Te (m. 38, beat 2.5)

## Themes

The library's standard-form analysis found 3 themes from the phrases, their ideas and the cadences. Form is the library's name for the pairing of the two parts; Commonality is how common that pairing is; Traits are what the library finds in the continuation.

| Theme | Form | Commonality | Measures | Length | Parts | Traits |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | hybrid | slightly-less-common | mm. 5 to 10 | 6 | compound-basic-idea (mm. 5 to 8), continuation (mm. 9 to 10, imperfect-authentic (m. 10, beat 1)) | - |
| 2 | period | very-common | mm. 15 to 24 | 10 | antecedent (mm. 15 to 19, half (m. 19, beat 1)), consequent (mm. 20 to 24, perfect-authentic (m. 24, beat 1)) | - |
| 3 | hybrid | slightly-less-common | mm. 28 to 33 | 6 | compound-basic-idea (mm. 28 to 31), continuation (mm. 32 to 33, imperfect-authentic (m. 33, beat 1)) | - |

## Form plan

The kernel's form structure detection grouped the phrases into 6 sections, closing a section at each phrase that ends on a perfect or imperfect authentic cadence; the phrases after the last such cadence form the last section. Sections are lettered by first appearance, and a section whose opening theme returns an earlier section's material takes that letter with a prime. Key is the key the section opens in.

| Section | Measures | Key | Phrases | Closing cadence | Returns |
| --- | --- | --- | --- | --- | --- |
| A | mm. 1 to 2 | C major | 1 | perfect-authentic (m. 2, beat 1) | - |
| B | mm. 3 to 10 | C major | 2 to 3 | imperfect-authentic (m. 10, beat 1) | - |
| C | mm. 11 to 24 | G major | 4 to 6 | perfect-authentic (m. 24, beat 1) | - |
| D | m. 25 | C major | 7 | perfect-authentic (m. 25, beat 1) | - |
| E | mm. 26 to 33 | C major | 8 to 9 | imperfect-authentic (m. 33, beat 1) | - |
| F | mm. 34 to 38 | C major | 10 | perfect-authentic (m. 38, beat 2.5) | - |

### Thematic returns

A later theme recalls an earlier one when the kernel's thematic return detection finds the same contour and rhythm in a stretch of both. Similarity is the lower of the two scores. Themes that share a phrase are not compared.

| Later theme | Measures | Section | Earlier theme | Measures | Section | Similarity |
| --- | --- | --- | --- | --- | --- | --- |
| 2 | mm. 15 to 24 | C | 1 | mm. 5 to 10 | B | 0.83 |
| 3 | mm. 28 to 33 | E | 1 | mm. 5 to 10 | B | 1.00 |
| 3 | mm. 28 to 33 | E | 2 | mm. 15 to 24 | C | 0.80 |

### Outline

- form (mm. 1 to 38)
  - section A (mm. 1 to 2, C major)
    - phrase 1 (mm. 1 to 2)
      - idea basic-idea (mm. 1 to 2)
  - section B (mm. 3 to 10, C major)
    - phrase 2 (mm. 3 to 4)
      - idea basic-idea (mm. 3 to 4)
    - phrase 3 (mm. 5 to 10)
      - subphrase 1 (mm. 5 to 8)
        - idea basic-idea (mm. 5 to 6)
        - idea contrasting-idea (mm. 7 to 8)
      - subphrase 2 (mm. 9 to 10)
        - idea contrasting-idea (mm. 9 to 10)
  - section C (mm. 11 to 24, G major)
    - phrase 4 (mm. 11 to 14)
      - idea basic-idea (mm. 11 to 12)
      - idea contrasting-idea (mm. 13 to 14)
    - phrase 5 (mm. 15 to 19)
      - idea basic-idea (mm. 15 to 16)
      - idea contrasting-idea (mm. 17 to 19)
    - phrase 6 (mm. 20 to 24)
      - idea basic-idea (mm. 20 to 21)
      - idea contrasting-idea (mm. 22 to 24)
  - section D (m. 25, C major)
    - phrase 7 (m. 25)
      - idea basic-idea (m. 25)
  - section E (mm. 26 to 33, C major)
    - phrase 8 (mm. 26 to 27)
      - idea basic-idea (mm. 26 to 27)
    - phrase 9 (mm. 28 to 33)
      - subphrase 1 (mm. 28 to 31)
        - idea basic-idea (mm. 28 to 29)
        - idea contrasting-idea (mm. 30 to 31)
      - subphrase 2 (mm. 32 to 33)
        - idea contrasting-idea (mm. 32 to 33)
  - section F (mm. 34 to 38, C major)
    - phrase 10 (mm. 34 to 38)
      - idea basic-idea (mm. 34 to 35)
      - idea contrasting-idea (mm. 36 to 38)

## Melody

The library's melody adapter read the soprano of each phrase as a line of notes: the soprano notes that start in the phrase. Range is the lowest and highest note and the semitones between them; Climax is the first note at the highest pitch, with how many notes sound it when it is not one; Contour counts the intervals between consecutive notes that go up, go down and repeat, and how many of them are steps and leaps.

| Phrase | Measures | Notes | Range | Climax | Up | Down | Same | Steps | Leaps |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 2 | 5 | G4 to E5 (9) | E5, m. 1, beat 2 | 2 | 2 | 0 | 0 | 4 |
| 2 | mm. 3 to 4 | 7 | B4 to F5 (6) | F5, m. 3, beat 1 | 2 | 3 | 1 | 4 | 1 |
| 3 | mm. 5 to 10 | 25 | G4 to A5 (14) | A5, m. 9, beat 2, 3 notes | 11 | 10 | 3 | 10 | 11 |
| 4 | mm. 11 to 14 | 21 | B4 to D6 (15) | D6, m. 12, beat 2.5 | 6 | 11 | 3 | 9 | 8 |
| 5 | mm. 15 to 19 | 13 | G4 to G5 (12) | G5, m. 17, beat 4 | 5 | 5 | 1 | 3 | 7 |
| 6 | mm. 20 to 24 | 19 | D3 to G5 (29) | G5, m. 20, beat 1, 9 notes | 4 | 6 | 7 | 4 | 6 |
| 7 | m. 25 | 1 | C4 to C4 (0) | C4, m. 25, beat 1 | 0 | 0 | 0 | 0 | 0 |
| 8 | mm. 26 to 27 | 7 | B3 to F4 (6) | F4, m. 26, beat 1 | 2 | 3 | 1 | 4 | 1 |
| 9 | mm. 28 to 33 | 25 | G3 to D5 (19) | D5, m. 32, beat 2, 3 notes | 12 | 8 | 4 | 9 | 11 |
| 10 | mm. 34 to 38 | 25 | C3 to A5 (33) | A5, m. 36, beat 1 | 6 | 14 | 3 | 10 | 10 |

### Leaps

A leap is an interval of a third or larger. The library's rule asks a leap of a fourth or larger to be followed by a step in the opposite direction: such a leap is Recovered when one follows, Unrecovered when a step the same way, a repeated note or another leap does, and Not judged when nothing follows it; a third asks nothing (Not required). Consecutive counts the pairs of leaps in a row the library's rule finds fault with.

| Phrase | Leaps | Recovered | Unrecovered | Not required | Not judged | Consecutive |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 4 | 0 | 1 | 2 | 1 | 2 |
| 2 | 1 | 0 | 0 | 1 | 0 | 0 |
| 3 | 11 | 0 | 4 | 6 | 1 | 6 |
| 4 | 8 | 0 | 2 | 6 | 0 | 1 |
| 5 | 7 | 0 | 2 | 4 | 1 | 3 |
| 6 | 6 | 0 | 0 | 4 | 2 | 1 |
| 7 | 0 | 0 | 0 | 0 | 0 | 0 |
| 8 | 1 | 0 | 0 | 1 | 0 | 0 |
| 9 | 11 | 0 | 6 | 4 | 1 | 5 |
| 10 | 10 | 0 | 3 | 7 | 0 | 1 |

### Non-chord tones

Each melody note is set against the chord of the segment it starts in, as the library's chord identification reads it: a Chord tone is one of the chord's tones, a Non-chord tone is one it left over and named, and Unclassified notes are in a segment with no chord or were left unnamed. By type counts the non-chord tones under the library's names.

| Phrase | Notes | Chord tones | Non-chord tones | Unclassified | By type |
| --- | --- | --- | --- | --- | --- |
| 1 | 5 | 5 | 0 | 0 | - |
| 2 | 7 | 7 | 0 | 0 | - |
| 3 | 25 | 23 | 1 | 1 | appoggiatura 1 |
| 4 | 21 | 21 | 0 | 0 | - |
| 5 | 13 | 12 | 0 | 1 | - |
| 6 | 19 | 13 | 1 | 5 | appoggiatura 1 |
| 7 | 1 | 1 | 0 | 0 | - |
| 8 | 7 | 7 | 0 | 0 | - |
| 9 | 25 | 24 | 1 | 0 | appoggiatura 1 |
| 10 | 25 | 24 | 0 | 1 | - |

### Tendencies

The library's melodic tendencies are soft preferences a well-formed line usually shows: Pitch proximity (steps outnumber leaps, small leaps outnumber large ones), Step declination (descending steps outnumber ascending ones), Step inertia (the line continues in its direction at least as often as it reverses), Melodic regression (extreme notes turn back toward the middle) and Melodic arch (the line rises to a climax in its middle half and ends below it); a phrase either shows a tendency or does not. The seventh degree column counts the notes the library labels the seventh degree that another note follows, judged by its leading tone rule: a leading tone goes up to the tonic by step, and a seventh a whole step below the tonic does not. Followed is the count that did, Broken the count that did not, with where each broken one stands.

| Phrase | Pitch proximity | Step declination | Step inertia | Melodic regression | Melodic arch | Seventh degree followed | Seventh degree broken |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | does not hold | holds | does not hold | holds | does not hold | 0 | 0 |
| 2 | holds | holds | holds | does not hold | does not hold | 1 | 0 |
| 3 | does not hold | does not hold | holds | does not hold | does not hold | 1 | 1 (m. 7, beat 3) |
| 4 | holds | holds | holds | does not hold | holds | 1 | 2 (m. 13, beat 2.5; m. 13, beat 4) |
| 5 | does not hold | does not hold | does not hold | holds | holds | 0 | 1 (m. 16, beat 1) |
| 6 | does not hold | holds | does not hold | does not hold | does not hold | 3 | 0 |
| 7 | holds | holds | holds | holds | does not hold | 0 | 0 |
| 8 | holds | holds | holds | does not hold | does not hold | 1 | 0 |
| 9 | does not hold | does not hold | holds | does not hold | does not hold | 1 | 0 |
| 10 | does not hold | holds | holds | does not hold | holds | 0 | 1 (m. 36, beat 4) |

## Progression patterns

58 recurring progressions of two to six chords, counted over the library's Roman numerals: a held chord counts once, and a progression does not cross an unlabelled chord or a change of key. Ranked by count, then length, then first occurrence; the top 10 are shown. Measures are where each occurrence begins and ends.

| Rank | Progression | Count | Measures |
| --- | --- | --- | --- |
| 1 | iii > ii | 8 | m. 4, mm. 10 to 11, m. 12, m. 14, m. 27, mm. 33 to 34, m. 35, m. 37 |
| 2 | iii > I | 6 | m. 1, m. 2, mm. 5 to 6, m. 24, m. 25, mm. 37 to 38 |
| 3 | I > V | 6 | m. 1, m. 2, m. 24, m. 25, m. 28, m. 38 |
| 4 | V > I | 6 | mm. 1 to 2, mm. 9 to 10, mm. 24 to 25, mm. 28 to 30, mm. 32 to 33, m. 38 |
| 5 | I > iii > I | 5 | m. 1, m. 2, mm. 5 to 6, m. 24, m. 25 |
| 6 | iii > I > V | 5 | m. 1, m. 2, m. 24, m. 25, mm. 37 to 38 |
| 7 | V > vi | 5 | m. 4, m. 7, m. 8, m. 27, m. 31 |
| 8 | IV > iii | 5 | m. 12, m. 14, mm. 21 to 22, m. 35, mm. 35 to 36 |
| 9 | I > iii > I > V | 4 | m. 1, m. 2, m. 24, m. 25 |
| 10 | I > V > I | 4 | mm. 1 to 2, mm. 24 to 25, mm. 28 to 30, m. 38 |

## Harmonic rhythm

How often the harmony changes. A change is a chord whose base Roman numeral or key differs from the chord before it, counted in the measure it starts in. Changes lists the measures of each row in order; Mean is changes per measure over the row.

| Measures | Changes | Mean |
| --- | --- | --- |
| mm. 1 to 4 | 4, 4, 3, 6 | 4.25 |
| mm. 5 to 8 | 3, 1, 4, 7 | 3.75 |
| mm. 9 to 12 | 4, 6, 4, 5 | 4.75 |
| mm. 13 to 16 | 6, 6, 1, 2 | 3.75 |
| mm. 17 to 20 | 2, 2, 1, 2 | 1.75 |
| mm. 21 to 24 | 2, 6, 4, 4 | 4 |
| mm. 25 to 28 | 4, 3, 6, 3 | 4 |
| mm. 29 to 32 | 0, 3, 7, 4 | 3.5 |
| mm. 33 to 36 | 6, 4, 5, 4 | 4.75 |
| mm. 37 to 38 | 6, 3 | 4.5 |

**Mean:** 3.87 changes per measure over 38 measures.

### Approach to cadences

Changes per measure over the two measures before each cadence's arrival (Approach), against the two measures before those (Before). Faster is yes when the approach changes harmony more often.

| Cadence | Arrival | Before | Approach | Faster |
| --- | --- | --- | --- | --- |
| 1 | m. 2 | none | 4 | no |
| 2 | m. 2 | none | 4 | no |
| 3 | m. 4 | 4 | 3.5 | no |
| 4 | m. 10 | 2.5 | 5.5 | yes |
| 5 | m. 12 | 5.5 | 5 | no |
| 6 | m. 14 | 5 | 5.5 | yes |
| 7 | m. 19 | 1.5 | 2 | yes |
| 8 | m. 24 | 2 | 5 | yes |
| 9 | m. 25 | 4 | 4 | no |
| 10 | m. 25 | 4 | 4 | no |
| 11 | m. 27 | 4 | 3.5 | no |
| 12 | m. 33 | 1.5 | 5.5 | yes |
| 13 | m. 35 | 5.5 | 5 | no |
| 14 | m. 37 | 5 | 4.5 | no |
| 15 | m. 38 | 4.5 | 5 | yes |

## Rhythm and meter

**Meter fit:** 4/4, scale 1, no pickup (fit 0.67); a better reading: 4/4, scale 2, no pickup (fit 0.85)

### Density

The library's density profile of each phrase, for all its notes and for each line that has a note in it (the soprano and bass of the outer voices, and Other for the rest). A note is in a phrase when its onset is; a chord is one attack. Attacks per beat is read measure by measure over the beats the phrase spans in it, as a mean and the largest; Mean gap is the mean distance between attacks in whole notes; On the beat is the share of attacks exactly on a beat. Durations lists the notes' lengths in whole notes, shortest first, with how many notes have each. The patterns that recur are the motive families' rhythms (see Motives).

| Phrase | Measures | Line | Notes | Attacks | Per beat | Max per beat | Mean gap | On the beat | Durations |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 2 | all | 8 | 6 | 1.12 | 1.25 | 0.2 | 83.3% | 1/8 (2), 1/4 (6) |
| 1 | mm. 1 to 2 | soprano | 5 | 5 | 1 | 1 | 0.25 | 80.0% | 1/8 (2), 1/4 (3) |
| 1 | mm. 1 to 2 | bass | 2 | 2 | 0.62 | 1 | 1 | 100.0% | 1/4 (2) |
| 1 | mm. 1 to 2 | other | 1 | 1 | 0.12 | 0.25 | 0 | 100.0% | 1/4 (1) |
| 2 | mm. 3 to 4 | all | 13 | 10 | 2 | 2 | 0.12 | 50.0% | 1/8 (10), 1/4 (3) |
| 2 | mm. 3 to 4 | soprano | 7 | 7 | 1.25 | 1.5 | 0.17 | 57.1% | 1/8 (7) |
| 2 | mm. 3 to 4 | bass | 3 | 3 | 0.75 | 1 | 0.5 | 100.0% | 1/4 (3) |
| 2 | mm. 3 to 4 | other | 3 | 3 | 0.75 | 1 | 0.5 | 33.3% | 1/8 (3) |
| 3 | mm. 5 to 10 | all | 50 | 37 | 1.79 | 2 | 0.14 | 56.8% | 1/8 (32), 1/4 (18) |
| 3 | mm. 5 to 10 | soprano | 25 | 25 | 1.17 | 1.75 | 0.21 | 72.0% | 1/8 (17), 1/4 (8) |
| 3 | mm. 5 to 10 | bass | 17 | 17 | 0.83 | 2 | 0.31 | 76.5% | 1/8 (8), 1/4 (9) |
| 3 | mm. 5 to 10 | other | 8 | 8 | 0.46 | 1 | 0.64 | 37.5% | 1/8 (7), 1/4 (1) |
| 4 | mm. 11 to 14 | all | 39 | 30 | 2 | 2 | 0.12 | 50.0% | 1/8 (30), 1/4 (9) |
| 4 | mm. 11 to 14 | soprano | 21 | 21 | 1.4 | 1.75 | 0.17 | 66.7% | 1/8 (17), 1/4 (4) |
| 4 | mm. 11 to 14 | bass | 13 | 13 | 0.85 | 2 | 0.29 | 69.2% | 1/8 (8), 1/4 (5) |
| 4 | mm. 11 to 14 | other | 5 | 5 | 0.35 | 0.67 | 0.66 | 20.0% | 1/8 (5) |
| 5 | mm. 15 to 19 | all | 31 | 25 | 1.25 | 1.5 | 0.2 | 76.0% | 1/8 (12), 1/4 (13), 1/2 (4), 1/1 (2) |
| 5 | mm. 15 to 19 | soprano | 13 | 13 | 0.65 | 1 | 0.4 | 92.3% | 1/8 (4), 1/4 (8), 1/2 (1) |
| 5 | mm. 15 to 19 | bass | 9 | 9 | 0.45 | 1 | 0.5 | 88.9% | 1/8 (3), 1/4 (1), 1/2 (3), 1/1 (2) |
| 5 | mm. 15 to 19 | other | 9 | 9 | 0.45 | 0.75 | 0.52 | 55.6% | 1/8 (5), 1/4 (4) |
| 6 | mm. 20 to 24 | all | 44 | 31 | 1.7 | 2 | 0.13 | 54.8% | 1/8 (28), 1/4 (16) |
| 6 | mm. 20 to 24 | soprano | 19 | 19 | 1.1 | 1.5 | 0.22 | 73.7% | 1/8 (17), 1/4 (2) |
| 6 | mm. 20 to 24 | bass | 13 | 13 | 0.8 | 1 | 0.33 | 100.0% | 1/4 (13) |
| 6 | mm. 20 to 24 | other | 12 | 12 | 0.6 | 1 | 0.31 | 25.0% | 1/8 (11), 1/4 (1) |
| 7 | m. 25 | all | 2 | 1 | 1 | 1 | 0 | 100.0% | 1/4 (2) |
| 7 | m. 25 | soprano | 1 | 1 | 1 | 1 | 0 | 100.0% | 1/4 (1) |
| 7 | m. 25 | bass | 1 | 1 | 1 | 1 | 0 | 100.0% | 1/4 (1) |
| 8 | mm. 26 to 27 | all | 13 | 10 | 2 | 2 | 0.12 | 50.0% | 1/8 (10), 1/4 (3) |
| 8 | mm. 26 to 27 | soprano | 7 | 7 | 1.25 | 1.5 | 0.17 | 57.1% | 1/8 (7) |
| 8 | mm. 26 to 27 | bass | 3 | 3 | 0.75 | 1 | 0.5 | 100.0% | 1/4 (3) |
| 8 | mm. 26 to 27 | other | 3 | 3 | 0.75 | 1 | 0.5 | 33.3% | 1/8 (3) |
| 9 | mm. 28 to 33 | all | 46 | 32 | 1.58 | 2 | 0.17 | 65.6% | 1/8 (22), 1/4 (24) |
| 9 | mm. 28 to 33 | soprano | 25 | 25 | 1.17 | 1.75 | 0.21 | 72.0% | 1/8 (12), 1/4 (13) |
| 9 | mm. 28 to 33 | bass | 15 | 15 | 0.75 | 2 | 0.36 | 73.3% | 1/8 (8), 1/4 (7) |
| 9 | mm. 28 to 33 | other | 6 | 6 | 0.38 | 1 | 0.9 | 100.0% | 1/8 (2), 1/4 (4) |
| 10 | mm. 34 to 38 | all | 47 | 37 | 1.93 | 2 | 0.12 | 51.4% | 1/8 (36), 1/4 (11) |
| 10 | mm. 34 to 38 | soprano | 25 | 25 | 1.28 | 1.75 | 0.18 | 56.0% | 1/8 (20), 1/4 (5) |
| 10 | mm. 34 to 38 | bass | 17 | 17 | 0.92 | 2 | 0.28 | 70.6% | 1/8 (11), 1/4 (6) |
| 10 | mm. 34 to 38 | other | 5 | 5 | 0.25 | 0.5 | 0.62 | 60.0% | 1/8 (5) |

### Syncopation

The library found no syncopation.

### Hemiola

The library found no hemiola.

### Hypermeter

The library reads each phrase in hypermeasures of two or four measures: the length that fills the phrase and holds the cadence's arrival in the last one. The first measure of each hypermeasure is a hyperdownbeat. A phrase that does not fill its hypermeasures ends in a short one, an internal expansion makes the last one longer, an external expansion adds one of its own and a contraction shortens the last. Score is the library's confidence in the reading. The form adapter does not report elided cadences, so no phrase is read as elided.

| Phrase | Measures | Length | Hyperdownbeats | Hypermeasures | Cadence arrives | Score |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 2 | 2 | 1 | 1 regular | m. 2 | 1 |
| 2 | mm. 3 to 4 | 2 | 3 | 1 regular | m. 4 | 1 |
| 3 | mm. 5 to 10 | 4 | 5, 9 | 1 regular, 1 contracted | m. 10 | 1 |
| 4 | mm. 11 to 14 | 4 | 11 | 1 regular | m. 14 | 1 |
| 5 | mm. 15 to 19 | 4 | 15, 19 | 1 regular, 1 short | m. 19 | 0.9 |
| 6 | mm. 20 to 24 | 4 | 20, 24 | 1 regular, 1 short | m. 24 | 0.9 |
| 7 | m. 25 | 4 | 25 | 1 short | m. 25 | 0.5 |
| 8 | mm. 26 to 27 | 2 | 26 | 1 regular | m. 27 | 1 |
| 9 | mm. 28 to 33 | 4 | 28, 32 | 1 regular, 1 contracted | m. 33 | 1 |
| 10 | mm. 34 to 38 | 4 | 34, 38 | 1 regular, 1 short | m. 38 | 0.9 |

### Tuplets and borrowed divisions

No tuplets were found.

## Texture

The library's texture adapter read every measure of the piece from the voices the kernel separated in each track or part, classified it, and merged consecutive measures of the same texture into passages. Category is the main texture (homorhythm and melody and accompaniment are homophony). Voices is the most sounding at once, Synchrony the share of attack times where the voices attack together, Pattern the score of a repeated cell (0 to 1: evidence for an accompaniment figure, not a name for it). Confidence is the texture's margin from its thresholds times the voice separation's.

| Measures | Texture | Category | Voices | Synchrony | Pattern | Confidence |
| --- | --- | --- | --- | --- | --- | --- |
| mm. 1 to 3 | melody and accompaniment | homophony | 2 | 0.22 | 0.19 | 0.72 |
| m. 4 | polyphony | polyphony | 2 | 0.25 | 0.62 | 0.6 |
| mm. 5 to 26 | melody and accompaniment | homophony | 3 | 0.28 | 0.34 | 0.72 |
| m. 27 | polyphony | polyphony | 2 | 0.25 | 0.62 | 0.66 |
| mm. 28 to 29 | melody and accompaniment | homophony | 2 | 0.2 | 0 | 0.78 |
| m. 30 | homorhythm | homophony | 3 | 1 | 0.67 | 1 |
| mm. 31 to 38 | melody and accompaniment | homophony | 2 | 0.26 | 0.53 | 0.73 |

### Changes

| Measure | From | To | Category changes |
| --- | --- | --- | --- |
| 4 | melody and accompaniment | polyphony | yes |
| 5 | polyphony | melody and accompaniment | yes |
| 27 | melody and accompaniment | polyphony | yes |
| 28 | polyphony | melody and accompaniment | yes |
| 30 | melody and accompaniment | homorhythm | no |
| 31 | homorhythm | melody and accompaniment | no |

## Voice leading

The kernel separated the notes of the whole piece into voices (voice 0 is the top) and the library's counterpoint adapter classified how each pair of voices moves from one chord segment to the next. Parallel motion keeps the interval type, similar motion changes it; a voice that rests in either segment gives no motion. Faults are the library's: parallel fifths and octaves between any pair of voices, direct fifths and octaves in the outer pair, and a seventh or leading tone left unresolved when its chord changes (voice 0 is the top).

3 voices over 205 chord segments, 55 motions between consecutive segments: 8 contrary, 5 similar, 3 parallel, 39 oblique, 0 static.

| Voices | Contrary | Similar | Parallel | Oblique | Static |
| --- | --- | --- | --- | --- | --- |
| 0 and 1 | 6 | 4 | 3 | 26 | 0 |
| 0 and 2 | 2 | 1 | 0 | 9 | 0 |
| 1 and 2 | 0 | 0 | 0 | 4 | 0 |

0 parallel fifths, 0 parallel octaves, 0 direct fifths or octaves in the outer voices, 9 unresolved sevenths, 0 unresolved leading tones.

| Location | Fault | Voices |
| --- | --- | --- |
| m. 3, beat 3 to m. 3, beat 4.5 | seventh-resolves-down | 0 |
| m. 8, beat 1 to m. 8, beat 2.5 | seventh-resolves-down | 0 |
| m. 26, beat 3 to m. 26, beat 4.5 | seventh-resolves-down | 1 |
| m. 34, beat 2 to m. 34, beat 3 | seventh-resolves-down | 0 |
| m. 34, beat 2 to m. 34, beat 3 | seventh-resolves-down | 2 |
| m. 34, beat 2.5 to m. 34, beat 3.5 | seventh-resolves-down | 0 |
| m. 34, beat 3.5 to m. 34, beat 4.5 | seventh-resolves-down | 0 |
| m. 34, beat 4 to m. 35, beat 1 | seventh-resolves-down | 0 |
| m. 34, beat 4 to m. 35, beat 1 | seventh-resolves-down | 2 |

3 chords with three or more voices sounding: the root is doubled in 1, the third in 0, the fifth in 1, the seventh in 0, no member doubled in 1. Doubling is counted, not judged. Spacing is judged only when four voices sound together; no chord has them.

## Schemas and sequences

The library's galant-schemas adapter matched its schema prototypes against the outer voices: the soprano and bass degree of each chord in its local key, with the chord's Roman numeral and the strength of its beat. Each match lists its stages by the measure they start in and the soprano and bass degrees found there (b is lowered, # raised; a stage with no soprano reads "-" and matches on the bass alone). Frame is the key the stages are read in, as a degree of the home key (1 is the home key). Confidence is the library's: the share of stages whose voices match, times the metric fit, times one minus the mean uncertainty of the chords.

No galant schema was found.

### Sequences

The library's scale-harmonization adapter found a model of one or more chords repeated, transposed by a constant diatonic interval, at least three times. Kind is the library's name for it when its rules for that pattern hold over whole units (rule-backed); the cycle of fifths has no rule in the library yet, so it is found from the Roman numerals alone. Units is how often the model sounds and, in brackets, how many chords it has; Steps the bass's diatonic steps from one unit to the next (up is positive). Stages read as in the schemas, in the home key.

| Measures | Kind | Rule-backed | Direction | Units (chords each) | Steps | Stages (soprano/bass) | Confidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| m. 8 | parallel-chords | yes | ascending | 4 (1) | +1 | m. 8 2/2, m. 8 3/3, m. 8 #4/#4, m. 8 5/5 | 0.21 |
| m. 10 | parallel-chords | yes | ascending | 4 (1) | +1 | m. 10 #4/#4, m. 10 5/5, m. 10 6/6, m. 10 7/7 | 0.21 |
| m. 12 | parallel-chords | yes | descending | 4 (1) | -1 | m. 12 1/1, m. 12 7/7, m. 12 6/6, m. 12 5/5 | 0.21 |
| mm. 23 to 24 | parallel-chords | yes | descending | 4 (1) | -1 | m. 23 4/4, m. 23 3/3, m. 23 2/2, m. 24 1/1 | 0.35 |
| m. 31 | parallel-chords | yes | ascending | 4 (1) | +1 | m. 31 5/5, m. 31 6/6, m. 31 7/7, m. 31 1/1 | 0.41 |
| m. 33 | parallel-chords | yes | ascending | 4 (1) | +1 | m. 33 7/7, m. 33 1/1, m. 33 2/2, m. 33 3/3 | 0.41 |
| m. 35 | parallel-chords | yes | descending | 4 (1) | -1 | m. 35 4/4, m. 35 3/3, m. 35 2/2, m. 35 1/1 | 0.36 |

## Markings

A MIDI file carries no tempo or dynamics markings.
