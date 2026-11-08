# Analysis Report: mozart-545.musicxml

## Summary

- **File:** mozart-545.musicxml, 2728 notes.
- **Input:** MusicXML score, 1 part merged into one texture. Meter: notated. Key signature: notated. Pitch spelling: notated.
- **Key:** G major, the key with the most time across 25 key regions.
- **Chords:** 224 chord events; 212 matched a chord (95%), 12 did not.
- **Most frequent chords:** I (63), IV (23), dominant7 (23).

## Meter

- **Time signature:** 4/4 (notated)
- **Measures:** 146 measures.
- **Pickup:** None.

## Key regions

Positions are measure and beat. Uncertainty is the library's, from 0 (clearly this key) to 1 (another key fits as well or better). The home key is the key with the most time across its regions.

- **Home key:** G major.
- **Opening key:** C major.
- **Closing key:** C major.
- **Notated key signatures:** C major or A minor at m. 1, beat 1.

| Position | Key | Uncertainty | Set by |
| --- | --- | --- | --- |
| m. 1, beat 1 to m. 8, beat 1 | C major | 0.71 | key signature |
| m. 8, beat 1 to m. 10, beat 1 | F major | 0.95 | profile |
| m. 10, beat 1 to m. 22, beat 1 | G major | 0.79 | profile |
| m. 22, beat 1 to m. 25, beat 1 | A minor | 0.86 | profile |
| m. 25, beat 1 to m. 29, beat 1 | G major | 0.65 | profile |
| m. 29, beat 1 to m. 36, beat 1 | C major | 0.71 | profile |
| m. 36, beat 1 to m. 38, beat 1 | F major | 0.95 | profile |
| m. 38, beat 1 to m. 50, beat 1 | G major | 0.79 | profile |
| m. 50, beat 1 to m. 53, beat 1 | A minor | 0.86 | profile |
| m. 53, beat 1 to m. 61, beat 1 | G major | 0.83 | profile |
| m. 61, beat 1 to m. 64, beat 1 | D minor | 0.71 | profile |
| m. 64, beat 1 to m. 69, beat 1 | A minor | 0.78 | profile |
| m. 69, beat 1 to m. 83, beat 1 | F major | 0.73 | profile |
| m. 83, beat 1 to m. 88, beat 1 | G major | 0.84 | profile |
| m. 88, beat 1 to m. 95, beat 1 | C major | 1.00 | profile |
| m. 95, beat 1 to m. 98, beat 1 | D minor | 1.00 | profile |
| m. 98, beat 1 to m. 104, beat 1 | C major | 0.72 | profile |
| m. 104, beat 1 to m. 106, beat 1 | G minor | 1.00 | profile |
| m. 106, beat 1 to m. 109, beat 1 | D minor | 0.71 | profile |
| m. 109, beat 1 to m. 114, beat 1 | A minor | 0.78 | profile |
| m. 114, beat 1 to m. 128, beat 1 | F major | 0.73 | profile |
| m. 128, beat 1 to m. 133, beat 1 | G major | 0.84 | profile |
| m. 133, beat 1 to m. 140, beat 1 | C major | 1.00 | profile |
| m. 140, beat 1 to m. 143, beat 1 | D minor | 1.00 | profile |
| m. 143, beat 1 to m. 147, beat 1 | C major | 0.69 | profile |

**Note:** The home key G major is not the major key or relative minor of any notated key signature. Both are reported; neither is preferred.

## Tonicizations

16 brief excursions of another key that return to the key before. Span is the stretch in the target key. The library tells a tonicization from a modulation by how long the new key lasts, whether it returns, and whether a cadence arrives in it; without a cadence it is a tonicization however long.

| Span | In | Target |
| --- | --- | --- |
| m. 8, beat 1 to m. 10, beat 1 | C major | F major |
| m. 10, beat 1 to m. 22, beat 1 | F major | G major |
| m. 22, beat 1 to m. 25, beat 1 | G major | A minor |
| m. 29, beat 1 to m. 36, beat 1 | G major | C major |
| m. 36, beat 1 to m. 38, beat 1 | C major | F major |
| m. 38, beat 1 to m. 50, beat 1 | F major | G major |
| m. 50, beat 1 to m. 53, beat 1 | G major | A minor |
| m. 64, beat 1 to m. 69, beat 1 | D minor | A minor |
| m. 83, beat 1 to m. 88, beat 1 | F major | G major |
| m. 88, beat 1 to m. 95, beat 1 | G major | C major |
| m. 95, beat 1 to m. 98, beat 1 | C major | D minor |
| m. 104, beat 1 to m. 106, beat 1 | C major | G minor |
| m. 109, beat 1 to m. 114, beat 1 | D minor | A minor |
| m. 128, beat 1 to m. 133, beat 1 | F major | G major |
| m. 133, beat 1 to m. 140, beat 1 | G major | C major |
| m. 140, beat 1 to m. 143, beat 1 | C major | D minor |

## Modulations

A modulation is a lasting new key. Position is where the new key arrives. Type is pivot when the library found a pivot chord and direct when it did not. A return goes back to the key before an excursion. The library gives the change's extent from the length of the new key, whether it returns, and whether a cadence arrives in it.

| Position | From | To | Extent | Type | Return |
| --- | --- | --- | --- | --- | --- |
| m. 61, beat 1 | G major | D minor | modulation | pivot | no |
| m. 69, beat 1 | A minor | F major | modulation | pivot | no |
| m. 106, beat 1 | G minor | D minor | modulation | pivot | no |
| m. 114, beat 1 | A minor | F major | modulation | pivot | no |

## Pivot chords

The chord at a modulation that is diatonic in both the old and the new key, named by root and quality, with no Roman numeral. Modulation is its row in the Modulations table, counting from 1.

| Position | Chord | Bass | Inversion | From | To | Modulation |
| --- | --- | --- | --- | --- | --- | --- |
| m. 60, beat 3 to m. 61, beat 1 | A minor7 | A | root position | G major | D minor | 1 |
| m. 69, beat 1 to m. 69, beat 3 | D minor7 | D | root position | A minor | F major | 2 |
| m. 106, beat 1 to m. 107, beat 1 | D minor | D | root position | G minor | D minor | 3 |
| m. 114, beat 1 to m. 114, beat 3 | D minor7 | D | root position | A minor | F major | 4 |

## Chords

Positions are measure and beat. Numeral is the Roman numeral in the local key region, which is named where it changes. Function is the library's harmonic function of the chord: an applied chord is a dominant toward its target degree, and a predominant is strong (IV, ii) or weak (iii, vi). Six-four is the library's type for a chord in second inversion. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Numeral | Function | Six-four | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 2, beat 1 | I | I (in C major) | tonic | - | C | major | C | root position | 4 | 1.00 | - |
| m. 2, beat 1 to m. 2, beat 3 | V | V6/4 | dominant | - | G | major | D | second | 2 | 0.07 | vii° (B diminished, score 0.88) |
| m. 2, beat 3 to m. 3, beat 1 | I | I | tonic | - | C | major | C | root position | 2 | 1.00 | - |
| m. 3, beat 1 to m. 3, beat 3 | IV | IV6/4 | predominant (strong) | neighbor | F | major | C | second | 2 | 0.06 | vi (A minor, score 0.99) |
| m. 3, beat 3 to m. 4, beat 1 | I | I | tonic | - | C | major | C | root position | 2 | 1.00 | - |
| m. 4, beat 1 to m. 4, beat 3 | V | V6 | dominant | - | G | major | B | first | 2 | 0.04 | iii (E minor, score 0.88) |
| m. 4, beat 3 to m. 5, beat 1 | I | I | tonic | - | C | major | C | root position | 2 | 1.00 | - |
| m. 5, beat 1 to m. 6, beat 1 | IV | IV | predominant (strong) | - | F | major | F | root position | 4 | 0.12 | minor7 (D minor7, score 0.75) |
| m. 6, beat 1 to m. 7, beat 1 | I | I6 | tonic | - | C | major | E | first | 4 | 0.12 | - |
| m. 7, beat 1 to m. 7, beat 2 | minor7 | ii7 | predominant (strong) | - | D | minor7 | D | root position | 1 | 0.59 | - |
| m. 7, beat 2 to m. 8, beat 1 | vii° | vii°6 | dominant | - | B | diminished | D | first | 3 | 0.11 | V (G major, score 0.69) |
| m. 8, beat 1 to m. 9, beat 1 | V | V (in F major) | dominant | - | C | major | C | root position | 4 | 0.00 | iii (A minor, score 0.89) |
| m. 9, beat 1 to m. 10, beat 1 | vi | vi6 | predominant (weak) | - | D | minor | F | first | 4 | 0.08 | I (F major, score 0.90) |
| m. 10, beat 1 to m. 10, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 10, beat 3 to m. 10, beat 4 | minor7 | ii7 (in G major) | predominant (strong) | - | A | minor7 | A | root position | 1 | 1.00 | - |
| m. 10, beat 4 to m. 11, beat 1 | V | V6 | dominant | - | D | major | F# | first | 1 | 0.22 | - |
| m. 11, beat 1 to m. 11, beat 2 | I | I | tonic | - | G | major | G | root position | 1 | 1.00 | - |
| m. 11, beat 2 to m. 11, beat 3 | IV | IV6/4 | predominant (strong) | neighbor | C | major | G | second | 1 | 0.33 | - |
| m. 11, beat 3 to m. 11, beat 4 | I | I | tonic | - | G | major | G | root position | 1 | 1.00 | - |
| m. 11, beat 4 to m. 12, beat 1 | IV | IV6/4 | predominant (strong) | neighbor | C | major | G | second | 1 | 0.33 | - |
| m. 12, beat 1 to m. 12, beat 4 | I | I | tonic | - | G | major | G | root position | 3 | 1.00 | - |
| m. 13, beat 1 to m. 14, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 14, beat 1 to m. 15, beat 1 | I | I6 | tonic | - | G | major | B | first | 4 | 0.21 | - |
| m. 15, beat 1 to m. 16, beat 1 | dominant7 | V4/2 | dominant | - | D | dominant7 | A | second | 4 | 0.19 | - |
| m. 16, beat 1 to m. 17, beat 1 | I | I6 | tonic | - | G | major | B | first | 4 | 0.21 | - |
| m. 17, beat 1 to m. 18, beat 1 | dominant7 | V4/2 | dominant | - | D | dominant7 | A | second | 4 | 0.19 | - |
| m. 18, beat 1 to m. 18, beat 3 | I | I6/4 | tonic | - | G | major | B | first | 2 | 0.09 | iii (B minor, score 0.96) |
| m. 18, beat 3 to m. 19, beat 1 | IV | IV6 | predominant (strong) | - | C | major | C | root position | 2 | 0.26 | - |
| m. 19, beat 1 to m. 19, beat 3 | vii° | vii°6/4 | dominant | - | F# | diminished | A | first | 2 | 0.09 | ii (A minor, score 0.96) |
| m. 19, beat 3 to m. 20, beat 1 | iii | iii6 | predominant (weak) | - | B | minor | B | root position | 2 | 0.26 | - |
| m. 20, beat 1 to m. 20, beat 3 | vi | vi6/4 | predominant (weak) | - | E | minor | G | first | 2 | 0.09 | I (G major, score 0.96) |
| m. 20, beat 3 to m. 21, beat 1 | ii | ii6 | predominant (strong) | - | A | minor | A | root position | 2 | 0.26 | - |
| m. 21, beat 1 to m. 21, beat 3 | V | V6/4 | dominant | - | D | major | F# | first | 2 | 0.09 | vii° (F# diminished, score 0.96) |
| m. 21, beat 3 to m. 22, beat 1 | I | I6 | tonic | - | G | major | G | root position | 2 | 0.32 | - |
| m. 22, beat 1 to m. 23, beat 1 | minor | i6 (in A minor) | tonic | - | A | minor | C | first | 4 | 0.31 | - |
| m. 23, beat 1 to m. 24, beat 1 | minor | i6 | tonic | - | A | minor | C | first | 4 | 0.16 | - |
| m. 24, beat 1 to m. 25, beat 1 | major | VII6/4 | predominant (weak) | - | G | major | D | second | 4 | 0.18 | - |
| m. 25, beat 1 to m. 26, beat 1 | minor7 | iii6/5 (in G major) | predominant (weak) | - | B | minor7 | D | first | 4 | 0.03 | dominant7 (D dominant7, score 0.65) |
| m. 26, beat 1 to m. 26, beat 4 | I | I | tonic | - | G | major | G | root position | 3 | 1.00 | - |
| m. 26, beat 4 to m. 27, beat 1 | dominant7 | V7 | dominant | - | D | dominant7 | D | root position | 1 | 1.00 | - |
| m. 27, beat 1 to m. 27, beat 4 | I | I | tonic | - | G | major | D | second | 3 | 1.00 | - |
| m. 27, beat 4 to m. 28, beat 1 | dominant7 | V7 | dominant | - | D | dominant7 | D | root position | 1 | 1.00 | - |
| m. 28, beat 1 to m. 28, beat 4 | I | I | tonic | - | G | major | G | root position | 3 | 1.00 | - |
| m. 29, beat 1 to m. 30, beat 1 | I | I (in C major) | tonic | - | C | major | C | root position | 4 | 1.00 | - |
| m. 30, beat 1 to m. 30, beat 3 | V | V6/4 | dominant | - | G | major | D | second | 2 | 0.07 | vii° (B diminished, score 0.88) |
| m. 30, beat 3 to m. 31, beat 1 | I | I | tonic | - | C | major | C | root position | 2 | 1.00 | - |
| m. 31, beat 1 to m. 31, beat 3 | IV | IV6/4 | predominant (strong) | neighbor | F | major | C | second | 2 | 0.06 | vi (A minor, score 0.99) |
| m. 31, beat 3 to m. 32, beat 1 | I | I | tonic | - | C | major | C | root position | 2 | 1.00 | - |
| m. 32, beat 1 to m. 32, beat 3 | V | V6 | dominant | - | G | major | B | first | 2 | 0.04 | iii (E minor, score 0.88) |
| m. 32, beat 3 to m. 33, beat 1 | I | I | tonic | - | C | major | C | root position | 2 | 1.00 | - |
| m. 33, beat 1 to m. 34, beat 1 | IV | IV | predominant (strong) | - | F | major | F | root position | 4 | 0.12 | minor7 (D minor7, score 0.75) |
| m. 34, beat 1 to m. 35, beat 1 | I | I6 | tonic | - | C | major | E | first | 4 | 0.12 | - |
| m. 35, beat 1 to m. 35, beat 2 | minor7 | ii7 | predominant (strong) | - | D | minor7 | D | root position | 1 | 0.59 | - |
| m. 35, beat 2 to m. 36, beat 1 | vii° | vii°6 | dominant | - | B | diminished | D | first | 3 | 0.11 | V (G major, score 0.69) |
| m. 36, beat 1 to m. 37, beat 1 | V | V (in F major) | dominant | - | C | major | C | root position | 4 | 0.00 | iii (A minor, score 0.89) |
| m. 37, beat 1 to m. 38, beat 1 | vi | vi6 | predominant (weak) | - | D | minor | F | first | 4 | 0.08 | I (F major, score 0.90) |
| m. 38, beat 1 to m. 38, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 38, beat 3 to m. 38, beat 4 | minor7 | ii7 (in G major) | predominant (strong) | - | A | minor7 | A | root position | 1 | 1.00 | - |
| m. 38, beat 4 to m. 39, beat 1 | V | V6 | dominant | - | D | major | F# | first | 1 | 0.22 | - |
| m. 39, beat 1 to m. 39, beat 2 | I | I | tonic | - | G | major | G | root position | 1 | 1.00 | - |
| m. 39, beat 2 to m. 39, beat 3 | IV | IV6/4 | predominant (strong) | neighbor | C | major | G | second | 1 | 0.33 | - |
| m. 39, beat 3 to m. 39, beat 4 | I | I | tonic | - | G | major | G | root position | 1 | 1.00 | - |
| m. 39, beat 4 to m. 40, beat 1 | IV | IV6/4 | predominant (strong) | neighbor | C | major | G | second | 1 | 0.33 | - |
| m. 40, beat 1 to m. 40, beat 4 | I | I | tonic | - | G | major | G | root position | 3 | 1.00 | - |
| m. 41, beat 1 to m. 42, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 42, beat 1 to m. 43, beat 1 | I | I6 | tonic | - | G | major | B | first | 4 | 0.21 | - |
| m. 43, beat 1 to m. 44, beat 1 | dominant7 | V4/2 | dominant | - | D | dominant7 | A | second | 4 | 0.19 | - |
| m. 44, beat 1 to m. 45, beat 1 | I | I6 | tonic | - | G | major | B | first | 4 | 0.21 | - |
| m. 45, beat 1 to m. 46, beat 1 | dominant7 | V4/2 | dominant | - | D | dominant7 | A | second | 4 | 0.19 | - |
| m. 46, beat 1 to m. 46, beat 3 | I | I6/4 | tonic | - | G | major | B | first | 2 | 0.09 | iii (B minor, score 0.96) |
| m. 46, beat 3 to m. 47, beat 1 | IV | IV6 | predominant (strong) | - | C | major | C | root position | 2 | 0.26 | - |
| m. 47, beat 1 to m. 47, beat 3 | vii° | vii°6/4 | dominant | - | F# | diminished | A | first | 2 | 0.09 | ii (A minor, score 0.96) |
| m. 47, beat 3 to m. 48, beat 1 | iii | iii6 | predominant (weak) | - | B | minor | B | root position | 2 | 0.26 | - |
| m. 48, beat 1 to m. 48, beat 3 | vi | vi6/4 | predominant (weak) | - | E | minor | G | first | 2 | 0.09 | I (G major, score 0.96) |
| m. 48, beat 3 to m. 49, beat 1 | ii | ii6 | predominant (strong) | - | A | minor | A | root position | 2 | 0.26 | - |
| m. 49, beat 1 to m. 49, beat 3 | V | V6/4 | dominant | - | D | major | F# | first | 2 | 0.09 | vii° (F# diminished, score 0.96) |
| m. 49, beat 3 to m. 50, beat 1 | I | I6 | tonic | - | G | major | G | root position | 2 | 0.32 | - |
| m. 50, beat 1 to m. 51, beat 1 | minor | i6 (in A minor) | tonic | - | A | minor | C | first | 4 | 0.31 | - |
| m. 51, beat 1 to m. 52, beat 1 | minor | i6 | tonic | - | A | minor | C | first | 4 | 0.16 | - |
| m. 52, beat 1 to m. 53, beat 1 | major | VII6/4 | predominant (weak) | - | G | major | D | second | 4 | 0.18 | - |
| m. 53, beat 1 to m. 54, beat 1 | minor7 | iii6/5 (in G major) | predominant (weak) | - | B | minor7 | D | first | 4 | 0.03 | dominant7 (D dominant7, score 0.65) |
| m. 54, beat 1 to m. 54, beat 4 | I | I | tonic | - | G | major | G | root position | 3 | 1.00 | - |
| m. 54, beat 4 to m. 55, beat 1 | dominant7 | V7 | dominant | - | D | dominant7 | D | root position | 1 | 1.00 | - |
| m. 55, beat 1 to m. 55, beat 4 | I | I | tonic | - | G | major | D | second | 3 | 1.00 | - |
| m. 55, beat 4 to m. 56, beat 1 | dominant7 | V7 | dominant | - | D | dominant7 | D | root position | 1 | 1.00 | - |
| m. 56, beat 1 to m. 56, beat 4 | I | I | tonic | - | G | major | G | root position | 3 | 1.00 | - |
| m. 57, beat 1 to m. 58, beat 1 | minor7 | ii4/2 | predominant (strong) | - | A | minor7 | G | third | 4 | 0.10 | I (G major, score 0.64) |
| m. 58, beat 1 to m. 58, beat 4 | I | I | tonic | - | G | major | D | second | 3 | 1.00 | - |
| m. 58, beat 4 to m. 59, beat 1 | dominant7 | V7 | dominant | - | D | dominant7 | D | root position | 1 | 1.00 | - |
| m. 59, beat 1 to m. 60, beat 1 | IV | IV6/4 | predominant (strong) | - | C | major | G | second | 4 | 0.04 | vi (E minor, score 0.78) |
| m. 60, beat 1 to m. 60, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 60, beat 2 to m. 60, beat 3 | V | V4/2/ii | dominant of 2 | - | D | major | D | root position | 1 | 0.53 | - |
| m. 60, beat 3 to m. 61, beat 1 | ii | V7/V | dominant of 5 | - | A | minor | A | root position | 2 | 0.00 | V (D major, score 0.56) |
| m. 61, beat 1 to m. 62, beat 1 | minor | i (in D minor) | tonic | - | D | minor | D | root position | 4 | 0.36 | - |
| m. 62, beat 1 to m. 62, beat 4 | minor | i | tonic | - | D | minor | A | second | 3 | 1.00 | - |
| m. 62, beat 4 to m. 63, beat 1 | dominant7 | V7 | dominant | - | A | dominant7 | A | root position | 1 | 1.00 | - |
| m. 63, beat 1 to m. 64, beat 1 | minor | i | tonic | - | D | minor | D | root position | 4 | 0.00 | diminished (B diminished, score 0.89) |
| m. 64, beat 1 to m. 65, beat 1 | diminished | viio (in A minor) | dominant | - | G# | diminished | D | second | 4 | 0.00 | major (E major, score 0.83) |
| m. 65, beat 1 to m. 66, beat 1 | major | VI6/4 | predominant (weak) | - | F | major | C | second | 4 | 0.07 | minor7 (D minor7, score 0.74) |
| m. 66, beat 1 to m. 67, beat 1 | minor | v6/4 | dominant | - | E | minor | B | second | 4 | 0.07 | major7 (C major7, score 0.74) |
| m. 67, beat 1 to m. 68, beat 1 | minor | iv6/4 | predominant (strong) | - | D | minor | A | second | 4 | 0.07 | half-diminished7 (B half-diminished7, score 0.74) |
| m. 68, beat 1 to m. 68, beat 2 | major | V6 | dominant | - | E | major | G# | first | 1 | 0.07 | diminished (G# diminished, score 0.91) |
| m. 68, beat 2 to m. 68, beat 4 | augmented | III+6/4 | predominant (weak) | - | C | augmented | A | - | 2 | 0.65 | - |
| m. 68, beat 4 to m. 69, beat 1 | major7 | VI6/5 | predominant (weak) | - | F | major7 | E | third | 1 | 0.23 | - |
| m. 69, beat 1 to m. 69, beat 3 | ii | ii6/4 (in F major) | predominant (strong) | - | G | minor | D | second | 2 | 0.00 | IV (A# major, score 0.95) |
| m. 69, beat 3 to m. 70, beat 1 | dominant7 | V7 | dominant | - | C | dominant7 | C | root position | 2 | 0.32 | - |
| m. 70, beat 1 to m. 71, beat 1 | I | I | tonic | - | F | major | F | root position | 4 | 1.00 | - |
| m. 71, beat 1 to m. 71, beat 3 | V | V6/4 | dominant | - | C | major | G | second | 2 | 0.07 | vii° (E diminished, score 0.88) |
| m. 71, beat 3 to m. 72, beat 1 | I | I | tonic | - | F | major | F | root position | 2 | 1.00 | - |
| m. 72, beat 1 to m. 72, beat 3 | IV | IV6/4 | predominant (strong) | neighbor | A# | major | F | second | 2 | 0.06 | vi (D minor, score 0.99) |
| m. 72, beat 3 to m. 73, beat 1 | I | I | tonic | - | F | major | F | root position | 2 | 1.00 | - |
| m. 73, beat 1 to m. 73, beat 3 | V | V6 | dominant | - | C | major | E | first | 2 | 0.04 | iii (A minor, score 0.88) |
| m. 73, beat 3 to m. 74, beat 1 | I | I | tonic | - | F | major | F | root position | 2 | 1.00 | - |
| m. 74, beat 1 to m. 75, beat 1 | IV | IV | predominant (strong) | - | A# | major | A# | root position | 4 | 0.12 | minor7 (G minor7, score 0.75) |
| m. 75, beat 1 to m. 76, beat 1 | I | I6 | tonic | - | F | major | A | first | 4 | 0.12 | - |
| m. 76, beat 1 to m. 76, beat 2 | minor7 | ii7 | predominant (strong) | - | G | minor7 | G | root position | 1 | 0.59 | - |
| m. 76, beat 2 to m. 77, beat 1 | vii° | vii°6 | dominant | - | E | diminished | G | first | 3 | 0.11 | V (C major, score 0.69) |
| m. 77, beat 1 to m. 78, beat 1 | I | I | tonic | - | F | major | F | root position | 4 | 0.00 | vi (D minor, score 0.86) |
| m. 78, beat 1 to m. 79, beat 1 | I | I | tonic | - | F | major | F | root position | 4 | 0.12 | minor7 (D minor7, score 0.74) |
| m. 79, beat 1 to m. 80, beat 1 | V | V6 | dominant | - | C | major | E | first | 4 | 0.12 | - |
| m. 80, beat 1 to m. 80, beat 2 | minor7 | vi7 | predominant (weak) | - | D | minor7 | D | root position | 1 | 1.00 | - |
| m. 80, beat 2 to m. 81, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 81, beat 1 to m. 82, beat 1 | V | V | dominant | - | C | major | C | root position | 4 | 0.00 | iii (A minor, score 0.89) |
| m. 82, beat 1 to m. 83, beat 1 | vi | vi6 | predominant (weak) | - | D | minor | F | first | 4 | 0.06 | I (F major, score 0.91) |
| m. 83, beat 1 to m. 83, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 83, beat 3 to m. 83, beat 4 | minor7 | ii7 (in G major) | predominant (strong) | - | A | minor7 | A | root position | 1 | 1.00 | - |
| m. 83, beat 4 to m. 83, beat 4.5 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 83, beat 4.5 to m. 84, beat 1 | dominant7 | V6/5 | dominant | - | D | dominant7 | F# | first | 0.5 | 0.26 | - |
| m. 84, beat 1 to m. 84, beat 2 | I | I | tonic | - | G | major | G | root position | 1 | 1.00 | - |
| m. 84, beat 2 to m. 84, beat 3 | IV | IV6/4 | predominant (strong) | neighbor | C | major | G | second | 1 | 0.35 | - |
| m. 84, beat 3 to m. 84, beat 4 | I | I | tonic | - | G | major | G | root position | 1 | 1.00 | - |
| m. 84, beat 4 to m. 85, beat 1 | IV | IV6/4 | predominant (strong) | neighbor | C | major | G | second | 1 | 0.35 | - |
| m. 85, beat 1 to m. 85, beat 4 | I | I | tonic | - | G | major | G | root position | 3 | 1.00 | - |
| m. 86, beat 1 to m. 87, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 87, beat 1 to m. 88, beat 1 | IV | IV6 | predominant (strong) | - | C | major | E | first | 4 | 0.21 | - |
| m. 88, beat 1 to m. 89, beat 1 | dominant7 | V4/2 (in C major) | dominant | - | G | dominant7 | D | second | 4 | 0.10 | vii° (B diminished, score 0.70) |
| m. 89, beat 1 to m. 90, beat 1 | I | I6 | tonic | - | C | major | E | first | 4 | 0.21 | - |
| m. 90, beat 1 to m. 91, beat 1 | dominant7 | V4/2 | dominant | - | G | dominant7 | D | second | 4 | 0.14 | - |
| m. 91, beat 1 to m. 91, beat 3 | I | I6/4 | tonic | - | C | major | E | first | 2 | 0.09 | iii (E minor, score 0.96) |
| m. 91, beat 3 to m. 92, beat 1 | IV | IV6 | predominant (strong) | - | F | major | F | root position | 2 | 0.26 | - |
| m. 92, beat 1 to m. 92, beat 3 | vii° | vii°6/4 | dominant | - | B | diminished | D | first | 2 | 0.09 | ii (D minor, score 0.96) |
| m. 92, beat 3 to m. 93, beat 1 | iii | iii6 | predominant (weak) | - | E | minor | E | root position | 2 | 0.26 | - |
| m. 93, beat 1 to m. 93, beat 3 | vi | vi6/4 | predominant (weak) | - | A | minor | C | first | 2 | 0.09 | I (C major, score 0.96) |
| m. 93, beat 3 to m. 94, beat 1 | ii | ii6 | predominant (strong) | - | D | minor | D | root position | 2 | 0.26 | - |
| m. 94, beat 1 to m. 94, beat 3 | V | V6/4 | dominant | - | G | major | B | first | 2 | 0.09 | vii° (B diminished, score 0.96) |
| m. 94, beat 3 to m. 95, beat 1 | I | I6 | tonic | - | C | major | C | root position | 2 | 0.32 | - |
| m. 95, beat 1 to m. 96, beat 1 | minor | i (in D minor) | tonic | - | D | minor | F | first | 4 | 1.00 | - |
| m. 96, beat 1 to m. 97, beat 1 | minor | v | dominant | - | A | minor | F# | - | 4 | 1.00 | - |
| m. 97, beat 1 to m. 98, beat 1 | major | VII6/4 | predominant (weak) | - | C | major | G | second | 4 | 0.12 | minor7 (A minor7, score 0.75) |
| m. 98, beat 1 to m. 99, beat 1 | minor7 | iii6/5 (in C major) | predominant (weak) | - | E | minor7 | G | first | 4 | 0.03 | dominant7 (G dominant7, score 0.65) |
| m. 99, beat 1 to m. 99, beat 4 | I | I | tonic | - | C | major | C | root position | 3 | 1.00 | - |
| m. 99, beat 4 to m. 100, beat 1 | dominant7 | V7 | dominant | - | G | dominant7 | G | root position | 1 | 1.00 | - |
| m. 100, beat 1 to m. 100, beat 4 | I | I | tonic | - | C | major | G | second | 3 | 1.00 | - |
| m. 100, beat 4 to m. 101, beat 1 | dominant7 | V7 | dominant | - | G | dominant7 | G | root position | 1 | 1.00 | - |
| m. 101, beat 1 to m. 101, beat 4 | I | I | tonic | - | C | major | C | root position | 3 | 1.00 | - |
| m. 102, beat 1 to m. 103, beat 1 | minor7 | vi4/2 | predominant (weak) | - | A | minor7 | G | third | 4 | 0.10 | V (G major, score 0.64) |
| m. 103, beat 1 to m. 103, beat 4 | V | V | dominant | - | G | major | D | second | 3 | 1.00 | - |
| m. 103, beat 4 to m. 104, beat 1 | minor7 | ii7 | predominant (strong) | - | D | minor7 | D | root position | 1 | 1.00 | - |
| m. 104, beat 1 to m. 105, beat 1 | minor | i (in G minor) | tonic | - | G | minor | G | root position | 4 | 0.00 | diminished (E diminished, score 0.84) |
| m. 105, beat 1 to m. 105, beat 2 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 105, beat 2 to m. 105, beat 3 | major | V | dominant | - | D | major | D | root position | 1 | 0.53 | - |
| m. 105, beat 3 to m. 106, beat 1 | major7 | V7/V | dominant of 5 | - | A# | major7 | A | third | 2 | 0.11 | minor (A minor, score 0.56) |
| m. 106, beat 1 to m. 107, beat 1 | minor | i (in D minor) | tonic | - | D | minor | D | root position | 4 | 0.36 | - |
| m. 107, beat 1 to m. 107, beat 4 | minor | i | tonic | - | D | minor | A | second | 3 | 1.00 | - |
| m. 107, beat 4 to m. 108, beat 1 | dominant7 | V7 | dominant | - | A | dominant7 | A | root position | 1 | 1.00 | - |
| m. 108, beat 1 to m. 109, beat 1 | minor | i | tonic | - | D | minor | D | root position | 4 | 0.00 | diminished (B diminished, score 0.89) |
| m. 109, beat 1 to m. 110, beat 1 | diminished | viio (in A minor) | dominant | - | G# | diminished | D | second | 4 | 0.00 | major (E major, score 0.83) |
| m. 110, beat 1 to m. 111, beat 1 | major | VI6/4 | predominant (weak) | - | F | major | C | second | 4 | 0.07 | minor7 (D minor7, score 0.74) |
| m. 111, beat 1 to m. 112, beat 1 | minor | v6/4 | dominant | - | E | minor | B | second | 4 | 0.07 | major7 (C major7, score 0.74) |
| m. 112, beat 1 to m. 113, beat 1 | minor | iv6/4 | predominant (strong) | - | D | minor | A | second | 4 | 0.07 | half-diminished7 (B half-diminished7, score 0.74) |
| m. 113, beat 1 to m. 113, beat 2 | major | V6 | dominant | - | E | major | G# | first | 1 | 0.07 | diminished (G# diminished, score 0.91) |
| m. 113, beat 2 to m. 113, beat 4 | augmented | III+6/4 | predominant (weak) | - | C | augmented | A | - | 2 | 0.65 | - |
| m. 113, beat 4 to m. 114, beat 1 | major7 | VI6/5 | predominant (weak) | - | F | major7 | E | third | 1 | 0.23 | - |
| m. 114, beat 1 to m. 114, beat 3 | ii | ii6/4 (in F major) | predominant (strong) | - | G | minor | D | second | 2 | 0.00 | IV (A# major, score 0.95) |
| m. 114, beat 3 to m. 115, beat 1 | dominant7 | V7 | dominant | - | C | dominant7 | C | root position | 2 | 0.32 | - |
| m. 115, beat 1 to m. 116, beat 1 | I | I | tonic | - | F | major | F | root position | 4 | 1.00 | - |
| m. 116, beat 1 to m. 116, beat 3 | V | V6/4 | dominant | - | C | major | G | second | 2 | 0.07 | vii° (E diminished, score 0.88) |
| m. 116, beat 3 to m. 117, beat 1 | I | I | tonic | - | F | major | F | root position | 2 | 1.00 | - |
| m. 117, beat 1 to m. 117, beat 3 | IV | IV6/4 | predominant (strong) | neighbor | A# | major | F | second | 2 | 0.06 | vi (D minor, score 0.99) |
| m. 117, beat 3 to m. 118, beat 1 | I | I | tonic | - | F | major | F | root position | 2 | 1.00 | - |
| m. 118, beat 1 to m. 118, beat 3 | V | V6 | dominant | - | C | major | E | first | 2 | 0.04 | iii (A minor, score 0.88) |
| m. 118, beat 3 to m. 119, beat 1 | I | I | tonic | - | F | major | F | root position | 2 | 1.00 | - |
| m. 119, beat 1 to m. 120, beat 1 | IV | IV | predominant (strong) | - | A# | major | A# | root position | 4 | 0.12 | minor7 (G minor7, score 0.75) |
| m. 120, beat 1 to m. 121, beat 1 | I | I6 | tonic | - | F | major | A | first | 4 | 0.12 | - |
| m. 121, beat 1 to m. 121, beat 2 | minor7 | ii7 | predominant (strong) | - | G | minor7 | G | root position | 1 | 0.59 | - |
| m. 121, beat 2 to m. 122, beat 1 | vii° | vii°6 | dominant | - | E | diminished | G | first | 3 | 0.11 | V (C major, score 0.69) |
| m. 122, beat 1 to m. 123, beat 1 | I | I | tonic | - | F | major | F | root position | 4 | 0.00 | vi (D minor, score 0.86) |
| m. 123, beat 1 to m. 124, beat 1 | I | I | tonic | - | F | major | F | root position | 4 | 0.12 | minor7 (D minor7, score 0.74) |
| m. 124, beat 1 to m. 125, beat 1 | V | V6 | dominant | - | C | major | E | first | 4 | 0.12 | - |
| m. 125, beat 1 to m. 125, beat 2 | minor7 | vi7 | predominant (weak) | - | D | minor7 | D | root position | 1 | 1.00 | - |
| m. 125, beat 2 to m. 126, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 126, beat 1 to m. 127, beat 1 | V | V | dominant | - | C | major | C | root position | 4 | 0.00 | iii (A minor, score 0.89) |
| m. 127, beat 1 to m. 128, beat 1 | vi | vi6 | predominant (weak) | - | D | minor | F | first | 4 | 0.06 | I (F major, score 0.91) |
| m. 128, beat 1 to m. 128, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 128, beat 3 to m. 128, beat 4 | minor7 | ii7 (in G major) | predominant (strong) | - | A | minor7 | A | root position | 1 | 1.00 | - |
| m. 128, beat 4 to m. 128, beat 4.5 | ii | ii | predominant (strong) | - | A | minor | A | root position | 0.5 | 1.00 | - |
| m. 128, beat 4.5 to m. 129, beat 1 | dominant7 | V6/5 | dominant | - | D | dominant7 | F# | first | 0.5 | 0.26 | - |
| m. 129, beat 1 to m. 129, beat 2 | I | I | tonic | - | G | major | G | root position | 1 | 1.00 | - |
| m. 129, beat 2 to m. 129, beat 3 | IV | IV6/4 | predominant (strong) | neighbor | C | major | G | second | 1 | 0.35 | - |
| m. 129, beat 3 to m. 129, beat 4 | I | I | tonic | - | G | major | G | root position | 1 | 1.00 | - |
| m. 129, beat 4 to m. 130, beat 1 | IV | IV6/4 | predominant (strong) | neighbor | C | major | G | second | 1 | 0.35 | - |
| m. 130, beat 1 to m. 130, beat 4 | I | I | tonic | - | G | major | G | root position | 3 | 1.00 | - |
| m. 131, beat 1 to m. 132, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 132, beat 1 to m. 133, beat 1 | IV | IV6 | predominant (strong) | - | C | major | E | first | 4 | 0.21 | - |
| m. 133, beat 1 to m. 134, beat 1 | dominant7 | V4/2 (in C major) | dominant | - | G | dominant7 | D | second | 4 | 0.10 | vii° (B diminished, score 0.70) |
| m. 134, beat 1 to m. 135, beat 1 | I | I6 | tonic | - | C | major | E | first | 4 | 0.21 | - |
| m. 135, beat 1 to m. 136, beat 1 | dominant7 | V4/2 | dominant | - | G | dominant7 | D | second | 4 | 0.14 | - |
| m. 136, beat 1 to m. 136, beat 3 | I | I6/4 | tonic | - | C | major | E | first | 2 | 0.09 | iii (E minor, score 0.96) |
| m. 136, beat 3 to m. 137, beat 1 | IV | IV6 | predominant (strong) | - | F | major | F | root position | 2 | 0.26 | - |
| m. 137, beat 1 to m. 137, beat 3 | vii° | vii°6/4 | dominant | - | B | diminished | D | first | 2 | 0.09 | ii (D minor, score 0.96) |
| m. 137, beat 3 to m. 138, beat 1 | iii | iii6 | predominant (weak) | - | E | minor | E | root position | 2 | 0.26 | - |
| m. 138, beat 1 to m. 138, beat 3 | vi | vi6/4 | predominant (weak) | - | A | minor | C | first | 2 | 0.09 | I (C major, score 0.96) |
| m. 138, beat 3 to m. 139, beat 1 | ii | ii6 | predominant (strong) | - | D | minor | D | root position | 2 | 0.26 | - |
| m. 139, beat 1 to m. 139, beat 3 | V | V6/4 | dominant | - | G | major | B | first | 2 | 0.09 | vii° (B diminished, score 0.96) |
| m. 139, beat 3 to m. 140, beat 1 | I | I6 | tonic | - | C | major | C | root position | 2 | 0.32 | - |
| m. 140, beat 1 to m. 141, beat 1 | minor | i (in D minor) | tonic | - | D | minor | F | first | 4 | 1.00 | - |
| m. 141, beat 1 to m. 142, beat 1 | minor | v | dominant | - | A | minor | F# | - | 4 | 1.00 | - |
| m. 142, beat 1 to m. 143, beat 1 | major | VII6/4 | predominant (weak) | - | C | major | G | second | 4 | 0.12 | minor7 (A minor7, score 0.75) |
| m. 143, beat 1 to m. 144, beat 1 | minor7 | iii6/5 (in C major) | predominant (weak) | - | E | minor7 | G | first | 4 | 0.03 | dominant7 (G dominant7, score 0.65) |
| m. 144, beat 1 to m. 144, beat 4 | I | I | tonic | - | C | major | C | root position | 3 | 1.00 | - |
| m. 144, beat 4 to m. 145, beat 1 | dominant7 | V7 | dominant | - | G | dominant7 | G | root position | 1 | 1.00 | - |
| m. 145, beat 1 to m. 145, beat 4 | I | I | tonic | - | C | major | G | second | 3 | 1.00 | - |
| m. 145, beat 4 to m. 146, beat 1 | dominant7 | V7 | dominant | - | G | dominant7 | G | root position | 1 | 1.00 | - |
| m. 146, beat 1 to m. 146, beat 4 | I | I | tonic | - | C | major | C | root position | 3 | 1.00 | - |

## Applied chords

Chords the library reads as applied (secondary) chords: the chord as it reads in the tonicized key, the chord it tonicizes and that chord's tonic. The last column checks each against the tonicization claims above for the same span; where they disagree both are shown and neither is changed.

| Position | Applied chord | Target | Resolution | Tonicization |
| --- | --- | --- | --- | --- |
| m. 60, beat 2 to m. 60, beat 3 | V4/2 (secondary-dominant) | ii (A) | to the target | DISAGREES: no tonicization is reported here |
| m. 60, beat 3 to m. 61, beat 1 | V7 (secondary-dominant) | V (D) | to the target | DISAGREES: no tonicization is reported here |
| m. 105, beat 3 to m. 106, beat 1 | V7 (secondary-dominant) | V (D) | to the target | DISAGREES: the tonicization is of G minor |
## Outer voices

The highest (soprano) and lowest (bass) sounding note of each chord's span, with its scale degree in that chord's local key. A flat or sharp sign marks a chromatic alteration. A degree is "-" where the span lies in no key region. A rest means the library found no note for that line in the span, a bridged one a short gap inside the line, and a register guard a note the library dropped rather than adopt an accompaniment note. Degrees carry the key region's uncertainty in the analysis document.

| Position | Key | Soprano | Degree | Bass | Degree |
| --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 2, beat 1 | C major | C5 | 1 | C4 | 1 |
| m. 2, beat 1 to m. 2, beat 3 | C major | B4 | 7 | D4 | 2 |
| m. 2, beat 3 to m. 3, beat 1 | C major | C5 | 1 | C4 | 1 |
| m. 3, beat 1 to m. 3, beat 3 | C major | A5 | 6 | C4 | 1 |
| m. 3, beat 3 to m. 4, beat 1 | C major | G5 | 5 | C4 | 1 |
| m. 4, beat 1 to m. 4, beat 3 | C major | G5 | 5 | B3 | 7 |
| m. 4, beat 3 to m. 5, beat 1 | C major | E5 | 3 | C4 | 1 |
| m. 5, beat 1 to m. 6, beat 1 | C major | C5 | 1 | F4 | 4 |
| m. 6, beat 1 to m. 7, beat 1 | C major | B4 | 7 | E3 | 3 |
| m. 7, beat 1 to m. 7, beat 2 | C major | A4 | 6 | D3 | 2 |
| m. 7, beat 2 to m. 8, beat 1 | C major | E5 | 3 | rest (register guard) | - |
| m. 8, beat 1 to m. 9, beat 1 | F major | G4 | 2 | C3 | 5 |
| m. 9, beat 1 to m. 10, beat 1 | F major | F4 | 1 | F3 | 1 |
| m. 10, beat 1 to m. 10, beat 3 | G major | C6 | 4 | F3 | b7 |
| m. 10, beat 3 to m. 10, beat 4 | G major | A5 | 2 | A3 | 2 |
| m. 10, beat 4 to m. 11, beat 1 | G major | F5 | b7 | F#3 | 7 |
| m. 11, beat 1 to m. 11, beat 2 | G major | G5 | 1 | G2 | 1 |
| m. 11, beat 2 to m. 11, beat 3 | G major | E5 | 6 | G2 | 1 |
| m. 11, beat 3 to m. 11, beat 4 | G major | G5 | 1 | G2 | 1 |
| m. 11, beat 4 to m. 12, beat 1 | G major | E5 | 6 | G2 | 1 |
| m. 12, beat 1 to m. 12, beat 4 | G major | D5 | 5 | G2 | 1 |
| m. 13, beat 1 to m. 14, beat 1 | G major | D4 | 5 | C#4 | #4 |
| m. 14, beat 1 to m. 15, beat 1 | G major | D6 | 5 | B3 | 3 |
| m. 15, beat 1 to m. 16, beat 1 | G major | A5 | 2 | B3 | 3 |
| m. 16, beat 1 to m. 17, beat 1 | G major | D6 | 5 | B3 | 3 |
| m. 17, beat 1 to m. 18, beat 1 | G major | A5 | 2 | B3 | 3 |
| m. 18, beat 1 to m. 18, beat 3 | G major | D6 | 5 | B3 | 3 |
| m. 18, beat 3 to m. 19, beat 1 | G major | G5 | 1 | C4 | 4 |
| m. 19, beat 1 to m. 19, beat 3 | G major | C6 | 4 | A3 | 2 |
| m. 19, beat 3 to m. 20, beat 1 | G major | F#5 | 7 | B3 | 3 |
| m. 20, beat 1 to m. 20, beat 3 | G major | B5 | 3 | G3 | 1 |
| m. 20, beat 3 to m. 21, beat 1 | G major | E5 | 6 | A3 | 2 |
| m. 21, beat 1 to m. 21, beat 3 | G major | A5 | 2 | F#3 | 7 |
| m. 21, beat 3 to m. 22, beat 1 | G major | G5 | 1 | G3 | 1 |
| m. 22, beat 1 to m. 23, beat 1 | A minor | A4 | 1 | C4 | 3 |
| m. 23, beat 1 to m. 24, beat 1 | A minor | A5 | 1 | C4 | 3 |
| m. 24, beat 1 to m. 25, beat 1 | A minor | D6 | 4 | D4 | 4 |
| m. 25, beat 1 to m. 26, beat 1 | G major | B5 | 3 | D4 | 5 |
| m. 26, beat 1 to m. 26, beat 4 | G major | G5 | 1 | G4 | 1 |
| m. 26, beat 4 to m. 27, beat 1 | G major | C6 | 4 | D4 | 5 |
| m. 27, beat 1 to m. 27, beat 4 | G major | G5 | 1 | G4 | 1 |
| m. 27, beat 4 to m. 28, beat 1 | G major | C5 | 4 | D3 | 5 |
| m. 28, beat 1 to m. 28, beat 4 | G major | G4 | 1 | G3 | 1 |
| m. 29, beat 1 to m. 30, beat 1 | C major | C5 | 1 | C4 | 1 |
| m. 30, beat 1 to m. 30, beat 3 | C major | B4 | 7 | D4 | 2 |
| m. 30, beat 3 to m. 31, beat 1 | C major | C5 | 1 | C4 | 1 |
| m. 31, beat 1 to m. 31, beat 3 | C major | A5 | 6 | C4 | 1 |
| m. 31, beat 3 to m. 32, beat 1 | C major | G5 | 5 | C4 | 1 |
| m. 32, beat 1 to m. 32, beat 3 | C major | G5 | 5 | B3 | 7 |
| m. 32, beat 3 to m. 33, beat 1 | C major | E5 | 3 | C4 | 1 |
| m. 33, beat 1 to m. 34, beat 1 | C major | C5 | 1 | F4 | 4 |
| m. 34, beat 1 to m. 35, beat 1 | C major | B4 | 7 | E3 | 3 |
| m. 35, beat 1 to m. 35, beat 2 | C major | A4 | 6 | D3 | 2 |
| m. 35, beat 2 to m. 36, beat 1 | C major | E5 | 3 | rest (register guard) | - |
| m. 36, beat 1 to m. 37, beat 1 | F major | G4 | 2 | C3 | 5 |
| m. 37, beat 1 to m. 38, beat 1 | F major | F4 | 1 | F3 | 1 |
| m. 38, beat 1 to m. 38, beat 3 | G major | C6 | 4 | F3 | b7 |
| m. 38, beat 3 to m. 38, beat 4 | G major | A5 | 2 | A3 | 2 |
| m. 38, beat 4 to m. 39, beat 1 | G major | F5 | b7 | F#3 | 7 |
| m. 39, beat 1 to m. 39, beat 2 | G major | G5 | 1 | G2 | 1 |
| m. 39, beat 2 to m. 39, beat 3 | G major | E5 | 6 | G2 | 1 |
| m. 39, beat 3 to m. 39, beat 4 | G major | G5 | 1 | G2 | 1 |
| m. 39, beat 4 to m. 40, beat 1 | G major | E5 | 6 | G2 | 1 |
| m. 40, beat 1 to m. 40, beat 4 | G major | D5 | 5 | G2 | 1 |
| m. 41, beat 1 to m. 42, beat 1 | G major | D4 | 5 | C#4 | #4 |
| m. 42, beat 1 to m. 43, beat 1 | G major | D6 | 5 | B3 | 3 |
| m. 43, beat 1 to m. 44, beat 1 | G major | A5 | 2 | B3 | 3 |
| m. 44, beat 1 to m. 45, beat 1 | G major | D6 | 5 | B3 | 3 |
| m. 45, beat 1 to m. 46, beat 1 | G major | A5 | 2 | B3 | 3 |
| m. 46, beat 1 to m. 46, beat 3 | G major | D6 | 5 | B3 | 3 |
| m. 46, beat 3 to m. 47, beat 1 | G major | G5 | 1 | C4 | 4 |
| m. 47, beat 1 to m. 47, beat 3 | G major | C6 | 4 | A3 | 2 |
| m. 47, beat 3 to m. 48, beat 1 | G major | F#5 | 7 | B3 | 3 |
| m. 48, beat 1 to m. 48, beat 3 | G major | B5 | 3 | G3 | 1 |
| m. 48, beat 3 to m. 49, beat 1 | G major | E5 | 6 | A3 | 2 |
| m. 49, beat 1 to m. 49, beat 3 | G major | A5 | 2 | F#3 | 7 |
| m. 49, beat 3 to m. 50, beat 1 | G major | G5 | 1 | G3 | 1 |
| m. 50, beat 1 to m. 51, beat 1 | A minor | A4 | 1 | C4 | 3 |
| m. 51, beat 1 to m. 52, beat 1 | A minor | A5 | 1 | C4 | 3 |
| m. 52, beat 1 to m. 53, beat 1 | A minor | D6 | 4 | D4 | 4 |
| m. 53, beat 1 to m. 54, beat 1 | G major | B5 | 3 | D4 | 5 |
| m. 54, beat 1 to m. 54, beat 4 | G major | G5 | 1 | G4 | 1 |
| m. 54, beat 4 to m. 55, beat 1 | G major | C6 | 4 | D4 | 5 |
| m. 55, beat 1 to m. 55, beat 4 | G major | G5 | 1 | G4 | 1 |
| m. 55, beat 4 to m. 56, beat 1 | G major | C5 | 4 | D3 | 5 |
| m. 56, beat 1 to m. 56, beat 4 | G major | G4 | 1 | G3 | 1 |
| m. 57, beat 1 to m. 58, beat 1 | G major | G4 | 1 | G2 | 1 |
| m. 58, beat 1 to m. 58, beat 4 | G major | G5 | 1 | G4 | 1 |
| m. 58, beat 4 to m. 59, beat 1 | G major | C5 | 4 | D3 | 5 |
| m. 59, beat 1 to m. 60, beat 1 | G major | G4 | 1 | G2 | 1 |
| m. 60, beat 1 to m. 60, beat 2 | G major | C#5 | #4 | A2 | 2 |
| m. 60, beat 2 to m. 60, beat 3 | G major | rest (register guard) | - | D3 | 5 |
| m. 60, beat 3 to m. 61, beat 1 | G major | E6 | 6 | A3 | 2 |
| m. 61, beat 1 to m. 62, beat 1 | D minor | F5 | 3 | D2 | 1 |
| m. 62, beat 1 to m. 62, beat 4 | D minor | D5 | 1 | D4 | 1 |
| m. 62, beat 4 to m. 63, beat 1 | D minor | G4 | 4 | A2 | 5 |
| m. 63, beat 1 to m. 64, beat 1 | D minor | F4 | 3 | D3 | 1 |
| m. 64, beat 1 to m. 65, beat 1 | A minor | D5 | 4 | G#3 | #7 |
| m. 65, beat 1 to m. 66, beat 1 | A minor | E6 | 5 | C3 | 3 |
| m. 66, beat 1 to m. 67, beat 1 | A minor | D6 | 4 | B3 | 2 |
| m. 67, beat 1 to m. 68, beat 1 | A minor | C6 | 3 | A3 | 1 |
| m. 68, beat 1 to m. 68, beat 2 | A minor | B5 | 2 | G#3 | #7 |
| m. 68, beat 2 to m. 68, beat 4 | A minor | G#5 | #7 | rest (register guard) | - |
| m. 68, beat 4 to m. 69, beat 1 | A minor | rest (register guard) | - | E3 | 5 |
| m. 69, beat 1 to m. 69, beat 3 | F major | D5 | 6 | D3 | 6 |
| m. 69, beat 3 to m. 70, beat 1 | F major | A4 | 3 | C3 | 5 |
| m. 70, beat 1 to m. 71, beat 1 | F major | F5 | 1 | rest (register guard) | - |
| m. 71, beat 1 to m. 71, beat 3 | F major | E5 | 7 | G4 | 2 |
| m. 71, beat 3 to m. 72, beat 1 | F major | F5 | 1 | F4 | 1 |
| m. 72, beat 1 to m. 72, beat 3 | F major | D6 | 6 | F4 | 1 |
| m. 72, beat 3 to m. 73, beat 1 | F major | C6 | 5 | F4 | 1 |
| m. 73, beat 1 to m. 73, beat 3 | F major | C6 | 5 | E4 | 7 |
| m. 73, beat 3 to m. 74, beat 1 | F major | A5 | 3 | F4 | 1 |
| m. 74, beat 1 to m. 75, beat 1 | F major | F5 | 1 | A#4 | 4 |
| m. 75, beat 1 to m. 76, beat 1 | F major | E5 | 7 | A3 | 3 |
| m. 76, beat 1 to m. 76, beat 2 | F major | D5 | 6 | G3 | 2 |
| m. 76, beat 2 to m. 77, beat 1 | F major | A5 | 3 | rest (register guard) | - |
| m. 77, beat 1 to m. 78, beat 1 | F major | C5 | 5 | F3 | 1 |
| m. 78, beat 1 to m. 79, beat 1 | F major | A5 | 3 | F3 | 1 |
| m. 79, beat 1 to m. 80, beat 1 | F major | G5 | 2 | E3 | 7 |
| m. 80, beat 1 to m. 80, beat 2 | F major | F5 | 1 | D3 | 6 |
| m. 80, beat 2 to m. 81, beat 1 | F major | rest (register guard) | - | G3 | 2 |
| m. 81, beat 1 to m. 82, beat 1 | F major | E5 | 7 | C3 | 5 |
| m. 82, beat 1 to m. 83, beat 1 | F major | D5 | 6 | F3 | 1 |
| m. 83, beat 1 to m. 83, beat 3 | G major | C6 | 4 | F3 | b7 |
| m. 83, beat 3 to m. 83, beat 4 | G major | A5 | 2 | A3 | 2 |
| m. 83, beat 4 to m. 83, beat 4.5 | G major | F5 | b7 | A3 | 2 |
| m. 83, beat 4.5 to m. 84, beat 1 | G major | D5 | 5 | F#3 | 7 |
| m. 84, beat 1 to m. 84, beat 2 | G major | G5 | 1 | G2 | 1 |
| m. 84, beat 2 to m. 84, beat 3 | G major | E5 | 6 | G2 | 1 |
| m. 84, beat 3 to m. 84, beat 4 | G major | G5 | 1 | G2 | 1 |
| m. 84, beat 4 to m. 85, beat 1 | G major | E5 | 6 | G2 | 1 |
| m. 85, beat 1 to m. 85, beat 4 | G major | D5 | 5 | G2 | 1 |
| m. 86, beat 1 to m. 87, beat 1 | G major | G4 | 1 | F#4 | 7 |
| m. 87, beat 1 to m. 88, beat 1 | G major | G5 | 1 | E4 | 6 |
| m. 88, beat 1 to m. 89, beat 1 | C major | D5 | 2 | E4 | 3 |
| m. 89, beat 1 to m. 90, beat 1 | C major | G5 | 5 | E4 | 3 |
| m. 90, beat 1 to m. 91, beat 1 | C major | D5 | 2 | E4 | 3 |
| m. 91, beat 1 to m. 91, beat 3 | C major | G5 | 5 | E3 | 3 |
| m. 91, beat 3 to m. 92, beat 1 | C major | C5 | 1 | F3 | 4 |
| m. 92, beat 1 to m. 92, beat 3 | C major | F5 | 4 | D3 | 2 |
| m. 92, beat 3 to m. 93, beat 1 | C major | B5 | 7 | E3 | 3 |
| m. 93, beat 1 to m. 93, beat 3 | C major | E6 | 3 | C4 | 1 |
| m. 93, beat 3 to m. 94, beat 1 | C major | A5 | 6 | D4 | 2 |
| m. 94, beat 1 to m. 94, beat 3 | C major | D6 | 2 | B3 | 7 |
| m. 94, beat 3 to m. 95, beat 1 | C major | C6 | 1 | C4 | 1 |
| m. 95, beat 1 to m. 96, beat 1 | D minor | D5 | 1 | D5 | 1 |
| m. 96, beat 1 to m. 97, beat 1 | D minor | A5 | 5 | rest (bridged) | - |
| m. 97, beat 1 to m. 98, beat 1 | D minor | B5 | #6 | G3 | 4 |
| m. 98, beat 1 to m. 99, beat 1 | C major | E5 | 3 | G3 | 5 |
| m. 99, beat 1 to m. 99, beat 4 | C major | C5 | 1 | C4 | 1 |
| m. 99, beat 4 to m. 100, beat 1 | C major | F5 | 4 | G3 | 5 |
| m. 100, beat 1 to m. 100, beat 4 | C major | C5 | 1 | C4 | 1 |
| m. 100, beat 4 to m. 101, beat 1 | C major | F4 | 4 | G2 | 5 |
| m. 101, beat 1 to m. 101, beat 4 | C major | C4 | 1 | C3 | 1 |
| m. 102, beat 1 to m. 103, beat 1 | C major | G4 | 5 | G2 | 5 |
| m. 103, beat 1 to m. 103, beat 4 | C major | G5 | 5 | G4 | 5 |
| m. 103, beat 4 to m. 104, beat 1 | C major | C5 | 1 | D3 | 2 |
| m. 104, beat 1 to m. 105, beat 1 | G minor | G4 | 1 | G2 | 1 |
| m. 105, beat 1 to m. 105, beat 2 | G minor | C#5 | #4 | A2 | 2 |
| m. 105, beat 2 to m. 105, beat 3 | G minor | rest (register guard) | - | D3 | 5 |
| m. 105, beat 3 to m. 106, beat 1 | G minor | E6 | #6 | A3 | 2 |
| m. 106, beat 1 to m. 107, beat 1 | D minor | F5 | 3 | D2 | 1 |
| m. 107, beat 1 to m. 107, beat 4 | D minor | D5 | 1 | D4 | 1 |
| m. 107, beat 4 to m. 108, beat 1 | D minor | G4 | 4 | A2 | 5 |
| m. 108, beat 1 to m. 109, beat 1 | D minor | F4 | 3 | D3 | 1 |
| m. 109, beat 1 to m. 110, beat 1 | A minor | D5 | 4 | G#3 | #7 |
| m. 110, beat 1 to m. 111, beat 1 | A minor | E6 | 5 | C3 | 3 |
| m. 111, beat 1 to m. 112, beat 1 | A minor | D6 | 4 | B3 | 2 |
| m. 112, beat 1 to m. 113, beat 1 | A minor | C6 | 3 | A3 | 1 |
| m. 113, beat 1 to m. 113, beat 2 | A minor | B5 | 2 | G#3 | #7 |
| m. 113, beat 2 to m. 113, beat 4 | A minor | G#5 | #7 | rest (register guard) | - |
| m. 113, beat 4 to m. 114, beat 1 | A minor | rest (register guard) | - | E3 | 5 |
| m. 114, beat 1 to m. 114, beat 3 | F major | D5 | 6 | D3 | 6 |
| m. 114, beat 3 to m. 115, beat 1 | F major | A4 | 3 | C3 | 5 |
| m. 115, beat 1 to m. 116, beat 1 | F major | F5 | 1 | rest (register guard) | - |
| m. 116, beat 1 to m. 116, beat 3 | F major | E5 | 7 | G4 | 2 |
| m. 116, beat 3 to m. 117, beat 1 | F major | F5 | 1 | F4 | 1 |
| m. 117, beat 1 to m. 117, beat 3 | F major | D6 | 6 | F4 | 1 |
| m. 117, beat 3 to m. 118, beat 1 | F major | C6 | 5 | F4 | 1 |
| m. 118, beat 1 to m. 118, beat 3 | F major | C6 | 5 | E4 | 7 |
| m. 118, beat 3 to m. 119, beat 1 | F major | A5 | 3 | F4 | 1 |
| m. 119, beat 1 to m. 120, beat 1 | F major | F5 | 1 | A#4 | 4 |
| m. 120, beat 1 to m. 121, beat 1 | F major | E5 | 7 | A3 | 3 |
| m. 121, beat 1 to m. 121, beat 2 | F major | D5 | 6 | G3 | 2 |
| m. 121, beat 2 to m. 122, beat 1 | F major | A5 | 3 | rest (register guard) | - |
| m. 122, beat 1 to m. 123, beat 1 | F major | C5 | 5 | F3 | 1 |
| m. 123, beat 1 to m. 124, beat 1 | F major | A5 | 3 | F3 | 1 |
| m. 124, beat 1 to m. 125, beat 1 | F major | G5 | 2 | E3 | 7 |
| m. 125, beat 1 to m. 125, beat 2 | F major | F5 | 1 | D3 | 6 |
| m. 125, beat 2 to m. 126, beat 1 | F major | rest (register guard) | - | G3 | 2 |
| m. 126, beat 1 to m. 127, beat 1 | F major | E5 | 7 | C3 | 5 |
| m. 127, beat 1 to m. 128, beat 1 | F major | D5 | 6 | F3 | 1 |
| m. 128, beat 1 to m. 128, beat 3 | G major | C6 | 4 | F3 | b7 |
| m. 128, beat 3 to m. 128, beat 4 | G major | A5 | 2 | A3 | 2 |
| m. 128, beat 4 to m. 128, beat 4.5 | G major | F5 | b7 | A3 | 2 |
| m. 128, beat 4.5 to m. 129, beat 1 | G major | D5 | 5 | F#3 | 7 |
| m. 129, beat 1 to m. 129, beat 2 | G major | G5 | 1 | G2 | 1 |
| m. 129, beat 2 to m. 129, beat 3 | G major | E5 | 6 | G2 | 1 |
| m. 129, beat 3 to m. 129, beat 4 | G major | G5 | 1 | G2 | 1 |
| m. 129, beat 4 to m. 130, beat 1 | G major | E5 | 6 | G2 | 1 |
| m. 130, beat 1 to m. 130, beat 4 | G major | D5 | 5 | G2 | 1 |
| m. 131, beat 1 to m. 132, beat 1 | G major | G4 | 1 | F#4 | 7 |
| m. 132, beat 1 to m. 133, beat 1 | G major | G5 | 1 | E4 | 6 |
| m. 133, beat 1 to m. 134, beat 1 | C major | D5 | 2 | E4 | 3 |
| m. 134, beat 1 to m. 135, beat 1 | C major | G5 | 5 | E4 | 3 |
| m. 135, beat 1 to m. 136, beat 1 | C major | D5 | 2 | E4 | 3 |
| m. 136, beat 1 to m. 136, beat 3 | C major | G5 | 5 | E3 | 3 |
| m. 136, beat 3 to m. 137, beat 1 | C major | C5 | 1 | F3 | 4 |
| m. 137, beat 1 to m. 137, beat 3 | C major | F5 | 4 | D3 | 2 |
| m. 137, beat 3 to m. 138, beat 1 | C major | B5 | 7 | E3 | 3 |
| m. 138, beat 1 to m. 138, beat 3 | C major | E6 | 3 | C4 | 1 |
| m. 138, beat 3 to m. 139, beat 1 | C major | A5 | 6 | D4 | 2 |
| m. 139, beat 1 to m. 139, beat 3 | C major | D6 | 2 | B3 | 7 |
| m. 139, beat 3 to m. 140, beat 1 | C major | C6 | 1 | C4 | 1 |
| m. 140, beat 1 to m. 141, beat 1 | D minor | D5 | 1 | D5 | 1 |
| m. 141, beat 1 to m. 142, beat 1 | D minor | A5 | 5 | rest (bridged) | - |
| m. 142, beat 1 to m. 143, beat 1 | D minor | B5 | #6 | G3 | 4 |
| m. 143, beat 1 to m. 144, beat 1 | C major | E5 | 3 | G3 | 5 |
| m. 144, beat 1 to m. 144, beat 4 | C major | C5 | 1 | C4 | 1 |
| m. 144, beat 4 to m. 145, beat 1 | C major | F5 | 4 | G3 | 5 |
| m. 145, beat 1 to m. 145, beat 4 | C major | C5 | 1 | C4 | 1 |
| m. 145, beat 4 to m. 146, beat 1 | C major | F4 | 4 | G2 | 5 |
| m. 146, beat 1 to m. 146, beat 4 | C major | C4 | 1 | C3 | 1 |

## Chord vocabulary

Each distinct Roman numeral of the chords the library numbered, for the whole piece and for each key region, sorted by count, then total length, then first appearance. Count is the number of chord rows with that numeral (every row in the Chords table is one). Runs counts a stretch of adjacent identical numerals in one key region once. Beats is the total length, Share its fraction of the scope's numbered chord length, and First where it first appears. Applied chords are listed on their own. Chords without a numeral are not counted.

### Overall

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 45 | 43 | 115 | 21.1% | m. 1, beat 1 |
| I6 | 14 | 14 | 48 | 8.8% | m. 6, beat 1 |
| IV6/4 | 13 | 13 | 20 | 3.7% | m. 3, beat 1 |
| V7 | 13 | 13 | 15 | 2.8% | m. 26, beat 4 |
| V6 | 10 | 10 | 20 | 3.7% | m. 4, beat 1 |
| i | 9 | 7 | 34 | 6.2% | m. 61, beat 1 |
| ii7 | 9 | 9 | 9 | 1.7% | m. 7, beat 1 |
| V4/2 | 8 | 8 | 32 | 5.9% | m. 15, beat 1 |
| V6/4 | 8 | 8 | 16 | 2.9% | m. 2, beat 1 |
| V | 6 | 6 | 20 | 3.7% | m. 8, beat 1 |
| IV6 | 6 | 6 | 16 | 2.9% | m. 18, beat 3 |
| IV | 4 | 4 | 16 | 2.9% | m. 5, beat 1 |
| vi6 | 4 | 4 | 16 | 2.9% | m. 9, beat 1 |
| i6 | 4 | 2 | 16 | 2.9% | m. 22, beat 1 |
| VII6/4 | 4 | 4 | 16 | 2.9% | m. 24, beat 1 |
| iii6/5 | 4 | 4 | 16 | 2.9% | m. 25, beat 1 |
| vii°6 | 4 | 4 | 12 | 2.2% | m. 7, beat 2 |
| I6/4 | 4 | 4 | 8 | 1.5% | m. 18, beat 1 |
| vii°6/4 | 4 | 4 | 8 | 1.5% | m. 19, beat 1 |
| iii6 | 4 | 4 | 8 | 1.5% | m. 19, beat 3 |
| vi6/4 | 4 | 4 | 8 | 1.5% | m. 20, beat 1 |
| ii6 | 4 | 4 | 8 | 1.5% | m. 20, beat 3 |
| viio | 2 | 2 | 8 | 1.5% | m. 64, beat 1 |
| VI6/4 | 2 | 2 | 8 | 1.5% | m. 65, beat 1 |
| v6/4 | 2 | 2 | 8 | 1.5% | m. 66, beat 1 |
| iv6/4 | 2 | 2 | 8 | 1.5% | m. 67, beat 1 |
| v | 2 | 2 | 8 | 1.5% | m. 96, beat 1 |
| III+6/4 | 2 | 2 | 4 | 0.7% | m. 68, beat 2 |
| ii6/4 | 2 | 2 | 4 | 0.7% | m. 69, beat 1 |
| VI6/5 | 2 | 2 | 2 | 0.4% | m. 68, beat 4 |
| vi7 | 2 | 2 | 2 | 0.4% | m. 80, beat 1 |
| ii | 2 | 2 | 1 | 0.2% | m. 83, beat 4 |
| V6/5 | 2 | 2 | 1 | 0.2% | m. 83, beat 4.5 |
| ii4/2 | 1 | 1 | 4 | 0.7% | m. 57, beat 1 |
| vi4/2 | 1 | 1 | 4 | 0.7% | m. 102, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 63 | 61 | 171 | 31.4% | m. 1, beat 1 |
| V | 24 | 24 | 56 | 10.3% | m. 2, beat 1 |
| IV | 23 | 23 | 52 | 9.6% | m. 3, beat 1 |
| V7 | 23 | 23 | 48 | 8.8% | m. 15, beat 1 |
| i | 13 | 9 | 50 | 9.2% | m. 22, beat 1 |
| ii7 | 10 | 10 | 13 | 2.4% | m. 7, beat 1 |
| vi | 8 | 8 | 24 | 4.4% | m. 9, beat 1 |
| vii° | 8 | 8 | 20 | 3.7% | m. 7, beat 2 |
| ii | 8 | 8 | 13 | 2.4% | m. 20, beat 3 |
| VII | 4 | 4 | 16 | 2.9% | m. 24, beat 1 |
| iii7 | 4 | 4 | 16 | 2.9% | m. 25, beat 1 |
| v | 4 | 4 | 16 | 2.9% | m. 66, beat 1 |
| iii | 4 | 4 | 8 | 1.5% | m. 19, beat 3 |
| vi7 | 3 | 3 | 6 | 1.1% | m. 80, beat 1 |
| viio | 2 | 2 | 8 | 1.5% | m. 64, beat 1 |
| VI | 2 | 2 | 8 | 1.5% | m. 65, beat 1 |
| iv | 2 | 2 | 8 | 1.5% | m. 67, beat 1 |
| III+ | 2 | 2 | 4 | 0.7% | m. 68, beat 2 |
| VI7 | 2 | 2 | 2 | 0.4% | m. 68, beat 4 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/V | 2 | 2 | 4 | 0.7% | m. 60, beat 3 |
| V4/2/ii | 1 | 1 | 1 | 0.2% | m. 60, beat 2 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/V | 2 | 2 | 4 | 0.7% | m. 60, beat 3 |
| V7/ii | 1 | 1 | 1 | 0.2% | m. 60, beat 2 |

### C major (region 0)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 4 | 4 | 10 | 35.7% | m. 1, beat 1 |
| IV | 1 | 1 | 4 | 14.3% | m. 5, beat 1 |
| I6 | 1 | 1 | 4 | 14.3% | m. 6, beat 1 |
| vii°6 | 1 | 1 | 3 | 10.7% | m. 7, beat 2 |
| V6/4 | 1 | 1 | 2 | 7.1% | m. 2, beat 1 |
| IV6/4 | 1 | 1 | 2 | 7.1% | m. 3, beat 1 |
| V6 | 1 | 1 | 2 | 7.1% | m. 4, beat 1 |
| ii7 | 1 | 1 | 1 | 3.6% | m. 7, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 5 | 5 | 14 | 50.0% | m. 1, beat 1 |
| IV | 2 | 2 | 6 | 21.4% | m. 3, beat 1 |
| V | 2 | 2 | 4 | 14.3% | m. 2, beat 1 |
| vii° | 1 | 1 | 3 | 10.7% | m. 7, beat 2 |
| ii7 | 1 | 1 | 1 | 3.6% | m. 7, beat 1 |

### F major (region 1)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V | 1 | 1 | 4 | 50.0% | m. 8, beat 1 |
| vi6 | 1 | 1 | 4 | 50.0% | m. 9, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V | 1 | 1 | 4 | 50.0% | m. 8, beat 1 |
| vi | 1 | 1 | 4 | 50.0% | m. 9, beat 1 |

### G major (region 2)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I6 | 3 | 3 | 10 | 24.4% | m. 14, beat 1 |
| I | 3 | 3 | 5 | 12.2% | m. 11, beat 1 |
| V4/2 | 2 | 2 | 8 | 19.5% | m. 15, beat 1 |
| IV6/4 | 2 | 2 | 2 | 4.9% | m. 11, beat 2 |
| I6/4 | 1 | 1 | 2 | 4.9% | m. 18, beat 1 |
| IV6 | 1 | 1 | 2 | 4.9% | m. 18, beat 3 |
| vii°6/4 | 1 | 1 | 2 | 4.9% | m. 19, beat 1 |
| iii6 | 1 | 1 | 2 | 4.9% | m. 19, beat 3 |
| vi6/4 | 1 | 1 | 2 | 4.9% | m. 20, beat 1 |
| ii6 | 1 | 1 | 2 | 4.9% | m. 20, beat 3 |
| V6/4 | 1 | 1 | 2 | 4.9% | m. 21, beat 1 |
| ii7 | 1 | 1 | 1 | 2.4% | m. 10, beat 3 |
| V6 | 1 | 1 | 1 | 2.4% | m. 10, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 7 | 7 | 17 | 41.5% | m. 11, beat 1 |
| IV | 3 | 3 | 4 | 9.8% | m. 11, beat 2 |
| V7 | 2 | 2 | 8 | 19.5% | m. 15, beat 1 |
| V | 2 | 2 | 3 | 7.3% | m. 10, beat 4 |
| vii° | 1 | 1 | 2 | 4.9% | m. 19, beat 1 |
| iii | 1 | 1 | 2 | 4.9% | m. 19, beat 3 |
| vi | 1 | 1 | 2 | 4.9% | m. 20, beat 1 |
| ii | 1 | 1 | 2 | 4.9% | m. 20, beat 3 |
| ii7 | 1 | 1 | 1 | 2.4% | m. 10, beat 3 |

### A minor (region 3)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i6 | 2 | 1 | 8 | 66.7% | m. 22, beat 1 |
| VII6/4 | 1 | 1 | 4 | 33.3% | m. 24, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 2 | 1 | 8 | 66.7% | m. 22, beat 1 |
| VII | 1 | 1 | 4 | 33.3% | m. 24, beat 1 |

### G major (region 4)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 9 | 60.0% | m. 26, beat 1 |
| V7 | 2 | 2 | 2 | 13.3% | m. 26, beat 4 |
| iii6/5 | 1 | 1 | 4 | 26.7% | m. 25, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 9 | 60.0% | m. 26, beat 1 |
| V7 | 2 | 2 | 2 | 13.3% | m. 26, beat 4 |
| iii7 | 1 | 1 | 4 | 26.7% | m. 25, beat 1 |

### C major (region 5)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 4 | 4 | 10 | 35.7% | m. 29, beat 1 |
| IV | 1 | 1 | 4 | 14.3% | m. 33, beat 1 |
| I6 | 1 | 1 | 4 | 14.3% | m. 34, beat 1 |
| vii°6 | 1 | 1 | 3 | 10.7% | m. 35, beat 2 |
| V6/4 | 1 | 1 | 2 | 7.1% | m. 30, beat 1 |
| IV6/4 | 1 | 1 | 2 | 7.1% | m. 31, beat 1 |
| V6 | 1 | 1 | 2 | 7.1% | m. 32, beat 1 |
| ii7 | 1 | 1 | 1 | 3.6% | m. 35, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 5 | 5 | 14 | 50.0% | m. 29, beat 1 |
| IV | 2 | 2 | 6 | 21.4% | m. 31, beat 1 |
| V | 2 | 2 | 4 | 14.3% | m. 30, beat 1 |
| vii° | 1 | 1 | 3 | 10.7% | m. 35, beat 2 |
| ii7 | 1 | 1 | 1 | 3.6% | m. 35, beat 1 |

### F major (region 6)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V | 1 | 1 | 4 | 50.0% | m. 36, beat 1 |
| vi6 | 1 | 1 | 4 | 50.0% | m. 37, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V | 1 | 1 | 4 | 50.0% | m. 36, beat 1 |
| vi | 1 | 1 | 4 | 50.0% | m. 37, beat 1 |

### G major (region 7)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I6 | 3 | 3 | 10 | 24.4% | m. 42, beat 1 |
| I | 3 | 3 | 5 | 12.2% | m. 39, beat 1 |
| V4/2 | 2 | 2 | 8 | 19.5% | m. 43, beat 1 |
| IV6/4 | 2 | 2 | 2 | 4.9% | m. 39, beat 2 |
| I6/4 | 1 | 1 | 2 | 4.9% | m. 46, beat 1 |
| IV6 | 1 | 1 | 2 | 4.9% | m. 46, beat 3 |
| vii°6/4 | 1 | 1 | 2 | 4.9% | m. 47, beat 1 |
| iii6 | 1 | 1 | 2 | 4.9% | m. 47, beat 3 |
| vi6/4 | 1 | 1 | 2 | 4.9% | m. 48, beat 1 |
| ii6 | 1 | 1 | 2 | 4.9% | m. 48, beat 3 |
| V6/4 | 1 | 1 | 2 | 4.9% | m. 49, beat 1 |
| ii7 | 1 | 1 | 1 | 2.4% | m. 38, beat 3 |
| V6 | 1 | 1 | 1 | 2.4% | m. 38, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 7 | 7 | 17 | 41.5% | m. 39, beat 1 |
| IV | 3 | 3 | 4 | 9.8% | m. 39, beat 2 |
| V7 | 2 | 2 | 8 | 19.5% | m. 43, beat 1 |
| V | 2 | 2 | 3 | 7.3% | m. 38, beat 4 |
| vii° | 1 | 1 | 2 | 4.9% | m. 47, beat 1 |
| iii | 1 | 1 | 2 | 4.9% | m. 47, beat 3 |
| vi | 1 | 1 | 2 | 4.9% | m. 48, beat 1 |
| ii | 1 | 1 | 2 | 4.9% | m. 48, beat 3 |
| ii7 | 1 | 1 | 1 | 2.4% | m. 38, beat 3 |

### A minor (region 8)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i6 | 2 | 1 | 8 | 66.7% | m. 50, beat 1 |
| VII6/4 | 1 | 1 | 4 | 33.3% | m. 52, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 2 | 1 | 8 | 66.7% | m. 50, beat 1 |
| VII | 1 | 1 | 4 | 33.3% | m. 52, beat 1 |

### G major (region 9)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 4 | 4 | 12 | 40.0% | m. 54, beat 1 |
| V7 | 3 | 3 | 3 | 10.0% | m. 54, beat 4 |
| iii6/5 | 1 | 1 | 4 | 13.3% | m. 53, beat 1 |
| ii4/2 | 1 | 1 | 4 | 13.3% | m. 57, beat 1 |
| IV6/4 | 1 | 1 | 4 | 13.3% | m. 59, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 4 | 4 | 12 | 40.0% | m. 54, beat 1 |
| V7 | 3 | 3 | 3 | 10.0% | m. 54, beat 4 |
| iii7 | 1 | 1 | 4 | 13.3% | m. 53, beat 1 |
| ii7 | 1 | 1 | 4 | 13.3% | m. 57, beat 1 |
| IV | 1 | 1 | 4 | 13.3% | m. 59, beat 1 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/V | 1 | 1 | 2 | 6.7% | m. 60, beat 3 |
| V4/2/ii | 1 | 1 | 1 | 3.3% | m. 60, beat 2 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/V | 1 | 1 | 2 | 6.7% | m. 60, beat 3 |
| V7/ii | 1 | 1 | 1 | 3.3% | m. 60, beat 2 |

### D minor (region 10)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 3 | 2 | 11 | 91.7% | m. 61, beat 1 |
| V7 | 1 | 1 | 1 | 8.3% | m. 62, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 3 | 2 | 11 | 91.7% | m. 61, beat 1 |
| V7 | 1 | 1 | 1 | 8.3% | m. 62, beat 4 |

### A minor (region 11)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| viio | 1 | 1 | 4 | 20.0% | m. 64, beat 1 |
| VI6/4 | 1 | 1 | 4 | 20.0% | m. 65, beat 1 |
| v6/4 | 1 | 1 | 4 | 20.0% | m. 66, beat 1 |
| iv6/4 | 1 | 1 | 4 | 20.0% | m. 67, beat 1 |
| III+6/4 | 1 | 1 | 2 | 10.0% | m. 68, beat 2 |
| V6 | 1 | 1 | 1 | 5.0% | m. 68, beat 1 |
| VI6/5 | 1 | 1 | 1 | 5.0% | m. 68, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| viio | 1 | 1 | 4 | 20.0% | m. 64, beat 1 |
| VI | 1 | 1 | 4 | 20.0% | m. 65, beat 1 |
| v | 1 | 1 | 4 | 20.0% | m. 66, beat 1 |
| iv | 1 | 1 | 4 | 20.0% | m. 67, beat 1 |
| III+ | 1 | 1 | 2 | 10.0% | m. 68, beat 2 |
| V | 1 | 1 | 1 | 5.0% | m. 68, beat 1 |
| VI7 | 1 | 1 | 1 | 5.0% | m. 68, beat 4 |

### F major (region 12)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 6 | 5 | 18 | 34.0% | m. 70, beat 1 |
| V6 | 2 | 2 | 6 | 11.3% | m. 73, beat 1 |
| IV | 1 | 1 | 4 | 7.5% | m. 74, beat 1 |
| I6 | 1 | 1 | 4 | 7.5% | m. 75, beat 1 |
| V | 1 | 1 | 4 | 7.5% | m. 81, beat 1 |
| vi6 | 1 | 1 | 4 | 7.5% | m. 82, beat 1 |
| vii°6 | 1 | 1 | 3 | 5.7% | m. 76, beat 2 |
| ii6/4 | 1 | 1 | 2 | 3.8% | m. 69, beat 1 |
| V7 | 1 | 1 | 2 | 3.8% | m. 69, beat 3 |
| V6/4 | 1 | 1 | 2 | 3.8% | m. 71, beat 1 |
| IV6/4 | 1 | 1 | 2 | 3.8% | m. 72, beat 1 |
| ii7 | 1 | 1 | 1 | 1.9% | m. 76, beat 1 |
| vi7 | 1 | 1 | 1 | 1.9% | m. 80, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 7 | 6 | 22 | 41.5% | m. 70, beat 1 |
| V | 4 | 4 | 12 | 22.6% | m. 71, beat 1 |
| IV | 2 | 2 | 6 | 11.3% | m. 72, beat 1 |
| vi | 1 | 1 | 4 | 7.5% | m. 82, beat 1 |
| vii° | 1 | 1 | 3 | 5.7% | m. 76, beat 2 |
| ii | 1 | 1 | 2 | 3.8% | m. 69, beat 1 |
| V7 | 1 | 1 | 2 | 3.8% | m. 69, beat 3 |
| ii7 | 1 | 1 | 1 | 1.9% | m. 76, beat 1 |
| vi7 | 1 | 1 | 1 | 1.9% | m. 80, beat 1 |

### G major (region 13)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 5 | 38.5% | m. 84, beat 1 |
| IV6/4 | 2 | 2 | 2 | 15.4% | m. 84, beat 2 |
| IV6 | 1 | 1 | 4 | 30.8% | m. 87, beat 1 |
| ii7 | 1 | 1 | 1 | 7.7% | m. 83, beat 3 |
| ii | 1 | 1 | 0.5 | 3.8% | m. 83, beat 4 |
| V6/5 | 1 | 1 | 0.5 | 3.8% | m. 83, beat 4.5 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| IV | 3 | 3 | 6 | 46.2% | m. 84, beat 2 |
| I | 3 | 3 | 5 | 38.5% | m. 84, beat 1 |
| ii7 | 1 | 1 | 1 | 7.7% | m. 83, beat 3 |
| ii | 1 | 1 | 0.5 | 3.8% | m. 83, beat 4 |
| V7 | 1 | 1 | 0.5 | 3.8% | m. 83, beat 4.5 |

### C major (region 14)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V4/2 | 2 | 2 | 8 | 28.6% | m. 88, beat 1 |
| I6 | 2 | 2 | 6 | 21.4% | m. 89, beat 1 |
| I6/4 | 1 | 1 | 2 | 7.1% | m. 91, beat 1 |
| IV6 | 1 | 1 | 2 | 7.1% | m. 91, beat 3 |
| vii°6/4 | 1 | 1 | 2 | 7.1% | m. 92, beat 1 |
| iii6 | 1 | 1 | 2 | 7.1% | m. 92, beat 3 |
| vi6/4 | 1 | 1 | 2 | 7.1% | m. 93, beat 1 |
| ii6 | 1 | 1 | 2 | 7.1% | m. 93, beat 3 |
| V6/4 | 1 | 1 | 2 | 7.1% | m. 94, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 8 | 28.6% | m. 89, beat 1 |
| V7 | 2 | 2 | 8 | 28.6% | m. 88, beat 1 |
| IV | 1 | 1 | 2 | 7.1% | m. 91, beat 3 |
| vii° | 1 | 1 | 2 | 7.1% | m. 92, beat 1 |
| iii | 1 | 1 | 2 | 7.1% | m. 92, beat 3 |
| vi | 1 | 1 | 2 | 7.1% | m. 93, beat 1 |
| ii | 1 | 1 | 2 | 7.1% | m. 93, beat 3 |
| V | 1 | 1 | 2 | 7.1% | m. 94, beat 1 |

### D minor (region 15)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 1 | 1 | 4 | 33.3% | m. 95, beat 1 |
| v | 1 | 1 | 4 | 33.3% | m. 96, beat 1 |
| VII6/4 | 1 | 1 | 4 | 33.3% | m. 97, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 1 | 1 | 4 | 33.3% | m. 95, beat 1 |
| v | 1 | 1 | 4 | 33.3% | m. 96, beat 1 |
| VII | 1 | 1 | 4 | 33.3% | m. 97, beat 1 |

### C major (region 16)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 9 | 39.1% | m. 99, beat 1 |
| V7 | 2 | 2 | 2 | 8.7% | m. 99, beat 4 |
| iii6/5 | 1 | 1 | 4 | 17.4% | m. 98, beat 1 |
| vi4/2 | 1 | 1 | 4 | 17.4% | m. 102, beat 1 |
| V | 1 | 1 | 3 | 13.0% | m. 103, beat 1 |
| ii7 | 1 | 1 | 1 | 4.3% | m. 103, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 9 | 39.1% | m. 99, beat 1 |
| V7 | 2 | 2 | 2 | 8.7% | m. 99, beat 4 |
| iii7 | 1 | 1 | 4 | 17.4% | m. 98, beat 1 |
| vi7 | 1 | 1 | 4 | 17.4% | m. 102, beat 1 |
| V | 1 | 1 | 3 | 13.0% | m. 103, beat 1 |
| ii7 | 1 | 1 | 1 | 4.3% | m. 103, beat 4 |

### G minor (region 17)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 1 | 1 | 4 | 57.1% | m. 104, beat 1 |
| V | 1 | 1 | 1 | 14.3% | m. 105, beat 2 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 1 | 1 | 4 | 57.1% | m. 104, beat 1 |
| V | 1 | 1 | 1 | 14.3% | m. 105, beat 2 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/V | 1 | 1 | 2 | 28.6% | m. 105, beat 3 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/V | 1 | 1 | 2 | 28.6% | m. 105, beat 3 |

### D minor (region 18)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 3 | 2 | 11 | 91.7% | m. 106, beat 1 |
| V7 | 1 | 1 | 1 | 8.3% | m. 107, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 3 | 2 | 11 | 91.7% | m. 106, beat 1 |
| V7 | 1 | 1 | 1 | 8.3% | m. 107, beat 4 |

### A minor (region 19)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| viio | 1 | 1 | 4 | 20.0% | m. 109, beat 1 |
| VI6/4 | 1 | 1 | 4 | 20.0% | m. 110, beat 1 |
| v6/4 | 1 | 1 | 4 | 20.0% | m. 111, beat 1 |
| iv6/4 | 1 | 1 | 4 | 20.0% | m. 112, beat 1 |
| III+6/4 | 1 | 1 | 2 | 10.0% | m. 113, beat 2 |
| V6 | 1 | 1 | 1 | 5.0% | m. 113, beat 1 |
| VI6/5 | 1 | 1 | 1 | 5.0% | m. 113, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| viio | 1 | 1 | 4 | 20.0% | m. 109, beat 1 |
| VI | 1 | 1 | 4 | 20.0% | m. 110, beat 1 |
| v | 1 | 1 | 4 | 20.0% | m. 111, beat 1 |
| iv | 1 | 1 | 4 | 20.0% | m. 112, beat 1 |
| III+ | 1 | 1 | 2 | 10.0% | m. 113, beat 2 |
| V | 1 | 1 | 1 | 5.0% | m. 113, beat 1 |
| VI7 | 1 | 1 | 1 | 5.0% | m. 113, beat 4 |

### F major (region 20)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 6 | 5 | 18 | 34.0% | m. 115, beat 1 |
| V6 | 2 | 2 | 6 | 11.3% | m. 118, beat 1 |
| IV | 1 | 1 | 4 | 7.5% | m. 119, beat 1 |
| I6 | 1 | 1 | 4 | 7.5% | m. 120, beat 1 |
| V | 1 | 1 | 4 | 7.5% | m. 126, beat 1 |
| vi6 | 1 | 1 | 4 | 7.5% | m. 127, beat 1 |
| vii°6 | 1 | 1 | 3 | 5.7% | m. 121, beat 2 |
| ii6/4 | 1 | 1 | 2 | 3.8% | m. 114, beat 1 |
| V7 | 1 | 1 | 2 | 3.8% | m. 114, beat 3 |
| V6/4 | 1 | 1 | 2 | 3.8% | m. 116, beat 1 |
| IV6/4 | 1 | 1 | 2 | 3.8% | m. 117, beat 1 |
| ii7 | 1 | 1 | 1 | 1.9% | m. 121, beat 1 |
| vi7 | 1 | 1 | 1 | 1.9% | m. 125, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 7 | 6 | 22 | 41.5% | m. 115, beat 1 |
| V | 4 | 4 | 12 | 22.6% | m. 116, beat 1 |
| IV | 2 | 2 | 6 | 11.3% | m. 117, beat 1 |
| vi | 1 | 1 | 4 | 7.5% | m. 127, beat 1 |
| vii° | 1 | 1 | 3 | 5.7% | m. 121, beat 2 |
| ii | 1 | 1 | 2 | 3.8% | m. 114, beat 1 |
| V7 | 1 | 1 | 2 | 3.8% | m. 114, beat 3 |
| ii7 | 1 | 1 | 1 | 1.9% | m. 121, beat 1 |
| vi7 | 1 | 1 | 1 | 1.9% | m. 125, beat 1 |

### G major (region 21)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 5 | 38.5% | m. 129, beat 1 |
| IV6/4 | 2 | 2 | 2 | 15.4% | m. 129, beat 2 |
| IV6 | 1 | 1 | 4 | 30.8% | m. 132, beat 1 |
| ii7 | 1 | 1 | 1 | 7.7% | m. 128, beat 3 |
| ii | 1 | 1 | 0.5 | 3.8% | m. 128, beat 4 |
| V6/5 | 1 | 1 | 0.5 | 3.8% | m. 128, beat 4.5 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| IV | 3 | 3 | 6 | 46.2% | m. 129, beat 2 |
| I | 3 | 3 | 5 | 38.5% | m. 129, beat 1 |
| ii7 | 1 | 1 | 1 | 7.7% | m. 128, beat 3 |
| ii | 1 | 1 | 0.5 | 3.8% | m. 128, beat 4 |
| V7 | 1 | 1 | 0.5 | 3.8% | m. 128, beat 4.5 |

### C major (region 22)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V4/2 | 2 | 2 | 8 | 28.6% | m. 133, beat 1 |
| I6 | 2 | 2 | 6 | 21.4% | m. 134, beat 1 |
| I6/4 | 1 | 1 | 2 | 7.1% | m. 136, beat 1 |
| IV6 | 1 | 1 | 2 | 7.1% | m. 136, beat 3 |
| vii°6/4 | 1 | 1 | 2 | 7.1% | m. 137, beat 1 |
| iii6 | 1 | 1 | 2 | 7.1% | m. 137, beat 3 |
| vi6/4 | 1 | 1 | 2 | 7.1% | m. 138, beat 1 |
| ii6 | 1 | 1 | 2 | 7.1% | m. 138, beat 3 |
| V6/4 | 1 | 1 | 2 | 7.1% | m. 139, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 8 | 28.6% | m. 134, beat 1 |
| V7 | 2 | 2 | 8 | 28.6% | m. 133, beat 1 |
| IV | 1 | 1 | 2 | 7.1% | m. 136, beat 3 |
| vii° | 1 | 1 | 2 | 7.1% | m. 137, beat 1 |
| iii | 1 | 1 | 2 | 7.1% | m. 137, beat 3 |
| vi | 1 | 1 | 2 | 7.1% | m. 138, beat 1 |
| ii | 1 | 1 | 2 | 7.1% | m. 138, beat 3 |
| V | 1 | 1 | 2 | 7.1% | m. 139, beat 1 |

### D minor (region 23)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 1 | 1 | 4 | 33.3% | m. 140, beat 1 |
| v | 1 | 1 | 4 | 33.3% | m. 141, beat 1 |
| VII6/4 | 1 | 1 | 4 | 33.3% | m. 142, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 1 | 1 | 4 | 33.3% | m. 140, beat 1 |
| v | 1 | 1 | 4 | 33.3% | m. 141, beat 1 |
| VII | 1 | 1 | 4 | 33.3% | m. 142, beat 1 |

### C major (region 24)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 9 | 60.0% | m. 144, beat 1 |
| V7 | 2 | 2 | 2 | 13.3% | m. 144, beat 4 |
| iii6/5 | 1 | 1 | 4 | 26.7% | m. 143, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 3 | 9 | 60.0% | m. 144, beat 1 |
| V7 | 2 | 2 | 2 | 13.3% | m. 144, beat 4 |
| iii7 | 1 | 1 | 4 | 26.7% | m. 143, beat 1 |

## Prolongations

The library's standard-harmony analysis found 16 prolongations: a tonic or dominant that lasts through the chords between it and the harmony that closes it. Measures run from the prolonged chord to the closing chord. Numeral is the prolonged chord. Reason is why the library reads the chords between as a prolongation.

| Measures | Function | Numeral | Key | Reason |
| --- | --- | --- | --- | --- |
| mm. 2 to 4 | tonic | I | C major | neighbor-six-four |
| mm. 3 to 5 | tonic | I | C major | tonic-line |
| m. 11 | tonic | I | G major | neighbor-six-four |
| mm. 11 to 12 | tonic | I | G major | neighbor-six-four |
| mm. 30 to 32 | tonic | I | C major | neighbor-six-four |
| mm. 31 to 33 | tonic | I | C major | tonic-line |
| m. 39 | tonic | I | G major | neighbor-six-four |
| mm. 39 to 40 | tonic | I | G major | neighbor-six-four |
| mm. 71 to 73 | tonic | I | F major | neighbor-six-four |
| mm. 72 to 74 | tonic | I | F major | tonic-line |
| m. 84 | tonic | I | G major | neighbor-six-four |
| mm. 84 to 85 | tonic | I | G major | neighbor-six-four |
| mm. 116 to 118 | tonic | I | F major | neighbor-six-four |
| mm. 117 to 119 | tonic | I | F major | tonic-line |
| m. 129 | tonic | I | G major | neighbor-six-four |
| mm. 129 to 130 | tonic | I | G major | neighbor-six-four |

## Non-chord tones

571 notes sound against a chord without being one of its tones; 196 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 2, beat 1 to m. 2, beat 3 | 14 | neighbor | V |
| m. 2, beat 1 to m. 2, beat 3 | 15 | passing | V |
| m. 4, beat 1 to m. 4, beat 3 | 37 | neighbor | V |
| m. 4, beat 1 to m. 4, beat 3 | 40 | passing | V |
| m. 4, beat 1 to m. 4, beat 3 | 41 | neighbor | V |
| m. 4, beat 1 to m. 4, beat 3 | 43 | neighbor | V |
| m. 5, beat 1 to m. 6, beat 1 | 51 | passing | IV |
| m. 5, beat 1 to m. 6, beat 1 | 53 | passing | IV |
| m. 5, beat 1 to m. 6, beat 1 | 54 | passing | IV |
| m. 5, beat 1 to m. 6, beat 1 | 56 | passing | IV |
| m. 5, beat 1 to m. 6, beat 1 | 58 | passing | IV |
| m. 5, beat 1 to m. 6, beat 1 | 60 | unclassified | IV |
| m. 5, beat 1 to m. 6, beat 1 | 63 | unclassified | IV |
| m. 5, beat 1 to m. 6, beat 1 | 65 | passing | IV |
| m. 6, beat 1 to m. 7, beat 1 | 70 | passing | I |
| m. 6, beat 1 to m. 7, beat 1 | 71 | passing | I |
| m. 6, beat 1 to m. 7, beat 1 | 73 | passing | I |
| m. 6, beat 1 to m. 7, beat 1 | 75 | passing | I |
| m. 6, beat 1 to m. 7, beat 1 | 77 | passing | I |
| m. 6, beat 1 to m. 7, beat 1 | 79 | unclassified | I |
| m. 6, beat 1 to m. 7, beat 1 | 83 | passing | I |
| m. 6, beat 1 to m. 7, beat 1 | 84 | passing | I |
| m. 7, beat 1 to m. 7, beat 2 | 89 | passing | minor7 |
| m. 7, beat 2 to m. 8, beat 1 | 92 | passing | vii° |
| m. 7, beat 2 to m. 8, beat 1 | 94 | passing | vii° |
| m. 7, beat 2 to m. 8, beat 1 | 96 | passing | vii° |
| m. 7, beat 2 to m. 8, beat 1 | 98 | unclassified | vii° |
| m. 7, beat 2 to m. 8, beat 1 | 102 | passing | vii° |
| m. 7, beat 2 to m. 8, beat 1 | 103 | passing | vii° |
| m. 8, beat 1 to m. 9, beat 1 | 108 | passing | V |
| m. 8, beat 1 to m. 9, beat 1 | 110 | passing | V |
| m. 8, beat 1 to m. 9, beat 1 | 111 | passing | V |
| m. 8, beat 1 to m. 9, beat 1 | 113 | passing | V |
| m. 8, beat 1 to m. 9, beat 1 | 115 | passing | V |
| m. 8, beat 1 to m. 9, beat 1 | 117 | unclassified | V |
| m. 8, beat 1 to m. 9, beat 1 | 120 | appoggiatura | V |
| m. 8, beat 1 to m. 9, beat 1 | 122 | passing | V |
| m. 9, beat 1 to m. 10, beat 1 | 127 | passing | vi |
| m. 9, beat 1 to m. 10, beat 1 | 129 | passing | vi |
| m. 9, beat 1 to m. 10, beat 1 | 131 | passing | vi |
| m. 9, beat 1 to m. 10, beat 1 | 132 | passing | vi |
| m. 9, beat 1 to m. 10, beat 1 | 135 | passing | vi |
| m. 9, beat 1 to m. 10, beat 1 | 136 | passing | vi |
| m. 9, beat 1 to m. 10, beat 1 | 138 | passing | vi |
| m. 9, beat 1 to m. 10, beat 1 | 140 | unclassified | vi |
| m. 10, beat 3 to m. 10, beat 4 | 151 | passing | minor7 |
| m. 10, beat 4 to m. 11, beat 1 | 157 | appoggiatura | V |
| m. 10, beat 4 to m. 11, beat 1 | 158 | passing | V |
| m. 10, beat 4 to m. 11, beat 1 | 160 | unclassified | V |
| m. 14, beat 1 to m. 15, beat 1 | 222 | passing | I |
| m. 14, beat 1 to m. 15, beat 1 | 226 | passing | I |
| m. 15, beat 1 to m. 16, beat 1 | 234 | neighbor | dominant7 |
| m. 15, beat 1 to m. 16, beat 1 | 237 | neighbor | dominant7 |
| m. 15, beat 1 to m. 16, beat 1 | 239 | unclassified | dominant7 |
| m. 15, beat 1 to m. 16, beat 1 | 240 | passing | dominant7 |
| m. 15, beat 1 to m. 16, beat 1 | 246 | unclassified | dominant7 |
| m. 15, beat 1 to m. 16, beat 1 | 250 | unclassified | dominant7 |
| m. 16, beat 1 to m. 17, beat 1 | 269 | passing | I |
| m. 16, beat 1 to m. 17, beat 1 | 273 | passing | I |
| m. 17, beat 1 to m. 18, beat 1 | 281 | neighbor | dominant7 |
| m. 17, beat 1 to m. 18, beat 1 | 284 | neighbor | dominant7 |
| m. 17, beat 1 to m. 18, beat 1 | 286 | unclassified | dominant7 |
| m. 17, beat 1 to m. 18, beat 1 | 287 | passing | dominant7 |
| m. 17, beat 1 to m. 18, beat 1 | 293 | unclassified | dominant7 |
| m. 17, beat 1 to m. 18, beat 1 | 297 | unclassified | dominant7 |
| m. 22, beat 1 to m. 23, beat 1 | 378 | passing | minor |
| m. 22, beat 1 to m. 23, beat 1 | 384 | unclassified | minor |
| m. 23, beat 1 to m. 24, beat 1 | 390 | unclassified | minor |
| m. 23, beat 1 to m. 24, beat 1 | 398 | neighbor | minor |
| m. 23, beat 1 to m. 24, beat 1 | 400 | neighbor | minor |
| m. 24, beat 1 to m. 25, beat 1 | 429 | passing | major |
| m. 24, beat 1 to m. 25, beat 1 | 433 | passing | major |
| m. 25, beat 1 to m. 26, beat 1 | 440 | unclassified | minor7 |
| m. 25, beat 1 to m. 26, beat 1 | 446 | unclassified | minor7 |
| m. 25, beat 1 to m. 26, beat 1 | 452 | unclassified | minor7 |
| m. 25, beat 1 to m. 26, beat 1 | 458 | unclassified | minor7 |
| m. 25, beat 1 to m. 26, beat 1 | 464 | unclassified | minor7 |
| m. 25, beat 1 to m. 26, beat 1 | 470 | unclassified | minor7 |
| m. 25, beat 1 to m. 26, beat 1 | 476 | unclassified | minor7 |
| m. 25, beat 1 to m. 26, beat 1 | 482 | unclassified | minor7 |
| m. 25, beat 1 to m. 26, beat 1 | 483 | unclassified | minor7 |
| m. 30, beat 1 to m. 30, beat 3 | 546 | neighbor | V |
| m. 30, beat 1 to m. 30, beat 3 | 547 | passing | V |
| m. 32, beat 1 to m. 32, beat 3 | 569 | neighbor | V |
| m. 32, beat 1 to m. 32, beat 3 | 572 | passing | V |
| m. 32, beat 1 to m. 32, beat 3 | 573 | neighbor | V |
| m. 32, beat 1 to m. 32, beat 3 | 575 | neighbor | V |
| m. 33, beat 1 to m. 34, beat 1 | 583 | passing | IV |
| m. 33, beat 1 to m. 34, beat 1 | 585 | passing | IV |
| m. 33, beat 1 to m. 34, beat 1 | 586 | passing | IV |
| m. 33, beat 1 to m. 34, beat 1 | 588 | passing | IV |
| m. 33, beat 1 to m. 34, beat 1 | 590 | passing | IV |
| m. 33, beat 1 to m. 34, beat 1 | 592 | unclassified | IV |
| m. 33, beat 1 to m. 34, beat 1 | 595 | unclassified | IV |
| m. 33, beat 1 to m. 34, beat 1 | 597 | passing | IV |
| m. 34, beat 1 to m. 35, beat 1 | 602 | passing | I |
| m. 34, beat 1 to m. 35, beat 1 | 603 | passing | I |
| m. 34, beat 1 to m. 35, beat 1 | 605 | passing | I |
| m. 34, beat 1 to m. 35, beat 1 | 607 | passing | I |
| m. 34, beat 1 to m. 35, beat 1 | 609 | passing | I |
| m. 34, beat 1 to m. 35, beat 1 | 611 | unclassified | I |
| m. 34, beat 1 to m. 35, beat 1 | 615 | passing | I |
| m. 34, beat 1 to m. 35, beat 1 | 616 | passing | I |
| m. 35, beat 1 to m. 35, beat 2 | 621 | passing | minor7 |
| m. 35, beat 2 to m. 36, beat 1 | 624 | passing | vii° |
| m. 35, beat 2 to m. 36, beat 1 | 626 | passing | vii° |
| m. 35, beat 2 to m. 36, beat 1 | 628 | passing | vii° |
| m. 35, beat 2 to m. 36, beat 1 | 630 | unclassified | vii° |
| m. 35, beat 2 to m. 36, beat 1 | 634 | passing | vii° |
| m. 35, beat 2 to m. 36, beat 1 | 635 | passing | vii° |
| m. 36, beat 1 to m. 37, beat 1 | 640 | passing | V |
| m. 36, beat 1 to m. 37, beat 1 | 642 | passing | V |
| m. 36, beat 1 to m. 37, beat 1 | 643 | passing | V |
| m. 36, beat 1 to m. 37, beat 1 | 645 | passing | V |
| m. 36, beat 1 to m. 37, beat 1 | 647 | passing | V |
| m. 36, beat 1 to m. 37, beat 1 | 649 | unclassified | V |
| m. 36, beat 1 to m. 37, beat 1 | 652 | appoggiatura | V |
| m. 36, beat 1 to m. 37, beat 1 | 654 | passing | V |
| m. 37, beat 1 to m. 38, beat 1 | 659 | passing | vi |
| m. 37, beat 1 to m. 38, beat 1 | 661 | passing | vi |
| m. 37, beat 1 to m. 38, beat 1 | 663 | passing | vi |
| m. 37, beat 1 to m. 38, beat 1 | 664 | passing | vi |
| m. 37, beat 1 to m. 38, beat 1 | 667 | passing | vi |
| m. 37, beat 1 to m. 38, beat 1 | 668 | passing | vi |
| m. 37, beat 1 to m. 38, beat 1 | 670 | passing | vi |
| m. 37, beat 1 to m. 38, beat 1 | 672 | unclassified | vi |
| m. 38, beat 3 to m. 38, beat 4 | 683 | passing | minor7 |
| m. 38, beat 4 to m. 39, beat 1 | 689 | appoggiatura | V |
| m. 38, beat 4 to m. 39, beat 1 | 690 | passing | V |
| m. 38, beat 4 to m. 39, beat 1 | 692 | unclassified | V |
| m. 42, beat 1 to m. 43, beat 1 | 754 | passing | I |
| m. 42, beat 1 to m. 43, beat 1 | 758 | passing | I |
| m. 43, beat 1 to m. 44, beat 1 | 766 | neighbor | dominant7 |
| m. 43, beat 1 to m. 44, beat 1 | 769 | neighbor | dominant7 |
| m. 43, beat 1 to m. 44, beat 1 | 771 | unclassified | dominant7 |
| m. 43, beat 1 to m. 44, beat 1 | 772 | passing | dominant7 |
| m. 43, beat 1 to m. 44, beat 1 | 778 | unclassified | dominant7 |
| m. 43, beat 1 to m. 44, beat 1 | 782 | unclassified | dominant7 |
| m. 44, beat 1 to m. 45, beat 1 | 801 | passing | I |
| m. 44, beat 1 to m. 45, beat 1 | 805 | passing | I |
| m. 45, beat 1 to m. 46, beat 1 | 813 | neighbor | dominant7 |
| m. 45, beat 1 to m. 46, beat 1 | 816 | neighbor | dominant7 |
| m. 45, beat 1 to m. 46, beat 1 | 818 | unclassified | dominant7 |
| m. 45, beat 1 to m. 46, beat 1 | 819 | passing | dominant7 |
| m. 45, beat 1 to m. 46, beat 1 | 825 | unclassified | dominant7 |
| m. 45, beat 1 to m. 46, beat 1 | 829 | unclassified | dominant7 |
| m. 50, beat 1 to m. 51, beat 1 | 910 | passing | minor |
| m. 50, beat 1 to m. 51, beat 1 | 916 | unclassified | minor |
| m. 51, beat 1 to m. 52, beat 1 | 922 | unclassified | minor |
| m. 51, beat 1 to m. 52, beat 1 | 930 | neighbor | minor |
| m. 51, beat 1 to m. 52, beat 1 | 932 | neighbor | minor |
| m. 52, beat 1 to m. 53, beat 1 | 961 | passing | major |
| m. 52, beat 1 to m. 53, beat 1 | 965 | passing | major |
| m. 53, beat 1 to m. 54, beat 1 | 972 | unclassified | minor7 |
| m. 53, beat 1 to m. 54, beat 1 | 978 | unclassified | minor7 |
| m. 53, beat 1 to m. 54, beat 1 | 984 | unclassified | minor7 |
| m. 53, beat 1 to m. 54, beat 1 | 990 | unclassified | minor7 |
| m. 53, beat 1 to m. 54, beat 1 | 996 | unclassified | minor7 |
| m. 53, beat 1 to m. 54, beat 1 | 1002 | unclassified | minor7 |
| m. 53, beat 1 to m. 54, beat 1 | 1008 | unclassified | minor7 |
| m. 53, beat 1 to m. 54, beat 1 | 1014 | unclassified | minor7 |
| m. 53, beat 1 to m. 54, beat 1 | 1015 | unclassified | minor7 |
| m. 57, beat 1 to m. 58, beat 1 | 1068 | unclassified | minor7 |
| m. 57, beat 1 to m. 58, beat 1 | 1070 | unclassified | minor7 |
| m. 57, beat 1 to m. 58, beat 1 | 1071 | unclassified | minor7 |
| m. 57, beat 1 to m. 58, beat 1 | 1072 | unclassified | minor7 |
| m. 57, beat 1 to m. 58, beat 1 | 1074 | unclassified | minor7 |
| m. 57, beat 1 to m. 58, beat 1 | 1075 | unclassified | minor7 |
| m. 57, beat 1 to m. 58, beat 1 | 1080 | unclassified | minor7 |
| m. 58, beat 1 to m. 58, beat 4 | 1084 | unclassified | I |
| m. 58, beat 1 to m. 58, beat 4 | 1088 | unclassified | I |
| m. 58, beat 1 to m. 58, beat 4 | 1090 | unclassified | I |
| m. 58, beat 1 to m. 58, beat 4 | 1092 | unclassified | I |
| m. 59, beat 1 to m. 60, beat 1 | 1102 | passing | IV |
| m. 59, beat 1 to m. 60, beat 1 | 1103 | unclassified | IV |
| m. 59, beat 1 to m. 60, beat 1 | 1105 | passing | IV |
| m. 59, beat 1 to m. 60, beat 1 | 1107 | passing | IV |
| m. 59, beat 1 to m. 60, beat 1 | 1110 | appoggiatura | IV |
| m. 59, beat 1 to m. 60, beat 1 | 1111 | unclassified | IV |
| m. 59, beat 1 to m. 60, beat 1 | 1113 | passing | IV |
| m. 59, beat 1 to m. 60, beat 1 | 1115 | passing | IV |
| m. 60, beat 2 to m. 60, beat 3 | 1121 | passing | V |
| m. 60, beat 2 to m. 60, beat 3 | 1123 | passing | V |
| m. 60, beat 3 to m. 61, beat 1 | 1125 | unclassified | ii |
| m. 60, beat 3 to m. 61, beat 1 | 1127 | unclassified | ii |
| m. 60, beat 3 to m. 61, beat 1 | 1128 | unclassified | ii |
| m. 60, beat 3 to m. 61, beat 1 | 1129 | unclassified | ii |
| m. 60, beat 3 to m. 61, beat 1 | 1131 | unclassified | ii |
| m. 61, beat 1 to m. 62, beat 1 | 1144 | unclassified | minor |
| m. 61, beat 1 to m. 62, beat 1 | 1145 | appoggiatura | minor |
| m. 61, beat 1 to m. 62, beat 1 | 1146 | unclassified | minor |
| m. 61, beat 1 to m. 62, beat 1 | 1147 | unclassified | minor |
| m. 61, beat 1 to m. 62, beat 1 | 1148 | unclassified | minor |
| m. 61, beat 1 to m. 62, beat 1 | 1149 | unclassified | minor |
| m. 63, beat 1 to m. 64, beat 1 | 1171 | passing | minor |
| m. 63, beat 1 to m. 64, beat 1 | 1173 | unclassified | minor |
| m. 63, beat 1 to m. 64, beat 1 | 1175 | passing | minor |
| m. 63, beat 1 to m. 64, beat 1 | 1176 | passing | minor |
| m. 63, beat 1 to m. 64, beat 1 | 1180 | unclassified | minor |
| m. 63, beat 1 to m. 64, beat 1 | 1182 | passing | minor |
| m. 63, beat 1 to m. 64, beat 1 | 1183 | passing | minor |
| m. 64, beat 1 to m. 65, beat 1 | 1187 | passing | diminished |
| m. 64, beat 1 to m. 65, beat 1 | 1189 | unclassified | diminished |
| m. 64, beat 1 to m. 65, beat 1 | 1190 | passing | diminished |
| m. 64, beat 1 to m. 65, beat 1 | 1192 | passing | diminished |
| m. 64, beat 1 to m. 65, beat 1 | 1196 | escape-tone | diminished |
| m. 64, beat 1 to m. 65, beat 1 | 1198 | unclassified | diminished |
| m. 64, beat 1 to m. 65, beat 1 | 1199 | passing | diminished |
| m. 65, beat 1 to m. 66, beat 1 | 1203 | appoggiatura | major |
| m. 65, beat 1 to m. 66, beat 1 | 1204 | unclassified | major |
| m. 65, beat 1 to m. 66, beat 1 | 1206 | passing | major |
| m. 65, beat 1 to m. 66, beat 1 | 1208 | passing | major |
| m. 65, beat 1 to m. 66, beat 1 | 1210 | unclassified | major |
| m. 65, beat 1 to m. 66, beat 1 | 1212 | escape-tone | major |
| m. 65, beat 1 to m. 66, beat 1 | 1214 | passing | major |
| m. 65, beat 1 to m. 66, beat 1 | 1215 | passing | major |
| m. 66, beat 1 to m. 67, beat 1 | 1219 | appoggiatura | minor |
| m. 66, beat 1 to m. 67, beat 1 | 1220 | unclassified | minor |
| m. 66, beat 1 to m. 67, beat 1 | 1222 | passing | minor |
| m. 66, beat 1 to m. 67, beat 1 | 1224 | passing | minor |
| m. 66, beat 1 to m. 67, beat 1 | 1226 | unclassified | minor |
| m. 66, beat 1 to m. 67, beat 1 | 1228 | escape-tone | minor |
| m. 66, beat 1 to m. 67, beat 1 | 1230 | passing | minor |
| m. 66, beat 1 to m. 67, beat 1 | 1231 | passing | minor |
| m. 67, beat 1 to m. 68, beat 1 | 1235 | appoggiatura | minor |
| m. 67, beat 1 to m. 68, beat 1 | 1236 | unclassified | minor |
| m. 67, beat 1 to m. 68, beat 1 | 1238 | passing | minor |
| m. 67, beat 1 to m. 68, beat 1 | 1240 | passing | minor |
| m. 67, beat 1 to m. 68, beat 1 | 1242 | unclassified | minor |
| m. 67, beat 1 to m. 68, beat 1 | 1244 | escape-tone | minor |
| m. 67, beat 1 to m. 68, beat 1 | 1246 | passing | minor |
| m. 67, beat 1 to m. 68, beat 1 | 1247 | passing | minor |
| m. 68, beat 1 to m. 68, beat 2 | 1252 | unclassified | major |
| m. 68, beat 2 to m. 68, beat 4 | 1254 | unclassified | augmented |
| m. 68, beat 2 to m. 68, beat 4 | 1256 | passing | augmented |
| m. 68, beat 2 to m. 68, beat 4 | 1258 | unclassified | augmented |
| m. 68, beat 2 to m. 68, beat 4 | 1260 | unclassified | augmented |
| m. 68, beat 4 to m. 69, beat 1 | 1262 | passing | major7 |
| m. 69, beat 1 to m. 69, beat 3 | 1268 | passing | ii |
| m. 69, beat 1 to m. 69, beat 3 | 1270 | passing | ii |
| m. 69, beat 1 to m. 69, beat 3 | 1272 | unclassified | ii |
| m. 69, beat 3 to m. 70, beat 1 | 1277 | passing | dominant7 |
| m. 69, beat 3 to m. 70, beat 1 | 1279 | passing | dominant7 |
| m. 69, beat 3 to m. 70, beat 1 | 1282 | passing | dominant7 |
| m. 71, beat 1 to m. 71, beat 3 | 1298 | neighbor | V |
| m. 71, beat 1 to m. 71, beat 3 | 1299 | passing | V |
| m. 73, beat 1 to m. 73, beat 3 | 1321 | neighbor | V |
| m. 73, beat 1 to m. 73, beat 3 | 1324 | passing | V |
| m. 73, beat 1 to m. 73, beat 3 | 1325 | neighbor | V |
| m. 73, beat 1 to m. 73, beat 3 | 1327 | neighbor | V |
| m. 74, beat 1 to m. 75, beat 1 | 1335 | passing | IV |
| m. 74, beat 1 to m. 75, beat 1 | 1337 | passing | IV |
| m. 74, beat 1 to m. 75, beat 1 | 1338 | passing | IV |
| m. 74, beat 1 to m. 75, beat 1 | 1340 | passing | IV |
| m. 74, beat 1 to m. 75, beat 1 | 1342 | passing | IV |
| m. 74, beat 1 to m. 75, beat 1 | 1344 | unclassified | IV |
| m. 74, beat 1 to m. 75, beat 1 | 1347 | appoggiatura | IV |
| m. 74, beat 1 to m. 75, beat 1 | 1349 | passing | IV |
| m. 75, beat 1 to m. 76, beat 1 | 1354 | passing | I |
| m. 75, beat 1 to m. 76, beat 1 | 1355 | passing | I |
| m. 75, beat 1 to m. 76, beat 1 | 1357 | passing | I |
| m. 75, beat 1 to m. 76, beat 1 | 1359 | passing | I |
| m. 75, beat 1 to m. 76, beat 1 | 1361 | passing | I |
| m. 75, beat 1 to m. 76, beat 1 | 1363 | unclassified | I |
| m. 75, beat 1 to m. 76, beat 1 | 1367 | passing | I |
| m. 75, beat 1 to m. 76, beat 1 | 1368 | passing | I |
| m. 76, beat 1 to m. 76, beat 2 | 1373 | passing | minor7 |
| m. 76, beat 2 to m. 77, beat 1 | 1376 | passing | vii° |
| m. 76, beat 2 to m. 77, beat 1 | 1378 | passing | vii° |
| m. 76, beat 2 to m. 77, beat 1 | 1380 | passing | vii° |
| m. 76, beat 2 to m. 77, beat 1 | 1382 | unclassified | vii° |
| m. 76, beat 2 to m. 77, beat 1 | 1386 | passing | vii° |
| m. 76, beat 2 to m. 77, beat 1 | 1387 | passing | vii° |
| m. 77, beat 1 to m. 78, beat 1 | 1392 | passing | I |
| m. 77, beat 1 to m. 78, beat 1 | 1394 | passing | I |
| m. 77, beat 1 to m. 78, beat 1 | 1395 | passing | I |
| m. 77, beat 1 to m. 78, beat 1 | 1397 | passing | I |
| m. 77, beat 1 to m. 78, beat 1 | 1399 | passing | I |
| m. 77, beat 1 to m. 78, beat 1 | 1401 | passing | I |
| m. 77, beat 1 to m. 78, beat 1 | 1402 | passing | I |
| m. 77, beat 1 to m. 78, beat 1 | 1404 | passing | I |
| m. 78, beat 1 to m. 79, beat 1 | 1408 | passing | I |
| m. 78, beat 1 to m. 79, beat 1 | 1410 | appoggiatura | I |
| m. 78, beat 1 to m. 79, beat 1 | 1412 | passing | I |
| m. 78, beat 1 to m. 79, beat 1 | 1413 | passing | I |
| m. 78, beat 1 to m. 79, beat 1 | 1415 | passing | I |
| m. 78, beat 1 to m. 79, beat 1 | 1416 | passing | I |
| m. 78, beat 1 to m. 79, beat 1 | 1420 | unclassified | I |
| m. 78, beat 1 to m. 79, beat 1 | 1422 | passing | I |
| m. 79, beat 1 to m. 80, beat 1 | 1427 | passing | V |
| m. 79, beat 1 to m. 80, beat 1 | 1429 | appoggiatura | V |
| m. 79, beat 1 to m. 80, beat 1 | 1430 | passing | V |
| m. 79, beat 1 to m. 80, beat 1 | 1432 | passing | V |
| m. 79, beat 1 to m. 80, beat 1 | 1434 | passing | V |
| m. 79, beat 1 to m. 80, beat 1 | 1436 | escape-tone | V |
| m. 79, beat 1 to m. 80, beat 1 | 1439 | neighbor | V |
| m. 79, beat 1 to m. 80, beat 1 | 1441 | passing | V |
| m. 80, beat 1 to m. 80, beat 2 | 1446 | passing | minor7 |
| m. 81, beat 1 to m. 82, beat 1 | 1465 | passing | V |
| m. 81, beat 1 to m. 82, beat 1 | 1467 | appoggiatura | V |
| m. 81, beat 1 to m. 82, beat 1 | 1469 | passing | V |
| m. 81, beat 1 to m. 82, beat 1 | 1470 | passing | V |
| m. 81, beat 1 to m. 82, beat 1 | 1472 | passing | V |
| m. 81, beat 1 to m. 82, beat 1 | 1473 | passing | V |
| m. 81, beat 1 to m. 82, beat 1 | 1477 | neighbor | V |
| m. 81, beat 1 to m. 82, beat 1 | 1479 | passing | V |
| m. 82, beat 1 to m. 83, beat 1 | 1485 | passing | vi |
| m. 82, beat 1 to m. 83, beat 1 | 1487 | passing | vi |
| m. 82, beat 1 to m. 83, beat 1 | 1489 | passing | vi |
| m. 82, beat 1 to m. 83, beat 1 | 1490 | passing | vi |
| m. 82, beat 1 to m. 83, beat 1 | 1493 | passing | vi |
| m. 82, beat 1 to m. 83, beat 1 | 1494 | passing | vi |
| m. 82, beat 1 to m. 83, beat 1 | 1496 | passing | vi |
| m. 82, beat 1 to m. 83, beat 1 | 1498 | unclassified | vi |
| m. 83, beat 3 to m. 83, beat 4 | 1509 | passing | minor7 |
| m. 83, beat 4 to m. 83, beat 4.5 | 1514 | passing | ii |
| m. 87, beat 1 to m. 88, beat 1 | 1580 | passing | IV |
| m. 87, beat 1 to m. 88, beat 1 | 1585 | unclassified | IV |
| m. 88, beat 1 to m. 89, beat 1 | 1592 | neighbor | dominant7 |
| m. 88, beat 1 to m. 89, beat 1 | 1595 | neighbor | dominant7 |
| m. 88, beat 1 to m. 89, beat 1 | 1597 | unclassified | dominant7 |
| m. 88, beat 1 to m. 89, beat 1 | 1598 | passing | dominant7 |
| m. 88, beat 1 to m. 89, beat 1 | 1604 | unclassified | dominant7 |
| m. 88, beat 1 to m. 89, beat 1 | 1608 | unclassified | dominant7 |
| m. 89, beat 1 to m. 90, beat 1 | 1627 | passing | I |
| m. 89, beat 1 to m. 90, beat 1 | 1631 | passing | I |
| m. 90, beat 1 to m. 91, beat 1 | 1639 | neighbor | dominant7 |
| m. 90, beat 1 to m. 91, beat 1 | 1642 | neighbor | dominant7 |
| m. 90, beat 1 to m. 91, beat 1 | 1644 | unclassified | dominant7 |
| m. 90, beat 1 to m. 91, beat 1 | 1645 | passing | dominant7 |
| m. 90, beat 1 to m. 91, beat 1 | 1651 | unclassified | dominant7 |
| m. 90, beat 1 to m. 91, beat 1 | 1655 | unclassified | dominant7 |
| m. 95, beat 1 to m. 96, beat 1 | 1734 | unclassified | minor |
| m. 95, beat 1 to m. 96, beat 1 | 1740 | neighbor | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1747 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1745 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1750 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1748 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1753 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1751 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1756 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1754 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1757 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1761 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1759 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1764 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1762 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1765 | neighbor | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1767 | unclassified | minor |
| m. 96, beat 1 to m. 97, beat 1 | 1769 | unclassified | minor |
| m. 97, beat 1 to m. 98, beat 1 | 1773 | passing | major |
| m. 97, beat 1 to m. 98, beat 1 | 1775 | passing | major |
| m. 97, beat 1 to m. 98, beat 1 | 1779 | passing | major |
| m. 97, beat 1 to m. 98, beat 1 | 1783 | passing | major |
| m. 97, beat 1 to m. 98, beat 1 | 1787 | passing | major |
| m. 97, beat 1 to m. 98, beat 1 | 1789 | passing | major |
| m. 97, beat 1 to m. 98, beat 1 | 1793 | passing | major |
| m. 97, beat 1 to m. 98, beat 1 | 1797 | passing | major |
| m. 98, beat 1 to m. 99, beat 1 | 1805 | unclassified | minor7 |
| m. 98, beat 1 to m. 99, beat 1 | 1811 | unclassified | minor7 |
| m. 98, beat 1 to m. 99, beat 1 | 1817 | unclassified | minor7 |
| m. 98, beat 1 to m. 99, beat 1 | 1823 | unclassified | minor7 |
| m. 98, beat 1 to m. 99, beat 1 | 1829 | unclassified | minor7 |
| m. 98, beat 1 to m. 99, beat 1 | 1835 | unclassified | minor7 |
| m. 98, beat 1 to m. 99, beat 1 | 1841 | unclassified | minor7 |
| m. 98, beat 1 to m. 99, beat 1 | 1846 | unclassified | minor7 |
| m. 98, beat 1 to m. 99, beat 1 | 1847 | unclassified | minor7 |
| m. 102, beat 1 to m. 103, beat 1 | 1900 | unclassified | minor7 |
| m. 102, beat 1 to m. 103, beat 1 | 1902 | unclassified | minor7 |
| m. 102, beat 1 to m. 103, beat 1 | 1903 | unclassified | minor7 |
| m. 102, beat 1 to m. 103, beat 1 | 1904 | unclassified | minor7 |
| m. 102, beat 1 to m. 103, beat 1 | 1906 | unclassified | minor7 |
| m. 102, beat 1 to m. 103, beat 1 | 1907 | unclassified | minor7 |
| m. 102, beat 1 to m. 103, beat 1 | 1912 | unclassified | minor7 |
| m. 103, beat 1 to m. 103, beat 4 | 1916 | unclassified | V |
| m. 103, beat 1 to m. 103, beat 4 | 1920 | unclassified | V |
| m. 103, beat 1 to m. 103, beat 4 | 1922 | unclassified | V |
| m. 103, beat 1 to m. 103, beat 4 | 1924 | unclassified | V |
| m. 103, beat 4 to m. 104, beat 1 | 1930 | unclassified | minor7 |
| m. 104, beat 1 to m. 105, beat 1 | 1934 | passing | minor |
| m. 104, beat 1 to m. 105, beat 1 | 1936 | appoggiatura | minor |
| m. 104, beat 1 to m. 105, beat 1 | 1938 | passing | minor |
| m. 104, beat 1 to m. 105, beat 1 | 1939 | passing | minor |
| m. 104, beat 1 to m. 105, beat 1 | 1943 | unclassified | minor |
| m. 104, beat 1 to m. 105, beat 1 | 1945 | passing | minor |
| m. 104, beat 1 to m. 105, beat 1 | 1946 | passing | minor |
| m. 105, beat 2 to m. 105, beat 3 | 1953 | passing | major |
| m. 105, beat 2 to m. 105, beat 3 | 1955 | passing | major |
| m. 105, beat 3 to m. 106, beat 1 | 1957 | unclassified | major7 |
| m. 105, beat 3 to m. 106, beat 1 | 1958 | appoggiatura | major7 |
| m. 105, beat 3 to m. 106, beat 1 | 1960 | unclassified | major7 |
| m. 105, beat 3 to m. 106, beat 1 | 1963 | unclassified | major7 |
| m. 106, beat 1 to m. 107, beat 1 | 1976 | unclassified | minor |
| m. 106, beat 1 to m. 107, beat 1 | 1977 | appoggiatura | minor |
| m. 106, beat 1 to m. 107, beat 1 | 1978 | unclassified | minor |
| m. 106, beat 1 to m. 107, beat 1 | 1979 | unclassified | minor |
| m. 106, beat 1 to m. 107, beat 1 | 1980 | unclassified | minor |
| m. 106, beat 1 to m. 107, beat 1 | 1981 | unclassified | minor |
| m. 108, beat 1 to m. 109, beat 1 | 2003 | passing | minor |
| m. 108, beat 1 to m. 109, beat 1 | 2005 | unclassified | minor |
| m. 108, beat 1 to m. 109, beat 1 | 2007 | passing | minor |
| m. 108, beat 1 to m. 109, beat 1 | 2008 | passing | minor |
| m. 108, beat 1 to m. 109, beat 1 | 2012 | unclassified | minor |
| m. 108, beat 1 to m. 109, beat 1 | 2014 | passing | minor |
| m. 108, beat 1 to m. 109, beat 1 | 2015 | passing | minor |
| m. 109, beat 1 to m. 110, beat 1 | 2019 | passing | diminished |
| m. 109, beat 1 to m. 110, beat 1 | 2021 | unclassified | diminished |
| m. 109, beat 1 to m. 110, beat 1 | 2022 | passing | diminished |
| m. 109, beat 1 to m. 110, beat 1 | 2024 | passing | diminished |
| m. 109, beat 1 to m. 110, beat 1 | 2028 | escape-tone | diminished |
| m. 109, beat 1 to m. 110, beat 1 | 2030 | unclassified | diminished |
| m. 109, beat 1 to m. 110, beat 1 | 2031 | passing | diminished |
| m. 110, beat 1 to m. 111, beat 1 | 2035 | appoggiatura | major |
| m. 110, beat 1 to m. 111, beat 1 | 2036 | unclassified | major |
| m. 110, beat 1 to m. 111, beat 1 | 2038 | passing | major |
| m. 110, beat 1 to m. 111, beat 1 | 2040 | passing | major |
| m. 110, beat 1 to m. 111, beat 1 | 2042 | unclassified | major |
| m. 110, beat 1 to m. 111, beat 1 | 2044 | escape-tone | major |
| m. 110, beat 1 to m. 111, beat 1 | 2046 | passing | major |
| m. 110, beat 1 to m. 111, beat 1 | 2047 | passing | major |
| m. 111, beat 1 to m. 112, beat 1 | 2051 | appoggiatura | minor |
| m. 111, beat 1 to m. 112, beat 1 | 2052 | unclassified | minor |
| m. 111, beat 1 to m. 112, beat 1 | 2054 | passing | minor |
| m. 111, beat 1 to m. 112, beat 1 | 2056 | passing | minor |
| m. 111, beat 1 to m. 112, beat 1 | 2058 | unclassified | minor |
| m. 111, beat 1 to m. 112, beat 1 | 2060 | escape-tone | minor |
| m. 111, beat 1 to m. 112, beat 1 | 2062 | passing | minor |
| m. 111, beat 1 to m. 112, beat 1 | 2063 | passing | minor |
| m. 112, beat 1 to m. 113, beat 1 | 2067 | appoggiatura | minor |
| m. 112, beat 1 to m. 113, beat 1 | 2068 | unclassified | minor |
| m. 112, beat 1 to m. 113, beat 1 | 2070 | passing | minor |
| m. 112, beat 1 to m. 113, beat 1 | 2072 | passing | minor |
| m. 112, beat 1 to m. 113, beat 1 | 2074 | unclassified | minor |
| m. 112, beat 1 to m. 113, beat 1 | 2076 | escape-tone | minor |
| m. 112, beat 1 to m. 113, beat 1 | 2078 | passing | minor |
| m. 112, beat 1 to m. 113, beat 1 | 2079 | passing | minor |
| m. 113, beat 1 to m. 113, beat 2 | 2084 | unclassified | major |
| m. 113, beat 2 to m. 113, beat 4 | 2086 | unclassified | augmented |
| m. 113, beat 2 to m. 113, beat 4 | 2088 | passing | augmented |
| m. 113, beat 2 to m. 113, beat 4 | 2090 | unclassified | augmented |
| m. 113, beat 2 to m. 113, beat 4 | 2092 | unclassified | augmented |
| m. 113, beat 4 to m. 114, beat 1 | 2094 | passing | major7 |
| m. 114, beat 1 to m. 114, beat 3 | 2100 | passing | ii |
| m. 114, beat 1 to m. 114, beat 3 | 2102 | passing | ii |
| m. 114, beat 1 to m. 114, beat 3 | 2104 | unclassified | ii |
| m. 114, beat 3 to m. 115, beat 1 | 2109 | passing | dominant7 |
| m. 114, beat 3 to m. 115, beat 1 | 2111 | passing | dominant7 |
| m. 114, beat 3 to m. 115, beat 1 | 2114 | passing | dominant7 |
| m. 116, beat 1 to m. 116, beat 3 | 2130 | neighbor | V |
| m. 116, beat 1 to m. 116, beat 3 | 2131 | passing | V |
| m. 118, beat 1 to m. 118, beat 3 | 2153 | neighbor | V |
| m. 118, beat 1 to m. 118, beat 3 | 2156 | passing | V |
| m. 118, beat 1 to m. 118, beat 3 | 2157 | neighbor | V |
| m. 118, beat 1 to m. 118, beat 3 | 2159 | neighbor | V |
| m. 119, beat 1 to m. 120, beat 1 | 2167 | passing | IV |
| m. 119, beat 1 to m. 120, beat 1 | 2169 | passing | IV |
| m. 119, beat 1 to m. 120, beat 1 | 2170 | passing | IV |
| m. 119, beat 1 to m. 120, beat 1 | 2172 | passing | IV |
| m. 119, beat 1 to m. 120, beat 1 | 2174 | passing | IV |
| m. 119, beat 1 to m. 120, beat 1 | 2176 | unclassified | IV |
| m. 119, beat 1 to m. 120, beat 1 | 2179 | appoggiatura | IV |
| m. 119, beat 1 to m. 120, beat 1 | 2181 | passing | IV |
| m. 120, beat 1 to m. 121, beat 1 | 2186 | passing | I |
| m. 120, beat 1 to m. 121, beat 1 | 2187 | passing | I |
| m. 120, beat 1 to m. 121, beat 1 | 2189 | passing | I |
| m. 120, beat 1 to m. 121, beat 1 | 2191 | passing | I |
| m. 120, beat 1 to m. 121, beat 1 | 2193 | passing | I |
| m. 120, beat 1 to m. 121, beat 1 | 2195 | unclassified | I |
| m. 120, beat 1 to m. 121, beat 1 | 2199 | passing | I |
| m. 120, beat 1 to m. 121, beat 1 | 2200 | passing | I |
| m. 121, beat 1 to m. 121, beat 2 | 2205 | passing | minor7 |
| m. 121, beat 2 to m. 122, beat 1 | 2208 | passing | vii° |
| m. 121, beat 2 to m. 122, beat 1 | 2210 | passing | vii° |
| m. 121, beat 2 to m. 122, beat 1 | 2212 | passing | vii° |
| m. 121, beat 2 to m. 122, beat 1 | 2214 | unclassified | vii° |
| m. 121, beat 2 to m. 122, beat 1 | 2218 | passing | vii° |
| m. 121, beat 2 to m. 122, beat 1 | 2219 | passing | vii° |
| m. 122, beat 1 to m. 123, beat 1 | 2224 | passing | I |
| m. 122, beat 1 to m. 123, beat 1 | 2226 | passing | I |
| m. 122, beat 1 to m. 123, beat 1 | 2227 | passing | I |
| m. 122, beat 1 to m. 123, beat 1 | 2229 | passing | I |
| m. 122, beat 1 to m. 123, beat 1 | 2231 | passing | I |
| m. 122, beat 1 to m. 123, beat 1 | 2233 | passing | I |
| m. 122, beat 1 to m. 123, beat 1 | 2234 | passing | I |
| m. 122, beat 1 to m. 123, beat 1 | 2236 | passing | I |
| m. 123, beat 1 to m. 124, beat 1 | 2240 | passing | I |
| m. 123, beat 1 to m. 124, beat 1 | 2242 | appoggiatura | I |
| m. 123, beat 1 to m. 124, beat 1 | 2244 | passing | I |
| m. 123, beat 1 to m. 124, beat 1 | 2245 | passing | I |
| m. 123, beat 1 to m. 124, beat 1 | 2247 | passing | I |
| m. 123, beat 1 to m. 124, beat 1 | 2248 | passing | I |
| m. 123, beat 1 to m. 124, beat 1 | 2252 | unclassified | I |
| m. 123, beat 1 to m. 124, beat 1 | 2254 | passing | I |
| m. 124, beat 1 to m. 125, beat 1 | 2259 | passing | V |
| m. 124, beat 1 to m. 125, beat 1 | 2261 | appoggiatura | V |
| m. 124, beat 1 to m. 125, beat 1 | 2262 | passing | V |
| m. 124, beat 1 to m. 125, beat 1 | 2264 | passing | V |
| m. 124, beat 1 to m. 125, beat 1 | 2266 | passing | V |
| m. 124, beat 1 to m. 125, beat 1 | 2268 | escape-tone | V |
| m. 124, beat 1 to m. 125, beat 1 | 2271 | neighbor | V |
| m. 124, beat 1 to m. 125, beat 1 | 2273 | passing | V |
| m. 125, beat 1 to m. 125, beat 2 | 2278 | passing | minor7 |
| m. 126, beat 1 to m. 127, beat 1 | 2297 | passing | V |
| m. 126, beat 1 to m. 127, beat 1 | 2299 | appoggiatura | V |
| m. 126, beat 1 to m. 127, beat 1 | 2301 | passing | V |
| m. 126, beat 1 to m. 127, beat 1 | 2302 | passing | V |
| m. 126, beat 1 to m. 127, beat 1 | 2304 | passing | V |
| m. 126, beat 1 to m. 127, beat 1 | 2305 | passing | V |
| m. 126, beat 1 to m. 127, beat 1 | 2309 | neighbor | V |
| m. 126, beat 1 to m. 127, beat 1 | 2311 | passing | V |
| m. 127, beat 1 to m. 128, beat 1 | 2317 | passing | vi |
| m. 127, beat 1 to m. 128, beat 1 | 2319 | passing | vi |
| m. 127, beat 1 to m. 128, beat 1 | 2321 | passing | vi |
| m. 127, beat 1 to m. 128, beat 1 | 2322 | passing | vi |
| m. 127, beat 1 to m. 128, beat 1 | 2325 | passing | vi |
| m. 127, beat 1 to m. 128, beat 1 | 2326 | passing | vi |
| m. 127, beat 1 to m. 128, beat 1 | 2328 | passing | vi |
| m. 127, beat 1 to m. 128, beat 1 | 2330 | unclassified | vi |
| m. 128, beat 3 to m. 128, beat 4 | 2341 | passing | minor7 |
| m. 128, beat 4 to m. 128, beat 4.5 | 2346 | passing | ii |
| m. 132, beat 1 to m. 133, beat 1 | 2412 | passing | IV |
| m. 132, beat 1 to m. 133, beat 1 | 2417 | unclassified | IV |
| m. 133, beat 1 to m. 134, beat 1 | 2424 | neighbor | dominant7 |
| m. 133, beat 1 to m. 134, beat 1 | 2427 | neighbor | dominant7 |
| m. 133, beat 1 to m. 134, beat 1 | 2429 | unclassified | dominant7 |
| m. 133, beat 1 to m. 134, beat 1 | 2430 | passing | dominant7 |
| m. 133, beat 1 to m. 134, beat 1 | 2436 | unclassified | dominant7 |
| m. 133, beat 1 to m. 134, beat 1 | 2440 | unclassified | dominant7 |
| m. 134, beat 1 to m. 135, beat 1 | 2459 | passing | I |
| m. 134, beat 1 to m. 135, beat 1 | 2463 | passing | I |
| m. 135, beat 1 to m. 136, beat 1 | 2471 | neighbor | dominant7 |
| m. 135, beat 1 to m. 136, beat 1 | 2474 | neighbor | dominant7 |
| m. 135, beat 1 to m. 136, beat 1 | 2476 | unclassified | dominant7 |
| m. 135, beat 1 to m. 136, beat 1 | 2477 | passing | dominant7 |
| m. 135, beat 1 to m. 136, beat 1 | 2483 | unclassified | dominant7 |
| m. 135, beat 1 to m. 136, beat 1 | 2487 | unclassified | dominant7 |
| m. 140, beat 1 to m. 141, beat 1 | 2566 | unclassified | minor |
| m. 140, beat 1 to m. 141, beat 1 | 2572 | neighbor | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2579 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2577 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2582 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2580 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2585 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2583 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2588 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2586 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2589 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2593 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2591 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2596 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2594 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2597 | neighbor | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2599 | unclassified | minor |
| m. 141, beat 1 to m. 142, beat 1 | 2601 | unclassified | minor |
| m. 142, beat 1 to m. 143, beat 1 | 2605 | passing | major |
| m. 142, beat 1 to m. 143, beat 1 | 2607 | passing | major |
| m. 142, beat 1 to m. 143, beat 1 | 2611 | passing | major |
| m. 142, beat 1 to m. 143, beat 1 | 2615 | passing | major |
| m. 142, beat 1 to m. 143, beat 1 | 2619 | passing | major |
| m. 142, beat 1 to m. 143, beat 1 | 2621 | passing | major |
| m. 142, beat 1 to m. 143, beat 1 | 2625 | passing | major |
| m. 142, beat 1 to m. 143, beat 1 | 2629 | passing | major |
| m. 143, beat 1 to m. 144, beat 1 | 2637 | unclassified | minor7 |
| m. 143, beat 1 to m. 144, beat 1 | 2643 | unclassified | minor7 |
| m. 143, beat 1 to m. 144, beat 1 | 2649 | unclassified | minor7 |
| m. 143, beat 1 to m. 144, beat 1 | 2655 | unclassified | minor7 |
| m. 143, beat 1 to m. 144, beat 1 | 2661 | unclassified | minor7 |
| m. 143, beat 1 to m. 144, beat 1 | 2667 | unclassified | minor7 |
| m. 143, beat 1 to m. 144, beat 1 | 2673 | unclassified | minor7 |
| m. 143, beat 1 to m. 144, beat 1 | 2678 | unclassified | minor7 |
| m. 143, beat 1 to m. 144, beat 1 | 2679 | unclassified | minor7 |

## Cadences

The library's standard-harmony analysis found 13 cadences from the chords, the meter and the soprano. Position is where the arrival begins. Key is the key region of the arrival. Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added before a section start.

| Position | Type | Key | Soprano | Beat strength | Boundary |
| --- | --- | --- | --- | --- | --- |
| m. 27, beat 1 | perfect-authentic | G major | 1 | strong | no |
| m. 28, beat 1 | perfect-authentic | G major | 1 | strong | no |
| m. 55, beat 1 | perfect-authentic | G major | 1 | strong | no |
| m. 56, beat 1 | perfect-authentic | G major | 1 | strong | no |
| m. 63, beat 1 | authentic-unspecified | D minor | 3 | strong | no |
| m. 70, beat 1 | perfect-authentic | F major | 1 | strong | no |
| m. 100, beat 1 | perfect-authentic | C major | 1 | strong | no |
| m. 101, beat 1 | perfect-authentic | C major | 1 | strong | no |
| m. 103, beat 1 | half | C major | 5 | strong | no |
| m. 108, beat 1 | authentic-unspecified | D minor | 3 | strong | no |
| m. 115, beat 1 | perfect-authentic | F major | 1 | strong | no |
| m. 145, beat 1 | perfect-authentic | C major | 1 | strong | no |
| m. 146, beat 1 | perfect-authentic | C major | 1 | strong | no |

### Cadence plan

1. m. 27, beat 1: perfect-authentic in G major
2. m. 28, beat 1: perfect-authentic in G major
3. m. 55, beat 1: perfect-authentic in G major
4. m. 56, beat 1: perfect-authentic in G major
5. m. 63, beat 1: authentic-unspecified in D minor
6. m. 70, beat 1: perfect-authentic in F major
7. m. 100, beat 1: perfect-authentic in C major
8. m. 101, beat 1: perfect-authentic in C major
9. m. 103, beat 1: half in C major
10. m. 108, beat 1: authentic-unspecified in D minor
11. m. 115, beat 1: perfect-authentic in F major
12. m. 145, beat 1: perfect-authentic in C major
13. m. 146, beat 1: perfect-authentic in C major

**Counts:** perfect-authentic 10, authentic-unspecified 2, half 1.

## Phrases

The library's standard-form analysis found 21 phrases from the soprano, the meter and the cadences. Length counts the measures of the phrase proper; Ends is what closed it: its cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in the piece; Expansion is a lengthening or shortening of the phrase's ending, or an extension after its cadence, with the measures it adds or removes.

| Phrase | Measures | Length | Cadence | Ends | Role | Expansion |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 9 | 9 | none | discontinuity | beginning | - |
| 2 | mm. 10 to 21 | 12 | none | discontinuity | middle | - |
| 3 | mm. 22 to 27 | 6 | perfect-authentic (m. 27, beat 1) | cadence | middle | contraction -2 (mm. 26 to 27) |
| 4 | mm. 28 to 29 | 2 | perfect-authentic (m. 28, beat 1) | cadence | middle | - |
| 5 | mm. 30 to 37 | 8 | none | discontinuity | middle | - |
| 6 | mm. 38 to 49 | 12 | none | discontinuity | middle | - |
| 7 | mm. 50 to 55 | 6 | perfect-authentic (m. 55, beat 1) | cadence | middle | contraction -2 (mm. 54 to 55) |
| 8 | m. 56 | 1 | perfect-authentic (m. 56, beat 1) | cadence | middle | - |
| 9 | mm. 57 to 63 | 7 | authentic-unspecified (m. 63, beat 1) | cadence | middle | - |
| 10 | mm. 64 to 70 | 7 | perfect-authentic (m. 70, beat 1) | cadence | middle | contraction -1 (mm. 68 to 70) |
| 11 | mm. 71 to 82 | 12 | none | discontinuity | middle | - |
| 12 | mm. 83 to 94 | 12 | none | discontinuity | middle | - |
| 13 | mm. 95 to 100 | 6 | perfect-authentic (m. 100, beat 1) | cadence | middle | contraction -2 (mm. 99 to 100) |
| 14 | m. 101 | 1 | perfect-authentic (m. 101, beat 1) | cadence | middle | - |
| 15 | mm. 102 to 103 | 2 | half (m. 103, beat 1) | cadence | middle | - |
| 16 | mm. 104 to 108 | 5 | authentic-unspecified (m. 108, beat 1) | cadence | middle | - |
| 17 | mm. 109 to 115 | 7 | perfect-authentic (m. 115, beat 1) | cadence | middle | contraction -1 (mm. 113 to 115) |
| 18 | mm. 116 to 127 | 12 | none | discontinuity | middle | - |
| 19 | mm. 128 to 139 | 12 | none | discontinuity | middle | - |
| 20 | mm. 140 to 145 | 6 | perfect-authentic (m. 145, beat 1) | cadence | middle | contraction -2 (mm. 144 to 145) |
| 21 | m. 146 | 1 | perfect-authentic (m. 146, beat 1) | cadence | ending | - |

- **Phrase 1:** ideas basic-idea (mm. 1 to 2), contrasting-idea (mm. 3 to 4), contrasting-idea (mm. 5 to 6), contrasting-idea (mm. 7 to 9); subphrases mm. 1 to 4, mm. 5 to 9
- **Phrase 2:** ideas basic-idea (mm. 10 to 11), contrasting-idea (mm. 12 to 13), contrasting-idea (mm. 14 to 15), contrasting-idea (mm. 16 to 17), contrasting-idea (mm. 18 to 19), contrasting-idea (mm. 20 to 21); subphrases mm. 10 to 13, mm. 14 to 21
- **Phrase 3:** ideas basic-idea (mm. 22 to 23), contrasting-idea (mm. 24 to 25), contrasting-idea (mm. 26 to 27); subphrases mm. 22 to 25, mm. 26 to 27; phrase model Tb (m. 22, beat 1), Tb (m. 23, beat 1), D (m. 26, beat 4), Te (m. 27, beat 1)
- **Phrase 4:** ideas basic-idea (mm. 28 to 29)
- **Phrase 5:** ideas basic-idea (mm. 30 to 31), contrasting-idea (mm. 32 to 33), contrasting-idea (mm. 34 to 35), contrasting-idea (mm. 36 to 37); subphrases mm. 30 to 33, mm. 34 to 37
- **Phrase 6:** ideas basic-idea (mm. 38 to 39), contrasting-idea (mm. 40 to 41), contrasting-idea (mm. 42 to 43), contrasting-idea (mm. 44 to 45), contrasting-idea (mm. 46 to 47), contrasting-idea (mm. 48 to 49); subphrases mm. 38 to 41, mm. 42 to 49
- **Phrase 7:** ideas basic-idea (mm. 50 to 51), contrasting-idea (mm. 52 to 53), contrasting-idea (mm. 54 to 55); subphrases mm. 50 to 53, mm. 54 to 55; phrase model Tb (m. 50, beat 1), Tb (m. 51, beat 1), D (m. 54, beat 4), Te (m. 55, beat 1)
- **Phrase 8:** ideas basic-idea (m. 56)
- **Phrase 9:** ideas basic-idea (mm. 57 to 58), contrasting-idea (mm. 59 to 60), contrasting-idea (mm. 61 to 63); subphrases mm. 57 to 60, mm. 61 to 63
- **Phrase 10:** ideas basic-idea (mm. 64 to 65), contrasting-idea (mm. 66 to 67), contrasting-idea (mm. 68 to 70); subphrases mm. 64 to 67, mm. 68 to 70; phrase model PD (m. 69, beat 1), D (m. 69, beat 3), Te (m. 70, beat 1)
- **Phrase 11:** ideas basic-idea (mm. 71 to 72), contrasting-idea (mm. 73 to 74), contrasting-idea (mm. 75 to 76), contrasting-idea (mm. 77 to 78), contrasting-idea (mm. 79 to 80), contrasting-idea (mm. 81 to 82); subphrases mm. 71 to 74, mm. 75 to 82
- **Phrase 12:** ideas basic-idea (mm. 83 to 84), contrasting-idea (mm. 85 to 86), contrasting-idea (mm. 87 to 88), contrasting-idea (mm. 89 to 90), contrasting-idea (mm. 91 to 92), contrasting-idea (mm. 93 to 94); subphrases mm. 83 to 86, mm. 87 to 94
- **Phrase 13:** ideas basic-idea (mm. 95 to 96), contrasting-idea (mm. 97 to 98), contrasting-idea (mm. 99 to 100); subphrases mm. 95 to 98, mm. 99 to 100; phrase model Tb (m. 95, beat 1), D (m. 99, beat 4), Te (m. 100, beat 1)
- **Phrase 14:** ideas basic-idea (m. 101)
- **Phrase 15:** ideas basic-idea (mm. 102 to 103); phrase model D (m. 103, beat 1)
- **Phrase 16:** ideas basic-idea (mm. 104 to 105), contrasting-idea (mm. 106 to 108)
- **Phrase 17:** ideas basic-idea (mm. 109 to 110), contrasting-idea (mm. 111 to 112), contrasting-idea (mm. 113 to 115); subphrases mm. 109 to 112, mm. 113 to 115; phrase model PD (m. 114, beat 1), D (m. 114, beat 3), Te (m. 115, beat 1)
- **Phrase 18:** ideas basic-idea (mm. 116 to 117), contrasting-idea (mm. 118 to 119), contrasting-idea (mm. 120 to 121), contrasting-idea (mm. 122 to 123), contrasting-idea (mm. 124 to 125), contrasting-idea (mm. 126 to 127); subphrases mm. 116 to 119, mm. 120 to 127
- **Phrase 19:** ideas basic-idea (mm. 128 to 129), contrasting-idea (mm. 130 to 131), contrasting-idea (mm. 132 to 133), contrasting-idea (mm. 134 to 135), contrasting-idea (mm. 136 to 137), contrasting-idea (mm. 138 to 139); subphrases mm. 128 to 131, mm. 132 to 139
- **Phrase 20:** ideas basic-idea (mm. 140 to 141), contrasting-idea (mm. 142 to 143), contrasting-idea (mm. 144 to 145); subphrases mm. 140 to 143, mm. 144 to 145; phrase model Tb (m. 140, beat 1), D (m. 144, beat 4), Te (m. 145, beat 1)
- **Phrase 21:** ideas basic-idea (m. 146)

## Themes

The library's standard-form analysis found 6 themes from the phrases, their ideas and the cadences. Form is the library's name for the pairing of the two parts; Commonality is how common that pairing is; Traits are what the library finds in the continuation.

| Theme | Form | Commonality | Measures | Length | Parts | Traits |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | hybrid | slightly-less-common | mm. 22 to 27 | 6 | compound-basic-idea (mm. 22 to 25), continuation (mm. 26 to 27, perfect-authentic (m. 27, beat 1)) | - |
| 2 | hybrid | slightly-less-common | mm. 50 to 55 | 6 | compound-basic-idea (mm. 50 to 53), continuation (mm. 54 to 55, perfect-authentic (m. 55, beat 1)) | - |
| 3 | hybrid | slightly-less-common | mm. 64 to 70 | 7 | compound-basic-idea (mm. 64 to 67), continuation (mm. 68 to 70, perfect-authentic (m. 70, beat 1)) | - |
| 4 | hybrid | slightly-less-common | mm. 95 to 100 | 6 | compound-basic-idea (mm. 95 to 98), continuation (mm. 99 to 100, perfect-authentic (m. 100, beat 1)) | - |
| 5 | hybrid | slightly-less-common | mm. 109 to 115 | 7 | compound-basic-idea (mm. 109 to 112), continuation (mm. 113 to 115, perfect-authentic (m. 115, beat 1)) | - |
| 6 | hybrid | slightly-less-common | mm. 140 to 145 | 6 | compound-basic-idea (mm. 140 to 143), continuation (mm. 144 to 145, perfect-authentic (m. 145, beat 1)) | - |

## Form plan

The kernel's form structure detection grouped the phrases into 10 sections, closing a section at each phrase that ends on a perfect or imperfect authentic cadence; the phrases after the last such cadence form the last section. Sections are lettered by first appearance, and a section whose opening theme returns an earlier section's material takes that letter with a prime. Key is the key the section opens in.

| Section | Measures | Key | Phrases | Closing cadence | Returns |
| --- | --- | --- | --- | --- | --- |
| A | mm. 1 to 27 | C major | 1 to 3 | perfect-authentic (m. 27, beat 1) | - |
| B | mm. 28 to 29 | G major | 4 | perfect-authentic (m. 28, beat 1) | - |
| C | mm. 30 to 55 | C major | 5 to 7 | perfect-authentic (m. 55, beat 1) | - |
| D | m. 56 | G major | 8 | perfect-authentic (m. 56, beat 1) | - |
| E | mm. 57 to 70 | G major | 9 to 10 | perfect-authentic (m. 70, beat 1) | - |
| F | mm. 71 to 100 | F major | 11 to 13 | perfect-authentic (m. 100, beat 1) | - |
| G | m. 101 | C major | 14 | perfect-authentic (m. 101, beat 1) | - |
| H | mm. 102 to 115 | C major | 15 to 17 | perfect-authentic (m. 115, beat 1) | - |
| I | mm. 116 to 145 | F major | 18 to 20 | perfect-authentic (m. 145, beat 1) | - |
| J | m. 146 | C major | 21 | perfect-authentic (m. 146, beat 1) | - |

### Thematic returns

A later theme recalls an earlier one when the kernel's thematic return detection finds the same contour and rhythm in a stretch of both. Similarity is the lower of the two scores. Themes that share a phrase are not compared.

| Later theme | Measures | Section | Earlier theme | Measures | Section | Similarity |
| --- | --- | --- | --- | --- | --- | --- |
| 2 | mm. 50 to 55 | C | 1 | mm. 22 to 27 | A | 1.00 |
| 5 | mm. 109 to 115 | H | 3 | mm. 64 to 70 | E | 1.00 |
| 6 | mm. 140 to 145 | I | 4 | mm. 95 to 100 | F | 1.00 |

### Outline

- form (mm. 1 to 146)
  - section A (mm. 1 to 27, C major)
    - phrase 1 (mm. 1 to 9)
      - subphrase 1 (mm. 1 to 4)
        - idea basic-idea (mm. 1 to 2)
        - idea contrasting-idea (mm. 3 to 4)
      - subphrase 2 (mm. 5 to 9)
        - idea contrasting-idea (mm. 5 to 6)
        - idea contrasting-idea (mm. 7 to 9)
    - phrase 2 (mm. 10 to 21)
      - subphrase 1 (mm. 10 to 13)
        - idea basic-idea (mm. 10 to 11)
        - idea contrasting-idea (mm. 12 to 13)
      - subphrase 2 (mm. 14 to 21)
        - idea contrasting-idea (mm. 14 to 15)
        - idea contrasting-idea (mm. 16 to 17)
        - idea contrasting-idea (mm. 18 to 19)
        - idea contrasting-idea (mm. 20 to 21)
    - phrase 3 (mm. 22 to 27)
      - subphrase 1 (mm. 22 to 25)
        - idea basic-idea (mm. 22 to 23)
        - idea contrasting-idea (mm. 24 to 25)
      - subphrase 2 (mm. 26 to 27)
        - idea contrasting-idea (mm. 26 to 27)
  - section B (mm. 28 to 29, G major)
    - phrase 4 (mm. 28 to 29)
      - idea basic-idea (mm. 28 to 29)
  - section C (mm. 30 to 55, C major)
    - phrase 5 (mm. 30 to 37)
      - subphrase 1 (mm. 30 to 33)
        - idea basic-idea (mm. 30 to 31)
        - idea contrasting-idea (mm. 32 to 33)
      - subphrase 2 (mm. 34 to 37)
        - idea contrasting-idea (mm. 34 to 35)
        - idea contrasting-idea (mm. 36 to 37)
    - phrase 6 (mm. 38 to 49)
      - subphrase 1 (mm. 38 to 41)
        - idea basic-idea (mm. 38 to 39)
        - idea contrasting-idea (mm. 40 to 41)
      - subphrase 2 (mm. 42 to 49)
        - idea contrasting-idea (mm. 42 to 43)
        - idea contrasting-idea (mm. 44 to 45)
        - idea contrasting-idea (mm. 46 to 47)
        - idea contrasting-idea (mm. 48 to 49)
    - phrase 7 (mm. 50 to 55)
      - subphrase 1 (mm. 50 to 53)
        - idea basic-idea (mm. 50 to 51)
        - idea contrasting-idea (mm. 52 to 53)
      - subphrase 2 (mm. 54 to 55)
        - idea contrasting-idea (mm. 54 to 55)
  - section D (m. 56, G major)
    - phrase 8 (m. 56)
      - idea basic-idea (m. 56)
  - section E (mm. 57 to 70, G major)
    - phrase 9 (mm. 57 to 63)
      - subphrase 1 (mm. 57 to 60)
        - idea basic-idea (mm. 57 to 58)
        - idea contrasting-idea (mm. 59 to 60)
      - subphrase 2 (mm. 61 to 63)
        - idea contrasting-idea (mm. 61 to 63)
    - phrase 10 (mm. 64 to 70)
      - subphrase 1 (mm. 64 to 67)
        - idea basic-idea (mm. 64 to 65)
        - idea contrasting-idea (mm. 66 to 67)
      - subphrase 2 (mm. 68 to 70)
        - idea contrasting-idea (mm. 68 to 70)
  - section F (mm. 71 to 100, F major)
    - phrase 11 (mm. 71 to 82)
      - subphrase 1 (mm. 71 to 74)
        - idea basic-idea (mm. 71 to 72)
        - idea contrasting-idea (mm. 73 to 74)
      - subphrase 2 (mm. 75 to 82)
        - idea contrasting-idea (mm. 75 to 76)
        - idea contrasting-idea (mm. 77 to 78)
        - idea contrasting-idea (mm. 79 to 80)
        - idea contrasting-idea (mm. 81 to 82)
    - phrase 12 (mm. 83 to 94)
      - subphrase 1 (mm. 83 to 86)
        - idea basic-idea (mm. 83 to 84)
        - idea contrasting-idea (mm. 85 to 86)
      - subphrase 2 (mm. 87 to 94)
        - idea contrasting-idea (mm. 87 to 88)
        - idea contrasting-idea (mm. 89 to 90)
        - idea contrasting-idea (mm. 91 to 92)
        - idea contrasting-idea (mm. 93 to 94)
    - phrase 13 (mm. 95 to 100)
      - subphrase 1 (mm. 95 to 98)
        - idea basic-idea (mm. 95 to 96)
        - idea contrasting-idea (mm. 97 to 98)
      - subphrase 2 (mm. 99 to 100)
        - idea contrasting-idea (mm. 99 to 100)
  - section G (m. 101, C major)
    - phrase 14 (m. 101)
      - idea basic-idea (m. 101)
  - section H (mm. 102 to 115, C major)
    - phrase 15 (mm. 102 to 103)
      - idea basic-idea (mm. 102 to 103)
    - phrase 16 (mm. 104 to 108)
      - idea basic-idea (mm. 104 to 105)
      - idea contrasting-idea (mm. 106 to 108)
    - phrase 17 (mm. 109 to 115)
      - subphrase 1 (mm. 109 to 112)
        - idea basic-idea (mm. 109 to 110)
        - idea contrasting-idea (mm. 111 to 112)
      - subphrase 2 (mm. 113 to 115)
        - idea contrasting-idea (mm. 113 to 115)
  - section I (mm. 116 to 145, F major)
    - phrase 18 (mm. 116 to 127)
      - subphrase 1 (mm. 116 to 119)
        - idea basic-idea (mm. 116 to 117)
        - idea contrasting-idea (mm. 118 to 119)
      - subphrase 2 (mm. 120 to 127)
        - idea contrasting-idea (mm. 120 to 121)
        - idea contrasting-idea (mm. 122 to 123)
        - idea contrasting-idea (mm. 124 to 125)
        - idea contrasting-idea (mm. 126 to 127)
    - phrase 19 (mm. 128 to 139)
      - subphrase 1 (mm. 128 to 131)
        - idea basic-idea (mm. 128 to 129)
        - idea contrasting-idea (mm. 130 to 131)
      - subphrase 2 (mm. 132 to 139)
        - idea contrasting-idea (mm. 132 to 133)
        - idea contrasting-idea (mm. 134 to 135)
        - idea contrasting-idea (mm. 136 to 137)
        - idea contrasting-idea (mm. 138 to 139)
    - phrase 20 (mm. 140 to 145)
      - subphrase 1 (mm. 140 to 143)
        - idea basic-idea (mm. 140 to 141)
        - idea contrasting-idea (mm. 142 to 143)
      - subphrase 2 (mm. 144 to 145)
        - idea contrasting-idea (mm. 144 to 145)
  - section J (m. 146, C major)
    - phrase 21 (m. 146)
      - idea basic-idea (m. 146)

## Motives

The library's motive analysis found 116 motive families in the soprano, the highest sounding note at each attack. A family is a short figure that recurs; Notes is the length of the figure, Pattern whether its occurrences match by scale steps or by semitones, Measures the measures the occurrences start in, Phrases the phrases they lie in (an occurrence across a phrase boundary lies in none), and Transformations how many occurrences are of each kind the library names against the first occurrence (exact, real-transposed, tonal-transposed, inverted, retrograde, augmented, diminished, fragmented, varied).

| Family | Notes | Degrees | Pattern | Occurrences | Measures | Phrases | Transformations |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 8 | 1 3 5 7 1 2 1 6 | scale steps | 4 | 1, 29, 70, 115 | 1 | exact 2, real-transposed 2 |
| 2 | 8 | 3 5 7 1 2 1 6 5 | scale steps | 4 | 1, 29, 70, 115 | 1 | exact 2, real-transposed 2 |
| 3 | 8 | 5 7 1 2 1 6 5 1 | scale steps | 4 | 1, 29, 70, 115 | 1 | exact 2, real-transposed 2 |
| 4 | 8 | 7 1 2 1 6 5 1 5 | scale steps | 4 | 2, 30, 71, 116 | 1, 5, 11, 18 | exact 2, real-transposed 2 |
| 5 | 8 | 1 6 5 1 5 4 5 4 | scale steps | 4 | 2, 30, 71, 116 | 1, 5, 11, 18 | exact 2, real-transposed 2 |
| 6 | 8 | 3 3 5 6 7 1 2 3 | scale steps | 4 | 4, 32, 73, 118 | 1, 5, 11, 18 | exact 2, real-transposed 2 |
| 7 | 8 | 3 5 6 7 1 2 3 4 | scale steps | 6 | 4, 7, 32, 35, 73, 118 | 1, 5, 11, 18 | exact 2, fragmented 2, real-transposed 2 |
| 8 | 8 | 6 7 1 2 3 4 5 6 | scale steps | 20 | 5, 6, 7, 8, 9, 33, 34, 35, 36, 37, 74, 75, 76, 77, 82, 119, 120, 121, 122, 127 | 1, 5, 11, 18 | exact 2, tonal-transposed 12, fragmented 4, real-transposed 2 |
| 9 | 8 | 2 3 4 5 6 5 4 3 | scale steps | 28 | 5, 6, 7, 8, 10, 33, 34, 35, 36, 38, 74, 75, 76, 77, 78, 79, 80, 81, 83, 119, 120, 121, 122, 123, 124, 125, 126, 128 | 1, 2, 5, 6, 11, 12, 18, 19 | exact 4, tonal-transposed 14, real-transposed 6, fragmented 4 |
| 10 | 8 | 6 5 4 3 2 1 7 6 | scale steps | 20 | 5, 6, 7, 8, 33, 34, 35, 36, 67, 74, 75, 76, 77, 97, 112, 119, 120, 121, 122, 142 | 1, 5, 10, 11, 13, 17, 18, 20 | exact 2, tonal-transposed 14, fragmented 2, real-transposed 2 |
| 11 | 8 | 2 1 7 6 5 6 7 1 | scale steps | 14 | 5, 6, 7, 8, 33, 34, 35, 36, 74, 75, 76, 119, 120, 121 | 1, 5, 11, 18 | exact 2, tonal-transposed 10, real-transposed 2 |
| 12 | 8 | 6 7 1 2 3 #4 b6 6 | scale steps | 6 | 9, 37, 63, 82, 108, 127 | 1, 5, 9, 11, 16, 18 | exact 2, fragmented 4 |
| 13 | 8 | 2 3 #4 b6 6 3 #4 b6 | scale steps | 4 | 9, 37, 82, 127 | 1, 5, 11, 18 | exact 4 |
| 14 | 8 | 6 3 #4 b6 6 7 1 2 | scale steps | 4 | 9, 37, 82, 127 | 1, 5, 11, 18 | exact 4 |
| 15 | 8 | 6 7 1 2 3 #4 5 #4 | scale steps | 4 | 9, 37, 82, 127 | - | exact 4 |
| 16 | 8 | 2 3 4 3 2 1 b7 6 | scale steps | 12 | 10, 38, 66, 83, 97, 111, 128, 142 | 2, 6, 10, 12, 13, 17, 19, 20 | exact 4, tonal-transposed 6, fragmented 2 |
| 17 | 8 | 2 1 b7 6 b7 1 2 1 | scale steps | 4 | 10, 38, 83, 128 | 2, 6, 12, 19 | exact 4 |
| 18 | 3 | 5 1 6 | scale steps | 4 | 11, 39, 84, 129 | 2, 6, 12, 19 | exact 4 |
| 19 | 8 | #4 5 #4 5 #4 5 #4 5 | scale steps | 2 | 13, 41 | 2, 6 | exact 2 |
| 20 | 8 | #4 5 #4 5 4 5 4 5 | scale steps | 2 | 13, 41 | 2, 6 | exact 2 |
| 21 | 8 | 4 5 4 5 4 5 4 5 | scale steps | 6 | 13, 41, 86, 131 | 2, 6, 12, 19 | exact 2, tonal-transposed 2, real-transposed 2 |
| 22 | 8 | 4 5 4 5 5 3 1 2 | scale steps | 2 | 13, 41 | 2, 6 | exact 2 |
| 23 | 8 | 5 3 1 2 3 2 1 2 | scale steps | 8 | 14, 16, 42, 44, 87, 89, 132, 134 | 2, 6, 12, 19 | exact 4, real-transposed 4 |
| 24 | 8 | 1 2 3 2 1 2 1 2 | scale steps | 8 | 14, 16, 42, 44, 87, 89, 132, 134 | 2, 6, 12, 19 | exact 4, real-transposed 4 |
| 25 | 8 | 2 1 2 1 2 1 2 1 | scale steps | 8 | 14, 16, 42, 44, 87, 89, 132, 134 | 2, 6, 12, 19 | exact 4, real-transposed 4 |
| 26 | 7 | 2 1 2 1 2 1 7 | scale steps | 10 | 15, 17, 43, 45, 88, 90, 98, 133, 135, 143 | 2, 6, 12, 13, 19, 20 | exact 4, real-transposed 4, tonal-transposed 2 |
| 27 | 8 | 7 4 5 3 5 4 5 2 | scale steps | 4 | 15, 17, 43, 45 | 2, 6 | exact 4 |
| 28 | 8 | 4 5 3 5 4 5 2 5 | scale steps | 4 | 15, 17, 43, 45 | 2, 6 | exact 4 |
| 29 | 8 | 4 5 2 5 5 3 1 2 | scale steps | 2 | 15, 43 | 2, 6 | exact 2 |
| 30 | 8 | 4 5 2 5 5 3 5 3 | scale steps | 2 | 17, 45 | 2, 6 | exact 2 |
| 31 | 8 | 5 3 5 3 1 6 4 6 | scale steps | 12 | 18, 19, 20, 46, 47, 48, 91, 93, 94, 136, 138, 139 | 2, 6, 12, 19 | exact 2, tonal-transposed 6, real-transposed 2, fragmented 2 |
| 32 | 8 | 3 5 3 1 6 4 6 1 | scale steps | 10 | 18, 19, 20, 46, 47, 48, 91, 93, 136, 138 | 2, 6, 12, 19 | exact 2, tonal-transposed 6, real-transposed 2 |
| 33 | 8 | 6 4 6 1 6 4 2 4 | scale steps | 10 | 18, 19, 20, 46, 47, 48, 91, 93, 136, 138 | 2, 6, 12, 19 | exact 2, tonal-transposed 6, real-transposed 2 |
| 34 | 8 | 4 6 1 6 4 2 4 2 | scale steps | 10 | 18, 19, 20, 46, 47, 48, 91, 93, 136, 138 | 2, 6, 12, 19 | exact 2, tonal-transposed 6, real-transposed 2 |
| 35 | 8 | 2 7 2 7 5 3 1 1 | scale steps | 4 | 21, 49, 94, 139 | 2, 6, 12, 19 | exact 2, real-transposed 2 |
| 36 | 8 | 7 2 7 5 3 1 1 5 | scale steps | 4 | 21, 49, 94, 139 | 2, 6, 12, 19 | exact 2, real-transposed 2 |
| 37 | 8 | 3 1 1 5 3 2 3 4 | scale steps | 2 | 21, 49 | - | exact 2 |
| 38 | 6 | 3 1 1 5 3 2 | scale steps | 4 | 21, 49, 94, 139 | - | exact 2, real-transposed 2 |
| 39 | 8 | 1 1 5 3 2 3 4 b6 | scale steps | 2 | 21, 49 | - | exact 2 |
| 40 | 8 | 1 2 b3 #4 5 7 1 2 | scale steps | 2 | 22, 50 | 3, 7 | exact 2 |
| 41 | 8 | 2 b3 #4 5 7 1 2 1 | scale steps | 2 | 22, 50 | 3, 7 | exact 2 |
| 42 | 8 | #4 5 7 1 2 1 7 1 | scale steps | 2 | 22, 50 | 3, 7 | exact 2 |
| 43 | 8 | 7 1 2 1 7 1 b3 1 | scale steps | 2 | 23, 51 | 3, 7 | exact 2 |
| 44 | 8 | b3 1 b3 1 2 b7 4 b3 | scale steps | 2 | 23, 51 | 3, 7 | exact 2 |
| 45 | 8 | b3 1 2 b7 4 b3 2 1 | scale steps | 2 | 23, 51 | 3, 7 | exact 2 |
| 46 | 8 | 2 b7 4 b3 2 1 b7 2 | scale steps | 2 | 24, 52 | 3, 7 | exact 2 |
| 47 | 8 | 4 b3 2 1 b7 2 1 2 | scale steps | 2 | 24, 52 | 3, 7 | exact 2 |
| 48 | 8 | 1 1 5 1 3 5 3 1 | scale steps | 4 | 26, 54, 99, 144 | 3, 7, 13, 20 | exact 2, real-transposed 2 |
| 49 | 8 | 1 5 1 3 5 3 1 3 | scale steps | 16 | 26, 27, 54, 55, 57, 58, 61, 62, 99, 100, 102, 103, 106, 107, 144, 145 | 3, 7, 9, 13, 15, 16, 20 | exact 2, real-transposed 6, tonal-transposed 8 |
| 50 | 8 | 5 3 1 3 4 2 7 2 | scale steps | 12 | 26, 27, 54, 55, 57, 58, 99, 100, 102, 103, 144, 145 | 3, 7, 9, 13, 15, 20 | exact 2, real-transposed 6, tonal-transposed 4 |
| 51 | 8 | 4 2 7 2 1 1 5 1 | scale steps | 6 | 26, 54, 57, 99, 102, 144 | 3, 7, 9, 13, 15, 20 | exact 4, real-transposed 2 |
| 52 | 5 | 4 2 7 2 1 | scale steps | 14 | 26, 27, 54, 55, 57, 58, 62, 99, 100, 102, 103, 107, 144, 145 | 3, 7, 9, 13, 15, 16, 20 | exact 4, real-transposed 8, varied 2 |
| 53 | 8 | 1 1 5 1 3 5 3 1 | scale steps | 8 | 27, 55, 58, 62, 100, 103, 107, 145 | 3, 7, 9, 13, 15, 16, 20 | exact 2, tonal-transposed 4, real-transposed 2 |
| 54 | 8 | 1 1 5 1 b3 5 b3 1 | scale steps | 2 | 57, 102 | 9, 15 | exact 2 |
| 55 | 8 | 4 2 7 2 1 4 5 6 | scale steps | 2 | 58, 103 | 9 | exact 2 |
| 56 | 8 | 1 4 5 6 7 1 1 b3 | scale steps | 2 | 59, 104 | 9, 16 | exact 2 |
| 57 | 8 | 4 5 6 7 1 1 b3 2 | scale steps | 2 | 59, 104 | 9, 16 | exact 2 |
| 58 | 8 | 1 1 b3 2 1 b7 6 5 | scale steps | 4 | 59, 76, 104, 121 | 9, 11, 16, 18 | exact 2, fragmented 2 |
| 59 | 8 | 1 b7 6 5 #4 5 6 7 | scale steps | 2 | 59, 104 | 9, 16 | exact 2 |
| 60 | 8 | #4 5 6 7 b2 2 #4 6 | scale steps | 2 | 60, 105 | 9, 16 | exact 2 |
| 61 | 8 | 5 6 7 b2 2 #4 6 5 | scale steps | 2 | 60, 105 | 9, 16 | exact 2 |
| 62 | 8 | 2 #4 6 5 #4 b3 2 1 | scale steps | 2 | 60, 105 | 9, 16 | exact 2 |
| 63 | 8 | #4 b3 2 1 b7 5 2 5 | scale steps | 2 | 60, 105 | 9, 16 | exact 2 |
| 64 | 8 | b3 1 5 1 b3 5 b3 1 | scale steps | 2 | 61, 106 | 9, 16 | exact 2 |
| 65 | 8 | 5 b3 1 b3 4 2 7 2 | scale steps | 4 | 61, 62, 106, 107 | 9, 16 | exact 2, real-transposed 2 |
| 66 | 8 | 4 2 7 2 1 1 5 1 | scale steps | 2 | 61, 106 | 9, 16 | exact 2 |
| 67 | 8 | 4 2 7 2 b3 1 2 b3 | scale steps | 2 | 62, 107 | 9, 16 | exact 2 |
| 68 | 8 | b3 1 2 b3 4 5 6 7 | scale steps | 2 | 63, 108 | 9, 16 | exact 2 |
| 69 | 8 | 4 5 6 7 1 1 b7 6 | scale steps | 2 | 63, 108 | 9, 16 | exact 2 |
| 70 | 8 | 1 1 b7 6 5 #4 6 b7 | scale steps | 4 | 63, 67, 108, 112 | 10, 17 | exact 2, fragmented 2 |
| 71 | 8 | 1 b7 6 5 #4 6 b7 1 | scale steps | 4 | 63, 67, 108, 112 | 10, 17 | exact 2, fragmented 2 |
| 72 | 8 | 7 2 b3 4 5 6 7 1 | scale steps | 2 | 64, 109 | 10, 17 | exact 2 |
| 73 | 8 | 5 6 7 1 2 7 b6 5 | scale steps | 2 | 64, 109 | 10, 17 | exact 2 |
| 74 | 8 | 2 7 b6 5 4 b3 1 5 | scale steps | 2 | 64, 109 | 10, 17 | exact 2 |
| 75 | 8 | 7 b6 5 4 b3 1 5 4 | scale steps | 2 | 64, 109 | 10, 17 | exact 2 |
| 76 | 8 | b3 1 5 4 b3 2 1 b7 | scale steps | 2 | 65, 110 | 10, 17 | exact 2 |
| 77 | 8 | b3 2 1 b7 b6 b6 5 4 | scale steps | 6 | 65, 66, 67, 110, 111, 112 | 10, 17 | exact 2, tonal-transposed 4 |
| 78 | 8 | b6 b6 5 4 b3 2 b7 4 | scale steps | 4 | 65, 66, 110, 111 | 10, 17 | exact 2, tonal-transposed 2 |
| 79 | 8 | b6 5 4 b3 2 b7 4 b3 | scale steps | 4 | 65, 66, 110, 111 | 10, 17 | exact 2, tonal-transposed 2 |
| 80 | 8 | 2 b7 4 b3 2 1 b7 b6 | scale steps | 8 | 66, 65, 67, 97, 110, 111, 112, 142 | 10, 13, 17, 20 | exact 2, fragmented 4, tonal-transposed 2 |
| 81 | 8 | 4 4 b3 2 1 7 5 2 | scale steps | 2 | 67, 112 | 10, 17 | exact 2 |
| 82 | 8 | 4 b3 2 1 7 5 2 1 | scale steps | 2 | 67, 112 | 10, 17 | exact 2 |
| 83 | 8 | 7 5 2 1 7 b6 5 4 | scale steps | 2 | 68, 113 | 10, 17 | exact 2 |
| 84 | 8 | 7 b6 5 4 b3 1 b7 b6 | scale steps | 2 | 68, 113 | 10, 17 | exact 2 |
| 85 | 8 | b3 1 b7 b6 5 4 b2 4 | scale steps | 2 | 68, 113 | 10, 17 | exact 2 |
| 86 | 8 | 1 b7 b6 5 4 b2 4 b3 | scale steps | 2 | 68, 113 | 10, 17 | exact 2 |
| 87 | 8 | 6 4 6 5 4 3 2 1 | scale steps | 2 | 69, 114 | 10, 17 | exact 2 |
| 88 | 8 | 4 3 2 1 7 1 2 3 | scale steps | 2 | 69, 114 | 10, 17 | exact 2 |
| 89 | 8 | 7 1 2 3 4 5 6 7 | scale steps | 2 | 69, 114 | 10, 17 | exact 2 |
| 90 | 8 | 4 5 6 7 1 3 5 7 | scale steps | 2 | 69, 114 | - | exact 2 |
| 91 | 8 | 6 5 4 3 3 4 5 6 | scale steps | 2 | 77, 122 | 11, 18 | exact 2 |
| 92 | 8 | 3 4 5 6 7 1 7 6 | scale steps | 8 | 78, 79, 80, 81, 123, 124, 125, 126 | 11, 18 | exact 2, tonal-transposed 4, real-transposed 2 |
| 93 | 8 | 1 7 6 5 3 2 3 #4 | scale steps | 6 | 78, 79, 80, 123, 124, 125 | 11, 18 | exact 2, tonal-transposed 4 |
| 94 | 8 | 3 2 3 #4 5 6 7 6 | scale steps | 6 | 78, 79, 80, 123, 124, 125 | 11, 18 | exact 2, tonal-transposed 4 |
| 95 | 8 | 5 #4 3 2 7 6 6 7 | scale steps | 2 | 81, 126 | 11, 18 | exact 2 |
| 96 | 8 | 7 6 6 7 1 2 3 #4 | scale steps | 2 | 81, 126 | 11, 18 | exact 2 |
| 97 | 8 | 6 6 7 1 2 3 #4 b6 | scale steps | 2 | 82, 127 | 11, 18 | exact 2 |
| 98 | 8 | b7 6 5 4 3 1 6 4 | scale steps | 4 | 83, 10, 38, 128 | 12, 2, 6, 19 | exact 2, fragmented 2 |
| 99 | 8 | 3 1 6 4 5 1 6 4 | scale steps | 2 | 84, 129 | 12, 19 | exact 2 |
| 100 | 8 | 6 4 5 1 6 4 5 1 | scale steps | 2 | 84, 129 | 12, 19 | exact 2 |
| 101 | 7 | 5 1 6 4 5 1 1 | scale steps | 2 | 84, 129 | 12, 19 | exact 2 |
| 102 | 8 | b7 1 b7 1 1 6 4 5 | scale steps | 2 | 86, 131 | 12, 19 | exact 2 |
| 103 | 8 | 7 5 3 1 2 3 2 1 | scale steps | 4 | 88, 87, 132, 133 | 12, 19 | exact 2, fragmented 2 |
| 104 | 8 | 7 4 5 2 5 5 3 5 | scale steps | 2 | 90, 135 | 12, 19 | exact 2 |
| 105 | 8 | 4 5 2 5 5 3 5 3 | scale steps | 2 | 90, 135 | 12, 19 | exact 2 |
| 106 | 8 | 4 2 4 2 7 5 3 5 | scale steps | 2 | 92, 137 | 12, 19 | exact 2 |
| 107 | 8 | 2 4 2 7 5 3 5 7 | scale steps | 2 | 92, 137 | 12, 19 | exact 2 |
| 108 | 8 | 5 3 5 7 5 3 1 3 | scale steps | 2 | 92, 137 | 12, 19 | exact 2 |
| 109 | 8 | 3 5 7 5 3 1 3 1 | scale steps | 2 | 92, 137 | 12, 19 | exact 2 |
| 110 | 5 | 7 1 7 1 5 | scale steps | 4 | 95, 96, 140, 141 | 13, 20 | exact 2, varied 2 |
| 111 | 8 | #4 5 #4 5 4 5 6 b7 | scale steps | 2 | 96, 141 | 13, 20 | exact 2 |
| 112 | 8 | #4 5 4 5 6 b7 1 2 | scale steps | 2 | 96, 141 | 13, 20 | exact 2 |
| 113 | 8 | 4 5 6 b7 1 2 1 b7 | scale steps | 2 | 97, 142 | 13, 20 | exact 2 |
| 114 | 8 | b3 2 1 b7 2 1 2 1 | scale steps | 2 | 97, 142 | 13, 20 | exact 2 |
| 115 | 8 | 3 2 3 2 3 2 3 2 | scale steps | 8 | 98, 143 | 13, 20 | exact 6, fragmented 2 |
| 116 | 8 | 3 2 3 2 3 2 1 2 | scale steps | 2 | 98, 143 | 13, 20 | exact 2 |

## Melody

The library's melody adapter read the soprano of each phrase as a line of notes: the soprano notes that start in the phrase. Range is the lowest and highest note and the semitones between them; Climax is the first note at the highest pitch, with how many notes sound it when it is not one; Contour counts the intervals between consecutive notes that go up, go down and repeat, and how many of them are steps and leaps.

| Phrase | Measures | Notes | Range | Climax | Up | Down | Same | Steps | Leaps |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 9 | 35 | E4 to C6 (20) | C6, m. 3, beat 4 | 18 | 15 | 1 | 8 | 24 |
| 2 | mm. 10 to 21 | 34 | D4 to D6 (24) | D6, m. 14, beat 1, 3 notes | 12 | 18 | 1 | 5 | 25 |
| 3 | mm. 22 to 27 | 23 | A4 to D6 (17) | D6, m. 24, beat 2, 2 notes | 9 | 6 | 7 | 6 | 9 |
| 4 | mm. 28 to 29 | 6 | G4 to B5 (16) | B5, m. 28, beat 2 | 3 | 2 | 0 | 0 | 5 |
| 5 | mm. 30 to 37 | 32 | E4 to C6 (20) | C6, m. 31, beat 4 | 16 | 14 | 1 | 8 | 21 |
| 6 | mm. 38 to 49 | 34 | D4 to D6 (24) | D6, m. 42, beat 1, 3 notes | 12 | 18 | 1 | 5 | 25 |
| 7 | mm. 50 to 55 | 23 | A4 to D6 (17) | D6, m. 52, beat 2, 2 notes | 9 | 6 | 7 | 6 | 9 |
| 8 | m. 56 | 3 | G4 to B5 (16) | B5, m. 56, beat 2 | 1 | 1 | 0 | 0 | 2 |
| 9 | mm. 57 to 63 | 26 | D4 to E6 (26) | E6, m. 60, beat 3.5 | 7 | 15 | 1 | 6 | 15 |
| 10 | mm. 64 to 70 | 25 | D4 to E6 (26) | E6, m. 65, beat 1.5 | 9 | 13 | 0 | 4 | 18 |
| 11 | mm. 71 to 82 | 44 | C4 to F6 (29) | F6, m. 72, beat 4 | 20 | 18 | 1 | 12 | 24 |
| 12 | mm. 83 to 94 | 34 | G4 to E6 (21) | E6, m. 93, beat 2.25 | 13 | 18 | 2 | 4 | 27 |
| 13 | mm. 95 to 100 | 19 | E4 to E6 (24) | E6, m. 97, beat 2.5 | 6 | 7 | 5 | 3 | 10 |
| 14 | m. 101 | 3 | C4 to C6 (24) | C6, m. 101, beat 2 | 1 | 1 | 0 | 0 | 2 |
| 15 | mm. 102 to 103 | 7 | G4 to D6 (19) | D6, m. 102, beat 3 | 3 | 3 | 0 | 1 | 5 |
| 16 | mm. 104 to 108 | 18 | D4 to E6 (26) | E6, m. 105, beat 3.5 | 4 | 10 | 1 | 4 | 9 |
| 17 | mm. 109 to 115 | 25 | D4 to E6 (26) | E6, m. 110, beat 1.5 | 9 | 13 | 0 | 4 | 18 |
| 18 | mm. 116 to 127 | 44 | C4 to F6 (29) | F6, m. 117, beat 4 | 20 | 18 | 1 | 12 | 24 |
| 19 | mm. 128 to 139 | 34 | G4 to E6 (21) | E6, m. 138, beat 2.25 | 13 | 18 | 2 | 4 | 27 |
| 20 | mm. 140 to 145 | 19 | E4 to E6 (24) | E6, m. 142, beat 2.5 | 6 | 7 | 5 | 3 | 10 |
| 21 | m. 146 | 3 | C4 to C6 (24) | C6, m. 146, beat 2 | 1 | 1 | 0 | 0 | 2 |

### Leaps

A leap is an interval of a third or larger. The library's rule asks a leap of a fourth or larger to be followed by a step in the opposite direction: such a leap is Recovered when one follows, Unrecovered when a step the same way, a repeated note or another leap does, and Not judged when nothing follows it; a third asks nothing (Not required). Consecutive counts the pairs of leaps in a row the library's rule finds fault with.

| Phrase | Leaps | Recovered | Unrecovered | Not required | Not judged | Consecutive |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 24 | 1 | 14 | 8 | 1 | 9 |
| 2 | 25 | 1 | 13 | 11 | 0 | 12 |
| 3 | 9 | 0 | 3 | 6 | 0 | 4 |
| 4 | 5 | 0 | 2 | 3 | 0 | 3 |
| 5 | 21 | 1 | 13 | 6 | 1 | 7 |
| 6 | 25 | 1 | 13 | 11 | 0 | 12 |
| 7 | 9 | 0 | 3 | 6 | 0 | 4 |
| 8 | 2 | 0 | 1 | 1 | 0 | 0 |
| 9 | 15 | 0 | 7 | 5 | 3 | 4 |
| 10 | 18 | 1 | 10 | 6 | 1 | 10 |
| 11 | 24 | 5 | 12 | 5 | 2 | 6 |
| 12 | 27 | 1 | 13 | 13 | 0 | 17 |
| 13 | 10 | 0 | 5 | 5 | 0 | 4 |
| 14 | 2 | 0 | 1 | 0 | 1 | 0 |
| 15 | 5 | 0 | 3 | 2 | 0 | 2 |
| 16 | 9 | 0 | 4 | 3 | 2 | 2 |
| 17 | 18 | 1 | 10 | 6 | 1 | 10 |
| 18 | 24 | 5 | 12 | 5 | 2 | 6 |
| 19 | 27 | 1 | 13 | 13 | 0 | 17 |
| 20 | 10 | 0 | 5 | 5 | 0 | 4 |
| 21 | 2 | 0 | 1 | 0 | 1 | 0 |

### Non-chord tones

Each melody note is set against the chord of the segment it starts in, as the library's chord identification reads it: a Chord tone is one of the chord's tones, a Non-chord tone is one it left over and named, and Unclassified notes are in a segment with no chord or were left unnamed. By type counts the non-chord tones under the library's names.

| Phrase | Notes | Chord tones | Non-chord tones | Unclassified | By type |
| --- | --- | --- | --- | --- | --- |
| 1 | 35 | 25 | 1 | 9 | passing 1 |
| 2 | 34 | 28 | 1 | 5 | appoggiatura 1 |
| 3 | 23 | 21 | 2 | 0 | passing 2 |
| 4 | 6 | 6 | 0 | 0 | - |
| 5 | 32 | 22 | 1 | 9 | passing 1 |
| 6 | 34 | 28 | 1 | 5 | appoggiatura 1 |
| 7 | 23 | 21 | 2 | 0 | passing 2 |
| 8 | 3 | 3 | 0 | 0 | - |
| 9 | 26 | 18 | 1 | 7 | appoggiatura 1 |
| 10 | 25 | 20 | 1 | 4 | escape-tone 1 |
| 11 | 44 | 32 | 2 | 10 | passing 1, neighbor 1 |
| 12 | 34 | 28 | 0 | 6 | - |
| 13 | 19 | 17 | 0 | 2 | - |
| 14 | 3 | 3 | 0 | 0 | - |
| 15 | 7 | 4 | 1 | 2 | appoggiatura 1 |
| 16 | 18 | 13 | 0 | 5 | - |
| 17 | 25 | 20 | 1 | 4 | escape-tone 1 |
| 18 | 44 | 32 | 2 | 10 | passing 1, neighbor 1 |
| 19 | 34 | 28 | 0 | 6 | - |
| 20 | 19 | 17 | 0 | 2 | - |
| 21 | 3 | 3 | 0 | 0 | - |

### Tendencies

The library's melodic tendencies are soft preferences a well-formed line usually shows: Pitch proximity (steps outnumber leaps, small leaps outnumber large ones), Step declination (descending steps outnumber ascending ones), Step inertia (the line continues in its direction at least as often as it reverses), Melodic regression (extreme notes turn back toward the middle) and Melodic arch (the line rises to a climax in its middle half and ends below it); a phrase either shows a tendency or does not. The seventh degree column counts the notes the library labels the seventh degree that another note follows, judged by its leading tone rule: a leading tone goes up to the tonic by step, and a seventh a whole step below the tonic does not. Followed is the count that did, Broken the count that did not, with where each broken one stands.

| Phrase | Pitch proximity | Step declination | Step inertia | Melodic regression | Melodic arch | Seventh degree followed | Seventh degree broken |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | does not hold | does not hold | does not hold | does not hold | does not hold | 0 | 3 (m. 2, beat 1; m. 6, beat 1.75; m. 7, beat 4) |
| 2 | does not hold | holds | does not hold | does not hold | holds | 0 | 3 (m. 14, beat 3.75; m. 16, beat 3.75; m. 20, beat 2.25) |
| 3 | does not hold | holds | holds | does not hold | holds | 1 | 10 (m. 25, beat 1.25; m. 25, beat 1.75; m. 25, beat 2.25; m. 25, beat 2.75; m. 25, beat 3.25; m. 25, beat 3.75; m. 25, beat 4.25; m. 25, beat 4.5; m. 26, beat 2.75; m. 27, beat 2.75) |
| 4 | does not hold | holds | holds | holds | does not hold | 0 | 1 (m. 28, beat 2) |
| 5 | does not hold | does not hold | holds | does not hold | does not hold | 0 | 3 (m. 30, beat 1; m. 34, beat 1.75; m. 35, beat 4) |
| 6 | does not hold | holds | does not hold | does not hold | holds | 0 | 3 (m. 42, beat 3.75; m. 44, beat 3.75; m. 48, beat 2.25) |
| 7 | does not hold | holds | holds | does not hold | holds | 1 | 10 (m. 53, beat 1.25; m. 53, beat 1.75; m. 53, beat 2.25; m. 53, beat 2.75; m. 53, beat 3.25; m. 53, beat 3.75; m. 53, beat 4.25; m. 53, beat 4.5; m. 54, beat 2.75; m. 55, beat 2.75) |
| 8 | does not hold | holds | does not hold | holds | holds | 0 | 1 (m. 56, beat 2) |
| 9 | does not hold | holds | does not hold | does not hold | holds | 3 | 0 |
| 10 | does not hold | does not hold | holds | does not hold | does not hold | 1 | 2 (m. 66, beat 2; m. 68, beat 1.5) |
| 11 | does not hold | holds | does not hold | does not hold | does not hold | 3 | 0 |
| 12 | does not hold | holds | does not hold | does not hold | does not hold | 0 | 3 (m. 88, beat 2; m. 90, beat 2; m. 92, beat 4.5) |
| 13 | does not hold | holds | holds | does not hold | holds | 0 | 1 (m. 97, beat 1.75) |
| 14 | does not hold | holds | does not hold | holds | holds | 0 | 0 |
| 15 | does not hold | holds | holds | does not hold | holds | 2 | 0 |
| 16 | does not hold | holds | does not hold | does not hold | does not hold | 1 | 0 |
| 17 | does not hold | does not hold | holds | does not hold | does not hold | 1 | 2 (m. 111, beat 2; m. 113, beat 1.5) |
| 18 | does not hold | holds | does not hold | does not hold | does not hold | 3 | 0 |
| 19 | does not hold | holds | does not hold | does not hold | does not hold | 0 | 3 (m. 133, beat 2; m. 135, beat 2; m. 137, beat 4.5) |
| 20 | does not hold | holds | holds | does not hold | holds | 0 | 1 (m. 142, beat 1.75) |
| 21 | does not hold | holds | does not hold | holds | holds | 0 | 0 |

### Development

Each family's occurrences in time order, as the measure they start in and the kind the library names against the family's prototype, which is the original.

- **Family 1:** m1 original > m29 exact > m70 real-transposed > m115 real-transposed
- **Family 2:** m1 original > m29 exact > m70 real-transposed > m115 real-transposed
- **Family 3:** m1 original > m29 exact > m70 real-transposed > m115 real-transposed
- **Family 4:** m2 original > m30 exact > m71 real-transposed > m116 real-transposed
- **Family 5:** m2 original > m30 exact > m71 real-transposed > m116 real-transposed
- **Family 6:** m4 original > m32 exact > m73 real-transposed > m118 real-transposed
- **Family 7:** m4 original > m7 fragmented > m32 exact > m35 fragmented > m73 real-transposed > m118 real-transposed
- **Family 8:** m5 original > m6 tonal-transposed > m7 tonal-transposed > m8 tonal-transposed > m9 fragmented > m33 exact > m34 tonal-transposed > m35 tonal-transposed > m36 tonal-transposed > m37 fragmented > m74 real-transposed > m75 tonal-transposed > m76 tonal-transposed > m77 tonal-transposed > m82 fragmented > m119 real-transposed > m120 tonal-transposed > m121 tonal-transposed > m122 tonal-transposed > m127 fragmented
- **Family 9:** m5 original > m6 tonal-transposed > m7 tonal-transposed > m8 real-transposed > m10 fragmented > m33 exact > m34 tonal-transposed > m35 tonal-transposed > m36 real-transposed > m38 fragmented > m74 real-transposed > m75 tonal-transposed > m76 tonal-transposed > m77 exact > m78 tonal-transposed > m79 real-transposed > m80 tonal-transposed > m81 tonal-transposed > m83 fragmented > m119 real-transposed > m120 tonal-transposed > m121 tonal-transposed > m122 exact > m123 tonal-transposed > m124 real-transposed > m125 tonal-transposed > m126 tonal-transposed > m128 fragmented
- **Family 10:** m5 original > m6 tonal-transposed > m7 tonal-transposed > m8 tonal-transposed > m33 exact > m34 tonal-transposed > m35 tonal-transposed > m36 tonal-transposed > m67 fragmented > m74 real-transposed > m75 tonal-transposed > m76 tonal-transposed > m77 tonal-transposed > m97 tonal-transposed > m112 fragmented > m119 real-transposed > m120 tonal-transposed > m121 tonal-transposed > m122 tonal-transposed > m142 tonal-transposed
- **Family 11:** m5 original > m6 tonal-transposed > m7 tonal-transposed > m8 tonal-transposed > m33 exact > m34 tonal-transposed > m35 tonal-transposed > m36 tonal-transposed > m74 real-transposed > m75 tonal-transposed > m76 tonal-transposed > m119 real-transposed > m120 tonal-transposed > m121 tonal-transposed
- **Family 12:** m9 original > m37 exact > m63 fragmented > m82 fragmented > m108 fragmented > m127 fragmented
- **Family 13:** m9 original > m37 exact > m82 exact > m127 exact
- **Family 14:** m9 original > m37 exact > m82 exact > m127 exact
- **Family 15:** m9 original > m37 exact > m82 exact > m127 exact
- **Family 16:** m10 original > m10 tonal-transposed > m38 exact > m38 tonal-transposed > m66 fragmented > m83 exact > m83 tonal-transposed > m97 tonal-transposed > m111 fragmented > m128 exact > m128 tonal-transposed > m142 tonal-transposed
- **Family 17:** m10 original > m38 exact > m83 exact > m128 exact
- **Family 18:** m11 original > m39 exact > m84 exact > m129 exact
- **Family 19:** m13 original > m41 exact
- **Family 20:** m13 original > m41 exact
- **Family 21:** m13 original > m41 exact > m86 tonal-transposed > m86 real-transposed > m131 tonal-transposed > m131 real-transposed
- **Family 22:** m13 original > m41 exact
- **Family 23:** m14 original > m16 exact > m42 exact > m44 exact > m87 real-transposed > m89 real-transposed > m132 real-transposed > m134 real-transposed
- **Family 24:** m14 original > m16 exact > m42 exact > m44 exact > m87 real-transposed > m89 real-transposed > m132 real-transposed > m134 real-transposed
- **Family 25:** m14 original > m16 exact > m42 exact > m44 exact > m87 real-transposed > m89 real-transposed > m132 real-transposed > m134 real-transposed
- **Family 26:** m15 original > m17 exact > m43 exact > m45 exact > m88 real-transposed > m90 real-transposed > m98 tonal-transposed > m133 real-transposed > m135 real-transposed > m143 tonal-transposed
- **Family 27:** m15 original > m17 exact > m43 exact > m45 exact
- **Family 28:** m15 original > m17 exact > m43 exact > m45 exact
- **Family 29:** m15 original > m43 exact
- **Family 30:** m17 original > m45 exact
- **Family 31:** m18 original > m19 tonal-transposed > m20 tonal-transposed > m46 exact > m47 tonal-transposed > m48 tonal-transposed > m91 real-transposed > m93 tonal-transposed > m94 fragmented > m136 real-transposed > m138 tonal-transposed > m139 fragmented
- **Family 32:** m18 original > m19 tonal-transposed > m20 tonal-transposed > m46 exact > m47 tonal-transposed > m48 tonal-transposed > m91 real-transposed > m93 tonal-transposed > m136 real-transposed > m138 tonal-transposed
- **Family 33:** m18 original > m19 tonal-transposed > m20 tonal-transposed > m46 exact > m47 tonal-transposed > m48 tonal-transposed > m91 real-transposed > m93 tonal-transposed > m136 real-transposed > m138 tonal-transposed
- **Family 34:** m18 original > m19 tonal-transposed > m20 tonal-transposed > m46 exact > m47 tonal-transposed > m48 tonal-transposed > m91 real-transposed > m93 tonal-transposed > m136 real-transposed > m138 tonal-transposed
- **Family 35:** m21 original > m49 exact > m94 real-transposed > m139 real-transposed
- **Family 36:** m21 original > m49 exact > m94 real-transposed > m139 real-transposed
- **Family 37:** m21 original > m49 exact
- **Family 38:** m21 original > m49 exact > m94 real-transposed > m139 real-transposed
- **Family 39:** m21 original > m49 exact
- **Family 40:** m22 original > m50 exact
- **Family 41:** m22 original > m50 exact
- **Family 42:** m22 original > m50 exact
- **Family 43:** m23 original > m51 exact
- **Family 44:** m23 original > m51 exact
- **Family 45:** m23 original > m51 exact
- **Family 46:** m24 original > m52 exact
- **Family 47:** m24 original > m52 exact
- **Family 48:** m26 original > m54 exact > m99 real-transposed > m144 real-transposed
- **Family 49:** m26 original > m27 real-transposed > m54 exact > m55 real-transposed > m57 tonal-transposed > m58 tonal-transposed > m61 tonal-transposed > m62 tonal-transposed > m99 real-transposed > m100 real-transposed > m102 tonal-transposed > m103 tonal-transposed > m106 tonal-transposed > m107 tonal-transposed > m144 real-transposed > m145 real-transposed
- **Family 50:** m26 original > m27 real-transposed > m54 exact > m55 real-transposed > m57 tonal-transposed > m58 tonal-transposed > m99 real-transposed > m100 real-transposed > m102 tonal-transposed > m103 tonal-transposed > m144 real-transposed > m145 real-transposed
- **Family 51:** m26 original > m54 exact > m57 exact > m99 real-transposed > m102 exact > m144 real-transposed
- **Family 52:** m26 original > m27 real-transposed > m54 exact > m55 real-transposed > m57 exact > m58 real-transposed > m62 varied > m99 real-transposed > m100 real-transposed > m102 exact > m103 real-transposed > m107 varied > m144 real-transposed > m145 real-transposed
- **Family 53:** m27 original > m55 exact > m58 tonal-transposed > m62 tonal-transposed > m100 real-transposed > m103 tonal-transposed > m107 tonal-transposed > m145 real-transposed
- **Family 54:** m57 original > m102 exact
- **Family 55:** m58 original > m103 exact
- **Family 56:** m59 original > m104 exact
- **Family 57:** m59 original > m104 exact
- **Family 58:** m59 original > m76 fragmented > m104 exact > m121 fragmented
- **Family 59:** m59 original > m104 exact
- **Family 60:** m60 original > m105 exact
- **Family 61:** m60 original > m105 exact
- **Family 62:** m60 original > m105 exact
- **Family 63:** m60 original > m105 exact
- **Family 64:** m61 original > m106 exact
- **Family 65:** m61 original > m62 real-transposed > m106 exact > m107 real-transposed
- **Family 66:** m61 original > m106 exact
- **Family 67:** m62 original > m107 exact
- **Family 68:** m63 original > m108 exact
- **Family 69:** m63 original > m108 exact
- **Family 70:** m63 original > m67 fragmented > m108 exact > m112 fragmented
- **Family 71:** m63 original > m67 fragmented > m108 exact > m112 fragmented
- **Family 72:** m64 original > m109 exact
- **Family 73:** m64 original > m109 exact
- **Family 74:** m64 original > m109 exact
- **Family 75:** m64 original > m109 exact
- **Family 76:** m65 original > m110 exact
- **Family 77:** m65 original > m66 tonal-transposed > m67 tonal-transposed > m110 exact > m111 tonal-transposed > m112 tonal-transposed
- **Family 78:** m65 original > m66 tonal-transposed > m110 exact > m111 tonal-transposed
- **Family 79:** m65 original > m66 tonal-transposed > m110 exact > m111 tonal-transposed
- **Family 80:** m65 fragmented > m66 original > m67 tonal-transposed > m97 fragmented > m110 fragmented > m111 exact > m112 tonal-transposed > m142 fragmented
- **Family 81:** m67 original > m112 exact
- **Family 82:** m67 original > m112 exact
- **Family 83:** m68 original > m113 exact
- **Family 84:** m68 original > m113 exact
- **Family 85:** m68 original > m113 exact
- **Family 86:** m68 original > m113 exact
- **Family 87:** m69 original > m114 exact
- **Family 88:** m69 original > m114 exact
- **Family 89:** m69 original > m114 exact
- **Family 90:** m69 original > m114 exact
- **Family 91:** m77 original > m122 exact
- **Family 92:** m78 original > m79 tonal-transposed > m80 tonal-transposed > m81 real-transposed > m123 exact > m124 tonal-transposed > m125 tonal-transposed > m126 real-transposed
- **Family 93:** m78 original > m79 tonal-transposed > m80 tonal-transposed > m123 exact > m124 tonal-transposed > m125 tonal-transposed
- **Family 94:** m78 original > m79 tonal-transposed > m80 tonal-transposed > m123 exact > m124 tonal-transposed > m125 tonal-transposed
- **Family 95:** m81 original > m126 exact
- **Family 96:** m81 original > m126 exact
- **Family 97:** m82 original > m127 exact
- **Family 98:** m10 fragmented > m38 fragmented > m83 original > m128 exact
- **Family 99:** m84 original > m129 exact
- **Family 100:** m84 original > m129 exact
- **Family 101:** m84 original > m129 exact
- **Family 102:** m86 original > m131 exact
- **Family 103:** m87 fragmented > m88 original > m132 fragmented > m133 exact
- **Family 104:** m90 original > m135 exact
- **Family 105:** m90 original > m135 exact
- **Family 106:** m92 original > m137 exact
- **Family 107:** m92 original > m137 exact
- **Family 108:** m92 original > m137 exact
- **Family 109:** m92 original > m137 exact
- **Family 110:** m95 original > m96 varied > m140 exact > m141 varied
- **Family 111:** m96 original > m141 exact
- **Family 112:** m96 original > m141 exact
- **Family 113:** m97 original > m142 exact
- **Family 114:** m97 original > m142 exact
- **Family 115:** m98 original > m98 exact > m98 exact > m98 fragmented > m143 exact > m143 exact > m143 exact > m143 fragmented
- **Family 116:** m98 original > m143 exact

## Progression patterns

45 recurring progressions of two to six chords, counted over the library's Roman numerals: a held chord counts once, and a progression does not cross an unlabelled chord or a change of key. Ranked by count, then length, then first occurrence; the top 10 are shown. Measures are where each occurrence begins and ends.

| Rank | Progression | Count | Measures |
| --- | --- | --- | --- |
| 1 | I > IV | 20 | mm. 2 to 3, mm. 4 to 5, m. 11, m. 11, m. 18, mm. 30 to 31, mm. 32 to 33, m. 39, m. 39, m. 46, mm. 71 to 72, mm. 73 to 74, m. 84, m. 84, m. 91, mm. 116 to 117, mm. 118 to 119, m. 129, m. 129, m. 136 |
| 2 | V7 > I | 20 | mm. 15 to 16, mm. 17 to 18, mm. 26 to 27, mm. 27 to 28, mm. 43 to 44, mm. 45 to 46, mm. 54 to 55, mm. 55 to 56, mm. 69 to 70, mm. 83 to 84, mm. 88 to 89, mm. 90 to 91, mm. 99 to 100, mm. 100 to 101, mm. 114 to 115, mm. 128 to 129, mm. 133 to 134, mm. 135 to 136, mm. 144 to 145, mm. 145 to 146 |
| 3 | I > IV > I | 16 | mm. 2 to 3, mm. 4 to 6, m. 11, mm. 11 to 12, mm. 30 to 31, mm. 32 to 34, m. 39, mm. 39 to 40, mm. 71 to 72, mm. 73 to 75, m. 84, mm. 84 to 85, mm. 116 to 117, mm. 118 to 120, m. 129, mm. 129 to 130 |
| 4 | I > V7 | 15 | mm. 14 to 15, mm. 16 to 17, m. 26, m. 27, mm. 42 to 43, mm. 44 to 45, m. 54, m. 55, m. 58, mm. 89 to 90, m. 99, m. 100, mm. 134 to 135, m. 144, m. 145 |
| 5 | I > V7 > I | 14 | mm. 14 to 16, mm. 16 to 18, mm. 26 to 27, mm. 27 to 28, mm. 42 to 44, mm. 44 to 46, mm. 54 to 55, mm. 55 to 56, mm. 89 to 91, mm. 99 to 100, mm. 100 to 101, mm. 134 to 136, mm. 144 to 145, mm. 145 to 146 |
| 6 | V > I | 14 | m. 2, m. 4, mm. 10 to 11, m. 21, m. 30, m. 32, mm. 38 to 39, m. 49, m. 71, m. 73, m. 94, m. 116, m. 118, m. 139 |
| 7 | V > I > IV > I | 10 | mm. 2 to 3, mm. 4 to 6, mm. 10 to 11, mm. 30 to 31, mm. 32 to 34, mm. 38 to 39, mm. 71 to 72, mm. 73 to 75, mm. 116 to 117, mm. 118 to 120 |
| 8 | I > V | 10 | mm. 1 to 2, mm. 3 to 4, mm. 29 to 30, mm. 31 to 32, mm. 70 to 71, mm. 72 to 73, mm. 77 to 79, mm. 115 to 116, mm. 117 to 118, mm. 122 to 124 |
| 9 | I > V > I > IV > I | 8 | mm. 1 to 3, mm. 3 to 6, mm. 29 to 31, mm. 31 to 34, mm. 70 to 72, mm. 72 to 75, mm. 115 to 117, mm. 117 to 120 |
| 10 | V7 > I > V7 > I | 8 | mm. 15 to 18, mm. 26 to 28, mm. 43 to 46, mm. 54 to 56, mm. 88 to 91, mm. 99 to 101, mm. 133 to 136, mm. 144 to 146 |

## Harmonic rhythm

How often the harmony changes. A change is a chord whose base Roman numeral or key differs from the chord before it, counted in the measure it starts in. Changes lists the measures of each row in order; Mean is changes per measure over the row.

| Measures | Changes | Mean |
| --- | --- | --- |
| mm. 1 to 4 | 1, 2, 2, 2 | 1.75 |
| mm. 5 to 8 | 1, 1, 2, 1 | 1.25 |
| mm. 9 to 12 | 1, 3, 4, 1 | 2.25 |
| mm. 13 to 16 | 1, 1, 1, 1 | 1 |
| mm. 17 to 20 | 1, 2, 2, 2 | 1.75 |
| mm. 21 to 24 | 2, 1, 0, 1 | 1 |
| mm. 25 to 28 | 1, 2, 2, 1 | 1.5 |
| mm. 29 to 32 | 1, 2, 2, 2 | 1.75 |
| mm. 33 to 36 | 1, 1, 2, 1 | 1.25 |
| mm. 37 to 40 | 1, 3, 4, 1 | 2.25 |
| mm. 41 to 44 | 1, 1, 1, 1 | 1 |
| mm. 45 to 48 | 1, 2, 2, 2 | 1.75 |
| mm. 49 to 52 | 2, 1, 0, 1 | 1 |
| mm. 53 to 56 | 1, 2, 2, 1 | 1.5 |
| mm. 57 to 60 | 1, 2, 1, 3 | 1.75 |
| mm. 61 to 64 | 1, 1, 1, 1 | 1 |
| mm. 65 to 68 | 1, 1, 1, 3 | 1.5 |
| mm. 69 to 72 | 2, 1, 2, 2 | 1.75 |
| mm. 73 to 76 | 2, 1, 1, 2 | 1.5 |
| mm. 77 to 80 | 1, 0, 1, 2 | 1 |
| mm. 81 to 84 | 1, 1, 4, 4 | 2.5 |
| mm. 85 to 88 | 1, 1, 1, 1 | 1 |
| mm. 89 to 92 | 1, 1, 2, 2 | 1.5 |
| mm. 93 to 96 | 2, 2, 1, 1 | 1.5 |
| mm. 97 to 100 | 1, 1, 2, 2 | 1.5 |
| mm. 101 to 104 | 1, 1, 2, 1 | 1.25 |
| mm. 105 to 108 | 3, 1, 1, 1 | 1.5 |
| mm. 109 to 112 | 1, 1, 1, 1 | 1 |
| mm. 113 to 116 | 3, 2, 1, 2 | 2 |
| mm. 117 to 120 | 2, 2, 1, 1 | 1.5 |
| mm. 121 to 124 | 2, 1, 0, 1 | 1 |
| mm. 125 to 128 | 2, 1, 1, 4 | 2 |
| mm. 129 to 132 | 4, 1, 1, 1 | 1.75 |
| mm. 133 to 136 | 1, 1, 1, 2 | 1.25 |
| mm. 137 to 140 | 2, 2, 2, 1 | 1.75 |
| mm. 141 to 144 | 1, 1, 1, 2 | 1.25 |
| mm. 145 to 146 | 2, 1 | 1.5 |

**Mean:** 1.49 changes per measure over 146 measures.

### Approach to cadences

Changes per measure over the two measures before each cadence's arrival (Approach), against the two measures before those (Before). Faster is yes when the approach changes harmony more often.

| Cadence | Arrival | Before | Approach | Faster |
| --- | --- | --- | --- | --- |
| 1 | m. 27 | 0.5 | 1.5 | yes |
| 2 | m. 28 | 1 | 2 | yes |
| 3 | m. 55 | 0.5 | 1.5 | yes |
| 4 | m. 56 | 1 | 2 | yes |
| 5 | m. 63 | 2 | 1 | no |
| 6 | m. 70 | 1 | 2.5 | yes |
| 7 | m. 100 | 1 | 1.5 | yes |
| 8 | m. 101 | 1 | 2 | yes |
| 9 | m. 103 | 2 | 1 | no |
| 10 | m. 108 | 2 | 1 | no |
| 11 | m. 115 | 1 | 2.5 | yes |
| 12 | m. 145 | 1 | 1.5 | yes |
| 13 | m. 146 | 1 | 2 | yes |

## Rhythm and meter

**Meter fit:** 4/4, scale 1, no pickup (fit 0.94); no better reading found

### Density

The library's density profile of each phrase, for all its notes and for each line that has a note in it (the soprano and bass of the outer voices, and Other for the rest). A note is in a phrase when its onset is; a chord is one attack. Attacks per beat is read measure by measure over the beats the phrase spans in it, as a mean and the largest; Mean gap is the mean distance between attacks in whole notes; On the beat is the share of attacks exactly on a beat. Durations lists the notes' lengths in whole notes, shortest first, with how many notes have each. The patterns that recur are the motive families' rhythms (see Motives).

| Phrase | Measures | Line | Notes | Attacks | Per beat | Max per beat | Mean gap | On the beat | Durations |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 9 | all | 141 | 111 | 3.08 | 3.75 | 0.08 | 32.4% | 1/32 (1), 41/960 (1), 17/320 (1), 1/16 (74), 1/8 (37), 1/4 (21), 3/8 (1), 1/2 (3), 1/1 (2) |
| 1 | mm. 1 to 9 | soprano | 35 | 35 | 0.97 | 1.5 | 0.26 | 57.1% | 17/320 (1), 1/16 (22), 1/8 (2), 1/4 (6), 3/8 (1), 1/2 (3) |
| 1 | mm. 1 to 9 | bass | 44 | 44 | 1.22 | 2 | 0.19 | 56.8% | 1/16 (5), 1/8 (30), 1/4 (8), 1/1 (1) |
| 1 | mm. 1 to 9 | other | 62 | 58 | 1.61 | 2.75 | 0.13 | 25.9% | 1/32 (1), 41/960 (1), 1/16 (47), 1/8 (5), 1/4 (7), 1/1 (1) |
| 2 | mm. 10 to 21 | all | 226 | 185 | 3.85 | 4.75 | 0.06 | 25.4% | 1/32 (12), 1/16 (166), 3/32 (4), 1/8 (13), 3/16 (8), 1/4 (20), 3/8 (3) |
| 2 | mm. 10 to 21 | soprano | 34 | 34 | 0.71 | 1.25 | 0.35 | 38.2% | 1/32 (2), 1/16 (19), 3/32 (2), 1/8 (4), 3/16 (3), 1/4 (2), 3/8 (2) |
| 2 | mm. 10 to 21 | bass | 35 | 35 | 0.73 | 1 | 0.34 | 34.3% | 1/16 (28), 1/8 (1), 3/16 (3), 1/4 (2), 3/8 (1) |
| 2 | mm. 10 to 21 | other | 157 | 144 | 3 | 3.75 | 0.08 | 27.1% | 1/32 (10), 1/16 (119), 3/32 (2), 1/8 (8), 3/16 (2), 1/4 (16) |
| 3 | mm. 22 to 27 | all | 147 | 92 | 3.96 | 8 | 0.06 | 25.0% | 1/32 (39), 1/16 (56), 3/32 (32), 1/8 (6), 7/32 (2), 1/4 (9), 11/32 (1), 1/2 (2) |
| 3 | mm. 22 to 27 | soprano | 23 | 23 | 1 | 2 | 0.25 | 39.1% | 1/32 (9), 1/16 (6), 1/8 (1), 7/32 (2), 1/4 (2), 11/32 (1), 1/2 (2) |
| 3 | mm. 22 to 27 | bass | 25 | 25 | 1.08 | 2 | 0.22 | 48.0% | 1/16 (14), 3/32 (8), 1/4 (3) |
| 3 | mm. 22 to 27 | other | 99 | 78 | 3.32 | 7.75 | 0.07 | 24.4% | 1/32 (30), 1/16 (36), 3/32 (24), 1/8 (5), 1/4 (4) |
| 4 | mm. 28 to 29 | all | 22 | 11 | 1.38 | 2 | 0.19 | 63.6% | 1/8 (8), 3/16 (11), 1/4 (2), 1/2 (1) |
| 4 | mm. 28 to 29 | soprano | 6 | 6 | 0.75 | 0.75 | 0.35 | 100.0% | 3/16 (3), 1/4 (2), 1/2 (1) |
| 4 | mm. 28 to 29 | bass | 11 | 11 | 1.38 | 2 | 0.19 | 63.6% | 1/8 (8), 3/16 (3) |
| 4 | mm. 28 to 29 | other | 5 | 3 | 0.38 | 0.75 | 0.25 | 100.0% | 3/16 (5) |
| 5 | mm. 30 to 37 | all | 130 | 103 | 3.22 | 3.75 | 0.08 | 31.1% | 1/32 (1), 41/960 (1), 17/320 (1), 1/16 (74), 1/8 (29), 1/4 (19), 3/8 (1), 1/2 (2), 1/1 (2) |
| 5 | mm. 30 to 37 | soprano | 32 | 32 | 1 | 1.5 | 0.26 | 53.1% | 17/320 (1), 1/16 (22), 1/8 (2), 1/4 (4), 3/8 (1), 1/2 (2) |
| 5 | mm. 30 to 37 | bass | 36 | 36 | 1.12 | 2 | 0.2 | 58.3% | 1/16 (5), 1/8 (22), 1/4 (8), 1/1 (1) |
| 5 | mm. 30 to 37 | other | 62 | 58 | 1.81 | 2.75 | 0.13 | 25.9% | 1/32 (1), 41/960 (1), 1/16 (47), 1/8 (5), 1/4 (7), 1/1 (1) |
| 6 | mm. 38 to 49 | all | 226 | 185 | 3.85 | 4.75 | 0.06 | 25.4% | 1/32 (12), 1/16 (166), 3/32 (4), 1/8 (13), 3/16 (8), 1/4 (20), 3/8 (3) |
| 6 | mm. 38 to 49 | soprano | 34 | 34 | 0.71 | 1.25 | 0.35 | 38.2% | 1/32 (2), 1/16 (19), 3/32 (2), 1/8 (4), 3/16 (3), 1/4 (2), 3/8 (2) |
| 6 | mm. 38 to 49 | bass | 35 | 35 | 0.73 | 1 | 0.34 | 34.3% | 1/16 (28), 1/8 (1), 3/16 (3), 1/4 (2), 3/8 (1) |
| 6 | mm. 38 to 49 | other | 157 | 144 | 3 | 3.75 | 0.08 | 27.1% | 1/32 (10), 1/16 (119), 3/32 (2), 1/8 (8), 3/16 (2), 1/4 (16) |
| 7 | mm. 50 to 55 | all | 147 | 92 | 3.96 | 8 | 0.06 | 25.0% | 1/32 (39), 1/16 (56), 3/32 (32), 1/8 (6), 7/32 (2), 1/4 (9), 11/32 (1), 1/2 (2) |
| 7 | mm. 50 to 55 | soprano | 23 | 23 | 1 | 2 | 0.25 | 39.1% | 1/32 (9), 1/16 (6), 1/8 (1), 7/32 (2), 1/4 (2), 11/32 (1), 1/2 (2) |
| 7 | mm. 50 to 55 | bass | 25 | 25 | 1.08 | 2 | 0.22 | 48.0% | 1/16 (14), 3/32 (8), 1/4 (3) |
| 7 | mm. 50 to 55 | other | 99 | 78 | 3.32 | 7.75 | 0.07 | 24.4% | 1/32 (30), 1/16 (36), 3/32 (24), 1/8 (5), 1/4 (4) |
| 8 | m. 56 | all | 11 | 3 | 1 | 1 | 0.25 | 100.0% | 3/16 (11) |
| 8 | m. 56 | soprano | 3 | 3 | 1 | 1 | 0.25 | 100.0% | 3/16 (3) |
| 8 | m. 56 | bass | 3 | 3 | 1 | 1 | 0.25 | 100.0% | 3/16 (3) |
| 8 | m. 56 | other | 5 | 3 | 1 | 1 | 0.25 | 100.0% | 3/16 (5) |
| 9 | mm. 57 to 63 | all | 121 | 100 | 3.57 | 4 | 0.07 | 28.0% | 1/16 (90), 1/4 (31) |
| 9 | mm. 57 to 63 | soprano | 26 | 26 | 0.93 | 1 | 0.27 | 69.2% | 1/16 (19), 1/4 (7) |
| 9 | mm. 57 to 63 | bass | 23 | 23 | 0.82 | 1 | 0.32 | 56.5% | 1/16 (12), 1/4 (11) |
| 9 | mm. 57 to 63 | other | 72 | 68 | 2.43 | 2.5 | 0.1 | 20.6% | 1/16 (59), 1/4 (13) |
| 10 | mm. 64 to 70 | all | 110 | 104 | 3.71 | 4 | 0.07 | 26.9% | 1/16 (85), 1/8 (8), 1/4 (12), 1/2 (5) |
| 10 | mm. 64 to 70 | soprano | 25 | 25 | 0.89 | 1 | 0.27 | 64.0% | 1/16 (17), 1/4 (7), 1/2 (1) |
| 10 | mm. 64 to 70 | bass | 23 | 23 | 0.82 | 1.5 | 0.31 | 43.5% | 1/16 (10), 1/8 (6), 1/4 (5), 1/2 (2) |
| 10 | mm. 64 to 70 | other | 62 | 60 | 2.14 | 2.75 | 0.1 | 10.0% | 1/16 (58), 1/8 (2), 1/2 (2) |
| 11 | mm. 71 to 82 | all | 204 | 164 | 3.42 | 4 | 0.07 | 29.3% | 1/32 (1), 41/960 (1), 17/320 (1), 1/16 (132), 1/8 (32), 1/4 (32), 3/8 (1), 1/2 (2), 1/1 (2) |
| 11 | mm. 71 to 82 | soprano | 44 | 44 | 0.92 | 1.5 | 0.28 | 68.2% | 17/320 (1), 1/16 (26), 1/8 (2), 1/4 (12), 3/8 (1), 1/2 (2) |
| 11 | mm. 71 to 82 | bass | 52 | 52 | 1.08 | 2 | 0.22 | 53.8% | 1/16 (18), 1/8 (26), 1/4 (7), 1/1 (1) |
| 11 | mm. 71 to 82 | other | 108 | 101 | 2.1 | 3.25 | 0.12 | 20.8% | 1/32 (1), 41/960 (1), 1/16 (88), 1/8 (4), 1/4 (13), 1/1 (1) |
| 12 | mm. 83 to 94 | all | 226 | 185 | 3.85 | 4.75 | 0.06 | 25.4% | 1/32 (12), 1/16 (166), 1/8 (18), 1/4 (24), 3/8 (4), 1/2 (1), 3/4 (1) |
| 12 | mm. 83 to 94 | soprano | 34 | 34 | 0.71 | 1.5 | 0.35 | 38.2% | 1/32 (2), 1/16 (19), 1/8 (6), 1/4 (3), 3/8 (2), 1/2 (1), 3/4 (1) |
| 12 | mm. 83 to 94 | bass | 35 | 35 | 0.73 | 1 | 0.34 | 31.4% | 1/16 (28), 1/8 (2), 1/4 (3), 3/8 (2) |
| 12 | mm. 83 to 94 | other | 157 | 144 | 3 | 3.75 | 0.08 | 27.1% | 1/32 (10), 1/16 (119), 1/8 (10), 1/4 (18) |
| 13 | mm. 95 to 100 | all | 153 | 90 | 3.88 | 8 | 0.06 | 25.6% | 1/32 (36), 1/16 (66), 3/32 (35), 1/8 (1), 7/32 (4), 1/4 (9), 1/2 (2) |
| 13 | mm. 95 to 100 | soprano | 19 | 19 | 0.83 | 1 | 0.31 | 47.4% | 1/32 (2), 1/16 (9), 7/32 (4), 1/4 (2), 1/2 (2) |
| 13 | mm. 95 to 100 | bass | 23 | 23 | 1 | 1.25 | 0.24 | 56.5% | 1/16 (10), 3/32 (10), 1/4 (3) |
| 13 | mm. 95 to 100 | other | 111 | 74 | 3.15 | 8 | 0.08 | 23.0% | 1/32 (34), 1/16 (47), 3/32 (25), 1/8 (1), 1/4 (4) |
| 14 | m. 101 | all | 11 | 3 | 1 | 1 | 0.25 | 100.0% | 3/16 (9), 1/4 (2) |
| 14 | m. 101 | soprano | 3 | 3 | 1 | 1 | 0.25 | 100.0% | 3/16 (2), 1/4 (1) |
| 14 | m. 101 | bass | 3 | 3 | 1 | 1 | 0.25 | 100.0% | 3/16 (2), 1/4 (1) |
| 14 | m. 101 | other | 5 | 3 | 1 | 1 | 0.25 | 100.0% | 3/16 (5) |
| 15 | mm. 102 to 103 | all | 29 | 22 | 3.12 | 3.25 | 0.08 | 31.8% | 1/16 (20), 1/4 (9) |
| 15 | mm. 102 to 103 | soprano | 7 | 7 | 1 | 1 | 0.25 | 71.4% | 1/16 (5), 1/4 (2) |
| 15 | mm. 102 to 103 | bass | 6 | 6 | 0.88 | 1 | 0.33 | 50.0% | 1/16 (3), 1/4 (3) |
| 15 | mm. 102 to 103 | other | 16 | 15 | 2.08 | 2.5 | 0.12 | 33.3% | 1/16 (12), 1/4 (4) |
| 16 | mm. 104 to 108 | all | 85 | 74 | 3.7 | 4 | 0.07 | 27.0% | 1/16 (66), 1/4 (19) |
| 16 | mm. 104 to 108 | soprano | 18 | 18 | 0.9 | 1 | 0.28 | 66.7% | 1/16 (13), 1/4 (5) |
| 16 | mm. 104 to 108 | bass | 16 | 16 | 0.8 | 1 | 0.33 | 56.2% | 1/16 (9), 1/4 (7) |
| 16 | mm. 104 to 108 | other | 51 | 49 | 2.45 | 2.5 | 0.1 | 16.3% | 1/16 (44), 1/4 (7) |
| 17 | mm. 109 to 115 | all | 110 | 104 | 3.71 | 4 | 0.07 | 26.9% | 1/16 (85), 1/8 (8), 1/4 (12), 1/2 (5) |
| 17 | mm. 109 to 115 | soprano | 25 | 25 | 0.89 | 1 | 0.27 | 64.0% | 1/16 (17), 1/4 (7), 1/2 (1) |
| 17 | mm. 109 to 115 | bass | 23 | 23 | 0.82 | 1.5 | 0.31 | 43.5% | 1/16 (10), 1/8 (6), 1/4 (5), 1/2 (2) |
| 17 | mm. 109 to 115 | other | 62 | 60 | 2.14 | 2.75 | 0.1 | 10.0% | 1/16 (58), 1/8 (2), 1/2 (2) |
| 18 | mm. 116 to 127 | all | 204 | 164 | 3.42 | 4 | 0.07 | 29.3% | 1/32 (1), 41/960 (1), 17/320 (1), 1/16 (132), 1/8 (32), 1/4 (32), 3/8 (1), 1/2 (2), 1/1 (2) |
| 18 | mm. 116 to 127 | soprano | 44 | 44 | 0.92 | 1.5 | 0.28 | 68.2% | 17/320 (1), 1/16 (26), 1/8 (2), 1/4 (12), 3/8 (1), 1/2 (2) |
| 18 | mm. 116 to 127 | bass | 52 | 52 | 1.08 | 2 | 0.22 | 53.8% | 1/16 (18), 1/8 (26), 1/4 (7), 1/1 (1) |
| 18 | mm. 116 to 127 | other | 108 | 101 | 2.1 | 3.25 | 0.12 | 20.8% | 1/32 (1), 41/960 (1), 1/16 (88), 1/8 (4), 1/4 (13), 1/1 (1) |
| 19 | mm. 128 to 139 | all | 226 | 185 | 3.85 | 4.75 | 0.06 | 25.4% | 1/32 (12), 1/16 (166), 1/8 (18), 1/4 (24), 3/8 (4), 1/2 (1), 3/4 (1) |
| 19 | mm. 128 to 139 | soprano | 34 | 34 | 0.71 | 1.5 | 0.35 | 38.2% | 1/32 (2), 1/16 (19), 1/8 (6), 1/4 (3), 3/8 (2), 1/2 (1), 3/4 (1) |
| 19 | mm. 128 to 139 | bass | 35 | 35 | 0.73 | 1 | 0.34 | 31.4% | 1/16 (28), 1/8 (2), 1/4 (3), 3/8 (2) |
| 19 | mm. 128 to 139 | other | 157 | 144 | 3 | 3.75 | 0.08 | 27.1% | 1/32 (10), 1/16 (119), 1/8 (10), 1/4 (18) |
| 20 | mm. 140 to 145 | all | 153 | 90 | 3.88 | 8 | 0.06 | 25.6% | 1/32 (36), 1/16 (66), 3/32 (35), 1/8 (1), 7/32 (4), 1/4 (9), 1/2 (2) |
| 20 | mm. 140 to 145 | soprano | 19 | 19 | 0.83 | 1 | 0.31 | 47.4% | 1/32 (2), 1/16 (9), 7/32 (4), 1/4 (2), 1/2 (2) |
| 20 | mm. 140 to 145 | bass | 23 | 23 | 1 | 1.25 | 0.24 | 56.5% | 1/16 (10), 3/32 (10), 1/4 (3) |
| 20 | mm. 140 to 145 | other | 111 | 74 | 3.15 | 8 | 0.08 | 23.0% | 1/32 (34), 1/16 (47), 3/32 (25), 1/8 (1), 1/4 (4) |
| 21 | m. 146 | all | 11 | 3 | 1 | 1 | 0.25 | 100.0% | 3/16 (9), 1/4 (2) |
| 21 | m. 146 | soprano | 3 | 3 | 1 | 1 | 0.25 | 100.0% | 3/16 (2), 1/4 (1) |
| 21 | m. 146 | bass | 3 | 3 | 1 | 1 | 0.25 | 100.0% | 3/16 (2), 1/4 (1) |
| 21 | m. 146 | other | 5 | 3 | 1 | 1 | 0.25 | 100.0% | 3/16 (5) |

### Syncopation

A note is syncopated when it is attacked at a weaker metric level and held through a stronger one that no note of its line attacks. Each line (the soprano and bass of the outer voices, and Other for the rest) is read on its own. Strength is the difference of the two levels, 1 to 4: an off-beat note held over the beat is 1, a note on a beat held over the barline is 2, an off-beat note held over the barline is 3.

| Line | Measure | Beat | Attacked at | Held through | Strength |
| --- | --- | --- | --- | --- | --- |
| soprano | 14 | 2 | beat | secondary-beat (m. 14, beat 3) | 1 |
| soprano | 16 | 2 | beat | secondary-beat (m. 16, beat 3) | 1 |
| soprano | 22 | 3.12 | finer | offbeat (m. 22, beat 3.5) | 1 |
| soprano | 22 | 4.12 | finer | offbeat (m. 22, beat 4.5) | 1 |
| soprano | 23 | 1.12 | finer | beat (m. 23, beat 2) | 2 |
| soprano | 24 | 2 | beat | secondary-beat (m. 24, beat 3) | 1 |
| soprano | 42 | 2 | beat | secondary-beat (m. 42, beat 3) | 1 |
| soprano | 44 | 2 | beat | secondary-beat (m. 44, beat 3) | 1 |
| soprano | 50 | 3.12 | finer | offbeat (m. 50, beat 3.5) | 1 |
| soprano | 50 | 4.12 | finer | offbeat (m. 50, beat 4.5) | 1 |
| soprano | 51 | 1.12 | finer | beat (m. 51, beat 2) | 2 |
| soprano | 52 | 2 | beat | secondary-beat (m. 52, beat 3) | 1 |
| soprano | 87 | 2 | beat | secondary-beat (m. 87, beat 3) | 1 |
| soprano | 88 | 2 | beat | secondary-beat (m. 88, beat 3) | 1 |
| soprano | 89 | 2 | beat | secondary-beat (m. 89, beat 3) | 1 |
| soprano | 90 | 2 | beat | secondary-beat (m. 90, beat 3) | 1 |
| soprano | 95 | 3.12 | finer | offbeat (m. 95, beat 3.5) | 1 |
| soprano | 95 | 4.12 | finer | offbeat (m. 95, beat 4.5) | 1 |
| soprano | 96 | 3.12 | finer | offbeat (m. 96, beat 3.5) | 1 |
| soprano | 96 | 4.12 | finer | offbeat (m. 96, beat 4.5) | 1 |
| soprano | 132 | 2 | beat | secondary-beat (m. 132, beat 3) | 1 |
| soprano | 133 | 2 | beat | secondary-beat (m. 133, beat 3) | 1 |
| soprano | 134 | 2 | beat | secondary-beat (m. 134, beat 3) | 1 |
| soprano | 135 | 2 | beat | secondary-beat (m. 135, beat 3) | 1 |
| soprano | 140 | 3.12 | finer | offbeat (m. 140, beat 3.5) | 1 |
| soprano | 140 | 4.12 | finer | offbeat (m. 140, beat 4.5) | 1 |
| soprano | 141 | 3.12 | finer | offbeat (m. 141, beat 3.5) | 1 |
| soprano | 141 | 4.12 | finer | offbeat (m. 141, beat 4.5) | 1 |

### Hemiola

The library found no hemiola.

### Hypermeter

The library reads each phrase in hypermeasures of two or four measures: the length that fills the phrase and holds the cadence's arrival in the last one. The first measure of each hypermeasure is a hyperdownbeat. A phrase that does not fill its hypermeasures ends in a short one, an internal expansion makes the last one longer, an external expansion adds one of its own and a contraction shortens the last. Score is the library's confidence in the reading. The form adapter does not report elided cadences, so no phrase is read as elided.

| Phrase | Measures | Length | Hyperdownbeats | Hypermeasures | Cadence arrives | Score |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 9 | 4 | 1, 5, 9 | 2 regular, 1 short | none | 0.89 |
| 2 | mm. 10 to 21 | 4 | 10, 14, 18 | 3 regular | none | 1 |
| 3 | mm. 22 to 27 | 4 | 22, 26 | 1 regular, 1 contracted | m. 27 | 1 |
| 4 | mm. 28 to 29 | 2 | 28 | 1 regular | m. 28 | 1 |
| 5 | mm. 30 to 37 | 4 | 30, 34 | 2 regular | none | 1 |
| 6 | mm. 38 to 49 | 4 | 38, 42, 46 | 3 regular | none | 1 |
| 7 | mm. 50 to 55 | 4 | 50, 54 | 1 regular, 1 contracted | m. 55 | 1 |
| 8 | m. 56 | 4 | 56 | 1 short | m. 56 | 0.5 |
| 9 | mm. 57 to 63 | 2 | 57, 59, 61, 63 | 3 regular, 1 short | m. 63 | 0.93 |
| 10 | mm. 64 to 70 | 4 | 64, 68 | 1 regular, 1 contracted | m. 70 | 1 |
| 11 | mm. 71 to 82 | 4 | 71, 75, 79 | 3 regular | none | 1 |
| 12 | mm. 83 to 94 | 4 | 83, 87, 91 | 3 regular | none | 1 |
| 13 | mm. 95 to 100 | 4 | 95, 99 | 1 regular, 1 contracted | m. 100 | 1 |
| 14 | m. 101 | 4 | 101 | 1 short | m. 101 | 0.5 |
| 15 | mm. 102 to 103 | 2 | 102 | 1 regular | m. 103 | 1 |
| 16 | mm. 104 to 108 | 4 | 104, 108 | 1 regular, 1 short | m. 108 | 0.9 |
| 17 | mm. 109 to 115 | 4 | 109, 113 | 1 regular, 1 contracted | m. 115 | 1 |
| 18 | mm. 116 to 127 | 4 | 116, 120, 124 | 3 regular | none | 1 |
| 19 | mm. 128 to 139 | 4 | 128, 132, 136 | 3 regular | none | 1 |
| 20 | mm. 140 to 145 | 4 | 140, 144 | 1 regular, 1 contracted | m. 145 | 1 |
| 21 | m. 146 | 4 | 146 | 1 short | m. 146 | 0.5 |

### Tuplets and borrowed divisions

A meter divides its beat two ways (simple) or three (compound). A tuplet divides it the other way: a triplet in a simple meter or a duplet in a compound one is a borrowed division. The library names those two and says at which level a group occurs (the whole Beat, a Subdivision of it, or several beats) and whether that is typical. Only triplets (sextuplets included) and duplets are looked for, read from the notes' times alone, rounded to a grid of 96 to the whole note. A run is two or more consecutive attacks of one ratio.

| Ratio | Name | Runs | Attacks | Borrowed |
| --- | --- | --- | --- | --- |
| 3:2 | triplet | 4 | 12 | yes |

| Measure | Beat | Ratio | Name | Meter | Borrowed | Level | Attacks |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 4 | 2 | 3:2 | triplet | simple | yes | subdivision | 3 |
| 32 | 2 | 3:2 | triplet | simple | yes | subdivision | 3 |
| 73 | 2 | 3:2 | triplet | simple | yes | subdivision | 3 |
| 118 | 2 | 3:2 | triplet | simple | yes | subdivision | 3 |

## Texture

The library's texture adapter read every measure of the piece from the voices the kernel separated in each track or part, classified it, and merged consecutive measures of the same texture into passages. Category is the main texture (homorhythm and melody and accompaniment are homophony). Voices is the most sounding at once, Synchrony the share of attack times where the voices attack together, Pattern the score of a repeated cell (0 to 1: evidence for an accompaniment figure, not a name for it). Confidence is the texture's margin from its thresholds times the voice separation's.

| Measures | Texture | Category | Voices | Synchrony | Pattern | Confidence |
| --- | --- | --- | --- | --- | --- | --- |
| mm. 1 to 3 | melody and accompaniment | homophony | 2 | 0.36 | 0.1 | 0.44 |
| m. 4 | polyphony | polyphony | 2 | 0.36 | 0 | 0.36 |
| mm. 5 to 11 | melody and accompaniment | homophony | 3 | 0.19 | 0.44 | 0.63 |
| m. 12 | homorhythm | homophony | 4 | 1 | 0 | 0.61 |
| m. 13 | monophony | monophony | 1 | 0 | 0.53 | 0.77 |
| mm. 14 to 17 | melody and accompaniment | homophony | 2 | 0.35 | 0.51 | 0.47 |
| mm. 18 to 21 | monophony | monophony | 1 | 0 | 0.33 | 0.63 |
| m. 22 | homorhythm | homophony | 3 | 0.8 | 0.83 | 0.55 |
| mm. 23 to 25 | melody and accompaniment | homophony | 3 | 0.53 | 0.67 | 0.65 |
| mm. 26 to 27 | polyphony | polyphony | 4 | 0.15 | 0.21 | 0.59 |
| m. 28 | homorhythm | homophony | 4 | 1 | 0.33 | 0.83 |
| mm. 29 to 31 | melody and accompaniment | homophony | 2 | 0.36 | 0.1 | 0.44 |
| m. 32 | polyphony | polyphony | 2 | 0.36 | 0 | 0.36 |
| mm. 33 to 39 | melody and accompaniment | homophony | 3 | 0.19 | 0.44 | 0.63 |
| m. 40 | homorhythm | homophony | 4 | 1 | 0 | 0.61 |
| m. 41 | monophony | monophony | 1 | 0 | 0.53 | 0.77 |
| mm. 42 to 45 | melody and accompaniment | homophony | 2 | 0.35 | 0.51 | 0.47 |
| mm. 46 to 49 | monophony | monophony | 1 | 0 | 0.33 | 0.63 |
| m. 50 | homorhythm | homophony | 3 | 0.8 | 0.83 | 0.55 |
| mm. 51 to 53 | melody and accompaniment | homophony | 3 | 0.53 | 0.67 | 0.65 |
| mm. 54 to 55 | polyphony | polyphony | 4 | 0.15 | 0.21 | 0.59 |
| m. 56 | homorhythm | homophony | 4 | 1 | 0.33 | 0.83 |
| m. 57 | melody and accompaniment | homophony | 4 | 0.15 | 0.36 | 0.8 |
| m. 58 | polyphony | polyphony | 4 | 0.15 | 0.17 | 0.54 |
| mm. 59 to 60 | monophony | monophony | 1 | 0 | 0.37 | 0.68 |
| m. 61 | melody and accompaniment | homophony | 4 | 0.15 | 0.36 | 0.73 |
| m. 62 | polyphony | polyphony | 4 | 0.15 | 0.17 | 0.54 |
| m. 63 | melody and accompaniment | homophony | 2 | 0.06 | 0.36 | 0.74 |
| mm. 64 to 68 | monophony | monophony | 1 | 0 | 0.48 | 0.78 |
| mm. 69 to 72 | melody and accompaniment | homophony | 4 | 0.29 | 0.2 | 0.52 |
| m. 73 | polyphony | polyphony | 2 | 0.36 | 0 | 0.3 |
| mm. 74 to 84 | melody and accompaniment | homophony | 3 | 0.16 | 0.42 | 0.67 |
| m. 85 | homorhythm | homophony | 4 | 1 | 0 | 0.61 |
| m. 86 | monophony | monophony | 1 | 0 | 0.53 | 0.46 |
| mm. 87 to 90 | melody and accompaniment | homophony | 2 | 0.35 | 0.51 | 0.52 |
| mm. 91 to 94 | monophony | monophony | 1 | 0 | 0.32 | 0.65 |
| mm. 95 to 96 | melody and accompaniment | homophony | 4 | 0.7 | 0.85 | 0.78 |
| m. 97 | homorhythm | homophony | 2 | 0.94 | 0.69 | 0.61 |
| mm. 98 to 99 | melody and accompaniment | homophony | 4 | 0.33 | 0.51 | 0.68 |
| m. 100 | polyphony | polyphony | 4 | 0.15 | 0.33 | 0.62 |
| m. 101 | homorhythm | homophony | 4 | 1 | 0 | 0.59 |
| m. 102 | melody and accompaniment | homophony | 4 | 0.15 | 0.36 | 0.76 |
| m. 103 | polyphony | polyphony | 4 | 0.15 | 0.17 | 0.54 |
| mm. 104 to 105 | monophony | monophony | 1 | 0 | 0.37 | 0.68 |
| m. 106 | melody and accompaniment | homophony | 4 | 0.15 | 0.36 | 0.73 |
| m. 107 | polyphony | polyphony | 4 | 0.15 | 0.17 | 0.54 |
| m. 108 | melody and accompaniment | homophony | 2 | 0.06 | 0.36 | 0.74 |
| mm. 109 to 113 | monophony | monophony | 1 | 0 | 0.48 | 0.78 |
| mm. 114 to 117 | melody and accompaniment | homophony | 4 | 0.29 | 0.2 | 0.52 |
| m. 118 | polyphony | polyphony | 2 | 0.36 | 0 | 0.3 |
| mm. 119 to 129 | melody and accompaniment | homophony | 3 | 0.16 | 0.42 | 0.67 |
| m. 130 | homorhythm | homophony | 4 | 1 | 0 | 0.61 |
| m. 131 | monophony | monophony | 1 | 0 | 0.53 | 0.46 |
| mm. 132 to 135 | melody and accompaniment | homophony | 2 | 0.35 | 0.51 | 0.52 |
| mm. 136 to 139 | monophony | monophony | 1 | 0 | 0.32 | 0.65 |
| mm. 140 to 141 | melody and accompaniment | homophony | 4 | 0.7 | 0.85 | 0.78 |
| m. 142 | homorhythm | homophony | 2 | 0.94 | 0.69 | 0.61 |
| mm. 143 to 144 | melody and accompaniment | homophony | 4 | 0.33 | 0.51 | 0.68 |
| m. 145 | polyphony | polyphony | 4 | 0.15 | 0.33 | 0.62 |
| m. 146 | homorhythm | homophony | 4 | 1 | 0 | 0.59 |

### Changes

| Measure | From | To | Category changes |
| --- | --- | --- | --- |
| 4 | melody and accompaniment | polyphony | yes |
| 5 | polyphony | melody and accompaniment | yes |
| 12 | melody and accompaniment | homorhythm | no |
| 13 | homorhythm | monophony | yes |
| 14 | monophony | melody and accompaniment | yes |
| 18 | melody and accompaniment | monophony | yes |
| 22 | monophony | homorhythm | yes |
| 23 | homorhythm | melody and accompaniment | no |
| 26 | melody and accompaniment | polyphony | yes |
| 28 | polyphony | homorhythm | yes |
| 29 | homorhythm | melody and accompaniment | no |
| 32 | melody and accompaniment | polyphony | yes |
| 33 | polyphony | melody and accompaniment | yes |
| 40 | melody and accompaniment | homorhythm | no |
| 41 | homorhythm | monophony | yes |
| 42 | monophony | melody and accompaniment | yes |
| 46 | melody and accompaniment | monophony | yes |
| 50 | monophony | homorhythm | yes |
| 51 | homorhythm | melody and accompaniment | no |
| 54 | melody and accompaniment | polyphony | yes |
| 56 | polyphony | homorhythm | yes |
| 57 | homorhythm | melody and accompaniment | no |
| 58 | melody and accompaniment | polyphony | yes |
| 59 | polyphony | monophony | yes |
| 61 | monophony | melody and accompaniment | yes |
| 62 | melody and accompaniment | polyphony | yes |
| 63 | polyphony | melody and accompaniment | yes |
| 64 | melody and accompaniment | monophony | yes |
| 69 | monophony | melody and accompaniment | yes |
| 73 | melody and accompaniment | polyphony | yes |
| 74 | polyphony | melody and accompaniment | yes |
| 85 | melody and accompaniment | homorhythm | no |
| 86 | homorhythm | monophony | yes |
| 87 | monophony | melody and accompaniment | yes |
| 91 | melody and accompaniment | monophony | yes |
| 95 | monophony | melody and accompaniment | yes |
| 97 | melody and accompaniment | homorhythm | no |
| 98 | homorhythm | melody and accompaniment | no |
| 100 | melody and accompaniment | polyphony | yes |
| 101 | polyphony | homorhythm | yes |
| 102 | homorhythm | melody and accompaniment | no |
| 103 | melody and accompaniment | polyphony | yes |
| 104 | polyphony | monophony | yes |
| 106 | monophony | melody and accompaniment | yes |
| 107 | melody and accompaniment | polyphony | yes |
| 108 | polyphony | melody and accompaniment | yes |
| 109 | melody and accompaniment | monophony | yes |
| 114 | monophony | melody and accompaniment | yes |
| 118 | melody and accompaniment | polyphony | yes |
| 119 | polyphony | melody and accompaniment | yes |
| 130 | melody and accompaniment | homorhythm | no |
| 131 | homorhythm | monophony | yes |
| 132 | monophony | melody and accompaniment | yes |
| 136 | melody and accompaniment | monophony | yes |
| 140 | monophony | melody and accompaniment | yes |
| 142 | melody and accompaniment | homorhythm | no |
| 143 | homorhythm | melody and accompaniment | no |
| 145 | melody and accompaniment | polyphony | yes |
| 146 | polyphony | homorhythm | yes |

## Voice leading

The kernel separated the notes of the whole piece into voices (voice 0 is the top) and the library's counterpoint adapter classified how each pair of voices moves from one chord segment to the next. Parallel motion keeps the interval type, similar motion changes it; a voice that rests in either segment gives no motion. Faults are the library's: parallel fifths and octaves between any pair of voices, direct fifths and octaves in the outer pair, and a seventh or leading tone left unresolved when its chord changes (voice 0 is the top).

4 voices over 224 chord segments, 169 motions between consecutive segments: 40 contrary, 58 similar, 24 parallel, 43 oblique, 4 static.

| Voices | Contrary | Similar | Parallel | Oblique | Static |
| --- | --- | --- | --- | --- | --- |
| 0 and 1 | 10 | 24 | 0 | 12 | 0 |
| 0 and 2 | 18 | 20 | 4 | 8 | 2 |
| 0 and 3 | 2 | 2 | 0 | 6 | 0 |
| 1 and 2 | 4 | 10 | 18 | 7 | 2 |
| 1 and 3 | 4 | 2 | 2 | 8 | 0 |
| 2 and 3 | 2 | 0 | 0 | 2 | 0 |

0 parallel fifths, 0 parallel octaves, 0 direct fifths or octaves in the outer voices, 16 unresolved sevenths, 6 unresolved leading tones.

| Location | Fault | Voices |
| --- | --- | --- |
| m. 10, beat 4 to m. 11, beat 2 | leading-tone-resolves | 3 |
| m. 26, beat 4 to m. 27, beat 4 | seventh-resolves-down | 0 |
| m. 27, beat 4 to m. 28, beat 4 | seventh-resolves-down | 0 |
| m. 38, beat 4 to m. 39, beat 2 | leading-tone-resolves | 3 |
| m. 54, beat 4 to m. 55, beat 4 | seventh-resolves-down | 0 |
| m. 55, beat 4 to m. 56, beat 4 | seventh-resolves-down | 0 |
| m. 57, beat 1 to m. 58, beat 4 | seventh-resolves-down | 1 |
| m. 57, beat 1 to m. 58, beat 4 | seventh-resolves-down | 2 |
| m. 58, beat 4 to m. 60, beat 1 | seventh-resolves-down | 0 |
| m. 69, beat 3 to m. 71, beat 1 | leading-tone-resolves | 0 |
| m. 69, beat 3 to m. 71, beat 1 | seventh-resolves-down | 1 |
| m. 83, beat 4.5 to m. 84, beat 2 | leading-tone-resolves | 3 |
| m. 99, beat 4 to m. 100, beat 4 | seventh-resolves-down | 0 |
| m. 100, beat 4 to m. 101, beat 4 | seventh-resolves-down | 0 |
| m. 102, beat 1 to m. 103, beat 4 | seventh-resolves-down | 1 |
| m. 102, beat 1 to m. 103, beat 4 | seventh-resolves-down | 2 |
| m. 103, beat 4 to m. 105, beat 1 | seventh-resolves-down | 0 |
| m. 114, beat 3 to m. 116, beat 1 | leading-tone-resolves | 0 |
| m. 114, beat 3 to m. 116, beat 1 | seventh-resolves-down | 1 |
| m. 128, beat 4.5 to m. 129, beat 2 | leading-tone-resolves | 3 |
| m. 144, beat 4 to m. 145, beat 4 | seventh-resolves-down | 0 |
| m. 145, beat 4 to m. 146, beat 4 | seventh-resolves-down | 0 |

60 chords with three or more voices sounding: the root is doubled in 24, the third in 0, the fifth in 0, the seventh in 14, no member doubled in 22. Doubling is counted, not judged. Spacing is judged by the library for the 14 chords with four voices all sounding: 0 too wide.

## Schemas and sequences

The library's galant-schemas adapter matched its schema prototypes against the outer voices: the soprano and bass degree of each chord in its local key, with the chord's Roman numeral and the strength of its beat. Each match lists its stages by the measure they start in and the soprano and bass degrees found there (b is lowered, # raised; a stage with no soprano reads "-" and matches on the bass alone). Frame is the key the stages are read in, as a degree of the home key (1 is the home key). Confidence is the library's: the share of stages whose voices match, times the metric fit, times one minus the mean uncertainty of the chords.

No galant schema was found.

### Sequences

The library's scale-harmonization adapter found a model of one or more chords repeated, transposed by a constant diatonic interval, at least three times. Kind is the library's name for it when its rules for that pattern hold over whole units (rule-backed); the cycle of fifths has no rule in the library yet, so it is found from the Roman numerals alone. Units is how often the model sounds and, in brackets, how many chords it has; Steps the bass's diatonic steps from one unit to the next (up is positive). Stages read as in the schemas, in the home key.

| Measures | Kind | Rule-backed | Direction | Units (chords each) | Steps | Stages (soprano/bass) | Confidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| mm. 18 to 21 | cycle-of-fifths | no | - | 4 (2) | -1 | m. 18 2/7, m. 18 5/1, m. 19 1/6, m. 19 #4/7, m. 20 7/5, m. 20 3/6, m. 21 6/#4, m. 21 5/5 | 0.04 |
| mm. 46 to 49 | cycle-of-fifths | no | - | 4 (2) | -1 | m. 46 2/7, m. 46 5/1, m. 47 1/6, m. 47 #4/7, m. 48 7/5, m. 48 3/6, m. 49 6/#4, m. 49 5/5 | 0.04 |
| mm. 65 to 68 | parallel-chords | yes | descending | 4 (1) | -1 | m. 65 3/1, m. 66 2/7, m. 67 1/6, m. 68 7/#5 | 0.12 |
| mm. 78 to 80 | parallel-chords | yes | descending | 3 (1) | -1 | m. 78 6/4, m. 79 5/3, m. 80 4/2 | 0.21 |
| mm. 91 to 94 | cycle-of-fifths | no | - | 4 (2) | -1 | m. 91 5/3, m. 91 1/4, m. 92 4/2, m. 92 7/3, m. 93 3/1, m. 93 6/2, m. 94 2/7, m. 94 1/1 | 0 |
| mm. 110 to 113 | parallel-chords | yes | descending | 4 (1) | -1 | m. 110 3/1, m. 111 2/7, m. 112 1/6, m. 113 7/#5 | 0.12 |
| mm. 123 to 125 | parallel-chords | yes | descending | 3 (1) | -1 | m. 123 6/4, m. 124 5/3, m. 125 4/2 | 0.21 |
| mm. 136 to 139 | cycle-of-fifths | no | - | 4 (2) | -1 | m. 136 5/3, m. 136 1/4, m. 137 4/2, m. 137 7/3, m. 138 3/1, m. 138 6/2, m. 139 2/7, m. 139 1/1 | 0 |

## Markings

The tempo and dynamics markings the score notates, read with the library's standard-markings rules. A tempo holds until the next one, a dynamic until the next in its part. Group and the loudness comparison are the library's; a text it does not recognise has none. Wedges are not read, and articulations are not imported.

The score has no tempo marking.

The score has no dynamics marking.
