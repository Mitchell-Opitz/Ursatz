# Analysis Report: field-1.musicxml

## Summary

- **File:** field-1.musicxml, 1191 notes.
- **Input:** MusicXML score, 1 part merged into one texture. Meter: notated. Key signature: inferred. Pitch spelling: notated.
- **Key:** Eb major, the key with the most time across 4 key regions.
- **Chords:** 268 chord events; 261 matched a chord (97%), 7 did not.
- **Most frequent chords:** I (95), dominant7 (42), major7 (26).

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
| m. 28, beat 1 to m. 34, beat 1 | F minor | 0.69 | profile |
| m. 34, beat 1 to m. 68, beat 1 | Eb major | 0.75 | profile |

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
| m. 34, beat 1 | F minor | Eb major | modulation | pivot | no |

## Pivot chords

The chord at a modulation that is diatonic in both the old and the new key, named by root and quality, with no Roman numeral. Modulation is its row in the Modulations table, counting from 1.

| Position | Chord | Bass | Inversion | From | To | Modulation |
| --- | --- | --- | --- | --- | --- | --- |
| m. 27, beat 1 to m. 27, beat 2 | C minor7 | A# | third | Bb major | F minor | 1 |
| m. 34, beat 1 to m. 34, beat 2 | A# major | A# | root position | F minor | Eb major | 2 |

## Chords

Positions are measure and beat. Numeral is the Roman numeral in the local key region, which is named where it changes. Function is the library's harmonic function of the chord: an applied chord is a dominant toward its target degree, and a predominant is strong (IV, ii) or weak (iii, vi). Six-four is the library's type for a chord in second inversion. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Numeral | Function | Six-four | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 4 to m. 2, beat 1 | I | I6/4 (in Eb major) | tonic | - | Eb | major | A# | second | 1 | 0.93 | - |
| m. 2, beat 1 to m. 2, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.93 | - |
| m. 2, beat 2 to m. 2, beat 3 | I | I6/4 | tonic | arpeggiating | D# | major | A# | second | 1 | 0.54 | - |
| m. 2, beat 3 to m. 2, beat 4 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.54 | - |
| m. 2, beat 4 to m. 3, beat 1 | minor7 | iii6/5 | predominant (weak) | - | G | minor7 | A# | first | 1 | 0.20 | - |
| m. 3, beat 1 to m. 3, beat 2 | V | V | dominant | - | A# | major | A# | root position | 1 | 0.12 | - |
| m. 3, beat 2 to m. 3, beat 3 | half-diminished7 | vii∅4/3 | dominant | - | D | half-diminished7 | G# | second | 1 | 0.08 | IV (G# major, score 0.74) |
| m. 3, beat 3 to m. 3, beat 4 | V | V6/4 | dominant | - | A# | major | F | second | 1 | 0.07 | half-diminished7 (D half-diminished7, score 0.98) |
| m. 3, beat 4 to m. 3, beat 4.33 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 0.33 | 0.55 | - |
| m. 3, beat 4.33 to m. 4, beat 1 | V | V | dominant | - | A# | major | A# | root position | 0.67 | 0.97 | - |
| m. 4, beat 1 to m. 4, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.76 | - |
| m. 4, beat 2 to m. 4, beat 3 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.76 | - |
| m. 4, beat 3 to m. 4, beat 4 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.08 | iii (G minor, score 0.97) |
| m. 4, beat 4 to m. 5, beat 1 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.08 | iii (G minor, score 0.97) |
| m. 5, beat 1 to m. 5, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.46 | - |
| m. 5, beat 2 to m. 5, beat 3 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.09 | iii (G minor, score 0.96) |
| m. 5, beat 3 to m. 5, beat 4 | I | I6/4 | tonic | arpeggiating | D# | major | A# | second | 1 | 0.08 | iii (G minor, score 0.97) |
| m. 5, beat 4 to m. 6, beat 1 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.14 | - |
| m. 6, beat 1 to m. 6, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 1.00 | - |
| m. 6, beat 2 to m. 6, beat 3 | I | I6/4 | tonic | arpeggiating | D# | major | A# | second | 1 | 0.48 | - |
| m. 6, beat 3 to m. 6, beat 4 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.48 | - |
| m. 6, beat 4 to m. 7, beat 1 | minor7 | iii6/5 | predominant (weak) | - | G | minor7 | A# | first | 1 | 0.25 | - |
| m. 7, beat 1 to m. 7, beat 2 | V | V | dominant | - | A# | major | A# | root position | 1 | 0.60 | - |
| m. 7, beat 2 to m. 7, beat 3 | half-diminished7 | vii∅4/3 | dominant | - | D | half-diminished7 | G# | second | 1 | 0.08 | IV (G# major, score 0.73) |
| m. 7, beat 3 to m. 7, beat 4 | V | V6/4 | dominant | - | A# | major | F | second | 1 | 0.46 | - |
| m. 7, beat 4 to m. 8, beat 1 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 1 | 0.27 | - |
| m. 8, beat 1 to m. 8, beat 2 | major7 | I7 | tonic | - | D# | major7 | D# | root position | 1 | 0.91 | - |
| m. 8, beat 2 to m. 8, beat 3 | I | I6/4 | tonic | arpeggiating | D# | major | A# | second | 1 | 0.54 | - |
| m. 8, beat 3 to m. 8, beat 4 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.54 | - |
| m. 8, beat 4 to m. 9, beat 1 | I | I6/4 | tonic | arpeggiating | D# | major | A# | second | 1 | 0.60 | - |
| m. 9, beat 1 to m. 9, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.97 | - |
| m. 9, beat 2 to m. 9, beat 3 | I | I6/4 | tonic | arpeggiating | D# | major | A# | second | 1 | 0.21 | - |
| m. 9, beat 3 to m. 9, beat 4 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.21 | - |
| m. 9, beat 4 to m. 10, beat 1 | major7 | I4/3 | tonic | - | D# | major7 | A# | second | 1 | 0.51 | - |
| m. 10, beat 1 to m. 10, beat 2 | vii° | vii°6/4 | dominant | - | D | diminished | G# | second | 1 | 0.26 | - |
| m. 10, beat 2 to m. 10, beat 3 | IV | IV6 | predominant (strong) | - | G# | major | C | first | 1 | 0.19 | - |
| m. 10, beat 3 to m. 10, beat 4 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 1 | 0.61 | - |
| m. 10, beat 4 to m. 11, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 11, beat 1 to m. 11, beat 2 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.42 | - |
| m. 11, beat 2 to m. 11, beat 3 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.68 | - |
| m. 11, beat 3 to m. 11, beat 4 | vii° | vii°6 | dominant | - | D | diminished | F | first | 1 | 0.11 | - |
| m. 11, beat 4 to m. 12, beat 1 | iii | iii6 | predominant (weak) | - | G | minor | A# | first | 1 | 0.38 | - |
| m. 12, beat 1 to m. 12, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.09 | minor7 (F minor7, score 0.65) |
| m. 12, beat 2 to m. 12, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.44 | - |
| m. 12, beat 3 to m. 12, beat 4 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.44 | - |
| m. 12, beat 4 to m. 13, beat 1 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.60 | - |
| m. 13, beat 1 to m. 13, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.62 | - |
| m. 13, beat 2 to m. 13, beat 3 | minor7 | iii6/5 | predominant (weak) | - | G | minor7 | A# | first | 1 | 0.21 | - |
| m. 13, beat 3 to m. 13, beat 4 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.31 | - |
| m. 13, beat 4 to m. 14, beat 1 | major7 | I4/3 | tonic | - | D# | major7 | A# | second | 1 | 0.49 | - |
| m. 14, beat 1 to m. 14, beat 2 | half-diminished7 | vii∅4/3 | dominant | - | D | half-diminished7 | G# | second | 1 | 0.08 | IV (G# major, score 0.72) |
| m. 14, beat 2 to m. 14, beat 3 | IV | IV6 | predominant (strong) | - | G# | major | C | first | 1 | 0.19 | - |
| m. 14, beat 3 to m. 14, beat 4 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 1 | 0.69 | - |
| m. 14, beat 4 to m. 15, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 15, beat 1 to m. 15, beat 2 | V | V | dominant | - | A# | major | A# | root position | 1 | 0.28 | - |
| m. 15, beat 2 to m. 15, beat 3 | half-diminished7 | vii∅4/3 | dominant | - | D | half-diminished7 | G# | second | 1 | 0.06 | dominant7 (A# dominant7, score 0.72) |
| m. 15, beat 3 to m. 15, beat 4 | half-diminished7 | vii∅6/5 | dominant | - | D | half-diminished7 | F | first | 1 | 0.27 | - |
| m. 15, beat 4 to m. 16, beat 1 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 1 | 0.26 | - |
| m. 16, beat 1 to m. 16, beat 2 | major7 | I7 | tonic | - | D# | major7 | D# | root position | 1 | 1.00 | - |
| m. 16, beat 2 to m. 16, beat 3 | I | I6/4 | tonic | arpeggiating | D# | major | A# | second | 1 | 0.69 | - |
| m. 16, beat 3 to m. 16, beat 4 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.99 | - |
| m. 16, beat 4 to m. 17, beat 1 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.38 | - |
| m. 17, beat 1 to m. 17, beat 2 | half-diminished7 | vii∅6/5 | dominant | - | D | half-diminished7 | F | first | 1 | 0.27 | - |
| m. 17, beat 2 to m. 17, beat 3 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.50 | - |
| m. 17, beat 3 to m. 17, beat 4 | minor7 | iii4/2 | predominant (weak) | - | G | minor7 | F | third | 1 | 0.01 | dominant7 (A# dominant7, score 0.94) |
| m. 17, beat 4 to m. 18, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.22 | - |
| m. 18, beat 1 to m. 18, beat 2 | major7 | I7 | tonic | - | D# | major7 | D# | root position | 1 | 0.20 | - |
| m. 18, beat 2 to m. 18, beat 3 | major7 | I4/3 | tonic | - | D# | major7 | A# | second | 1 | 0.43 | - |
| m. 18, beat 3 to m. 18, beat 4 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.67 | - |
| m. 18, beat 4 to m. 19, beat 1 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.15 | - |
| m. 19, beat 1 to m. 19, beat 2 | vii° | vii°6 | dominant | - | D | diminished | F | first | 1 | 0.35 | - |
| m. 19, beat 2 to m. 19, beat 3 | half-diminished7 | vii∅4/3 | dominant | - | D | half-diminished7 | A# | - | 1 | 0.05 | IV (G# major, score 0.64) |
| m. 19, beat 3 to m. 19, beat 4 | half-diminished7 | vii∅6/5 | dominant | - | D | half-diminished7 | F | first | 1 | 0.07 | minor7 (G minor7, score 0.78) |
| m. 19, beat 4 to m. 19, beat 4.33 | iii | iii6 | predominant (weak) | - | G | minor | A# | first | 0.33 | 0.23 | - |
| m. 19, beat 4.33 to m. 20, beat 1 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 0.67 | 0.05 | vii° (D diminished, score 1.00) |
| m. 20, beat 1 to m. 20, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.25 | - |
| m. 20, beat 2 to m. 20, beat 3 | I | I6/4 | tonic | arpeggiating | D# | major | A# | second | 1 | 0.69 | - |
| m. 20, beat 3 to m. 20, beat 4 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.45 | - |
| m. 20, beat 4 to m. 20, beat 4.37 | minor7 | ii4/2 | predominant (strong) | - | F | minor7 | D# | third | 0.37 | 0.27 | - |
| m. 20, beat 4.37 to m. 21, beat 1 | major7 | I4/2 | tonic | - | D# | major7 | D | third | 0.63 | 0.45 | - |
| m. 21, beat 1 to m. 21, beat 2 | I | I (in Bb major) | tonic | - | A# | major | A# | root position | 1 | 0.44 | - |
| m. 21, beat 2 to m. 21, beat 3 | I | I6/4 | tonic | arpeggiating | A# | major | F | second | 1 | 0.09 | iii (D minor, score 0.95) |
| m. 21, beat 3 to m. 21, beat 4 | I | I6 | tonic | - | A# | major | D | first | 1 | 0.09 | iii (D minor, score 0.95) |
| m. 21, beat 4 to m. 22, beat 1 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.15 | - |
| m. 22, beat 1 to m. 22, beat 2 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 1 | 0.45 | - |
| m. 22, beat 2 to m. 22, beat 3 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 1 | 0.56 | - |
| m. 22, beat 3 to m. 22, beat 4 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 1 | 0.39 | - |
| m. 22, beat 4 to m. 23, beat 1 | minor7 | iii6/5 | predominant (weak) | - | D | minor7 | F | first | 1 | 0.43 | - |
| m. 23, beat 1 to m. 23, beat 2 | minor7 | ii4/2 | predominant (strong) | - | C | minor7 | A# | third | 1 | 0.10 | I (A# major, score 0.62) |
| m. 23, beat 2 to m. 23, beat 3 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.53 | - |
| m. 23, beat 3 to m. 23, beat 4 | I | I6 | tonic | - | A# | major | D | first | 1 | 0.32 | - |
| m. 23, beat 4 to m. 24, beat 1 | major7 | I4/3 | tonic | - | A# | major7 | F | second | 1 | 0.00 | minor7 (G minor7, score 0.88) |
| m. 24, beat 1 to m. 24, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 24, beat 2 to m. 24, beat 3 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.09 | iii (D minor, score 0.95) |
| m. 24, beat 3 to m. 24, beat 4 | I | I6 | tonic | - | A# | major | D | first | 1 | 0.10 | - |
| m. 24, beat 4 to m. 25, beat 1 | minor7 | vi4/2 | predominant (weak) | - | G | minor7 | F | third | 1 | 0.01 | I (A# major, score 0.97) |
| m. 25, beat 1 to m. 25, beat 2 | minor7 | vi6/5 | predominant (weak) | - | G | minor7 | A# | first | 1 | 0.00 | major7 (D# major7, score 1.04) |
| m. 25, beat 2 to m. 25, beat 3 | minor7 | vi4/2 | predominant (weak) | - | G | minor7 | F | third | 1 | 0.00 | I (A# major, score 1.05) |
| m. 25, beat 3 to m. 25, beat 4 | I | I6 | tonic | - | A# | major | D | first | 1 | 0.10 | - |
| m. 25, beat 4 to m. 26, beat 1 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.17 | - |
| m. 26, beat 1 to m. 26, beat 3 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 2 | 0.84 | - |
| m. 26, beat 3 to m. 26, beat 4 | dominant7 | V4/3 | dominant | - | F | dominant7 | C | second | 1 | 0.15 | - |
| m. 26, beat 4 to m. 27, beat 1 | dominant7 | V7 | dominant | - | F | dominant7 | F | root position | 1 | 0.61 | - |
| m. 27, beat 1 to m. 27, beat 2 | minor7 | ii4/2 | predominant (strong) | - | C | minor7 | A# | third | 1 | 0.13 | I (A# major, score 0.61) |
| m. 27, beat 2 to m. 27, beat 3 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.45 | - |
| m. 27, beat 3 to m. 27, beat 4 | I | I6 | tonic | - | A# | major | D | first | 1 | 0.45 | - |
| m. 27, beat 4 to m. 28, beat 1 | I | I6/4 | tonic | - | A# | major | F | second | 1 | 0.30 | - |
| m. 28, beat 1 to m. 28, beat 2 | major | IV (in F minor) | predominant (strong) | - | A# | major | A# | root position | 1 | 0.67 | - |
| m. 28, beat 2 to m. 28, beat 3 | minor | iv | predominant (strong) | - | A# | minor | A# | root position | 1 | 0.35 | - |
| m. 28, beat 3 to m. 28, beat 3.33 | minor | i6 | tonic | - | F | minor | G# | first | 0.33 | 0.87 | - |
| m. 28, beat 3.33 to m. 28, beat 4 | minor | i | tonic | - | F | minor | F | root position | 0.67 | 0.87 | - |
| m. 28, beat 4 to m. 29, beat 1 | diminished | viio6 | dominant | - | E | diminished | G | first | 1 | 0.20 | - |
| m. 29, beat 1 to m. 29, beat 2 | minor | i | tonic | - | F | minor | F | root position | 1 | 0.58 | - |
| m. 29, beat 2 to m. 29, beat 3 | minor | i6 | tonic | - | F | minor | G# | first | 1 | 0.23 | - |
| m. 29, beat 3 to m. 29, beat 4 | minor | i | tonic | - | F | minor | F | root position | 1 | 0.68 | - |
| m. 29, beat 4 to m. 30, beat 1 | minor | i6 | tonic | - | F | minor | G# | first | 1 | 0.13 | - |
| m. 30, beat 1 to m. 30, beat 2 | major | V | dominant | - | C | major | C | root position | 1 | 0.46 | - |
| m. 30, beat 2 to m. 30, beat 3 | half-diminished7 | ii/o6/5 | predominant (strong) | - | G | half-diminished7 | A# | first | 1 | 0.19 | - |
| m. 30, beat 3 to m. 30, beat 4 | major | V | dominant | - | C | major | C | root position | 1 | 0.35 | - |
| m. 30, beat 4 to m. 31, beat 1 | augmented | III+6 | predominant (weak) | - | G# | augmented | A# | - | 1 | 0.04 | dominant7 (C dominant7, score 0.65) |
| m. 31, beat 1 to m. 31, beat 2 | half-diminished7 | ii/o4/2 | predominant (strong) | - | G | half-diminished7 | F | third | 1 | 0.21 | - |
| m. 31, beat 2 to m. 31, beat 3 | minor | i6 | tonic | - | F | minor | G# | first | 1 | 0.50 | - |
| m. 31, beat 3 to m. 31, beat 4 | minor | i | tonic | - | F | minor | F | root position | 1 | 1.00 | - |
| m. 31, beat 4 to m. 32, beat 1 | minor | i6 | tonic | - | F | minor | G# | first | 1 | 0.22 | - |
| m. 32, beat 1 to m. 32, beat 2 | major | IV | predominant (strong) | - | A# | major | A# | root position | 1 | 0.81 | - |
| m. 32, beat 2 to m. 32, beat 3 | dominant7 | IV4/2 | predominant (strong) | - | A# | dominant7 | G# | third | 1 | 0.37 | - |
| m. 32, beat 3 to m. 32, beat 4 | minor | ii6 | predominant (strong) | - | G | minor | A# | first | 1 | 0.41 | - |
| m. 32, beat 4 to m. 33, beat 1 | minor7 | ii6/5 | predominant (strong) | - | G | minor7 | G# | - | 1 | 0.09 | dominant7 (A# dominant7, score 0.67) |
| m. 33, beat 1 to m. 33, beat 2 | major | VII | predominant (weak) | - | D# | major | D# | root position | 1 | 0.33 | - |
| m. 33, beat 2 to m. 33, beat 3 | major | VII6/4 | predominant (weak) | - | D# | major | A# | second | 1 | 0.54 | - |
| m. 33, beat 3 to m. 33, beat 4 | minor7 | i6/5 | tonic | - | F | minor7 | G# | first | 1 | 0.31 | - |
| m. 33, beat 4 to m. 34, beat 1 | minor | v | dominant | - | C | minor | C | root position | 1 | 0.02 | minor7 (F minor7, score 0.90) |
| m. 34, beat 1 to m. 34, beat 2 | V | V (in Eb major) | dominant | - | A# | major | A# | root position | 1 | 0.59 | - |
| m. 34, beat 2 to m. 34, beat 3 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 1 | 0.07 | IV (G# major, score 0.60) |
| m. 34, beat 3 to m. 34, beat 4 | V | V | dominant | - | A# | major | A# | root position | 1 | 0.46 | - |
| m. 34, beat 4 to m. 35, beat 1 | minor7 | iii6/5 | predominant (weak) | - | G | minor7 | G# | - | 1 | 0.29 | - |
| m. 35, beat 1 to m. 35, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 1.00 | - |
| m. 35, beat 2 to m. 35, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.69 | - |
| m. 35, beat 3 to m. 35, beat 4 | I | I | tonic | - | D# | major | D# | root position | 1 | 1.00 | - |
| m. 35, beat 4 to m. 36, beat 1 | major7 | I4/3 | tonic | - | D# | major7 | A# | second | 1 | 0.04 | minor7 (C minor7, score 0.86) |
| m. 36, beat 1 to m. 36, beat 2 | dominant7 | V4/3 | dominant | - | A# | dominant7 | F | second | 1 | 0.15 | - |
| m. 36, beat 2 to m. 36, beat 3 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.31 | - |
| m. 36, beat 3 to m. 36, beat 4 | vii° | vii°6 | dominant | - | D | diminished | F | first | 1 | 0.10 | - |
| m. 36, beat 4 to m. 37, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.25 | - |
| m. 37, beat 1 to m. 37, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 1.00 | - |
| m. 37, beat 2 to m. 37, beat 3 | I | I6/4 | tonic | arpeggiating | D# | major | A# | second | 1 | 0.69 | - |
| m. 37, beat 3 to m. 37, beat 4 | I | I | tonic | - | D# | major | D# | root position | 1 | 1.00 | - |
| m. 37, beat 4 to m. 38, beat 1 | major7 | I4/3 | tonic | - | D# | major7 | A# | second | 1 | 0.03 | minor7 (C minor7, score 0.90) |
| m. 38, beat 1 to m. 38, beat 2 | dominant7 | V4/3 | dominant | - | A# | dominant7 | F | second | 1 | 0.07 | minor7 (G minor7, score 0.90) |
| m. 38, beat 2 to m. 38, beat 3 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.19 | - |
| m. 38, beat 3 to m. 38, beat 4 | half-diminished7 | vii∅6/5 | dominant | - | D | half-diminished7 | F | first | 1 | 0.06 | dominant7 (A# dominant7, score 0.78) |
| m. 38, beat 4 to m. 39, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.20 | - |
| m. 39, beat 1 to m. 39, beat 2 | major7 | I7 | tonic | - | D# | major7 | D# | root position | 1 | 0.21 | - |
| m. 39, beat 2 to m. 39, beat 3 | major7 | I4/3 | tonic | - | D# | major7 | A# | second | 1 | 0.55 | - |
| m. 39, beat 3 to m. 39, beat 4 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.57 | - |
| m. 39, beat 4 to m. 40, beat 1 | major7 | I4/3 | tonic | - | D# | major7 | A# | second | 1 | 0.37 | - |
| m. 40, beat 1 to m. 40, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 40, beat 2 to m. 40, beat 3 | minor7 | ii4/3 | predominant (strong) | - | F | minor7 | A | - | 1 | 0.71 | - |
| m. 40, beat 3 to m. 40, beat 4 | vii° | vii°6/4 | dominant | - | D | diminished | G# | second | 1 | 0.10 | ii (F minor, score 0.60) |
| m. 40, beat 4 to m. 41, beat 1 | V | V | dominant | - | A# | major | A# | root position | 1 | 0.36 | - |
| m. 41, beat 1 to m. 41, beat 2 | minor7 | vi4/3 | predominant (weak) | - | C | minor7 | G | second | 1 | 0.16 | - |
| m. 41, beat 2 to m. 41, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 41, beat 3 to m. 41, beat 4 | dominant7 | V4/3 | dominant | - | A# | dominant7 | F | second | 1 | 0.05 | minor7 (F minor7, score 0.72) |
| m. 41, beat 4 to m. 41, beat 4.33 | major7 | IV6/5 | predominant (strong) | - | G# | major7 | C | first | 0.33 | 0.07 | vi (C minor, score 0.97) |
| m. 41, beat 4.33 to m. 42, beat 1 | ii | ii6 | predominant (strong) | - | F | minor | G# | first | 0.67 | 0.03 | major7 (G# major7, score 0.86) |
| m. 42, beat 1 to m. 42, beat 2 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.28 | - |
| m. 42, beat 2 to m. 42, beat 3 | major7 | I6/5 | tonic | - | D# | major7 | G | first | 1 | 0.01 | minor7 (C minor7, score 0.93) |
| m. 42, beat 3 to m. 42, beat 4 | iii | iii6 | predominant (weak) | - | G | minor | A# | first | 1 | 0.13 | - |
| m. 42, beat 4 to m. 43, beat 1 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 1 | 0.13 | - |
| m. 43, beat 1 to m. 43, beat 2 | major7 | I7 | tonic | - | D# | major7 | D# | root position | 1 | 0.66 | - |
| m. 43, beat 2 to m. 43, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.20 | - |
| m. 43, beat 3 to m. 43, beat 4 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.90 | - |
| m. 43, beat 4.07 to m. 44, beat 1 | I | I6/4 | tonic | - | Eb | major | A# | second | 0.93 | 0.90 | - |
| m. 44, beat 1 to m. 44, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.73 | - |
| m. 44, beat 2 to m. 44, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.60 | - |
| m. 44, beat 3 to m. 44, beat 4 | vi | vi | predominant (weak) | - | C | minor | C | root position | 1 | 0.15 | - |
| m. 44, beat 4 to m. 45, beat 1 | vii° | vii°6 | dominant | - | D | diminished | F | first | 1 | 0.01 | minor7 (F minor7, score 0.77) |
| m. 45, beat 1 to m. 45, beat 2 | V | V | dominant | - | A# | major | A# | root position | 1 | 0.53 | - |
| m. 45, beat 2 to m. 45, beat 3 | half-diminished7 | vii∅4/3 | dominant | - | D | half-diminished7 | G# | second | 1 | 0.07 | IV (G# major, score 0.74) |
| m. 45, beat 3 to m. 45, beat 4 | V | V6/4 | dominant | - | A# | major | F | second | 1 | 0.07 | half-diminished7 (D half-diminished7, score 0.97) |
| m. 45, beat 4 to m. 45, beat 4.33 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 0.33 | 0.54 | - |
| m. 45, beat 4.33 to m. 46, beat 1 | V | V | dominant | - | A# | major | A# | root position | 0.67 | 0.14 | - |
| m. 46, beat 1 to m. 46, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.81 | - |
| m. 46, beat 2 to m. 46, beat 4 | I | I6/4 | tonic | - | D# | major | A# | second | 2 | 0.32 | - |
| m. 46, beat 4 to m. 47, beat 1 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.11 | - |
| m. 47, beat 1 to m. 47, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.40 | - |
| m. 47, beat 2 to m. 47, beat 3 | I | I6/4 | tonic | arpeggiating | D# | major | A# | second | 1 | 0.09 | iii (G minor, score 0.95) |
| m. 47, beat 3 to m. 47, beat 4 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.08 | iii (G minor, score 0.88) |
| m. 47, beat 4 to m. 48, beat 1 | I | I6/4 | tonic | arpeggiating | D# | major | A# | second | 1 | 0.14 | - |
| m. 48, beat 1 to m. 48, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.93 | - |
| m. 48, beat 2 to m. 48, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.48 | - |
| m. 48, beat 3 to m. 48, beat 4 | major7 | I6/5 | tonic | - | D# | major7 | G | first | 1 | 0.36 | - |
| m. 48, beat 4 to m. 49, beat 1 | minor7 | vi4/2 | predominant (weak) | - | C | minor7 | A# | third | 1 | 0.04 | major7 (D# major7, score 0.84) |
| m. 49, beat 1 to m. 49, beat 2 | major7 | I4/3 | tonic | - | D# | major7 | A# | second | 1 | 0.14 | dominant7 (A# dominant7, score 0.60) |
| m. 49, beat 2 to m. 49, beat 3 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.23 | - |
| m. 49, beat 3 to m. 49, beat 4 | dominant7 | V4/3 | dominant | - | A# | dominant7 | F | second | 1 | 0.03 | half-diminished7 (D half-diminished7, score 0.99) |
| m. 49, beat 4 to m. 50, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.18 | - |
| m. 50, beat 1 to m. 50, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.04 | minor7 (F minor7, score 0.66) |
| m. 50, beat 2 to m. 50, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.46 | - |
| m. 50, beat 3 to m. 50, beat 4 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.46 | - |
| m. 50, beat 4 to m. 51, beat 1 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.22 | - |
| m. 51, beat 1 to m. 51, beat 3 | minor7 | iii7 | predominant (weak) | - | G | minor7 | G | root position | 2 | 0.78 | - |
| m. 51, beat 3 to m. 51, beat 4 | vii° | vii° | dominant | - | D | diminished | D | root position | 1 | 0.65 | - |
| m. 51, beat 4 to m. 52, beat 1 | major7 | I6/5 | tonic | - | D# | major7 | G | first | 1 | 0.48 | - |
| m. 52, beat 1 to m. 52, beat 2 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 1 | 0.06 | half-diminished7 (D half-diminished7, score 0.66) |
| m. 52, beat 2 to m. 52, beat 3 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 1 | 0.29 | - |
| m. 52, beat 3 to m. 52, beat 4 | IV | IV6/4 | predominant (strong) | - | Ab | major | D# | second | 1 | 0.29 | - |
| m. 52, beat 4 to m. 53, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 53, beat 1 to m. 53, beat 2 | V | V | dominant | - | A# | major | A# | root position | 1 | 0.31 | - |
| m. 53, beat 2 to m. 53, beat 3 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 1 | 0.29 | - |
| m. 53, beat 3 to m. 53, beat 4 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.59 | - |
| m. 53, beat 4 to m. 54, beat 1 | iii | iii6 | predominant (weak) | - | G | minor | G# | - | 1 | 0.30 | - |
| m. 54, beat 1 to m. 54, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.10 | minor7 (F minor7, score 0.65) |
| m. 54, beat 2 to m. 54, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.53 | - |
| m. 54, beat 3 to m. 54, beat 4 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.53 | - |
| m. 54, beat 4 to m. 55, beat 1 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.22 | - |
| m. 55, beat 1 to m. 55, beat 3 | minor7 | iii7 | predominant (weak) | - | G | minor7 | G | root position | 2 | 0.69 | - |
| m. 55, beat 3 to m. 55, beat 4 | vii° | vii° | dominant | - | D | diminished | D | root position | 1 | 0.40 | - |
| m. 55, beat 4 to m. 56, beat 1 | major7 | I6/5 | tonic | - | D# | major7 | G | first | 1 | 0.50 | - |
| m. 56, beat 1 to m. 56, beat 2 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 1 | 0.20 | - |
| m. 56, beat 2 to m. 56, beat 3 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 1 | 0.66 | - |
| m. 56, beat 3 to m. 56, beat 4 | major7 | IV4/3 | predominant (strong) | - | G# | major7 | D# | second | 1 | 0.07 | minor7 (C minor7, score 0.92) |
| m. 56, beat 4 to m. 57, beat 1 | minor7 | ii6/5 | predominant (strong) | - | F | minor7 | G# | first | 1 | 0.30 | - |
| m. 57, beat 1 to m. 57, beat 1.33 | V | V | dominant | - | A# | major | A# | root position | 0.33 | 1.00 | - |
| m. 57, beat 1.33 to m. 57, beat 2 | V | V | dominant | - | A# | major | A# | root position | 0.67 | 0.28 | - |
| m. 57, beat 2 to m. 57, beat 3 | IV | IV | predominant (strong) | - | G# | major | G# | root position | 1 | 0.01 | half-diminished7 (D half-diminished7, score 0.76) |
| m. 57, beat 3 to m. 57, beat 4 | minor7 | vi4/2 | predominant (weak) | - | C | minor7 | A# | third | 1 | 0.17 | - |
| m. 57, beat 4 to m. 58, beat 1 | dominant7 | V4/2 | dominant | - | A# | dominant7 | G# | third | 1 | 0.28 | - |
| m. 58, beat 1 to m. 58, beat 2 | major7 | I7 | tonic | - | D# | major7 | D# | root position | 1 | 0.92 | - |
| m. 58, beat 2 to m. 58, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.43 | - |
| m. 58, beat 3 to m. 58, beat 4 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.43 | - |
| m. 58, beat 4 to m. 59, beat 1 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.22 | - |
| m. 59, beat 1 to m. 59, beat 2 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.34 | - |
| m. 59, beat 2 to m. 59, beat 3 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.69 | - |
| m. 59, beat 3 to m. 59, beat 4 | dominant7 | V4/3 | dominant | - | A# | dominant7 | F | second | 1 | 0.21 | - |
| m. 59, beat 4 to m. 60, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.32 | - |
| m. 60, beat 1 to m. 60, beat 2 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.36 | - |
| m. 60, beat 2 to m. 60, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.20 | - |
| m. 60, beat 3 to m. 60, beat 4 | major7 | I6/5 | tonic | - | D# | major7 | G | first | 1 | 0.22 | - |
| m. 60, beat 4 to m. 61, beat 1 | major7 | I4/3 | tonic | - | D# | major7 | A# | second | 1 | 0.15 | - |
| m. 61, beat 1 to m. 61, beat 2 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.43 | - |
| m. 61, beat 2 to m. 61, beat 3 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.66 | - |
| m. 61, beat 3 to m. 61, beat 4 | dominant7 | V4/3 | dominant | - | A# | dominant7 | F | second | 1 | 0.23 | - |
| m. 61, beat 4 to m. 62, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.42 | - |
| m. 62, beat 1 to m. 62, beat 2 | major7 | I7 | tonic | - | D# | major7 | D# | root position | 1 | 0.33 | - |
| m. 62, beat 2 to m. 62, beat 3 | major7 | I4/3 | tonic | - | D# | major7 | A# | second | 1 | 0.28 | - |
| m. 62, beat 3 to m. 62, beat 4 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.08 | iii (G minor, score 0.97) |
| m. 62, beat 4 to m. 63, beat 1 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.18 | - |
| m. 63, beat 1 to m. 63, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 63, beat 2 to m. 63, beat 3 | minor7 | ii6/5 | predominant (strong) | - | F | minor7 | G# | first | 1 | 0.20 | - |
| m. 63, beat 3 to m. 63, beat 4 | vii° | vii°6/4 | dominant | - | D | diminished | G# | second | 1 | 0.15 | ii (F minor, score 0.54) |
| m. 63, beat 4 to m. 64, beat 1 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.37 | - |
| m. 64, beat 1 to m. 64, beat 2 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.18 | - |
| m. 64, beat 2 to m. 64, beat 3 | iii | vii°6/5/ii | dominant of 2 | - | G | minor | G | root position | 1 | 0.78 | - |
| m. 64, beat 3 to m. 64, beat 4 | ii | ii6 | predominant (strong) | - | F | minor | G# | first | 1 | 0.12 | major7 (G# major7, score 0.68) |
| m. 64, beat 4 to m. 65, beat 1 | ii | ii6 | predominant (strong) | - | F | minor | G# | first | 1 | 0.02 | dominant7 (A# dominant7, score 0.74) |
| m. 65, beat 1 to m. 65, beat 1.33 | V | V | dominant | - | A# | major | A# | root position | 0.33 | 1.00 | - |
| m. 65, beat 1.33 to m. 65, beat 2 | I | I6/4 | tonic | - | D# | major | A# | second | 0.67 | 0.14 | - |
| m. 65, beat 2 to m. 65, beat 3 | I | I6 | tonic | - | D# | major | G | first | 1 | 0.14 | - |
| m. 65, beat 3 to m. 65, beat 4 | dominant7 | V7 | dominant | - | A# | dominant7 | A# | root position | 1 | 0.00 | iii (G minor, score 0.87) |
| m. 65, beat 4 to m. 66, beat 1 | dominant7 | V4/3 | dominant | - | A# | dominant7 | F | second | 1 | 0.18 | - |
| m. 66, beat 1 to m. 66, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.97 | - |
| m. 66, beat 2 to m. 66, beat 3 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.10 | - |
| m. 66, beat 3 to m. 66, beat 4 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.56 | - |
| m. 66, beat 4 to m. 67, beat 1 | I | I6/4 | tonic | - | D# | major | A# | second | 1 | 0.08 | iii (G minor, score 0.97) |
| m. 67, beat 1 to m. 67, beat 2 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.74 | - |
| m. 67, beat 2 to m. 67, beat 3 | I | I6/4 | tonic | arpeggiating | D# | major | A# | second | 1 | 0.40 | - |
| m. 67, beat 3 to m. 67, beat 4 | I | I | tonic | - | D# | major | D# | root position | 1 | 0.86 | - |

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
| m. 33, beat 1 to m. 33, beat 2 | F minor | F5 | 1 | D#3 | 7 |
| m. 33, beat 2 to m. 33, beat 3 | F minor | D#5 | 7 | A#3 | 4 |
| m. 33, beat 3 to m. 33, beat 4 | F minor | D#5 | 7 | G#3 | 3 |
| m. 33, beat 4 to m. 34, beat 1 | F minor | D#5 | 7 | C4 | 5 |
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
| I6/4 | 43 | 43 | 43.59 | 17.0% | m. 1, beat 4 |
| I | 28 | 28 | 28 | 10.9% | m. 2, beat 1 |
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
| i | 4 | 4 | 3.67 | 1.4% | m. 28, beat 3.33 |
| ii6 | 4 | 3 | 3.67 | 1.4% | m. 32, beat 3 |
| iii6 | 4 | 4 | 3.33 | 1.3% | m. 11, beat 4 |
| V6/4 | 3 | 3 | 3 | 1.2% | m. 3, beat 3 |
| vii°6/4 | 3 | 3 | 3 | 1.2% | m. 10, beat 1 |
| ii6/5 | 3 | 3 | 3 | 1.2% | m. 32, beat 4 |
| ii4/2 | 3 | 3 | 2.37 | 0.9% | m. 20, beat 4 |
| iii7 | 2 | 2 | 4 | 1.6% | m. 51, beat 1 |
| IV6 | 2 | 2 | 2 | 0.8% | m. 10, beat 2 |
| vii° | 2 | 2 | 2 | 0.8% | m. 51, beat 3 |
| iii4/2 | 1 | 1 | 1 | 0.4% | m. 17, beat 3 |
| vi6/5 | 1 | 1 | 1 | 0.4% | m. 25, beat 1 |
| iv | 1 | 1 | 1 | 0.4% | m. 28, beat 2 |
| viio6 | 1 | 1 | 1 | 0.4% | m. 28, beat 4 |
| ii/o6/5 | 1 | 1 | 1 | 0.4% | m. 30, beat 2 |
| III+6 | 1 | 1 | 1 | 0.4% | m. 30, beat 4 |
| ii/o4/2 | 1 | 1 | 1 | 0.4% | m. 31, beat 1 |
| IV4/2 | 1 | 1 | 1 | 0.4% | m. 32, beat 2 |
| VII | 1 | 1 | 1 | 0.4% | m. 33, beat 1 |
| VII6/4 | 1 | 1 | 1 | 0.4% | m. 33, beat 2 |
| i6/5 | 1 | 1 | 1 | 0.4% | m. 33, beat 3 |
| v | 1 | 1 | 1 | 0.4% | m. 33, beat 4 |
| ii4/3 | 1 | 1 | 1 | 0.4% | m. 40, beat 2 |
| vi4/3 | 1 | 1 | 1 | 0.4% | m. 41, beat 1 |
| vi | 1 | 1 | 1 | 0.4% | m. 44, beat 3 |
| IV6/4 | 1 | 1 | 1 | 0.4% | m. 52, beat 3 |
| IV4/3 | 1 | 1 | 1 | 0.4% | m. 56, beat 3 |
| I4/2 | 1 | 1 | 0.63 | 0.2% | m. 20, beat 4.37 |
| IV6/5 | 1 | 1 | 0.33 | 0.1% | m. 41, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 95 | 95 | 95.59 | 37.2% | m. 1, beat 4 |
| V7 | 41 | 36 | 40.33 | 15.7% | m. 3, beat 4 |
| I7 | 24 | 24 | 23.63 | 9.2% | m. 8, beat 1 |
| V | 18 | 17 | 15.67 | 6.1% | m. 3, beat 1 |
| IV | 12 | 10 | 12 | 4.7% | m. 10, beat 2 |
| vii∅7 | 10 | 10 | 10 | 3.9% | m. 3, beat 2 |
| vii° | 9 | 9 | 9 | 3.5% | m. 10, beat 1 |
| i | 9 | 9 | 8 | 3.1% | m. 28, beat 3 |
| iii7 | 8 | 8 | 10 | 3.9% | m. 2, beat 4 |
| ii7 | 7 | 7 | 6.37 | 2.5% | m. 20, beat 4 |
| vi7 | 6 | 6 | 6 | 2.3% | m. 24, beat 4 |
| ii | 4 | 3 | 3.67 | 1.4% | m. 32, beat 3 |
| iii | 4 | 4 | 3.33 | 1.3% | m. 11, beat 4 |
| IV7 | 3 | 3 | 2.33 | 0.9% | m. 32, beat 2 |
| ii/o7 | 2 | 2 | 2 | 0.8% | m. 30, beat 2 |
| VII | 2 | 2 | 2 | 0.8% | m. 33, beat 1 |
| iv | 1 | 1 | 1 | 0.4% | m. 28, beat 2 |
| viio | 1 | 1 | 1 | 0.4% | m. 28, beat 4 |
| III+ | 1 | 1 | 1 | 0.4% | m. 30, beat 4 |
| i7 | 1 | 1 | 1 | 0.4% | m. 33, beat 3 |
| v | 1 | 1 | 1 | 0.4% | m. 33, beat 4 |
| vi | 1 | 1 | 1 | 0.4% | m. 44, beat 3 |

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
| i6 | 5 | 5 | 4.33 | 18.1% | m. 28, beat 3 |
| i | 4 | 4 | 3.67 | 15.3% | m. 28, beat 3.33 |
| IV | 2 | 2 | 2 | 8.3% | m. 28, beat 1 |
| V | 2 | 2 | 2 | 8.3% | m. 30, beat 1 |
| iv | 1 | 1 | 1 | 4.2% | m. 28, beat 2 |
| viio6 | 1 | 1 | 1 | 4.2% | m. 28, beat 4 |
| ii/o6/5 | 1 | 1 | 1 | 4.2% | m. 30, beat 2 |
| III+6 | 1 | 1 | 1 | 4.2% | m. 30, beat 4 |
| ii/o4/2 | 1 | 1 | 1 | 4.2% | m. 31, beat 1 |
| IV4/2 | 1 | 1 | 1 | 4.2% | m. 32, beat 2 |
| ii6 | 1 | 1 | 1 | 4.2% | m. 32, beat 3 |
| ii6/5 | 1 | 1 | 1 | 4.2% | m. 32, beat 4 |
| VII | 1 | 1 | 1 | 4.2% | m. 33, beat 1 |
| VII6/4 | 1 | 1 | 1 | 4.2% | m. 33, beat 2 |
| i6/5 | 1 | 1 | 1 | 4.2% | m. 33, beat 3 |
| v | 1 | 1 | 1 | 4.2% | m. 33, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 9 | 9 | 8 | 33.3% | m. 28, beat 3 |
| IV | 2 | 2 | 2 | 8.3% | m. 28, beat 1 |
| V | 2 | 2 | 2 | 8.3% | m. 30, beat 1 |
| ii/o7 | 2 | 2 | 2 | 8.3% | m. 30, beat 2 |
| VII | 2 | 2 | 2 | 8.3% | m. 33, beat 1 |
| iv | 1 | 1 | 1 | 4.2% | m. 28, beat 2 |
| viio | 1 | 1 | 1 | 4.2% | m. 28, beat 4 |
| III+ | 1 | 1 | 1 | 4.2% | m. 30, beat 4 |
| IV7 | 1 | 1 | 1 | 4.2% | m. 32, beat 2 |
| ii | 1 | 1 | 1 | 4.2% | m. 32, beat 3 |
| ii7 | 1 | 1 | 1 | 4.2% | m. 32, beat 4 |
| i7 | 1 | 1 | 1 | 4.2% | m. 33, beat 3 |
| v | 1 | 1 | 1 | 4.2% | m. 33, beat 4 |

### Eb major (region 3)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I6/4 | 22 | 22 | 22.59 | 17.3% | m. 35, beat 2 |
| I | 16 | 16 | 16 | 12.2% | m. 35, beat 1 |
| V7 | 15 | 13 | 15 | 11.5% | m. 36, beat 2 |
| I6 | 9 | 9 | 9 | 6.9% | m. 46, beat 4 |
| V | 9 | 8 | 7 | 5.3% | m. 34, beat 1 |
| I4/3 | 7 | 7 | 7 | 5.3% | m. 35, beat 4 |
| V4/3 | 7 | 7 | 7 | 5.3% | m. 36, beat 1 |
| I6/5 | 5 | 5 | 5 | 3.8% | m. 42, beat 2 |
| IV | 5 | 3 | 5 | 3.8% | m. 52, beat 1 |
| V4/2 | 5 | 5 | 4.33 | 3.3% | m. 34, beat 2 |
| I7 | 4 | 4 | 4 | 3.1% | m. 39, beat 1 |
| ii6 | 3 | 2 | 2.67 | 2.0% | m. 41, beat 4.33 |
| iii7 | 2 | 2 | 4 | 3.1% | m. 51, beat 1 |
| vii°6 | 2 | 2 | 2 | 1.5% | m. 36, beat 3 |
| vii°6/4 | 2 | 2 | 2 | 1.5% | m. 40, beat 3 |
| iii6 | 2 | 2 | 2 | 1.5% | m. 42, beat 3 |
| vi4/2 | 2 | 2 | 2 | 1.5% | m. 48, beat 4 |
| vii° | 2 | 2 | 2 | 1.5% | m. 51, beat 3 |
| ii6/5 | 2 | 2 | 2 | 1.5% | m. 56, beat 4 |
| iii6/5 | 1 | 1 | 1 | 0.8% | m. 34, beat 4 |
| vii∅6/5 | 1 | 1 | 1 | 0.8% | m. 38, beat 3 |
| ii4/3 | 1 | 1 | 1 | 0.8% | m. 40, beat 2 |
| vi4/3 | 1 | 1 | 1 | 0.8% | m. 41, beat 1 |
| vi | 1 | 1 | 1 | 0.8% | m. 44, beat 3 |
| vii∅4/3 | 1 | 1 | 1 | 0.8% | m. 45, beat 2 |
| V6/4 | 1 | 1 | 1 | 0.8% | m. 45, beat 3 |
| IV6/4 | 1 | 1 | 1 | 0.8% | m. 52, beat 3 |
| IV4/3 | 1 | 1 | 1 | 0.8% | m. 56, beat 3 |
| IV6/5 | 1 | 1 | 0.33 | 0.3% | m. 41, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 47 | 47 | 47.59 | 36.4% | m. 35, beat 1 |
| V7 | 27 | 25 | 26.33 | 20.1% | m. 34, beat 2 |
| I7 | 16 | 16 | 16 | 12.2% | m. 35, beat 4 |
| V | 10 | 9 | 8 | 6.1% | m. 34, beat 1 |
| vii° | 6 | 6 | 6 | 4.6% | m. 36, beat 3 |
| IV | 6 | 4 | 6 | 4.6% | m. 52, beat 1 |
| iii7 | 3 | 3 | 5 | 3.8% | m. 34, beat 4 |
| ii7 | 3 | 3 | 3 | 2.3% | m. 40, beat 2 |
| vi7 | 3 | 3 | 3 | 2.3% | m. 41, beat 1 |
| ii | 3 | 2 | 2.67 | 2.0% | m. 41, beat 4.33 |
| vii∅7 | 2 | 2 | 2 | 1.5% | m. 38, beat 3 |
| iii | 2 | 2 | 2 | 1.5% | m. 42, beat 3 |
| IV7 | 2 | 2 | 1.33 | 1.0% | m. 41, beat 4 |
| vi | 1 | 1 | 1 | 0.8% | m. 44, beat 3 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°6/5/ii | 1 | 1 | 1 | 0.8% | m. 64, beat 2 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vii°7/ii | 1 | 1 | 1 | 0.8% | m. 64, beat 2 |

## Prolongations

The library's standard-harmony analysis found 1 prolongation: a tonic or dominant that lasts through the chords between it and the harmony that closes it. Measures run from the prolonged chord to the closing chord. Numeral is the prolonged chord. Reason is why the library reads the chords between as a prolongation.

| Measures | Function | Numeral | Key | Reason |
| --- | --- | --- | --- | --- |
| mm. 28 to 29 | tonic | i | F minor | tonic-line |

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
| m. 33, beat 1 to m. 33, beat 2 | 498 | unclassified | major |
| m. 33, beat 4 to m. 34, beat 1 | 512 | unclassified | minor |
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

The library's standard-harmony analysis found 13 cadences from the chords, the meter and the soprano. Position is where the arrival begins. Key is the key region of the arrival. Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added before a section start.

| Position | Type | Key | Soprano | Beat strength | Boundary |
| --- | --- | --- | --- | --- | --- |
| m. 3, beat 1 | half | Eb major | 1 | strong | no |
| m. 4, beat 1 | perfect-authentic | Eb major | 1 | strong | no |
| m. 7, beat 1 | half | Eb major | 6 | strong | no |
| m. 18, beat 1 | evaded | Eb major | 2 | strong | no |
| m. 30, beat 1 | half | F minor | 2 | strong | no |
| m. 30, beat 3 | half | F minor | 2 | medium | no |
| m. 37, beat 1 | perfect-authentic | Eb major | 1 | strong | no |
| m. 46, beat 1 | perfect-authentic | Eb major | 1 | strong | no |
| m. 50, beat 1 | imperfect-authentic | Eb major | 3 | strong | no |
| m. 57, beat 1 | half | Eb major | b6 | strong | no |
| m. 60, beat 1 | evaded | Eb major | 3 | strong | no |
| m. 64, beat 1 | evaded | Eb major | 5 | strong | no |
| m. 65, beat 3 | half | Eb major | 7 | medium | no |

### Competing readings

A cadence the evidence does not settle has two readings, the likelier first, each with its share of the evidence.

- m60 b1 evaded cadence (50%), or tonic prolongation (50%)
- m64 b1 evaded cadence (50%), or tonic prolongation (50%)

### Cadence plan

1. m. 3, beat 1: half in Eb major
2. m. 4, beat 1: perfect-authentic in Eb major
3. m. 7, beat 1: half in Eb major
4. m. 18, beat 1: evaded in Eb major
5. m. 30, beat 1: half in F minor
6. m. 30, beat 3: half in F minor
7. m. 37, beat 1: perfect-authentic in Eb major
8. m. 46, beat 1: perfect-authentic in Eb major
9. m. 50, beat 1: imperfect-authentic in Eb major
10. m. 57, beat 1: half in Eb major
11. m. 60, beat 1: evaded in Eb major
12. m. 64, beat 1: evaded in Eb major
13. m. 65, beat 3: half in Eb major

**Counts:** half 6, perfect-authentic 3, evaded 3, imperfect-authentic 1.

## Phrases

The library's standard-form analysis found 10 phrases from the soprano, the meter and the cadences. Length counts the measures of the phrase proper; Ends is what closed it: its cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in the piece; Expansion is a lengthening or shortening of the phrase's ending, or an extension after its cadence, with the measures it adds or removes.

| Phrase | Measures | Length | Cadence | Ends | Role | Expansion |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 3 | 3 | half (m. 3, beat 1) | cadence | beginning | - |
| 2 | m. 4 | 1 | perfect-authentic (m. 4, beat 1) | cadence | middle | - |
| 3 | mm. 5 to 7 | 3 | half (m. 7, beat 1) | cadence | middle | - |
| 4 | mm. 8 to 15 | 8 | none | discontinuity | middle | - |
| 5 | mm. 16 to 30 | 15 | half (m. 30, beat 1) | cadence | middle | internal +7 (mm. 20 to 30) |
| 6 | mm. 31 to 37 | 7 | perfect-authentic (m. 37, beat 1) | cadence | middle | contraction -1 (mm. 35 to 37) |
| 7 | mm. 38 to 46 | 9 | perfect-authentic (m. 46, beat 1) | cadence | middle | internal +1 (mm. 42 to 46) |
| 8 | mm. 47 to 50 | 4 | imperfect-authentic (m. 50, beat 1) | cadence | middle | - |
| 9 | mm. 51 to 57 | 7 | half (m. 57, beat 1) | cadence | middle | contraction -1 (mm. 55 to 57) |
| 10 | mm. 58 to 65 | 8 | half (m. 65, beat 3) | cadence | ending | external post-cadential-extension +2 (mm. 66 to 67) |

- **Phrase 1:** ideas basic-idea (mm. 1 to 3); phrase model Tb (m. 1, beat 4), D (m. 3, beat 1)
- **Phrase 2:** ideas basic-idea (m. 4)
- **Phrase 3:** ideas basic-idea (mm. 5 to 7); phrase model Tb (m. 5, beat 1), D (m. 7, beat 1)
- **Phrase 4:** ideas basic-idea (mm. 8 to 9), contrasting-idea (mm. 10 to 11), contrasting-idea (mm. 12 to 13), contrasting-idea (mm. 14 to 15); subphrases mm. 8 to 11, mm. 12 to 15
- **Phrase 5:** ideas basic-idea (mm. 16 to 17), contrasting-idea (mm. 18 to 19), contrasting-idea (mm. 20 to 21), contrasting-idea (mm. 22 to 23), contrasting-idea (mm. 24 to 25), contrasting-idea (mm. 26 to 27), contrasting-idea (mm. 28 to 30); subphrases mm. 16 to 19, mm. 20 to 30; phrase model Tb (m. 16, beat 1), D (m. 30, beat 1)
- **Phrase 6:** ideas basic-idea (mm. 31 to 32), contrasting-idea (mm. 33 to 34), contrasting-idea (mm. 35 to 37); subphrases mm. 31 to 34, mm. 35 to 37; phrase model D (m. 36, beat 4), Te (m. 37, beat 1)
- **Phrase 7:** ideas basic-idea (mm. 38 to 39), contrasting-idea (mm. 40 to 41), contrasting-idea (mm. 42 to 43), contrasting-idea (mm. 44 to 46); subphrases mm. 38 to 41, mm. 42 to 46; phrase model D (m. 45, beat 4.33), Te (m. 46, beat 1)
- **Phrase 8:** ideas basic-idea (mm. 47 to 48), contrasting-idea (mm. 49 to 50); phrase model Tb (m. 47, beat 1), D (m. 49, beat 4), Te (m. 50, beat 1)
- **Phrase 9:** ideas basic-idea (mm. 51 to 52), contrasting-idea (mm. 53 to 54), contrasting-idea (mm. 55 to 57); subphrases mm. 51 to 54, mm. 55 to 57; phrase model PD (m. 56, beat 1), PD (m. 56, beat 2), D (m. 57, beat 1), D (m. 57, beat 1.33)
- **Phrase 10:** ideas basic-idea (mm. 58 to 59), contrasting-idea (mm. 60 to 61), contrasting-idea (mm. 62 to 63), contrasting-idea (mm. 64 to 65); subphrases mm. 58 to 61, mm. 62 to 65; phrase model Tb (m. 58, beat 1), D (m. 65, beat 3)

## Themes

The library's standard-form analysis found 5 themes from the phrases, their ideas and the cadences. Form is the library's name for the pairing of the two parts; Commonality is how common that pairing is; Traits are what the library finds in the continuation.

| Theme | Form | Commonality | Measures | Length | Parts | Traits |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | hybrid | slightly-less-common | mm. 16 to 30 | 15 | compound-basic-idea (mm. 16 to 19), continuation (mm. 20 to 30, half (m. 30, beat 1)) | - |
| 2 | hybrid | slightly-less-common | mm. 31 to 37 | 7 | compound-basic-idea (mm. 31 to 34), continuation (mm. 35 to 37, perfect-authentic (m. 37, beat 1)) | - |
| 3 | hybrid | slightly-less-common | mm. 38 to 46 | 9 | compound-basic-idea (mm. 38 to 41), continuation (mm. 42 to 46, perfect-authentic (m. 46, beat 1)) | - |
| 4 | hybrid | slightly-less-common | mm. 51 to 57 | 7 | compound-basic-idea (mm. 51 to 54), continuation (mm. 55 to 57, half (m. 57, beat 1)) | - |
| 5 | hybrid | slightly-less-common | mm. 58 to 65 | 8 | compound-basic-idea (mm. 58 to 61), continuation (mm. 62 to 65, half (m. 65, beat 3)) | - |

## Form plan

The kernel's form structure detection grouped the phrases into 5 sections, closing a section at each phrase that ends on a perfect or imperfect authentic cadence; the phrases after the last such cadence form the last section. Sections are lettered by first appearance, and a section whose opening theme returns an earlier section's material takes that letter with a prime. Key is the key the section opens in.

| Section | Measures | Key | Phrases | Closing cadence | Returns |
| --- | --- | --- | --- | --- | --- |
| A | mm. 1 to 4 | Eb major | 1 to 2 | perfect-authentic (m. 4, beat 1) | - |
| B | mm. 5 to 37 | Eb major | 3 to 6 | perfect-authentic (m. 37, beat 1) | - |
| C | mm. 38 to 46 | Eb major | 7 | perfect-authentic (m. 46, beat 1) | - |
| D | mm. 47 to 50 | Eb major | 8 | imperfect-authentic (m. 50, beat 1) | - |
| E | mm. 51 to 67 | Eb major | 9 to 10 | half (m. 65, beat 3) | - |

### Outline

- form (mm. 1 to 67)
  - section A (mm. 1 to 4, Eb major)
    - phrase 1 (mm. 1 to 3)
      - idea basic-idea (mm. 1 to 3)
    - phrase 2 (m. 4)
      - idea basic-idea (m. 4)
  - section B (mm. 5 to 37, Eb major)
    - phrase 3 (mm. 5 to 7)
      - idea basic-idea (mm. 5 to 7)
    - phrase 4 (mm. 8 to 15)
      - subphrase 1 (mm. 8 to 11)
        - idea basic-idea (mm. 8 to 9)
        - idea contrasting-idea (mm. 10 to 11)
      - subphrase 2 (mm. 12 to 15)
        - idea contrasting-idea (mm. 12 to 13)
        - idea contrasting-idea (mm. 14 to 15)
    - phrase 5 (mm. 16 to 30)
      - subphrase 1 (mm. 16 to 19)
        - idea basic-idea (mm. 16 to 17)
        - idea contrasting-idea (mm. 18 to 19)
      - subphrase 2 (mm. 20 to 30)
        - idea contrasting-idea (mm. 20 to 21)
        - idea contrasting-idea (mm. 22 to 23)
        - idea contrasting-idea (mm. 24 to 25)
        - idea contrasting-idea (mm. 26 to 27)
        - idea contrasting-idea (mm. 28 to 30)
    - phrase 6 (mm. 31 to 37)
      - subphrase 1 (mm. 31 to 34)
        - idea basic-idea (mm. 31 to 32)
        - idea contrasting-idea (mm. 33 to 34)
      - subphrase 2 (mm. 35 to 37)
        - idea contrasting-idea (mm. 35 to 37)
  - section C (mm. 38 to 46, Eb major)
    - phrase 7 (mm. 38 to 46)
      - subphrase 1 (mm. 38 to 41)
        - idea basic-idea (mm. 38 to 39)
        - idea contrasting-idea (mm. 40 to 41)
      - subphrase 2 (mm. 42 to 46)
        - idea contrasting-idea (mm. 42 to 43)
        - idea contrasting-idea (mm. 44 to 46)
  - section D (mm. 47 to 50, Eb major)
    - phrase 8 (mm. 47 to 50)
      - idea basic-idea (mm. 47 to 48)
      - idea contrasting-idea (mm. 49 to 50)
  - section E (mm. 51 to 67, Eb major)
    - phrase 9 (mm. 51 to 57)
      - subphrase 1 (mm. 51 to 54)
        - idea basic-idea (mm. 51 to 52)
        - idea contrasting-idea (mm. 53 to 54)
      - subphrase 2 (mm. 55 to 57)
        - idea contrasting-idea (mm. 55 to 57)
    - phrase 10 (mm. 58 to 65)
      - subphrase 1 (mm. 58 to 61)
        - idea basic-idea (mm. 58 to 59)
        - idea contrasting-idea (mm. 60 to 61)
      - subphrase 2 (mm. 62 to 65)
        - idea contrasting-idea (mm. 62 to 63)
        - idea contrasting-idea (mm. 64 to 65)

## Motives

The library's motive analysis found 1 motive family in the soprano, the highest sounding note at each attack. A family is a short figure that recurs; Notes is the length of the figure, Pattern whether its occurrences match by scale steps or by semitones, Measures the measures the occurrences start in, Phrases the phrases they lie in (an occurrence across a phrase boundary lies in none), and Transformations how many occurrences are of each kind the library names against the first occurrence (exact, real-transposed, tonal-transposed, inverted, retrograde, augmented, diminished, fragmented, varied).

| Family | Notes | Pattern | Occurrences | Measures | Phrases | Transformations |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 4 | scale steps | 4 | 31, 50, 58, 64 | 6, 10 | exact 1, real-transposed 2, fragmented 1 |

## Melody

The library's melody adapter read the soprano of each phrase as a line of notes: the soprano notes that start in the phrase. Range is the lowest and highest note and the semitones between them; Climax is the first note at the highest pitch, with how many notes sound it when it is not one; Contour counts the intervals between consecutive notes that go up, go down and repeat, and how many of them are steps and leaps.

| Phrase | Measures | Notes | Range | Climax | Up | Down | Same | Steps | Leaps |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 3 | 4 | A#4 to F5 (7) | F5, m. 2, beat 4.02 | 2 | 1 | 0 | 2 | 1 |
| 2 | m. 4 | 1 | D#5 to D#5 (0) | D#5, m. 4, beat 1.05 | 0 | 0 | 0 | 0 | 0 |
| 3 | mm. 5 to 7 | 4 | D#5 to C6 (9) | C6, m. 7, beat 1.97 | 2 | 1 | 0 | 1 | 2 |
| 4 | mm. 8 to 15 | 17 | B4 to C6 (13) | C6, m. 15, beat 2.15 | 4 | 10 | 2 | 9 | 3 |
| 5 | mm. 16 to 30 | 37 | D#5 to D#6 (12) | D#6, m. 22, beat 2.09 | 14 | 16 | 5 | 19 | 9 |
| 6 | mm. 31 to 37 | 15 | D4 to D#6 (25) | D#6, m. 37, beat 1.99 | 6 | 5 | 3 | 5 | 5 |
| 7 | mm. 38 to 46 | 24 | C5 to G#6 (20) | G#6, m. 38, beat 1.34 | 9 | 13 | 0 | 11 | 11 |
| 8 | mm. 47 to 50 | 11 | D#5 to G6 (16) | G6, m. 48, beat 3.33 | 4 | 6 | 0 | 1 | 9 |
| 9 | mm. 51 to 57 | 19 | F4 to C6 (19) | C6, m. 56, beat 3.33 | 7 | 11 | 0 | 6 | 9 |
| 10 | mm. 58 to 67 | 32 | G4 to A#6 (27) | A#6, m. 65, beat 1 | 12 | 17 | 2 | 10 | 16 |

### Leaps

A leap is an interval of a third or larger. The library's rule asks a leap of a fourth or larger to be followed by a step in the opposite direction: such a leap is Recovered when one follows, Unrecovered when a step the same way, a repeated note or another leap does, and Not judged when nothing follows it; a third asks nothing (Not required). Consecutive counts the pairs of leaps in a row the library's rule finds fault with.

| Phrase | Leaps | Recovered | Unrecovered | Not required | Not judged | Consecutive |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 1 | 0 | 1 | 0 | 0 | 0 |
| 2 | 0 | 0 | 0 | 0 | 0 | 0 |
| 3 | 2 | 1 | 0 | 0 | 1 | 0 |
| 4 | 3 | 1 | 0 | 1 | 1 | 0 |
| 5 | 9 | 4 | 2 | 2 | 1 | 0 |
| 6 | 5 | 1 | 1 | 2 | 1 | 0 |
| 7 | 11 | 1 | 3 | 7 | 0 | 5 |
| 8 | 9 | 0 | 5 | 4 | 0 | 7 |
| 9 | 9 | 3 | 4 | 2 | 0 | 4 |
| 10 | 16 | 0 | 10 | 6 | 0 | 6 |

### Non-chord tones

Each melody note is set against the chord of the segment it starts in, as the library's chord identification reads it: a Chord tone is one of the chord's tones, a Non-chord tone is one it left over and named, and Unclassified notes are in a segment with no chord or were left unnamed. By type counts the non-chord tones under the library's names.

| Phrase | Notes | Chord tones | Non-chord tones | Unclassified | By type |
| --- | --- | --- | --- | --- | --- |
| 1 | 4 | 3 | 0 | 1 | - |
| 2 | 1 | 1 | 0 | 0 | - |
| 3 | 4 | 3 | 1 | 0 | anticipation 1 |
| 4 | 17 | 11 | 2 | 4 | anticipation 1, escape-tone 1 |
| 5 | 37 | 26 | 7 | 4 | neighbor 1, suspension 2, anticipation 1, escape-tone 3 |
| 6 | 15 | 13 | 0 | 2 | - |
| 7 | 24 | 14 | 4 | 6 | anticipation 3, escape-tone 1 |
| 8 | 11 | 9 | 1 | 1 | suspension 1 |
| 9 | 19 | 11 | 1 | 7 | neighbor 1 |
| 10 | 32 | 21 | 3 | 8 | passing 1, appoggiatura 1, escape-tone 1 |

### Tendencies

The library's melodic tendencies are soft preferences a well-formed line usually shows: Pitch proximity (steps outnumber leaps, small leaps outnumber large ones), Step declination (descending steps outnumber ascending ones), Step inertia (the line continues in its direction at least as often as it reverses), Melodic regression (extreme notes turn back toward the middle) and Melodic arch (the line rises to a climax in its middle half and ends below it); a phrase either shows a tendency or does not. The seventh degree column counts the notes the library labels the seventh degree that another note follows, judged by its leading tone rule: a leading tone goes up to the tonic by step, and a seventh a whole step below the tonic does not. Followed is the count that did, Broken the count that did not, with where each broken one stands.

| Phrase | Pitch proximity | Step declination | Step inertia | Melodic regression | Melodic arch | Seventh degree followed | Seventh degree broken |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | does not hold | holds | holds | holds | holds | 0 | 0 |
| 2 | holds | holds | holds | holds | does not hold | 0 | 0 |
| 3 | does not hold | holds | holds | holds | does not hold | 0 | 0 |
| 4 | holds | holds | holds | does not hold | does not hold | 0 | 2 (m. 10, beat 1.08; m. 14, beat 1.03) |
| 5 | does not hold | holds | holds | does not hold | holds | 0 | 0 |
| 6 | does not hold | holds | holds | does not hold | does not hold | 1 | 1 (m. 34, beat 1.04) |
| 7 | does not hold | holds | does not hold | does not hold | does not hold | 0 | 2 (m. 44, beat 4.93; m. 45, beat 3.01) |
| 8 | does not hold | holds | holds | does not hold | holds | 0 | 2 (m. 48, beat 3.83; m. 49, beat 3) |
| 9 | does not hold | holds | holds | does not hold | does not hold | 0 | 2 (m. 52, beat 1.02; m. 56, beat 1.01) |
| 10 | does not hold | holds | holds | holds | does not hold | 1 | 1 (m. 65, beat 3) |

### Development

Each family's occurrences in time order, as the measure they start in and the kind the library names against the family's prototype, which is the original.

- **Family 1:** m31 original > m50 real-transposed > m58 real-transposed > m64 fragmented

## Progression patterns

31 recurring progressions of two to six chords, counted over the library's Roman numerals: a held chord counts once, and a progression does not cross an unlabelled chord or a change of key. Ranked by count, then length, then first occurrence; the top 10 are shown. Measures are where each occurrence begins and ends.

| Rank | Progression | Count | Measures |
| --- | --- | --- | --- |
| 1 | I > I7 | 9 | mm. 8 to 9, m. 13, m. 23, m. 35, m. 37, m. 39, m. 42, mm. 46 to 48, m. 60 |
| 2 | V7 > I7 > I | 7 | mm. 7 to 9, mm. 15 to 16, mm. 17 to 18, mm. 38 to 39, mm. 42 to 44, mm. 57 to 58, mm. 61 to 62 |
| 3 | V7 > I | 6 | mm. 19 to 20, mm. 36 to 37, mm. 49 to 50, mm. 59 to 60, mm. 63 to 64, mm. 65 to 67 |
| 4 | I > iii7 | 5 | mm. 1 to 2, mm. 4 to 6, mm. 12 to 13, mm. 50 to 51, mm. 54 to 55 |
| 5 | V > V7 | 5 | m. 3, m. 7, m. 34, m. 45, m. 53 |
| 6 | V > vii∅7 | 4 | m. 3, m. 7, m. 15, m. 45 |
| 7 | I > V7 | 4 | mm. 21 to 22, mm. 25 to 26, mm. 58 to 59, m. 65 |
| 8 | I7 > V7 | 4 | mm. 35 to 36, mm. 37 to 38, m. 49, mm. 60 to 61 |
| 9 | V > vii∅7 > V > V7 | 3 | m. 3, m. 7, m. 45 |
| 10 | V > V7 > V | 3 | m. 3, m. 34, m. 45 |

## Harmonic rhythm

How often the harmony changes. A change is a chord whose base Roman numeral or key differs from the chord before it, counted in the measure it starts in. Changes lists the measures of each row in order; Mean is changes per measure over the row.

| Measures | Changes | Mean |
| --- | --- | --- |
| mm. 1 to 4 | 1, 1, 5, 1 | 2 |
| mm. 5 to 8 | 0, 1, 4, 2 | 1.75 |
| mm. 9 to 12 | 1, 3, 3, 1 | 2 |
| mm. 13 to 16 | 3, 3, 3, 2 | 2.75 |
| mm. 17 to 20 | 4, 2, 4, 3 | 3.25 |
| mm. 21 to 24 | 1, 2, 3, 3 | 2.25 |
| mm. 25 to 28 | 1, 1, 2, 4 | 2 |
| mm. 29 to 32 | 1, 4, 2, 4 | 2.75 |
| mm. 33 to 36 | 3, 4, 2, 3 | 3 |
| mm. 37 to 40 | 2, 3, 3, 4 | 3 |
| mm. 41 to 44 | 5, 4, 2, 2 | 3.25 |
| mm. 45 to 48 | 5, 1, 0, 2 | 2 |
| mm. 49 to 52 | 2, 1, 3, 2 | 2 |
| mm. 53 to 56 | 3, 1, 3, 3 | 2.5 |
| mm. 57 to 60 | 4, 2, 1, 2 | 2.25 |
| mm. 61 to 64 | 1, 2, 4, 3 | 2.5 |
| mm. 65 to 67 | 3, 1, 0 | 1.33 |

**Mean:** 2.4 changes per measure over 67 measures.

### Approach to cadences

Changes per measure over the two measures before each cadence's arrival (Approach), against the two measures before those (Before). Faster is yes when the approach changes harmony more often.

| Cadence | Arrival | Before | Approach | Faster |
| --- | --- | --- | --- | --- |
| 1 | m. 3 | none | 1 | no |
| 2 | m. 4 | 1 | 3 | yes |
| 3 | m. 7 | 3 | 0.5 | no |
| 4 | m. 18 | 3 | 3 | no |
| 5 | m. 30 | 1.5 | 2.5 | yes |
| 6 | m. 30 | 1.5 | 2.5 | yes |
| 7 | m. 37 | 3.5 | 2.5 | no |
| 8 | m. 46 | 3 | 3.5 | yes |
| 9 | m. 50 | 0.5 | 2 | yes |
| 10 | m. 57 | 2 | 3 | yes |
| 11 | m. 60 | 3.5 | 1.5 | no |
| 12 | m. 64 | 1.5 | 3 | yes |
| 13 | m. 65 | 1.5 | 3.5 | yes |

## Rhythm and meter

**Meter fit:** 12/8, scale 1, no pickup (fit 0.82); no better reading found

### Density

The library's density profile of each phrase, for all its notes and for each line that has a note in it (the soprano and bass of the outer voices, and Other for the rest). A note is in a phrase when its onset is; a chord is one attack. Attacks per beat is read measure by measure over the beats the phrase spans in it, as a mean and the largest; Mean gap is the mean distance between attacks in whole notes; On the beat is the share of attacks exactly on a beat. Durations lists the notes' lengths in whole notes, shortest first, with how many notes have each. The patterns that recur are the motive families' rhythms (see Motives).

| Phrase | Measures | Line | Notes | Attacks | Per beat | Max per beat | Mean gap | On the beat | Durations |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 3 | all | 21 | 21 | 3 | 5 | 0.11 | 28.6% | 1/16 (1), 1/8 (8), 1/4 (6), 161/480 (1), 35/96 (1), 3/8 (2), 37/96 (1), 55/48 (1) |
| 1 | mm. 1 to 3 | soprano | 4 | 4 | 0.33 | 0.75 | 0.61 | 25.0% | 1/16 (1), 161/480 (1), 3/8 (1), 55/48 (1) |
| 1 | mm. 1 to 3 | bass | 5 | 5 | 0.67 | 1 | 0.38 | 100.0% | 1/8 (2), 1/4 (2), 3/8 (1) |
| 1 | mm. 1 to 3 | other | 12 | 12 | 2 | 4 | 0.16 | 0.0% | 1/8 (6), 1/4 (4), 35/96 (1), 37/96 (1) |
| 2 | m. 4 | all | 4 | 4 | 4 | 4 | 0.08 | 25.0% | 1/8 (2), 1/4 (1), 23/40 (1) |
| 2 | m. 4 | soprano | 1 | 1 | 1 | 1 | 0 | 0.0% | 23/40 (1) |
| 2 | m. 4 | bass | 1 | 1 | 1 | 1 | 0 | 100.0% | 1/8 (1) |
| 2 | m. 4 | other | 2 | 2 | 2 | 2 | 0.12 | 0.0% | 1/8 (1), 1/4 (1) |
| 3 | mm. 5 to 7 | all | 32 | 31 | 3.83 | 5 | 0.11 | 29.0% | 1/8 (14), 1/4 (10), 35/96 (1), 89/240 (1), 3/8 (3), 31/80 (1), 37/32 (1), 643/480 (1) |
| 3 | mm. 5 to 7 | soprano | 4 | 4 | 0.58 | 1 | 1.12 | 25.0% | 89/240 (1), 31/80 (1), 37/32 (1), 643/480 (1) |
| 3 | mm. 5 to 7 | bass | 9 | 9 | 1 | 1 | 0.38 | 100.0% | 1/8 (3), 1/4 (3), 3/8 (3) |
| 3 | mm. 5 to 7 | other | 19 | 19 | 2.33 | 3 | 0.17 | 0.0% | 1/8 (11), 1/4 (7), 35/96 (1) |
| 4 | mm. 8 to 15 | all | 126 | 121 | 3.78 | 4.5 | 0.1 | 26.4% | 11/480 (1), 1/32 (3), 17/480 (2), 11/240 (1), 23/480 (2), 1/20 (1), 1/16 (1), 1/8 (49), 23/160 (1), 7/32 (1), 1/4 (38), 41/160 (1), 5/16 (1), 43/120 (1), 29/80 (1), 59/160 (1), ... |
| 4 | mm. 8 to 15 | soprano | 17 | 17 | 0.53 | 0.75 | 0.68 | 11.8% | 17/480 (1), 11/240 (1), 1/16 (1), 23/160 (1), 29/80 (1), 89/240 (1), 3/8 (1), 19/48 (1), 13/32 (1), 33/80 (1), 47/96 (1), 151/240 (1), 101/160 (1), 111/160 (1), 89/120 (1), 9/8 (1), ... |
| 4 | mm. 8 to 15 | bass | 30 | 30 | 0.94 | 1 | 0.4 | 100.0% | 1/8 (8), 1/4 (13), 3/8 (9) |
| 4 | mm. 8 to 15 | other | 79 | 78 | 2.44 | 3.25 | 0.15 | 5.1% | 11/480 (1), 1/32 (3), 17/480 (1), 23/480 (2), 1/20 (1), 1/8 (41), 7/32 (1), 1/4 (25), 41/160 (1), 5/16 (1), 43/120 (1), 59/160 (1) |
| 5 | mm. 16 to 30 | all | 245 | 226 | 4.02 | 5.5 | 0.09 | 24.8% | 1/32 (2), 3/80 (1), 19/480 (1), 7/160 (1), 1/20 (1), 13/240 (1), 7/120 (2), 11/160 (1), 3/40 (1), 19/240 (1), 3/32 (4), 47/480 (1), 49/480 (1), 53/480 (1), 9/80 (1), 19/160 (2), ... |
| 5 | mm. 16 to 30 | soprano | 37 | 37 | 0.67 | 1 | 0.58 | 13.5% | 1/32 (1), 19/480 (1), 1/20 (1), 7/120 (1), 3/32 (1), 49/480 (1), 9/80 (1), 19/160 (1), 1/8 (2), 13/96 (2), 67/480 (1), 17/120 (1), 73/480 (2), 5/32 (1), 3/16 (1), 23/120 (1), ... |
| 5 | mm. 16 to 30 | bass | 56 | 56 | 0.98 | 1.25 | 0.38 | 94.6% | 1/8 (28), 17/120 (1), 1/4 (27) |
| 5 | mm. 16 to 30 | other | 152 | 146 | 2.58 | 3.5 | 0.15 | 4.8% | 1/32 (1), 3/80 (1), 7/160 (1), 13/240 (1), 7/120 (1), 11/160 (1), 3/40 (1), 19/240 (1), 3/32 (3), 47/480 (1), 53/480 (1), 19/160 (1), 29/240 (1), 1/8 (84), 13/96 (1), 11/80 (1), ... |
| 6 | mm. 31 to 37 | all | 96 | 90 | 3.64 | 4 | 0.11 | 27.8% | 17/160 (1), 13/120 (1), 9/80 (1), 11/96 (1), 1/8 (52), 21/160 (1), 71/480 (1), 73/480 (1), 5/32 (1), 7/32 (1), 1/4 (23), 173/480 (1), 89/240 (2), 3/8 (1), 91/240 (1), 47/120 (1), ... |
| 6 | mm. 31 to 37 | soprano | 16 | 16 | 0.68 | 1 | 0.62 | 25.0% | 11/96 (1), 1/8 (2), 71/480 (1), 5/32 (1), 1/4 (1), 173/480 (1), 89/240 (1), 3/8 (1), 47/120 (1), 13/32 (1), 99/160 (1), 5/8 (1), 151/240 (1), 15/16 (1), 177/160 (1) |
| 6 | mm. 31 to 37 | bass | 24 | 24 | 0.96 | 1 | 0.39 | 100.0% | 1/8 (11), 1/4 (13) |
| 6 | mm. 31 to 37 | other | 56 | 56 | 2.21 | 3 | 0.17 | 3.6% | 17/160 (1), 13/120 (1), 9/80 (1), 1/8 (39), 21/160 (1), 73/480 (1), 7/32 (1), 1/4 (9), 89/240 (1), 91/240 (1) |
| 7 | mm. 38 to 46 | all | 170 | 146 | 4.31 | 5.25 | 0.08 | 21.9% | 1/240 (1), 7/480 (1), 1/32 (1), 3/80 (1), 1/24 (3), 23/480 (1), 5/96 (1), 9/160 (1), 1/16 (2), 1/15 (1), 11/160 (1), 37/480 (1), 13/160 (1), 43/480 (2), 3/32 (5), 47/480 (1), ... |
| 7 | mm. 38 to 46 | soprano | 24 | 24 | 0.75 | 1 | 0.52 | 16.7% | 1/24 (2), 5/96 (1), 9/160 (1), 1/16 (1), 43/480 (1), 1/10 (1), 1/8 (1), 2/15 (1), 13/96 (1), 11/80 (1), 5/32 (3), 13/80 (1), 31/96 (1), 11/30 (1), 59/160 (1), 89/240 (1), ... |
| 7 | mm. 38 to 46 | bass | 34 | 34 | 1.03 | 1.25 | 0.36 | 88.2% | 1/8 (16), 1/4 (16), 3/8 (2) |
| 7 | mm. 38 to 46 | other | 112 | 104 | 3.06 | 4.25 | 0.12 | 10.6% | 1/240 (1), 7/480 (1), 1/32 (1), 3/80 (1), 1/24 (1), 23/480 (1), 1/16 (1), 1/15 (1), 11/160 (1), 37/480 (1), 13/160 (1), 43/480 (1), 3/32 (5), 47/480 (1), 1/10 (1), 53/480 (2), ... |
| 8 | mm. 47 to 50 | all | 81 | 62 | 4.81 | 5.75 | 0.08 | 21.0% | 1/32 (1), 1/16 (23), 41/480 (1), 3/32 (6), 1/8 (25), 31/240 (1), 7/32 (1), 1/4 (16), 5/16 (1), 3/8 (3), 63/160 (1), 17/32 (1), 27/32 (1) |
| 8 | mm. 47 to 50 | soprano | 12 | 12 | 0.94 | 1 | 0.41 | 25.0% | 1/16 (2), 3/32 (2), 1/8 (3), 7/32 (1), 1/4 (1), 63/160 (1), 17/32 (1), 27/32 (1) |
| 8 | mm. 47 to 50 | bass | 13 | 13 | 1 | 1 | 0.38 | 100.0% | 1/8 (4), 1/4 (6), 3/8 (3) |
| 8 | mm. 47 to 50 | other | 56 | 48 | 3.75 | 5 | 0.1 | 8.3% | 1/32 (1), 1/16 (21), 41/480 (1), 3/32 (4), 1/8 (18), 31/240 (1), 1/4 (9), 5/16 (1) |
| 9 | mm. 51 to 57 | all | 114 | 105 | 4.5 | 7 | 0.09 | 23.8% | 1/96 (1), 1/80 (1), 1/48 (1), 1/24 (1), 7/160 (1), 5/96 (2), 9/160 (1), 1/16 (5), 17/240 (1), 7/96 (1), 3/32 (5), 1/8 (48), 17/120 (1), 9/40 (1), 59/240 (1), 1/4 (28), ... |
| 9 | mm. 51 to 57 | soprano | 19 | 19 | 0.79 | 1.25 | 0.48 | 15.8% | 5/96 (2), 1/16 (1), 3/32 (2), 1/8 (2), 9/40 (1), 59/240 (1), 1/4 (1), 11/30 (1), 59/160 (2), 3/8 (1), 13/32 (1), 223/480 (1), 19/40 (1), 247/480 (1), 61/96 (1) |
| 9 | mm. 51 to 57 | bass | 24 | 24 | 1.07 | 2 | 0.4 | 95.8% | 1/8 (12), 1/4 (9), 3/8 (3) |
| 9 | mm. 51 to 57 | other | 71 | 68 | 2.96 | 5 | 0.14 | 5.9% | 1/96 (1), 1/80 (1), 1/48 (1), 1/24 (1), 7/160 (1), 9/160 (1), 1/16 (4), 17/240 (1), 7/96 (1), 3/32 (3), 1/8 (34), 17/120 (1), 1/4 (18), 41/160 (1), 127/480 (1), 163/480 (1) |
| 10 | mm. 58 to 67 | all | 213 | 174 | 4.35 | 6.25 | 0.08 | 22.4% | 1/32 (4), 17/480 (2), 1/24 (1), 1/20 (1), 1/16 (20), 11/160 (2), 17/240 (1), 7/96 (1), 13/160 (1), 41/480 (1), 43/480 (1), 3/32 (5), 1/10 (1), 5/48 (3), 7/60 (1), 19/160 (1), ... |
| 10 | mm. 58 to 67 | soprano | 32 | 32 | 0.8 | 1 | 0.44 | 31.2% | 1/16 (3), 17/240 (1), 43/480 (1), 3/32 (1), 1/10 (1), 5/48 (1), 1/8 (3), 31/240 (1), 13/96 (1), 3/20 (1), 41/240 (2), 3/16 (1), 7/32 (1), 1/4 (6), 41/160 (2), 43/160 (1), ... |
| 10 | mm. 58 to 67 | bass | 40 | 40 | 1 | 1.25 | 0.37 | 97.5% | 1/8 (14), 1/4 (20), 3/8 (6) |
| 10 | mm. 58 to 67 | other | 141 | 122 | 3.05 | 5 | 0.12 | 5.7% | 1/32 (4), 17/480 (2), 1/24 (1), 1/20 (1), 1/16 (17), 11/160 (2), 7/96 (1), 13/160 (1), 41/480 (1), 3/32 (4), 5/48 (2), 7/60 (1), 19/160 (1), 1/8 (62), 31/240 (1), 21/160 (2), ... |

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
| 2 | m. 4 | 4 | 4 | 1 short | m. 4 | 0.5 |
| 3 | mm. 5 to 7 | 2 | 5, 7 | 1 regular, 1 short | m. 7 | 0.83 |
| 4 | mm. 8 to 15 | 4 | 8, 12 | 2 regular | none | 1 |
| 5 | mm. 16 to 30 | 4 | 16, 20 | 1 regular, 1 expanded | m. 23 | 1 |
| 6 | mm. 31 to 37 | 4 | 31, 35 | 1 regular, 1 contracted | m. 37 | 1 |
| 7 | mm. 38 to 46 | 4 | 38, 42 | 1 regular, 1 expanded | m. 45 | 1 |
| 8 | mm. 47 to 50 | 4 | 47 | 1 regular | m. 50 | 1 |
| 9 | mm. 51 to 57 | 4 | 51, 55 | 1 regular, 1 contracted | m. 57 | 1 |
| 10 | mm. 58 to 67 | 4 | 58, 62, 66 | 2 regular, 1 extension | m. 65 | 1 |

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

4 voices over 268 chord segments, 128 motions between consecutive segments: 25 contrary, 24 similar, 4 parallel, 72 oblique, 3 static.

| Voices | Contrary | Similar | Parallel | Oblique | Static |
| --- | --- | --- | --- | --- | --- |
| 0 and 1 | 2 | 2 | 0 | 3 | 1 |
| 0 and 2 | 3 | 2 | 0 | 7 | 0 |
| 0 and 3 | 5 | 8 | 2 | 14 | 2 |
| 1 and 2 | 2 | 3 | 0 | 5 | 0 |
| 1 and 3 | 9 | 4 | 2 | 31 | 0 |
| 2 and 3 | 4 | 5 | 0 | 12 | 0 |

0 parallel fifths, 1 parallel octave, 1 direct fifths or octaves in the outer voices, 19 unresolved sevenths, 2 unresolved leading tones.

| Location | Fault | Voices |
| --- | --- | --- |
| m. 23, beat 3 to m. 24, beat 1 | direct-perfects | 0 and 3 |
| m. 41, beat 4 to m. 42, beat 1 | parallel-octaves | 1 and 3 |
| m. 3, beat 4 to m. 4, beat 1 | seventh-resolves-down | 3 |
| m. 7, beat 4 to m. 8, beat 2 | leading-tone-resolves | 0 |
| m. 7, beat 4 to m. 8, beat 2 | seventh-resolves-down | 3 |
| m. 17, beat 3 to m. 18, beat 1 | seventh-resolves-down | 3 |
| m. 18, beat 2 to m. 18, beat 4 | seventh-resolves-down | 0 |
| m. 19, beat 4.33 to m. 20, beat 2 | seventh-resolves-down | 3 |
| m. 20, beat 4.37 to m. 21, beat 2 | seventh-resolves-down | 3 |
| m. 23, beat 1 to m. 23, beat 3 | seventh-resolves-down | 3 |
| m. 30, beat 2 to m. 30, beat 4 | seventh-resolves-down | 0 |
| m. 31, beat 1 to m. 31, beat 3 | seventh-resolves-down | 3 |
| m. 32, beat 2 to m. 32, beat 4 | seventh-resolves-down | 3 |
| m. 33, beat 3 to m. 34, beat 1 | seventh-resolves-down | 1 |
| m. 34, beat 2 to m. 34, beat 4 | seventh-resolves-down | 3 |
| m. 36, beat 2 to m. 36, beat 4 | seventh-resolves-down | 2 |
| m. 36, beat 4 to m. 37, beat 2 | seventh-resolves-down | 2 |
| m. 42, beat 4 to m. 43, beat 2 | seventh-resolves-down | 3 |
| m. 43, beat 1 to m. 43, beat 3 | seventh-resolves-down | 0 |
| m. 48, beat 4 to m. 49, beat 2 | seventh-resolves-down | 3 |
| m. 53, beat 2 to m. 53, beat 4 | seventh-resolves-down | 3 |
| m. 55, beat 3 to m. 56, beat 1 | leading-tone-resolves | 3 |
| m. 63, beat 4 to m. 64, beat 2 | seventh-resolves-down | 1 |

43 chords with three or more voices sounding: the root is doubled in 13, the third in 5, the fifth in 2, the seventh in 2, no member doubled in 21. Doubling is counted, not judged. Spacing is judged by the library for the 4 chords with four voices all sounding: 2 too wide.

| Location | Chord | Gap too wide |
| --- | --- | --- |
| m. 20, beat 3 to m. 20, beat 4 | 77 | voices 1 and 2 |
| m. 66, beat 3 to m. 66, beat 4 | 263 | voices 1 and 2 |

## Markings

The tempo and dynamics markings the score notates, read with the library's standard-markings rules. A tempo holds until the next one, a dynamic until the next in its part. Group and the loudness comparison are the library's; a text it does not recognise has none. Wedges are not read, and articulations are not imported.

The score has no tempo marking.

The score has no dynamics marking.
