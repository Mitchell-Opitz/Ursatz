# Analysis Report: clementi-36.musicxml

## Summary

- **File:** clementi-36.musicxml, 333 notes.
- **Input:** MusicXML score, 1 part merged into one texture. Meter: notated. Key signature: notated. Pitch spelling: notated.
- **Key:** C major, the key with the most time across 5 key regions.
- **Chords:** 47 chord events; 45 matched a chord (96%), 2 did not.
- **Most frequent chords:** I (23), V (6), ii (5).

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
| m. 1, beat 1 to m. 7, beat 1 | C major | 0.82 | key signature |
| m. 7, beat 1 to m. 16, beat 1 | G major | 0.75 | profile |
| m. 16, beat 1 to m. 20, beat 1 | C minor | 0.83 | profile |
| m. 20, beat 1 to m. 23, beat 1 | G major | 0.94 | profile |
| m. 23, beat 1 to m. 39, beat 1 | C major | 0.74 | profile |

## Modulations

A modulation is a lasting new key. Position is where the new key arrives. Type is pivot when the library found a pivot chord and direct when it did not. A return goes back to the key before an excursion. The library gives the change's extent from the length of the new key, whether it returns, and whether a cadence arrives in it.

| Position | From | To | Extent | Type | Return |
| --- | --- | --- | --- | --- | --- |
| m. 7, beat 1 | C major | G major | modulation | pivot | no |
| m. 16, beat 1 | G major | C minor | weak-modulation | direct | no |
| m. 20, beat 1 | C minor | G major | weak-modulation | pivot | yes |
| m. 23, beat 1 | G major | C major | modulation | pivot | no |

## Pivot chords

The chord at a modulation that is diatonic in both the old and the new key, named by root and quality, with no Roman numeral. Modulation is its row in the Modulations table, counting from 1.

| Position | Chord | Bass | Inversion | From | To | Modulation |
| --- | --- | --- | --- | --- | --- | --- |
| m. 7, beat 1 to m. 7, beat 2 | G major | G | root position | C major | G major | 1 |
| m. 19, beat 1 to m. 20, beat 1 | G major | G | root position | C minor | G major | 3 |
| m. 23, beat 1 to m. 24, beat 1 | E minor7 | G | first | G major | C major | 4 |

## Chords

Positions are measure and beat. Numeral is the Roman numeral in the local key region, which is named where it changes. Function is the library's harmonic function of the chord: an applied chord is a dominant toward its target degree, and a predominant is strong (IV, ii) or weak (iii, vi). Six-four is the library's type for a chord in second inversion. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Numeral | Function | Six-four | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 2, beat 1 | I | I (in C major) | tonic | - | C | major | C | root position | 4 | 1.00 | - |
| m. 2, beat 1 to m. 3, beat 1 | I | I | tonic | - | C | major | C | root position | 4 | 1.00 | - |
| m. 3, beat 1 to m. 4, beat 1 | I | I | tonic | - | C | major | C | root position | 4 | 0.04 | minor7 (D minor7, score 0.72) |
| m. 4, beat 1 to m. 5, beat 1 | V | V | dominant | - | G | major | G | root position | 4 | 0.12 | - |
| m. 5, beat 1 to m. 6, beat 1 | I | I | tonic | - | C | major | C | root position | 4 | 1.00 | - |
| m. 6, beat 1 to m. 7, beat 1 | I | I | tonic | - | C | major | F# | - | 4 | 1.00 | - |
| m. 7, beat 1 to m. 7, beat 2 | I | I (in G major) | tonic | - | G | major | G | root position | 1 | 1.00 | - |
| m. 7, beat 2 to m. 7, beat 3 | ii | ii6 | predominant (strong) | - | A | minor | C | first | 1 | 0.36 | - |
| m. 7, beat 3 to m. 7, beat 4 | I | I6/4 | dominant | cadential | G | major | D | second | 1 | 0.14 | - |
| m. 7, beat 4 to m. 8, beat 1 | V | V | dominant | - | D | major | D | root position | 1 | 1.00 | - |
| m. 8, beat 1 to m. 9, beat 1 | I | I | tonic | - | G | major | G | root position | 4 | 0.06 | IV (C major, score 0.76) |
| m. 9, beat 1 to m. 10, beat 1 | V | V6 | dominant | - | D | major | F# | first | 4 | 0.16 | - |
| m. 10, beat 1 to m. 11, beat 1 | I | I | tonic | - | G | major | G | root position | 4 | 0.00 | vi (E minor, score 0.75) |
| m. 11, beat 1 to m. 12, beat 1 | ii | ii | predominant (strong) | - | A | minor | A | root position | 4 | 0.15 | - |
| m. 12, beat 1 to m. 13, beat 1 | I | I6 | tonic | - | G | major | B | first | 4 | 0.14 | - |
| m. 13, beat 1 to m. 14, beat 1 | vii° | vii°6/4 | dominant | - | F# | diminished | C | second | 4 | 0.13 | - |
| m. 14, beat 1 to m. 14, beat 3 | iii | iii6 | predominant (weak) | - | B | minor | D | first | 2 | 0.11 | minor7 (E minor7, score 0.66) |
| m. 14, beat 3 to m. 15, beat 1 | iii | iii6 | predominant (weak) | - | B | minor | D | first | 2 | 0.18 | - |
| m. 15, beat 1 to m. 15, beat 4 | I | I | tonic | - | G | major | G | root position | 3 | 1.00 | - |
| m. 16, beat 1 to m. 17, beat 1 | dominant7 | V4/2 (in C minor) | dominant | - | G | dominant7 | F | third | 4 | 0.01 | diminished (B diminished, score 0.89) |
| m. 17, beat 1 to m. 18, beat 1 | minor | i6 | tonic | - | C | minor | D# | first | 4 | 0.16 | - |
| m. 18, beat 1 to m. 18, beat 3 | diminished | viio | dominant | - | B | diminished | B | root position | 2 | 1.00 | - |
| m. 18, beat 3 to m. 19, beat 1 | minor | i | tonic | - | C | minor | C | root position | 2 | 1.00 | - |
| m. 19, beat 1 to m. 20, beat 1 | major | V | dominant | - | G | major | G | root position | 4 | 0.30 | - |
| m. 20, beat 1 to m. 21, beat 1 | I | V4/2/IV (in G major) | dominant of 4 | - | G | major | G | root position | 4 | 0.99 | - |
| m. 21, beat 1 to m. 22, beat 1 | IV | IV6/4 | predominant (strong) | - | C | major | G | second | 4 | 0.25 | - |
| m. 22, beat 1 to m. 22, beat 2 | iii | iii | predominant (weak) | - | B | minor | B | root position | 1 | 1.00 | - |
| m. 22, beat 2 to m. 22, beat 3 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 22, beat 3 to m. 23, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 23, beat 1 to m. 24, beat 1 | V | V (in C major) | dominant | - | G | major | G | root position | 4 | 0.00 | iii (E minor, score 0.97) |
| m. 24, beat 1 to m. 25, beat 1 | I | I | tonic | - | C | major | C | root position | 4 | 1.00 | - |
| m. 25, beat 1 to m. 26, beat 1 | I | I | tonic | - | C | major | C | root position | 4 | 1.00 | - |
| m. 26, beat 1 to m. 27, beat 1 | I | I | tonic | - | C | major | C | root position | 4 | 0.04 | minor7 (D minor7, score 0.72) |
| m. 27, beat 1 to m. 28, beat 1 | V | V | dominant | - | G | major | D | second | 4 | 0.12 | - |
| m. 28, beat 1 to m. 29, beat 1 | I | I | tonic | - | C | major | C | root position | 4 | 1.00 | - |
| m. 29, beat 1 to m. 30, beat 1 | I | I | tonic | - | C | major | C | root position | 4 | 1.00 | - |
| m. 30, beat 1 to m. 30, beat 2 | I | I6/4 | tonic | - | C | major | G | second | 1 | 1.00 | - |
| m. 30, beat 2 to m. 30, beat 3 | ii | ii | predominant (strong) | - | D | minor | D | root position | 1 | 1.00 | - |
| m. 30, beat 3 to m. 31, beat 1 | I | I6/4 | tonic | - | C | major | G | second | 2 | 0.13 | - |
| m. 31, beat 1 to m. 32, beat 1 | I | I | tonic | - | C | major | C | root position | 4 | 0.06 | IV (F major, score 0.76) |
| m. 32, beat 1 to m. 33, beat 1 | V | V6 | dominant | - | G | major | B | first | 4 | 0.16 | - |
| m. 33, beat 1 to m. 34, beat 1 | I | I | tonic | - | C | major | C | root position | 4 | 0.00 | vi (A minor, score 0.75) |
| m. 34, beat 1 to m. 35, beat 1 | ii | ii | predominant (strong) | - | D | minor | D | root position | 4 | 0.15 | - |
| m. 35, beat 1 to m. 36, beat 1 | I | I6 | tonic | - | C | major | E | first | 4 | 0.14 | - |
| m. 36, beat 1 to m. 37, beat 1 | ii | ii6 | predominant (strong) | - | D | minor | F | first | 4 | 0.04 | IV (F major, score 0.81) |
| m. 37, beat 1 to m. 38, beat 1 | iii | iii6 | predominant (weak) | - | E | minor | G | first | 4 | 0.28 | - |
| m. 38, beat 1 to m. 38, beat 4 | I | I | tonic | - | C | major | C | root position | 3 | 1.00 | - |

## Applied chords

Chords the library reads as applied (secondary) chords: the chord as it reads in the tonicized key, the chord it tonicizes and that chord's tonic. The last column checks each against the tonicization claims above for the same span; where they disagree both are shown and neither is changed.

| Position | Applied chord | Target | Resolution | Tonicization |
| --- | --- | --- | --- | --- |
| m. 20, beat 1 to m. 21, beat 1 | V4/2 (secondary-dominant) | IV (C) | to the target | DISAGREES: no tonicization is reported here |
## Outer voices

The highest (soprano) and lowest (bass) sounding note of each chord's span, with its scale degree in that chord's local key. A flat or sharp sign marks a chromatic alteration. A degree is "-" where the span lies in no key region. A rest means the library found no note for that line in the span, a bridged one a short gap inside the line, and a register guard a note the library dropped rather than adopt an accompaniment note. Degrees carry the key region's uncertainty in the analysis document.

| Position | Key | Soprano | Degree | Bass | Degree |
| --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 2, beat 1 | C major | C5 | 1 | C3 | 1 |
| m. 2, beat 1 to m. 3, beat 1 | C major | C5 | 1 | C3 | 1 |
| m. 3, beat 1 to m. 4, beat 1 | C major | F5 | 4 | C3 | 1 |
| m. 4, beat 1 to m. 5, beat 1 | C major | D5 | 2 | G2 | 5 |
| m. 5, beat 1 to m. 6, beat 1 | C major | C5 | 1 | C3 | 1 |
| m. 6, beat 1 to m. 7, beat 1 | C major | E5 | 3 | C4 | 1 |
| m. 7, beat 1 to m. 7, beat 2 | G major | D5 | 5 | G3 | 1 |
| m. 7, beat 2 to m. 7, beat 3 | G major | C5 | 4 | C3 | 4 |
| m. 7, beat 3 to m. 7, beat 4 | G major | B4 | 3 | D3 | 5 |
| m. 7, beat 4 to m. 8, beat 1 | G major | A4 | 2 | D2 | 5 |
| m. 8, beat 1 to m. 9, beat 1 | G major | A4 | 2 | G2 | 1 |
| m. 9, beat 1 to m. 10, beat 1 | G major | A4 | 2 | F#3 | 7 |
| m. 10, beat 1 to m. 11, beat 1 | G major | C5 | 4 | G3 | 1 |
| m. 11, beat 1 to m. 12, beat 1 | G major | C5 | 4 | A3 | 2 |
| m. 12, beat 1 to m. 13, beat 1 | G major | G5 | 1 | B3 | 3 |
| m. 13, beat 1 to m. 14, beat 1 | G major | F#5 | 7 | C4 | 4 |
| m. 14, beat 1 to m. 14, beat 3 | G major | E5 | 6 | D3 | 5 |
| m. 14, beat 3 to m. 15, beat 1 | G major | D5 | 5 | D2 | 5 |
| m. 15, beat 1 to m. 15, beat 4 | G major | G4 | 1 | G2 | 1 |
| m. 16, beat 1 to m. 17, beat 1 | C minor | B4 | #7 | F4 | 4 |
| m. 17, beat 1 to m. 18, beat 1 | C minor | D#5 | 3 | D#4 | 3 |
| m. 18, beat 1 to m. 18, beat 3 | C minor | F5 | 4 | B3 | #7 |
| m. 18, beat 3 to m. 19, beat 1 | C minor | D#5 | 3 | C4 | 1 |
| m. 19, beat 1 to m. 20, beat 1 | C minor | C5 | 1 | G3 | 5 |
| m. 20, beat 1 to m. 21, beat 1 | G major | G5 | 1 | F4 | b7 |
| m. 21, beat 1 to m. 22, beat 1 | G major | G5 | 1 | D#4 | b6 |
| m. 22, beat 1 to m. 22, beat 2 | G major | D#5 | b6 | B3 | 3 |
| m. 22, beat 2 to m. 22, beat 3 | G major | F5 | b7 | rest (register guard) | - |
| m. 22, beat 3 to m. 23, beat 1 | G major | F5 | b7 | C4 | 4 |
| m. 23, beat 1 to m. 24, beat 1 | C major | G5 | 5 | G3 | 5 |
| m. 24, beat 1 to m. 25, beat 1 | C major | C4 | 1 | C3 | 1 |
| m. 25, beat 1 to m. 26, beat 1 | C major | C4 | 1 | C3 | 1 |
| m. 26, beat 1 to m. 27, beat 1 | C major | F4 | 4 | C3 | 1 |
| m. 27, beat 1 to m. 28, beat 1 | C major | D4 | 2 | G2 | 5 |
| m. 28, beat 1 to m. 29, beat 1 | C major | C4 | 1 | C2 | 1 |
| m. 29, beat 1 to m. 30, beat 1 | C major | E4 | 3 | C3 | 1 |
| m. 30, beat 1 to m. 30, beat 2 | C major | G4 | 5 | G3 | 5 |
| m. 30, beat 2 to m. 30, beat 3 | C major | F4 | 4 | D4 | 2 |
| m. 30, beat 3 to m. 31, beat 1 | C major | E4 | 3 | G2 | 5 |
| m. 31, beat 1 to m. 32, beat 1 | C major | D4 | 2 | C3 | 1 |
| m. 32, beat 1 to m. 33, beat 1 | C major | D4 | 2 | B2 | 7 |
| m. 33, beat 1 to m. 34, beat 1 | C major | F4 | 4 | C3 | 1 |
| m. 34, beat 1 to m. 35, beat 1 | C major | F4 | 4 | D3 | 2 |
| m. 35, beat 1 to m. 36, beat 1 | C major | C5 | 1 | E3 | 3 |
| m. 36, beat 1 to m. 37, beat 1 | C major | A5 | 6 | F3 | 4 |
| m. 37, beat 1 to m. 38, beat 1 | C major | A4 | 6 | G3 | 5 |
| m. 38, beat 1 to m. 38, beat 4 | C major | C4 | 1 | C2 | 1 |

## Chord vocabulary

Each distinct Roman numeral of the chords the library numbered, for the whole piece and for each key region, sorted by count, then total length, then first appearance. Count is the number of chord rows with that numeral (every row in the Chords table is one). Runs counts a stretch of adjacent identical numerals in one key region once. Beats is the total length, Share its fraction of the scope's numbered chord length, and First where it first appears. Applied chords are listed on their own. Chords without a numeral are not counted.

### Overall

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 17 | 11 | 63 | 42.9% | m. 1, beat 1 |
| V | 5 | 5 | 17 | 11.6% | m. 4, beat 1 |
| ii | 3 | 3 | 9 | 6.1% | m. 11, beat 1 |
| iii6 | 3 | 2 | 8 | 5.4% | m. 14, beat 1 |
| I6/4 | 3 | 3 | 4 | 2.7% | m. 7, beat 3 |
| V6 | 2 | 2 | 8 | 5.4% | m. 9, beat 1 |
| I6 | 2 | 2 | 8 | 5.4% | m. 12, beat 1 |
| ii6 | 2 | 2 | 5 | 3.4% | m. 7, beat 2 |
| vii°6/4 | 1 | 1 | 4 | 2.7% | m. 13, beat 1 |
| V4/2 | 1 | 1 | 4 | 2.7% | m. 16, beat 1 |
| i6 | 1 | 1 | 4 | 2.7% | m. 17, beat 1 |
| IV6/4 | 1 | 1 | 4 | 2.7% | m. 21, beat 1 |
| viio | 1 | 1 | 2 | 1.4% | m. 18, beat 1 |
| i | 1 | 1 | 2 | 1.4% | m. 18, beat 3 |
| iii | 1 | 1 | 1 | 0.7% | m. 22, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 22 | 16 | 75 | 51.0% | m. 1, beat 1 |
| V | 7 | 7 | 25 | 17.0% | m. 4, beat 1 |
| ii | 5 | 5 | 14 | 9.5% | m. 7, beat 2 |
| iii | 4 | 3 | 9 | 6.1% | m. 14, beat 1 |
| i | 2 | 2 | 6 | 4.1% | m. 17, beat 1 |
| vii° | 1 | 1 | 4 | 2.7% | m. 13, beat 1 |
| V7 | 1 | 1 | 4 | 2.7% | m. 16, beat 1 |
| IV | 1 | 1 | 4 | 2.7% | m. 21, beat 1 |
| viio | 1 | 1 | 2 | 1.4% | m. 18, beat 1 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V4/2/IV | 1 | 1 | 4 | 2.7% | m. 20, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/IV | 1 | 1 | 4 | 2.7% | m. 20, beat 1 |

### C major (region 0)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 5 | 2 | 20 | 83.3% | m. 1, beat 1 |
| V | 1 | 1 | 4 | 16.7% | m. 4, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 5 | 2 | 20 | 83.3% | m. 1, beat 1 |
| V | 1 | 1 | 4 | 16.7% | m. 4, beat 1 |

### G major (region 1)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 4 | 4 | 12 | 34.3% | m. 7, beat 1 |
| iii6 | 2 | 1 | 4 | 11.4% | m. 14, beat 1 |
| V6 | 1 | 1 | 4 | 11.4% | m. 9, beat 1 |
| ii | 1 | 1 | 4 | 11.4% | m. 11, beat 1 |
| I6 | 1 | 1 | 4 | 11.4% | m. 12, beat 1 |
| vii°6/4 | 1 | 1 | 4 | 11.4% | m. 13, beat 1 |
| ii6 | 1 | 1 | 1 | 2.9% | m. 7, beat 2 |
| I6/4 | 1 | 1 | 1 | 2.9% | m. 7, beat 3 |
| V | 1 | 1 | 1 | 2.9% | m. 7, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 6 | 6 | 17 | 48.6% | m. 7, beat 1 |
| ii | 2 | 2 | 5 | 14.3% | m. 7, beat 2 |
| V | 2 | 2 | 5 | 14.3% | m. 7, beat 4 |
| iii | 2 | 1 | 4 | 11.4% | m. 14, beat 1 |
| vii° | 1 | 1 | 4 | 11.4% | m. 13, beat 1 |

### C minor (region 2)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V4/2 | 1 | 1 | 4 | 25.0% | m. 16, beat 1 |
| i6 | 1 | 1 | 4 | 25.0% | m. 17, beat 1 |
| V | 1 | 1 | 4 | 25.0% | m. 19, beat 1 |
| viio | 1 | 1 | 2 | 12.5% | m. 18, beat 1 |
| i | 1 | 1 | 2 | 12.5% | m. 18, beat 3 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| i | 2 | 2 | 6 | 37.5% | m. 17, beat 1 |
| V7 | 1 | 1 | 4 | 25.0% | m. 16, beat 1 |
| V | 1 | 1 | 4 | 25.0% | m. 19, beat 1 |
| viio | 1 | 1 | 2 | 12.5% | m. 18, beat 1 |

### G major (region 3)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| IV6/4 | 1 | 1 | 4 | 44.4% | m. 21, beat 1 |
| iii | 1 | 1 | 1 | 11.1% | m. 22, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| IV | 1 | 1 | 4 | 44.4% | m. 21, beat 1 |
| iii | 1 | 1 | 1 | 11.1% | m. 22, beat 1 |

**Applied chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V4/2/IV | 1 | 1 | 4 | 44.4% | m. 20, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| V7/IV | 1 | 1 | 4 | 44.4% | m. 20, beat 1 |

### C major (region 4)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 8 | 5 | 31 | 49.2% | m. 24, beat 1 |
| V | 2 | 2 | 8 | 12.7% | m. 23, beat 1 |
| ii | 2 | 2 | 5 | 7.9% | m. 30, beat 2 |
| I6/4 | 2 | 2 | 3 | 4.8% | m. 30, beat 1 |
| V6 | 1 | 1 | 4 | 6.3% | m. 32, beat 1 |
| I6 | 1 | 1 | 4 | 6.3% | m. 35, beat 1 |
| ii6 | 1 | 1 | 4 | 6.3% | m. 36, beat 1 |
| iii6 | 1 | 1 | 4 | 6.3% | m. 37, beat 1 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 11 | 8 | 38 | 60.3% | m. 24, beat 1 |
| V | 3 | 3 | 12 | 19.0% | m. 23, beat 1 |
| ii | 3 | 3 | 9 | 14.3% | m. 30, beat 2 |
| iii | 1 | 1 | 4 | 6.3% | m. 37, beat 1 |

## Prolongations

The library's standard-harmony analysis found 3 prolongations: a tonic or dominant that lasts through the chords between it and the harmony that closes it. Measures run from the prolonged chord to the closing chord. Numeral is the prolonged chord. Reason is why the library reads the chords between as a prolongation.

| Measures | Function | Numeral | Key | Reason |
| --- | --- | --- | --- | --- |
| mm. 8 to 11 | tonic | I | G major | tonic-line |
| mm. 15 to 18 | tonic | I | G major | tonic-line |
| mm. 31 to 34 | tonic | I | C major | tonic-line |

## Non-chord tones

69 notes sound against a chord without being one of its tones; 9 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 3, beat 1 to m. 4, beat 1 | 12 | passing | I |
| m. 3, beat 1 to m. 4, beat 1 | 15 | passing | I |
| m. 3, beat 1 to m. 4, beat 1 | 17 | neighbor | I |
| m. 3, beat 1 to m. 4, beat 1 | 20 | neighbor | I |
| m. 4, beat 1 to m. 5, beat 1 | 24 | passing | V |
| m. 4, beat 1 to m. 5, beat 1 | 26 | passing | V |
| m. 4, beat 1 to m. 5, beat 1 | 29 | unclassified | V |
| m. 4, beat 1 to m. 5, beat 1 | 30 | unclassified | V |
| m. 6, beat 1 to m. 7, beat 1 | 44 | unclassified | I |
| m. 8, beat 1 to m. 9, beat 1 | 60 | passing | I |
| m. 8, beat 1 to m. 9, beat 1 | 62 | passing | I |
| m. 8, beat 1 to m. 9, beat 1 | 64 | passing | I |
| m. 8, beat 1 to m. 9, beat 1 | 65 | passing | I |
| m. 10, beat 1 to m. 11, beat 1 | 81 | passing | I |
| m. 10, beat 1 to m. 11, beat 1 | 83 | passing | I |
| m. 10, beat 1 to m. 11, beat 1 | 84 | passing | I |
| m. 10, beat 1 to m. 11, beat 1 | 86 | passing | I |
| m. 11, beat 1 to m. 12, beat 1 | 90 | appoggiatura | ii |
| m. 11, beat 1 to m. 12, beat 1 | 93 | escape-tone | ii |
| m. 11, beat 1 to m. 12, beat 1 | 96 | appoggiatura | ii |
| m. 11, beat 1 to m. 12, beat 1 | 99 | escape-tone | ii |
| m. 12, beat 1 to m. 13, beat 1 | 105 | passing | I |
| m. 12, beat 1 to m. 13, beat 1 | 107 | passing | I |
| m. 13, beat 1 to m. 14, beat 1 | 111 | changing-tone | vii° |
| m. 13, beat 1 to m. 14, beat 1 | 112 | changing-tone | vii° |
| m. 13, beat 1 to m. 14, beat 1 | 116 | passing | vii° |
| m. 13, beat 1 to m. 14, beat 1 | 118 | unclassified | vii° |
| m. 14, beat 1 to m. 14, beat 3 | 119 | suspension | iii |
| m. 14, beat 1 to m. 14, beat 3 | 122 | passing | iii |
| m. 14, beat 3 to m. 15, beat 1 | 126 | passing | iii |
| m. 14, beat 3 to m. 15, beat 1 | 128 | passing | iii |
| m. 19, beat 1 to m. 20, beat 1 | 155 | passing | major |
| m. 20, beat 1 to m. 21, beat 1 | 161 | unclassified | I |
| m. 21, beat 1 to m. 22, beat 1 | 173 | unclassified | IV |
| m. 22, beat 1 to m. 22, beat 2 | 186 | passing | iii |
| m. 23, beat 1 to m. 24, beat 1 | 199 | passing | V |
| m. 23, beat 1 to m. 24, beat 1 | 200 | passing | V |
| m. 26, beat 1 to m. 27, beat 1 | 214 | passing | I |
| m. 26, beat 1 to m. 27, beat 1 | 217 | passing | I |
| m. 26, beat 1 to m. 27, beat 1 | 219 | neighbor | I |
| m. 26, beat 1 to m. 27, beat 1 | 222 | neighbor | I |
| m. 27, beat 1 to m. 28, beat 1 | 226 | passing | V |
| m. 27, beat 1 to m. 28, beat 1 | 228 | passing | V |
| m. 27, beat 1 to m. 28, beat 1 | 231 | unclassified | V |
| m. 27, beat 1 to m. 28, beat 1 | 232 | unclassified | V |
| m. 30, beat 3 to m. 31, beat 1 | 254 | neighbor | I |
| m. 30, beat 3 to m. 31, beat 1 | 255 | unclassified | I |
| m. 31, beat 1 to m. 32, beat 1 | 258 | passing | I |
| m. 31, beat 1 to m. 32, beat 1 | 260 | passing | I |
| m. 31, beat 1 to m. 32, beat 1 | 262 | passing | I |
| m. 31, beat 1 to m. 32, beat 1 | 263 | passing | I |
| m. 33, beat 1 to m. 34, beat 1 | 279 | passing | I |
| m. 33, beat 1 to m. 34, beat 1 | 281 | passing | I |
| m. 33, beat 1 to m. 34, beat 1 | 282 | passing | I |
| m. 33, beat 1 to m. 34, beat 1 | 284 | passing | I |
| m. 34, beat 1 to m. 35, beat 1 | 288 | appoggiatura | ii |
| m. 34, beat 1 to m. 35, beat 1 | 291 | escape-tone | ii |
| m. 34, beat 1 to m. 35, beat 1 | 294 | appoggiatura | ii |
| m. 34, beat 1 to m. 35, beat 1 | 297 | escape-tone | ii |
| m. 35, beat 1 to m. 36, beat 1 | 303 | passing | I |
| m. 35, beat 1 to m. 36, beat 1 | 305 | passing | I |
| m. 36, beat 1 to m. 37, beat 1 | 309 | passing | ii |
| m. 36, beat 1 to m. 37, beat 1 | 311 | passing | ii |
| m. 36, beat 1 to m. 37, beat 1 | 314 | passing | ii |
| m. 36, beat 1 to m. 37, beat 1 | 315 | passing | ii |
| m. 37, beat 1 to m. 38, beat 1 | 319 | changing-tone | iii |
| m. 37, beat 1 to m. 38, beat 1 | 320 | changing-tone | iii |
| m. 37, beat 1 to m. 38, beat 1 | 324 | changing-tone | iii |
| m. 37, beat 1 to m. 38, beat 1 | 325 | changing-tone | iii |

## Cadences

The library's standard-harmony analysis found 5 cadences from the chords, the meter and the soprano. Position is where the arrival begins. Key is the key region of the arrival. Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added before a section start.

| Position | Type | Key | Soprano | Beat strength | Boundary |
| --- | --- | --- | --- | --- | --- |
| m. 5, beat 1 | perfect-authentic | C major | 1 | strong | no |
| m. 8, beat 1 | imperfect-authentic | G major | 2 | strong | no |
| m. 19, beat 1 | half | C minor | 1 | strong | no |
| m. 24, beat 1 | perfect-authentic | C major | 1 | strong | no |
| m. 28, beat 1 | perfect-authentic | C major | 1 | strong | no |

### Cadence plan

1. m. 5, beat 1: perfect-authentic in C major
2. m. 8, beat 1: imperfect-authentic in G major
3. m. 19, beat 1: half in C minor
4. m. 24, beat 1: perfect-authentic in C major
5. m. 28, beat 1: perfect-authentic in C major

**Counts:** perfect-authentic 3, imperfect-authentic 1, half 1.

## Phrases

The library's standard-form analysis found 7 phrases from the soprano, the meter and the cadences. Length counts the measures of the phrase proper; Ends is what closed it: its cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in the piece; Expansion is a lengthening or shortening of the phrase's ending, or an extension after its cadence, with the measures it adds or removes.

| Phrase | Measures | Length | Cadence | Ends | Role | Expansion |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 7 | 7 | perfect-authentic (m. 5, beat 1) | cadence | beginning | contraction -1 (mm. 5 to 7) |
| 2 | m. 8 | 1 | imperfect-authentic (m. 8, beat 1) | cadence | middle | - |
| 3 | mm. 9 to 19 | 11 | half (m. 19, beat 1) | cadence | middle | internal +3 (mm. 13 to 19) |
| 4 | mm. 20 to 26 | 7 | perfect-authentic (m. 24, beat 1) | cadence | middle | contraction -1 (mm. 24 to 26) |
| 5 | mm. 27 to 29 | 3 | perfect-authentic (m. 28, beat 1) | cadence | middle | - |
| 6 | mm. 30 to 36 | 7 | none | discontinuity | middle | - |
| 7 | mm. 37 to 38 | 2 | none | none | ending | - |

- **Phrase 1:** ideas basic-idea (mm. 1 to 2), contrasting-idea (mm. 3 to 4), contrasting-idea (mm. 5 to 7); subphrases mm. 1 to 4, mm. 5 to 7; phrase model Tb (m. 1, beat 1), Tb (m. 2, beat 1), Tb (m. 3, beat 1), D (m. 4, beat 1), Te (m. 5, beat 1), Te (m. 6, beat 1), Te (m. 7, beat 1)
- **Phrase 2:** ideas basic-idea (m. 8)
- **Phrase 3:** ideas basic-idea (mm. 9 to 10), contrasting-idea (mm. 11 to 12), contrasting-idea (mm. 13 to 14), contrasting-idea (mm. 15 to 16), contrasting-idea (mm. 17 to 19); subphrases mm. 9 to 12, mm. 13 to 19; phrase model D (m. 19, beat 1)
- **Phrase 4:** ideas basic-idea (mm. 20 to 21), contrasting-idea (mm. 22 to 23), contrasting-idea (mm. 24 to 26); subphrases mm. 20 to 23, mm. 24 to 26; phrase model PD (m. 21, beat 1), D (m. 23, beat 1), Te (m. 24, beat 1), Te (m. 25, beat 1), Te (m. 26, beat 1)
- **Phrase 5:** ideas basic-idea (mm. 27 to 29); phrase model D (m. 27, beat 1), Te (m. 28, beat 1), Te (m. 29, beat 1)
- **Phrase 6:** ideas basic-idea (mm. 30 to 31), contrasting-idea (mm. 32 to 33), contrasting-idea (mm. 34 to 36); subphrases mm. 30 to 33, mm. 34 to 36
- **Phrase 7:** ideas basic-idea (mm. 37 to 38)

## Themes

The library's standard-form analysis found 3 themes from the phrases, their ideas and the cadences. Form is the library's name for the pairing of the two parts; Commonality is how common that pairing is; Traits are what the library finds in the continuation.

| Theme | Form | Commonality | Measures | Length | Parts | Traits |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | hybrid | slightly-less-common | mm. 1 to 7 | 7 | compound-basic-idea (mm. 1 to 4), continuation (mm. 5 to 7, perfect-authentic (m. 5, beat 1)) | - |
| 2 | hybrid | slightly-less-common | mm. 9 to 19 | 11 | compound-basic-idea (mm. 9 to 12), continuation (mm. 13 to 19, half (m. 19, beat 1)) | - |
| 3 | hybrid | slightly-less-common | mm. 20 to 26 | 7 | compound-basic-idea (mm. 20 to 23), continuation (mm. 24 to 26, perfect-authentic (m. 24, beat 1)) | - |

## Form plan

The kernel's form structure detection grouped the phrases into 5 sections, closing a section at each phrase that ends on a perfect or imperfect authentic cadence; the phrases after the last such cadence form the last section. Sections are lettered by first appearance, and a section whose opening theme returns an earlier section's material takes that letter with a prime. Key is the key the section opens in.

| Section | Measures | Key | Phrases | Closing cadence | Returns |
| --- | --- | --- | --- | --- | --- |
| A | mm. 1 to 7 | C major | 1 | perfect-authentic (m. 5, beat 1) | - |
| B | m. 8 | G major | 2 | imperfect-authentic (m. 8, beat 1) | - |
| A' | mm. 9 to 26 | G major | 3 to 4 | perfect-authentic (m. 24, beat 1) | returns A (similarity 0.80) |
| C | mm. 27 to 29 | C major | 5 | perfect-authentic (m. 28, beat 1) | - |
| D | mm. 30 to 38 | C major | 6 to 7 | none | - |

### Thematic returns

A later theme recalls an earlier one when the kernel's thematic return detection finds the same contour and rhythm in a stretch of both. Similarity is the lower of the two scores. Themes that share a phrase are not compared.

| Later theme | Measures | Section | Earlier theme | Measures | Section | Similarity |
| --- | --- | --- | --- | --- | --- | --- |
| 2 | mm. 9 to 19 | A' | 1 | mm. 1 to 7 | A | 0.80 |

### Outline

- form (mm. 1 to 38)
  - section A (mm. 1 to 7, C major)
    - phrase 1 (mm. 1 to 7)
      - subphrase 1 (mm. 1 to 4)
        - idea basic-idea (mm. 1 to 2)
        - idea contrasting-idea (mm. 3 to 4)
      - subphrase 2 (mm. 5 to 7)
        - idea contrasting-idea (mm. 5 to 7)
  - section B (m. 8, G major)
    - phrase 2 (m. 8)
      - idea basic-idea (m. 8)
  - section A' (mm. 9 to 26, G major)
    - phrase 3 (mm. 9 to 19)
      - subphrase 1 (mm. 9 to 12)
        - idea basic-idea (mm. 9 to 10)
        - idea contrasting-idea (mm. 11 to 12)
      - subphrase 2 (mm. 13 to 19)
        - idea contrasting-idea (mm. 13 to 14)
        - idea contrasting-idea (mm. 15 to 16)
        - idea contrasting-idea (mm. 17 to 19)
    - phrase 4 (mm. 20 to 26)
      - subphrase 1 (mm. 20 to 23)
        - idea basic-idea (mm. 20 to 21)
        - idea contrasting-idea (mm. 22 to 23)
      - subphrase 2 (mm. 24 to 26)
        - idea contrasting-idea (mm. 24 to 26)
  - section C (mm. 27 to 29, C major)
    - phrase 5 (mm. 27 to 29)
      - idea basic-idea (mm. 27 to 29)
  - section D (mm. 30 to 38, C major)
    - phrase 6 (mm. 30 to 36)
      - subphrase 1 (mm. 30 to 33)
        - idea basic-idea (mm. 30 to 31)
        - idea contrasting-idea (mm. 32 to 33)
      - subphrase 2 (mm. 34 to 36)
        - idea contrasting-idea (mm. 34 to 36)
    - phrase 7 (mm. 37 to 38)
      - idea basic-idea (mm. 37 to 38)

## Melody

The library's melody adapter read the soprano of each phrase as a line of notes: the soprano notes that start in the phrase. Range is the lowest and highest note and the semitones between them; Climax is the first note at the highest pitch, with how many notes sound it when it is not one; Contour counts the intervals between consecutive notes that go up, go down and repeat, and how many of them are steps and leaps.

| Phrase | Measures | Notes | Range | Climax | Up | Down | Same | Steps | Leaps |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 7 | 28 | G4 to G5 (12) | G5, m. 2, beat 4, 2 notes | 10 | 15 | 1 | 8 | 17 |
| 2 | m. 8 | 7 | A4 to G5 (10) | G5, m. 8, beat 4.5 | 6 | 0 | 0 | 6 | 0 |
| 3 | mm. 9 to 19 | 47 | G4 to D6 (19) | D6, m. 12, beat 2.5 | 18 | 21 | 6 | 20 | 19 |
| 4 | mm. 20 to 26 | 32 | D3 to G5 (29) | G5, m. 20, beat 1, 9 notes | 9 | 13 | 8 | 8 | 14 |
| 5 | mm. 27 to 29 | 13 | G3 to C5 (17) | C5, m. 29, beat 4 | 5 | 5 | 1 | 2 | 8 |
| 6 | mm. 30 to 36 | 39 | D4 to A5 (19) | A5, m. 36, beat 1 | 17 | 16 | 5 | 22 | 11 |
| 7 | mm. 37 to 38 | 8 | C3 to A4 (21) | A4, m. 37, beat 1.5 | 2 | 4 | 0 | 3 | 3 |

### Leaps

A leap is an interval of a third or larger. The library's rule asks a leap of a fourth or larger to be followed by a step in the opposite direction: such a leap is Recovered when one follows, Unrecovered when a step the same way, a repeated note or another leap does, and Not judged when nothing follows it; a third asks nothing (Not required). Consecutive counts the pairs of leaps in a row the library's rule finds fault with.

| Phrase | Leaps | Recovered | Unrecovered | Not required | Not judged | Consecutive |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 17 | 1 | 4 | 12 | 0 | 11 |
| 2 | 0 | 0 | 0 | 0 | 0 | 0 |
| 3 | 19 | 1 | 6 | 11 | 1 | 4 |
| 4 | 14 | 1 | 3 | 9 | 1 | 7 |
| 5 | 8 | 0 | 2 | 5 | 1 | 5 |
| 6 | 11 | 1 | 6 | 4 | 0 | 1 |
| 7 | 3 | 0 | 0 | 3 | 0 | 0 |

### Non-chord tones

Each melody note is set against the chord of the segment it starts in, as the library's chord identification reads it: a Chord tone is one of the chord's tones, a Non-chord tone is one it left over and named, and Unclassified notes are in a segment with no chord or were left unnamed. By type counts the non-chord tones under the library's names.

| Phrase | Notes | Chord tones | Non-chord tones | Unclassified | By type |
| --- | --- | --- | --- | --- | --- |
| 1 | 28 | 24 | 2 | 2 | passing 1, neighbor 1 |
| 2 | 7 | 3 | 4 | 0 | passing 3, retardation 1 |
| 3 | 47 | 35 | 9 | 3 | passing 6, neighbor 1, appoggiatura 1, retardation 1 |
| 4 | 32 | 21 | 2 | 9 | passing 1, neighbor 1 |
| 5 | 13 | 12 | 1 | 0 | passing 1 |
| 6 | 39 | 26 | 10 | 3 | passing 9, appoggiatura 1 |
| 7 | 8 | 4 | 2 | 2 | appoggiatura 2 |

### Tendencies

The library's melodic tendencies are soft preferences a well-formed line usually shows: Pitch proximity (steps outnumber leaps, small leaps outnumber large ones), Step declination (descending steps outnumber ascending ones), Step inertia (the line continues in its direction at least as often as it reverses), Melodic regression (extreme notes turn back toward the middle) and Melodic arch (the line rises to a climax in its middle half and ends below it); a phrase either shows a tendency or does not. The seventh degree column counts the notes the library labels the seventh degree that another note follows, judged by its leading tone rule: a leading tone goes up to the tonic by step, and a seventh a whole step below the tonic does not. Followed is the count that did, Broken the count that did not, with where each broken one stands.

| Phrase | Pitch proximity | Step declination | Step inertia | Melodic regression | Melodic arch | Seventh degree followed | Seventh degree broken |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | does not hold | holds | does not hold | does not hold | holds | 1 | 1 (m. 4, beat 2) |
| 2 | holds | does not hold | holds | does not hold | does not hold | 1 | 0 |
| 3 | holds | holds | does not hold | does not hold | holds | 0 | 6 (m. 10, beat 4.5; m. 12, beat 2; m. 12, beat 3.5; m. 14, beat 2.5; m. 14, beat 4; m. 16, beat 1) |
| 4 | does not hold | holds | does not hold | does not hold | does not hold | 1 | 0 |
| 5 | does not hold | holds | holds | does not hold | does not hold | 0 | 1 (m. 27, beat 2) |
| 6 | does not hold | does not hold | holds | does not hold | does not hold | 2 | 1 (m. 36, beat 4) |
| 7 | does not hold | does not hold | does not hold | does not hold | does not hold | 0 | 0 |

## Progression patterns

8 recurring progressions of two to six chords, counted over the library's Roman numerals: a held chord counts once, and a progression does not cross an unlabelled chord or a change of key. Ranked by count, then length, then first occurrence. Measures are where each occurrence begins and ends.

| Rank | Progression | Count | Measures |
| --- | --- | --- | --- |
| 1 | V > I | 6 | mm. 4 to 6, mm. 7 to 8, mm. 9 to 10, mm. 23 to 26, mm. 27 to 30, mm. 32 to 33 |
| 2 | I > V > I | 5 | mm. 1 to 6, mm. 7 to 8, mm. 8 to 10, mm. 24 to 30, mm. 30 to 33 |
| 3 | I > ii | 5 | m. 7, mm. 10 to 11, mm. 28 to 30, mm. 33 to 34, mm. 35 to 36 |
| 4 | I > ii > I | 4 | m. 7, mm. 10 to 12, mm. 28 to 31, mm. 33 to 35 |
| 5 | I > V > I > ii > I | 3 | mm. 8 to 12, mm. 24 to 31, mm. 30 to 35 |
| 6 | V > I > V > I > ii > I | 2 | mm. 7 to 12, mm. 23 to 31 |
| 7 | I > ii > I > V > I | 2 | mm. 7 to 8, mm. 28 to 33 |
| 8 | iii > I | 2 | mm. 14 to 15, mm. 37 to 38 |

## Harmonic rhythm

How often the harmony changes. A change is a chord whose base Roman numeral or key differs from the chord before it, counted in the measure it starts in. Changes lists the measures of each row in order; Mean is changes per measure over the row.

| Measures | Changes | Mean |
| --- | --- | --- |
| mm. 1 to 4 | 1, 0, 0, 1 | 0.5 |
| mm. 5 to 8 | 1, 0, 4, 1 | 1.5 |
| mm. 9 to 12 | 1, 1, 1, 1 | 1 |
| mm. 13 to 16 | 1, 1, 1, 1 | 1 |
| mm. 17 to 20 | 1, 2, 1, 1 | 1.25 |
| mm. 21 to 24 | 1, 2, 1, 1 | 1.25 |
| mm. 25 to 28 | 0, 0, 1, 1 | 0.5 |
| mm. 29 to 32 | 0, 2, 0, 1 | 0.75 |
| mm. 33 to 36 | 1, 1, 1, 1 | 1 |
| mm. 37 to 38 | 1, 1 | 1 |

**Mean:** 0.97 changes per measure over 38 measures.

### Approach to cadences

Changes per measure over the two measures before each cadence's arrival (Approach), against the two measures before those (Before). Faster is yes when the approach changes harmony more often.

| Cadence | Arrival | Before | Approach | Faster |
| --- | --- | --- | --- | --- |
| 1 | m. 5 | 0.5 | 0.5 | no |
| 2 | m. 8 | 1 | 2 | yes |
| 3 | m. 19 | 1 | 1.5 | yes |
| 4 | m. 24 | 1 | 1.5 | yes |
| 5 | m. 28 | 0.5 | 0.5 | no |

## Rhythm and meter

**Meter fit:** 4/4, scale 1, no pickup (fit 0.94); no better reading found

### Density

The library's density profile of each phrase, for all its notes and for each line that has a note in it (the soprano and bass of the outer voices, and Other for the rest). A note is in a phrase when its onset is; a chord is one attack. Attacks per beat is read measure by measure over the beats the phrase spans in it, as a mean and the largest; Mean gap is the mean distance between attacks in whole notes; On the beat is the share of attacks exactly on a beat. Durations lists the notes' lengths in whole notes, shortest first, with how many notes have each. The patterns that recur are the motive families' rhythms (see Motives).

| Phrase | Measures | Line | Notes | Attacks | Per beat | Max per beat | Mean gap | On the beat | Durations |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 7 | all | 49 | 39 | 1.61 | 2 | 0.16 | 64.1% | 1/8 (28), 1/4 (21) |
| 1 | mm. 1 to 7 | soprano | 28 | 28 | 1.11 | 1.5 | 0.22 | 71.4% | 1/8 (19), 1/4 (9) |
| 1 | mm. 1 to 7 | bass | 14 | 14 | 0.61 | 1.25 | 0.46 | 85.7% | 1/8 (4), 1/4 (10) |
| 1 | mm. 1 to 7 | other | 7 | 7 | 0.36 | 1 | 0.94 | 42.9% | 1/8 (5), 1/4 (2) |
| 2 | m. 8 | all | 9 | 8 | 2 | 2 | 0.12 | 50.0% | 1/8 (8), 1/4 (1) |
| 2 | m. 8 | soprano | 7 | 7 | 1.75 | 1.75 | 0.12 | 42.9% | 1/8 (7) |
| 2 | m. 8 | bass | 1 | 1 | 0.25 | 0.25 | 0 | 100.0% | 1/4 (1) |
| 2 | m. 8 | other | 1 | 1 | 0.25 | 0.25 | 0 | 100.0% | 1/8 (1) |
| 3 | mm. 9 to 19 | all | 93 | 73 | 1.66 | 2 | 0.15 | 58.9% | 1/8 (60), 1/4 (27), 1/2 (4), 1/1 (2) |
| 3 | mm. 9 to 19 | soprano | 47 | 47 | 1.07 | 1.75 | 0.23 | 72.3% | 1/8 (30), 1/4 (16), 1/2 (1) |
| 3 | mm. 9 to 19 | bass | 31 | 31 | 0.7 | 2 | 0.33 | 71.0% | 1/8 (19), 1/4 (7), 1/2 (3), 1/1 (2) |
| 3 | mm. 9 to 19 | other | 15 | 15 | 0.34 | 0.75 | 0.68 | 46.7% | 1/8 (11), 1/4 (4) |
| 4 | mm. 20 to 26 | all | 64 | 48 | 1.71 | 2 | 0.15 | 58.3% | 1/8 (40), 1/4 (24) |
| 4 | mm. 20 to 26 | soprano | 32 | 32 | 1.14 | 1.5 | 0.22 | 68.8% | 1/8 (27), 1/4 (5) |
| 4 | mm. 20 to 26 | bass | 17 | 17 | 0.61 | 1 | 0.41 | 100.0% | 1/4 (17) |
| 4 | mm. 20 to 26 | other | 15 | 15 | 0.54 | 1 | 0.46 | 33.3% | 1/8 (13), 1/4 (2) |
| 5 | mm. 27 to 29 | all | 22 | 18 | 1.5 | 2 | 0.16 | 66.7% | 1/8 (12), 1/4 (10) |
| 5 | mm. 27 to 29 | soprano | 13 | 13 | 1.08 | 1.25 | 0.23 | 76.9% | 1/8 (7), 1/4 (6) |
| 5 | mm. 27 to 29 | bass | 7 | 7 | 0.58 | 1.25 | 0.33 | 71.4% | 1/8 (4), 1/4 (3) |
| 5 | mm. 27 to 29 | other | 2 | 2 | 0.17 | 0.25 | 1.38 | 50.0% | 1/8 (1), 1/4 (1) |
| 6 | mm. 30 to 36 | all | 71 | 52 | 1.86 | 2 | 0.13 | 53.8% | 1/8 (48), 1/4 (23) |
| 6 | mm. 30 to 36 | soprano | 39 | 39 | 1.39 | 1.75 | 0.18 | 64.1% | 1/8 (27), 1/4 (12) |
| 6 | mm. 30 to 36 | bass | 24 | 24 | 0.86 | 2 | 0.28 | 66.7% | 1/8 (16), 1/4 (8) |
| 6 | mm. 30 to 36 | other | 8 | 8 | 0.29 | 0.75 | 0.95 | 75.0% | 1/8 (5), 1/4 (3) |
| 7 | mm. 37 to 38 | all | 16 | 13 | 1.62 | 2 | 0.12 | 53.8% | 1/8 (12), 1/4 (4) |
| 7 | mm. 37 to 38 | soprano | 8 | 8 | 1 | 1.5 | 0.18 | 37.5% | 1/8 (7), 1/4 (1) |
| 7 | mm. 37 to 38 | bass | 6 | 6 | 0.75 | 1 | 0.3 | 83.3% | 1/8 (3), 1/4 (3) |
| 7 | mm. 37 to 38 | other | 2 | 2 | 0.25 | 0.5 | 0.5 | 100.0% | 1/8 (2) |

### Syncopation

The library found no syncopation.

### Hemiola

The library found no hemiola.

### Hypermeter

The library reads each phrase in hypermeasures of two or four measures: the length that fills the phrase and holds the cadence's arrival in the last one. The first measure of each hypermeasure is a hyperdownbeat. A phrase that does not fill its hypermeasures ends in a short one, an internal expansion makes the last one longer, an external expansion adds one of its own and a contraction shortens the last. Score is the library's confidence in the reading. The form adapter does not report elided cadences, so no phrase is read as elided.

| Phrase | Measures | Length | Hyperdownbeats | Hypermeasures | Cadence arrives | Score |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 7 | 4 | 1, 5 | 1 regular, 1 contracted | m. 5 | 1 |
| 2 | m. 8 | 4 | 8 | 1 short | m. 8 | 0.5 |
| 3 | mm. 9 to 19 | 4 | 9, 13 | 1 regular, 1 expanded | m. 16 | 1 |
| 4 | mm. 20 to 26 | 4 | 20, 24 | 1 regular, 1 contracted | m. 24 | 1 |
| 5 | mm. 27 to 29 | 4 | 27 | 1 short | m. 28 | 0.5 |
| 6 | mm. 30 to 36 | 2 | 30, 32, 34, 36 | 3 regular, 1 short | none | 0.86 |
| 7 | mm. 37 to 38 | 2 | 37 | 1 regular | none | 1 |

### Tuplets and borrowed divisions

No tuplets were found.

## Texture

The library's texture adapter read every measure of the piece from the voices the kernel separated in each track or part, classified it, and merged consecutive measures of the same texture into passages. Category is the main texture (homorhythm and melody and accompaniment are homophony). Voices is the most sounding at once, Synchrony the share of attack times where the voices attack together, Pattern the score of a repeated cell (0 to 1: evidence for an accompaniment figure, not a name for it). Confidence is the texture's margin from its thresholds times the voice separation's.

| Measures | Texture | Category | Voices | Synchrony | Pattern | Confidence |
| --- | --- | --- | --- | --- | --- | --- |
| mm. 1 to 3 | melody and accompaniment | homophony | 2 | 0.22 | 0.19 | 0.61 |
| m. 4 | polyphony | polyphony | 2 | 0.25 | 0.62 | 0.47 |
| mm. 5 to 26 | melody and accompaniment | homophony | 3 | 0.28 | 0.34 | 0.63 |
| m. 27 | polyphony | polyphony | 2 | 0.25 | 0.62 | 0.59 |
| mm. 28 to 29 | melody and accompaniment | homophony | 2 | 0.2 | 0 | 0.69 |
| m. 30 | homorhythm | homophony | 3 | 1 | 0.67 | 0.87 |
| mm. 31 to 38 | melody and accompaniment | homophony | 2 | 0.26 | 0.44 | 0.56 |

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

3 voices over 47 chord segments, 35 motions between consecutive segments: 8 contrary, 13 similar, 8 parallel, 4 oblique, 2 static.

| Voices | Contrary | Similar | Parallel | Oblique | Static |
| --- | --- | --- | --- | --- | --- |
| 0 and 1 | 4 | 7 | 5 | 1 | 0 |
| 0 and 2 | 3 | 3 | 0 | 1 | 1 |
| 1 and 2 | 1 | 3 | 3 | 2 | 1 |

0 parallel fifths, 0 parallel octaves, 1 direct fifths or octaves in the outer voices, 0 unresolved sevenths, 1 unresolved leading tone.

| Location | Fault | Voices |
| --- | --- | --- |
| m. 3, beat 1 to m. 5, beat 1 | direct-perfects | 0 and 2 |
| m. 23, beat 1 to m. 25, beat 1 | leading-tone-resolves | 1 |

3 chords with three or more voices sounding: the root is doubled in 1, the third in 0, the fifth in 1, the seventh in 0, no member doubled in 1. Doubling is counted, not judged. Spacing is judged only when four voices sound together; no chord has them.

## Markings

The tempo and dynamics markings the score notates, read with the library's standard-markings rules. A tempo holds until the next one, a dynamic until the next in its part. Group and the loudness comparison are the library's; a text it does not recognise has none. Wedges are not read, and articulations are not imported.

The score has no tempo marking.

The score has no dynamics marking.
