# Analysis Report: bach-114.musicxml

## Summary

- **File:** bach-114.musicxml, 430 notes.
- **Input:** MusicXML score, 1 part merged into one texture. Meter: notated. Key signature: inferred. Pitch spelling: notated.
- **Key:** G major, the key with the most time across 4 key regions.
- **Chords:** 48 chord events; 46 matched a chord (96%), 2 did not.
- **Most frequent chords:** I (17), vi (9), IV (7).

## Meter

- **Time signature:** 4/4 (notated)
- **Measures:** 24 measures.
- **Pickup:** None.

## Key regions

Positions are measure and beat. Uncertainty is the library's, from 0 (clearly this key) to 1 (another key fits as well or better). The home key is the key with the most time across its regions.

- **Home key:** G major.
- **Opening key:** G major.
- **Closing key:** D major.

| Position | Key | Uncertainty | Set by |
| --- | --- | --- | --- |
| m. 1, beat 1 to m. 13, beat 1 | G major | 0.63 | profile |
| m. 13, beat 1 to m. 17, beat 1 | D major | 0.88 | profile |
| m. 17, beat 1 to m. 20, beat 1 | G major | 0.76 | profile |
| m. 20, beat 1 to m. 25, beat 1 | D major | 0.91 | profile |

## Tonicizations

1 brief excursion of another key that returns to the key before. Span is the stretch in the target key. The library tells a tonicization from a modulation by how long the new key lasts, whether it returns, and whether a cadence arrives in it; without a cadence it is a tonicization however long.

| Span | In | Target |
| --- | --- | --- |
| m. 13, beat 1 to m. 17, beat 1 | G major | D major |

## Chords

Positions are measure and beat. Numeral is the Roman numeral in the local key region, which is named where it changes. Function is the library's harmonic function of the chord: an applied chord is a dominant toward its target degree, and a predominant is strong (IV, ii) or weak (iii, vi). Six-four is the library's type for a chord in second inversion. Duration is in beats. Confidence is the library's confidence in the chord. The runner-up is shown only when its score is within 0.10 of the best chord's.

| Position | Chord | Numeral | Function | Six-four | Root | Quality | Bass | Inversion | Beats | Confidence | Runner-up |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 4 | I | I (in G major) | tonic | - | G | major | G | root position | 3 | 0.61 | - |
| m. 1, beat 4 to m. 2, beat 1 | IV | IV | predominant (strong) | - | C | major | C | root position | 1 | 0.45 | - |
| m. 2, beat 1 to m. 2, beat 3 | vi | vi | predominant (weak) | - | E | minor | B | second | 2 | 0.14 | - |
| m. 2, beat 3 to m. 2, beat 4 | ii | ii | predominant (strong) | - | A | minor | A | root position | 1 | 0.23 | - |
| m. 2, beat 4 to m. 3, beat 1 | I | I | tonic | - | G | major | G | root position | 1 | 0.70 | - |
| m. 3, beat 1 to m. 3, beat 4 | I | I | tonic | - | G | major | G | root position | 3 | 0.43 | - |
| m. 3, beat 4 to m. 4, beat 1 | minor7 | iii6/5 | predominant (weak) | - | B | minor7 | D | first | 1 | 0.16 | - |
| m. 4, beat 1 to m. 4, beat 4 | I | I6 | tonic | - | G | major | G | root position | 3 | 0.30 | - |
| m. 4, beat 4 to m. 5, beat 1 | IV | IV | predominant (strong) | - | C | major | C | root position | 1 | 0.45 | - |
| m. 5, beat 1 to m. 5, beat 3 | vi | vi | predominant (weak) | - | E | minor | G | first | 2 | 0.09 | major7 (C major7, score 0.78) |
| m. 5, beat 3 to m. 6, beat 1 | vii° | vii°6 | dominant | - | F# | diminished | F# | root position | 2 | 0.12 | ii (A minor, score 0.67) |
| m. 6, beat 1 to m. 7, beat 1 | I | I6 | tonic | - | G | major | G | root position | 4 | 0.44 | - |
| m. 7, beat 1 to m. 7, beat 3 | I | I | tonic | - | G | major | G | root position | 2 | 0.67 | - |
| m. 7, beat 3 to m. 8, beat 1 | IV | IV6/4 | predominant (strong) | - | C | major | C | root position | 2 | 0.36 | - |
| m. 8, beat 1 to m. 8, beat 3 | vi | vi | predominant (weak) | - | E | minor | B | second | 2 | 0.14 | - |
| m. 8, beat 3 to m. 8, beat 4 | ii | ii | predominant (strong) | - | A | minor | A | root position | 1 | 0.23 | - |
| m. 8, beat 4 to m. 9, beat 1 | I | I | tonic | - | G | major | G | root position | 1 | 0.70 | - |
| m. 9, beat 1 to m. 9, beat 4 | I | I | tonic | - | G | major | G | root position | 3 | 0.43 | - |
| m. 9, beat 4 to m. 10, beat 1 | minor7 | iii6/5 | predominant (weak) | - | B | minor7 | D | first | 1 | 0.16 | - |
| m. 10, beat 1 to m. 10, beat 4 | I | I6 | tonic | - | G | major | G | root position | 3 | 0.30 | - |
| m. 10, beat 4 to m. 11, beat 1 | IV | IV | predominant (strong) | - | C | major | C | root position | 1 | 0.45 | - |
| m. 11, beat 1 to m. 11, beat 3 | vi | vi | predominant (weak) | - | E | minor | G | first | 2 | 0.09 | major7 (C major7, score 0.78) |
| m. 11, beat 3 to m. 12, beat 1 | vii° | vii°6 | dominant | - | F# | diminished | F# | root position | 2 | 0.12 | ii (A minor, score 0.67) |
| m. 12, beat 1 to m. 13, beat 1 | I | I6 | tonic | - | G | major | G | root position | 4 | 0.44 | - |
| m. 13, beat 1 to m. 13, beat 3 | IV | IV (in D major) | predominant (strong) | - | G | major | F# | - | 2 | 0.45 | - |
| m. 13, beat 3 to m. 13, beat 4 | I | I6 | tonic | - | D | major | F# | first | 1 | 0.41 | - |
| m. 13, beat 4 to m. 14, beat 1 | ii | ii | predominant (strong) | - | E | minor | E | root position | 1 | 0.71 | - |
| m. 14, beat 1 to m. 15, beat 1 | V | V6/4 | dominant | - | A | major | A | root position | 4 | 0.16 | - |
| m. 15, beat 1 to m. 16, beat 1 | I | I | tonic | - | D | major | D | root position | 4 | 0.78 | - |
| m. 16, beat 1 to m. 16, beat 2 | vi | vi | predominant (weak) | - | B | minor | B | root position | 1 | 0.00 | IV (G major, score 0.98) |
| m. 16, beat 2 to m. 16, beat 4 | ii | ii6/4 | predominant (strong) | neighbor | E | minor | B | second | 2 | 0.42 | - |
| m. 16, beat 4 to m. 16, beat 4.5 | vi | vi | predominant (weak) | - | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 16, beat 4.5 to m. 17, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 17, beat 1 to m. 17, beat 2 | I | I (in G major) | tonic | - | G | major | G | root position | 1 | 0.72 | - |
| m. 17, beat 2 to m. 18, beat 1 | V | V | dominant | - | D | major | D | root position | 3 | 0.09 | iii (B minor, score 0.72) |
| m. 18, beat 1 to m. 19, beat 1 | I | I | tonic | - | G | major | G | root position | 4 | 0.55 | - |
| m. 19, beat 1 to m. 19, beat 3 | I | I | tonic | - | G | major | F# | - | 2 | 0.45 | - |
| m. 19, beat 3 to m. 19, beat 4 | V | V6 | dominant | - | D | major | F# | first | 1 | 0.41 | - |
| m. 19, beat 4 to m. 20, beat 1 | vi | vi | predominant (weak) | - | E | minor | E | root position | 1 | 0.71 | - |
| m. 20, beat 1 to m. 21, beat 1 | V | V6/4 (in D major) | dominant | - | A | major | A | root position | 4 | 0.16 | - |
| m. 21, beat 1 to m. 22, beat 1 | I | I | tonic | - | D | major | D | root position | 4 | 0.78 | - |
| m. 22, beat 1 to m. 22, beat 2 | vi | vi | predominant (weak) | - | B | minor | B | root position | 1 | 0.00 | IV (G major, score 0.98) |
| m. 22, beat 2 to m. 22, beat 4 | ii | ii6/4 | predominant (strong) | neighbor | E | minor | B | second | 2 | 0.42 | - |
| m. 22, beat 4 to m. 22, beat 4.5 | vi | vi | predominant (weak) | - | B | minor | B | root position | 0.5 | 1.00 | - |
| m. 22, beat 4.5 to m. 23, beat 1 | no match | unlabelled: the library identified no chord | - | - | - | - | - | - | - | - | - |
| m. 23, beat 1 to m. 23, beat 2 | IV | IV | predominant (strong) | - | G | major | G | root position | 1 | 0.72 | - |
| m. 23, beat 2 to m. 24, beat 1 | I | I | tonic | - | D | major | D | root position | 3 | 0.09 | vi (B minor, score 0.72) |
| m. 24, beat 1 to m. 25, beat 1 | IV | IV | predominant (strong) | - | G | major | G | root position | 4 | 0.74 | - |
## Outer voices

The highest (soprano) and lowest (bass) sounding note of each chord's span, with its scale degree in that chord's local key. A flat or sharp sign marks a chromatic alteration. A degree is "-" where the span lies in no key region. A rest means the library found no note for that line in the span, a bridged one a short gap inside the line, and a register guard a note the library dropped rather than adopt an accompaniment note. Degrees carry the key region's uncertainty in the analysis document.

| Position | Key | Soprano | Degree | Bass | Degree |
| --- | --- | --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 4 | G major | D5 | 5 | G3 | 1 |
| m. 1, beat 4 to m. 2, beat 1 | G major | E5 | 6 | C4 | 4 |
| m. 2, beat 1 to m. 2, beat 3 | G major | F#5 | 7 | rest (bridged) | - |
| m. 2, beat 3 to m. 2, beat 4 | G major | D5 | 5 | A3 | 2 |
| m. 2, beat 4 to m. 3, beat 1 | G major | B4 | 3 | A4 | 2 |
| m. 3, beat 1 to m. 3, beat 4 | G major | C5 | 4 | G3 | 1 |
| m. 3, beat 4 to m. 4, beat 1 | G major | A4 | 2 | D3 | 5 |
| m. 4, beat 1 to m. 4, beat 4 | G major | D5 | 5 | B3 | 3 |
| m. 4, beat 4 to m. 5, beat 1 | G major | E5 | 6 | C4 | 4 |
| m. 5, beat 1 to m. 5, beat 3 | G major | F#5 | 7 | rest (bridged) | - |
| m. 5, beat 3 to m. 6, beat 1 | G major | D5 | 5 | A3 | 2 |
| m. 6, beat 1 to m. 7, beat 1 | G major | C5 | 4 | rest (bridged) | - |
| m. 7, beat 1 to m. 7, beat 3 | G major | D5 | 5 | G3 | 1 |
| m. 7, beat 3 to m. 8, beat 1 | G major | G4 | 1 | G4 | 1 |
| m. 8, beat 1 to m. 8, beat 3 | G major | F#5 | 7 | rest (bridged) | - |
| m. 8, beat 3 to m. 8, beat 4 | G major | D5 | 5 | A3 | 2 |
| m. 8, beat 4 to m. 9, beat 1 | G major | B4 | 3 | A4 | 2 |
| m. 9, beat 1 to m. 9, beat 4 | G major | C5 | 4 | G3 | 1 |
| m. 9, beat 4 to m. 10, beat 1 | G major | A4 | 2 | D3 | 5 |
| m. 10, beat 1 to m. 10, beat 4 | G major | D5 | 5 | B3 | 3 |
| m. 10, beat 4 to m. 11, beat 1 | G major | E5 | 6 | C4 | 4 |
| m. 11, beat 1 to m. 11, beat 3 | G major | F#5 | 7 | rest (bridged) | - |
| m. 11, beat 3 to m. 12, beat 1 | G major | D5 | 5 | A3 | 2 |
| m. 12, beat 1 to m. 13, beat 1 | G major | C5 | 4 | rest (bridged) | - |
| m. 13, beat 1 to m. 13, beat 3 | D major | B5 | 6 | G3 | 4 |
| m. 13, beat 3 to m. 13, beat 4 | D major | F#5 | 3 | F#3 | 3 |
| m. 13, beat 4 to m. 14, beat 1 | D major | G5 | 4 | E3 | 2 |
| m. 14, beat 1 to m. 15, beat 1 | D major | G5 | 4 | E3 | 2 |
| m. 15, beat 1 to m. 16, beat 1 | D major | F#5 | 3 | D4 | 1 |
| m. 16, beat 1 to m. 16, beat 2 | D major | D5 | 1 | B3 | 6 |
| m. 16, beat 2 to m. 16, beat 4 | D major | G4 | 4 | B3 | 6 |
| m. 16, beat 4 to m. 16, beat 4.5 | D major | D5 | 1 | B3 | 6 |
| m. 16, beat 4.5 to m. 17, beat 1 | D major | C5 | b7 | A3 | 5 |
| m. 17, beat 1 to m. 17, beat 2 | G major | B4 | 3 | G3 | 1 |
| m. 17, beat 2 to m. 18, beat 1 | G major | G4 | 1 | D4 | 5 |
| m. 18, beat 1 to m. 19, beat 1 | G major | C5 | 4 | G3 | 1 |
| m. 19, beat 1 to m. 19, beat 3 | G major | B5 | 3 | G3 | 1 |
| m. 19, beat 3 to m. 19, beat 4 | G major | F#5 | 7 | F#3 | 7 |
| m. 19, beat 4 to m. 20, beat 1 | G major | G5 | 1 | E3 | 6 |
| m. 20, beat 1 to m. 21, beat 1 | D major | G5 | 4 | E3 | 2 |
| m. 21, beat 1 to m. 22, beat 1 | D major | F#5 | 3 | D4 | 1 |
| m. 22, beat 1 to m. 22, beat 2 | D major | D5 | 1 | B3 | 6 |
| m. 22, beat 2 to m. 22, beat 4 | D major | G4 | 4 | B3 | 6 |
| m. 22, beat 4 to m. 22, beat 4.5 | D major | D5 | 1 | B3 | 6 |
| m. 22, beat 4.5 to m. 23, beat 1 | D major | C5 | b7 | A3 | 5 |
| m. 23, beat 1 to m. 23, beat 2 | D major | B4 | 6 | G3 | 4 |
| m. 23, beat 2 to m. 24, beat 1 | D major | G4 | 4 | D4 | 1 |
| m. 24, beat 1 to m. 25, beat 1 | D major | C5 | b7 | G3 | 4 |

## Chord vocabulary

Each distinct Roman numeral of the chords the library numbered, for the whole piece and for each key region, sorted by count, then total length, then first appearance. Count is the number of chord rows with that numeral (every row in the Chords table is one). Runs counts a stretch of adjacent identical numerals in one key region once. Beats is the total length, Share its fraction of the scope's numbered chord length, and First where it first appears. Applied chords are listed on their own. Chords without a numeral are not counted.

### Overall

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 12 | 9 | 31 | 32.6% | m. 1, beat 1 |
| vi | 9 | 9 | 12 | 12.6% | m. 2, beat 1 |
| IV | 6 | 6 | 10 | 10.5% | m. 1, beat 4 |
| I6 | 5 | 5 | 15 | 15.8% | m. 4, beat 1 |
| ii | 3 | 3 | 3 | 3.2% | m. 2, beat 3 |
| V6/4 | 2 | 2 | 8 | 8.4% | m. 14, beat 1 |
| vii°6 | 2 | 2 | 4 | 4.2% | m. 5, beat 3 |
| ii6/4 | 2 | 2 | 4 | 4.2% | m. 16, beat 2 |
| iii6/5 | 2 | 2 | 2 | 2.1% | m. 3, beat 4 |
| V | 1 | 1 | 3 | 3.2% | m. 17, beat 2 |
| IV6/4 | 1 | 1 | 2 | 2.1% | m. 7, beat 3 |
| V6 | 1 | 1 | 1 | 1.1% | m. 19, beat 3 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 17 | 14 | 46 | 48.4% | m. 1, beat 1 |
| vi | 9 | 9 | 12 | 12.6% | m. 2, beat 1 |
| IV | 7 | 7 | 12 | 12.6% | m. 1, beat 4 |
| ii | 5 | 5 | 7 | 7.4% | m. 2, beat 3 |
| V | 4 | 4 | 12 | 12.6% | m. 14, beat 1 |
| vii° | 2 | 2 | 4 | 4.2% | m. 5, beat 3 |
| iii7 | 2 | 2 | 2 | 2.1% | m. 3, beat 4 |

### G major (region 0)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 6 | 4 | 13 | 27.1% | m. 1, beat 1 |
| I6 | 4 | 4 | 14 | 29.2% | m. 4, beat 1 |
| vi | 4 | 4 | 8 | 16.7% | m. 2, beat 1 |
| IV | 3 | 3 | 3 | 6.2% | m. 1, beat 4 |
| vii°6 | 2 | 2 | 4 | 8.3% | m. 5, beat 3 |
| ii | 2 | 2 | 2 | 4.2% | m. 2, beat 3 |
| iii6/5 | 2 | 2 | 2 | 4.2% | m. 3, beat 4 |
| IV6/4 | 1 | 1 | 2 | 4.2% | m. 7, beat 3 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 10 | 8 | 27 | 56.2% | m. 1, beat 1 |
| vi | 4 | 4 | 8 | 16.7% | m. 2, beat 1 |
| IV | 4 | 4 | 5 | 10.4% | m. 1, beat 4 |
| vii° | 2 | 2 | 4 | 8.3% | m. 5, beat 3 |
| ii | 2 | 2 | 2 | 4.2% | m. 2, beat 3 |
| iii7 | 2 | 2 | 2 | 4.2% | m. 3, beat 4 |

### D major (region 1)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| vi | 2 | 2 | 1.5 | 9.7% | m. 16, beat 1 |
| V6/4 | 1 | 1 | 4 | 25.8% | m. 14, beat 1 |
| I | 1 | 1 | 4 | 25.8% | m. 15, beat 1 |
| IV | 1 | 1 | 2 | 12.9% | m. 13, beat 1 |
| ii6/4 | 1 | 1 | 2 | 12.9% | m. 16, beat 2 |
| I6 | 1 | 1 | 1 | 6.5% | m. 13, beat 3 |
| ii | 1 | 1 | 1 | 6.5% | m. 13, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 2 | 2 | 5 | 32.3% | m. 13, beat 3 |
| ii | 2 | 2 | 3 | 19.4% | m. 13, beat 4 |
| vi | 2 | 2 | 1.5 | 9.7% | m. 16, beat 1 |
| V | 1 | 1 | 4 | 25.8% | m. 14, beat 1 |
| IV | 1 | 1 | 2 | 12.9% | m. 13, beat 1 |

### G major (region 2)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 2 | 7 | 58.3% | m. 17, beat 1 |
| V | 1 | 1 | 3 | 25.0% | m. 17, beat 2 |
| V6 | 1 | 1 | 1 | 8.3% | m. 19, beat 3 |
| vi | 1 | 1 | 1 | 8.3% | m. 19, beat 4 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 3 | 2 | 7 | 58.3% | m. 17, beat 1 |
| V | 2 | 2 | 4 | 33.3% | m. 17, beat 2 |
| vi | 1 | 1 | 1 | 8.3% | m. 19, beat 4 |

### D major (region 3)

**Diatonic chords**

| Numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 2 | 2 | 7 | 35.9% | m. 21, beat 1 |
| IV | 2 | 2 | 5 | 25.6% | m. 23, beat 1 |
| vi | 2 | 2 | 1.5 | 7.7% | m. 22, beat 1 |
| V6/4 | 1 | 1 | 4 | 20.5% | m. 20, beat 1 |
| ii6/4 | 1 | 1 | 2 | 10.3% | m. 22, beat 2 |

By base numeral (inversions together):

| Base numeral | Count | Runs | Beats | Share | First |
| --- | --- | --- | --- | --- | --- |
| I | 2 | 2 | 7 | 35.9% | m. 21, beat 1 |
| IV | 2 | 2 | 5 | 25.6% | m. 23, beat 1 |
| vi | 2 | 2 | 1.5 | 7.7% | m. 22, beat 1 |
| V | 1 | 1 | 4 | 20.5% | m. 20, beat 1 |
| ii | 1 | 1 | 2 | 10.3% | m. 22, beat 2 |

## Non-chord tones

132 notes sound against a chord without being one of its tones; 26 the library left unclassified. Type is the library's name for the figure. Note is the imported note's index.

| Position | Note | Type | Against |
| --- | --- | --- | --- |
| m. 1, beat 1 to m. 1, beat 4 | 5 | unclassified | I |
| m. 1, beat 1 to m. 1, beat 4 | 7 | neighbor | I |
| m. 1, beat 1 to m. 1, beat 4 | 8 | passing | I |
| m. 1, beat 4 to m. 2, beat 1 | 16 | neighbor | IV |
| m. 1, beat 4 to m. 2, beat 1 | 18 | escape-tone | IV |
| m. 2, beat 1 to m. 2, beat 3 | 20 | passing | vi |
| m. 2, beat 1 to m. 2, beat 3 | 25 | appoggiatura | vi |
| m. 2, beat 3 to m. 2, beat 4 | 29 | neighbor | ii |
| m. 2, beat 4 to m. 3, beat 1 | 32 | neighbor | I |
| m. 3, beat 1 to m. 3, beat 4 | 35 | neighbor | I |
| m. 3, beat 1 to m. 3, beat 4 | 37 | passing | I |
| m. 3, beat 1 to m. 3, beat 4 | 39 | neighbor | I |
| m. 3, beat 1 to m. 3, beat 4 | 43 | passing | I |
| m. 3, beat 4 to m. 4, beat 1 | 51 | appoggiatura | minor7 |
| m. 4, beat 1 to m. 4, beat 4 | 57 | escape-tone | I |
| m. 4, beat 1 to m. 4, beat 4 | 59 | unclassified | I |
| m. 4, beat 1 to m. 4, beat 4 | 60 | passing | I |
| m. 4, beat 4 to m. 5, beat 1 | 70 | neighbor | IV |
| m. 4, beat 4 to m. 5, beat 1 | 72 | escape-tone | IV |
| m. 5, beat 1 to m. 5, beat 3 | 74 | passing | vi |
| m. 5, beat 1 to m. 5, beat 3 | 77 | unclassified | vi |
| m. 5, beat 1 to m. 5, beat 3 | 80 | passing | vi |
| m. 5, beat 1 to m. 5, beat 3 | 83 | neighbor | vi |
| m. 5, beat 3 to m. 6, beat 1 | 87 | neighbor | vii° |
| m. 5, beat 3 to m. 6, beat 1 | 89 | appoggiatura | vii° |
| m. 5, beat 3 to m. 6, beat 1 | 93 | unclassified | vii° |
| m. 5, beat 3 to m. 6, beat 1 | 92 | passing | vii° |
| m. 6, beat 1 to m. 7, beat 1 | 94 | neighbor | I |
| m. 6, beat 1 to m. 7, beat 1 | 96 | passing | I |
| m. 6, beat 1 to m. 7, beat 1 | 100 | passing | I |
| m. 6, beat 1 to m. 7, beat 1 | 99 | passing | I |
| m. 6, beat 1 to m. 7, beat 1 | 103 | passing | I |
| m. 6, beat 1 to m. 7, beat 1 | 106 | neighbor | I |
| m. 7, beat 1 to m. 7, beat 3 | 115 | unclassified | I |
| m. 7, beat 1 to m. 7, beat 3 | 117 | neighbor | I |
| m. 7, beat 1 to m. 7, beat 3 | 118 | passing | I |
| m. 7, beat 3 to m. 8, beat 1 | 126 | neighbor | IV |
| m. 7, beat 3 to m. 8, beat 1 | 128 | escape-tone | IV |
| m. 8, beat 1 to m. 8, beat 3 | 130 | passing | vi |
| m. 8, beat 1 to m. 8, beat 3 | 135 | appoggiatura | vi |
| m. 8, beat 3 to m. 8, beat 4 | 139 | neighbor | ii |
| m. 8, beat 4 to m. 9, beat 1 | 142 | neighbor | I |
| m. 9, beat 1 to m. 9, beat 4 | 145 | neighbor | I |
| m. 9, beat 1 to m. 9, beat 4 | 147 | passing | I |
| m. 9, beat 1 to m. 9, beat 4 | 149 | neighbor | I |
| m. 9, beat 1 to m. 9, beat 4 | 153 | passing | I |
| m. 9, beat 4 to m. 10, beat 1 | 161 | appoggiatura | minor7 |
| m. 10, beat 1 to m. 10, beat 4 | 167 | escape-tone | I |
| m. 10, beat 1 to m. 10, beat 4 | 169 | unclassified | I |
| m. 10, beat 1 to m. 10, beat 4 | 170 | passing | I |
| m. 10, beat 4 to m. 11, beat 1 | 180 | neighbor | IV |
| m. 10, beat 4 to m. 11, beat 1 | 182 | escape-tone | IV |
| m. 11, beat 1 to m. 11, beat 3 | 184 | passing | vi |
| m. 11, beat 1 to m. 11, beat 3 | 187 | unclassified | vi |
| m. 11, beat 1 to m. 11, beat 3 | 190 | passing | vi |
| m. 11, beat 1 to m. 11, beat 3 | 193 | neighbor | vi |
| m. 11, beat 3 to m. 12, beat 1 | 197 | neighbor | vii° |
| m. 11, beat 3 to m. 12, beat 1 | 199 | appoggiatura | vii° |
| m. 11, beat 3 to m. 12, beat 1 | 203 | unclassified | vii° |
| m. 11, beat 3 to m. 12, beat 1 | 202 | passing | vii° |
| m. 12, beat 1 to m. 13, beat 1 | 204 | neighbor | I |
| m. 12, beat 1 to m. 13, beat 1 | 206 | passing | I |
| m. 12, beat 1 to m. 13, beat 1 | 210 | passing | I |
| m. 12, beat 1 to m. 13, beat 1 | 209 | passing | I |
| m. 12, beat 1 to m. 13, beat 1 | 213 | passing | I |
| m. 12, beat 1 to m. 13, beat 1 | 216 | neighbor | I |
| m. 13, beat 1 to m. 13, beat 3 | 223 | passing | IV |
| m. 13, beat 1 to m. 13, beat 3 | 227 | anticipation | IV |
| m. 13, beat 1 to m. 13, beat 3 | 226 | escape-tone | IV |
| m. 13, beat 3 to m. 13, beat 4 | 229 | passing | I |
| m. 13, beat 4 to m. 14, beat 1 | 236 | passing | ii |
| m. 14, beat 1 to m. 15, beat 1 | 237 | escape-tone | V |
| m. 14, beat 1 to m. 15, beat 1 | 239 | unclassified | V |
| m. 14, beat 1 to m. 15, beat 1 | 242 | neighbor | V |
| m. 14, beat 1 to m. 15, beat 1 | 248 | passing | V |
| m. 14, beat 1 to m. 15, beat 1 | 250 | passing | V |
| m. 14, beat 1 to m. 15, beat 1 | 252 | passing | V |
| m. 14, beat 1 to m. 15, beat 1 | 254 | unclassified | V |
| m. 14, beat 1 to m. 15, beat 1 | 253 | neighbor | V |
| m. 15, beat 1 to m. 16, beat 1 | 258 | neighbor | I |
| m. 15, beat 1 to m. 16, beat 1 | 257 | neighbor | I |
| m. 15, beat 1 to m. 16, beat 1 | 263 | unclassified | I |
| m. 15, beat 1 to m. 16, beat 1 | 268 | unclassified | I |
| m. 16, beat 1 to m. 16, beat 2 | 271 | unclassified | vi |
| m. 16, beat 2 to m. 16, beat 4 | 277 | unclassified | ii |
| m. 16, beat 2 to m. 16, beat 4 | 280 | neighbor | ii |
| m. 16, beat 2 to m. 16, beat 4 | 282 | unclassified | ii |
| m. 17, beat 1 to m. 17, beat 2 | 289 | passing | I |
| m. 17, beat 2 to m. 18, beat 1 | 293 | passing | V |
| m. 17, beat 2 to m. 18, beat 1 | 297 | passing | V |
| m. 17, beat 2 to m. 18, beat 1 | 299 | passing | V |
| m. 17, beat 2 to m. 18, beat 1 | 302 | passing | V |
| m. 17, beat 2 to m. 18, beat 1 | 304 | escape-tone | V |
| m. 17, beat 2 to m. 18, beat 1 | 303 | escape-tone | V |
| m. 18, beat 1 to m. 19, beat 1 | 306 | appoggiatura | I |
| m. 18, beat 1 to m. 19, beat 1 | 308 | neighbor | I |
| m. 18, beat 1 to m. 19, beat 1 | 311 | appoggiatura | I |
| m. 18, beat 1 to m. 19, beat 1 | 310 | unclassified | I |
| m. 18, beat 1 to m. 19, beat 1 | 317 | neighbor | I |
| m. 19, beat 1 to m. 19, beat 3 | 328 | passing | I |
| m. 19, beat 1 to m. 19, beat 3 | 332 | anticipation | I |
| m. 19, beat 1 to m. 19, beat 3 | 331 | escape-tone | I |
| m. 19, beat 3 to m. 19, beat 4 | 334 | passing | V |
| m. 19, beat 4 to m. 20, beat 1 | 341 | unclassified | vi |
| m. 20, beat 1 to m. 21, beat 1 | 342 | unclassified | V |
| m. 20, beat 1 to m. 21, beat 1 | 344 | unclassified | V |
| m. 20, beat 1 to m. 21, beat 1 | 347 | neighbor | V |
| m. 20, beat 1 to m. 21, beat 1 | 353 | passing | V |
| m. 20, beat 1 to m. 21, beat 1 | 355 | passing | V |
| m. 20, beat 1 to m. 21, beat 1 | 357 | passing | V |
| m. 20, beat 1 to m. 21, beat 1 | 359 | unclassified | V |
| m. 20, beat 1 to m. 21, beat 1 | 358 | neighbor | V |
| m. 21, beat 1 to m. 22, beat 1 | 363 | neighbor | I |
| m. 21, beat 1 to m. 22, beat 1 | 362 | neighbor | I |
| m. 21, beat 1 to m. 22, beat 1 | 368 | unclassified | I |
| m. 21, beat 1 to m. 22, beat 1 | 373 | unclassified | I |
| m. 22, beat 1 to m. 22, beat 2 | 376 | unclassified | vi |
| m. 22, beat 2 to m. 22, beat 4 | 382 | unclassified | ii |
| m. 22, beat 2 to m. 22, beat 4 | 385 | neighbor | ii |
| m. 22, beat 2 to m. 22, beat 4 | 387 | unclassified | ii |
| m. 23, beat 1 to m. 23, beat 2 | 394 | passing | IV |
| m. 23, beat 2 to m. 24, beat 1 | 398 | passing | I |
| m. 23, beat 2 to m. 24, beat 1 | 402 | passing | I |
| m. 23, beat 2 to m. 24, beat 1 | 404 | passing | I |
| m. 23, beat 2 to m. 24, beat 1 | 407 | passing | I |
| m. 23, beat 2 to m. 24, beat 1 | 409 | escape-tone | I |
| m. 23, beat 2 to m. 24, beat 1 | 408 | escape-tone | I |
| m. 24, beat 1 to m. 25, beat 1 | 411 | appoggiatura | IV |
| m. 24, beat 1 to m. 25, beat 1 | 413 | neighbor | IV |
| m. 24, beat 1 to m. 25, beat 1 | 416 | appoggiatura | IV |
| m. 24, beat 1 to m. 25, beat 1 | 415 | unclassified | IV |
| m. 24, beat 1 to m. 25, beat 1 | 422 | neighbor | IV |

## Cadences

The library's standard-harmony analysis found 1 cadence from the chords, the meter and the soprano. Position is where the arrival begins. Key is the key region of the arrival. Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added before a section start.

| Position | Type | Key | Soprano | Beat strength | Boundary |
| --- | --- | --- | --- | --- | --- |
| m. 18, beat 1 | imperfect-authentic | G major | 4 | strong | no |

### Cadence plan

1. m. 18, beat 1: imperfect-authentic in G major

**Counts:** imperfect-authentic 1.

## Phrases

The library's standard-form analysis found 2 phrases from the soprano, the meter and the cadences. Length counts the measures of the phrase proper; Ends is what closed it: its cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in the piece; Expansion is a lengthening or shortening of the phrase's ending, or an extension after its cadence, with the measures it adds or removes.

| Phrase | Measures | Length | Cadence | Ends | Role | Expansion |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 12 | 12 | none | discontinuity | beginning | - |
| 2 | mm. 13 to 19 | 7 | imperfect-authentic (m. 18, beat 1) | cadence | ending | contraction -1 (mm. 17 to 19); external codetta +5 (mm. 20 to 24) |

- **Phrase 1:** ideas basic-idea (mm. 1 to 2), contrasting-idea (mm. 3 to 4), contrasting-idea (mm. 5 to 6), contrasting-idea (mm. 7 to 8), contrasting-idea (mm. 9 to 10), contrasting-idea (mm. 11 to 12); subphrases mm. 1 to 4, mm. 5 to 12
- **Phrase 2:** ideas basic-idea (mm. 13 to 14), contrasting-idea (mm. 15 to 16), contrasting-idea (mm. 17 to 19); subphrases mm. 13 to 16, mm. 17 to 19; phrase model D (m. 17, beat 2), Te (m. 18, beat 1), Te (m. 19, beat 1)

## Themes

The library's standard-form analysis found 1 theme from the phrases, their ideas and the cadences. Form is the library's name for the pairing of the two parts; Commonality is how common that pairing is; Traits are what the library finds in the continuation.

| Theme | Form | Commonality | Measures | Length | Parts | Traits |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | hybrid | slightly-less-common | mm. 13 to 19 | 7 | compound-basic-idea (mm. 13 to 16), continuation (mm. 17 to 19, imperfect-authentic (m. 18, beat 1)) | - |

## Form plan

The kernel's form structure detection grouped the phrases into 1 section, closing a section at each phrase that ends on a perfect or imperfect authentic cadence; the phrases after the last such cadence form the last section. Sections are lettered by first appearance, and a section whose opening theme returns an earlier section's material takes that letter with a prime. Key is the key the section opens in.

| Section | Measures | Key | Phrases | Closing cadence | Returns |
| --- | --- | --- | --- | --- | --- |
| A | mm. 1 to 24 | G major | 1 to 2 | imperfect-authentic (m. 18, beat 1) | - |

### Outline

- form (mm. 1 to 24)
  - section A (mm. 1 to 24, G major)
    - phrase 1 (mm. 1 to 12)
      - subphrase 1 (mm. 1 to 4)
        - idea basic-idea (mm. 1 to 2)
        - idea contrasting-idea (mm. 3 to 4)
      - subphrase 2 (mm. 5 to 12)
        - idea contrasting-idea (mm. 5 to 6)
        - idea contrasting-idea (mm. 7 to 8)
        - idea contrasting-idea (mm. 9 to 10)
        - idea contrasting-idea (mm. 11 to 12)
    - phrase 2 (mm. 13 to 19)
      - subphrase 1 (mm. 13 to 16)
        - idea basic-idea (mm. 13 to 14)
        - idea contrasting-idea (mm. 15 to 16)
      - subphrase 2 (mm. 17 to 19)
        - idea contrasting-idea (mm. 17 to 19)

## Motives

The library's motive analysis found 36 motive families in the soprano, the highest sounding note at each attack. A family is a short figure that recurs; Notes is the length of the figure, Pattern whether its occurrences match by scale steps or by semitones, Measures the measures the occurrences start in, Phrases the phrases they lie in (an occurrence across a phrase boundary lies in none), and Transformations how many occurrences are of each kind the library names against the first occurrence (exact, real-transposed, tonal-transposed, inverted, retrograde, augmented, diminished, fragmented, varied).

| Family | Notes | Degrees | Pattern | Occurrences | Measures | Phrases | Transformations |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 8 | 5 1 2 3 4 5 1 1 | scale steps | 4 | 1, 4, 7, 10 | 1 | exact 4 |
| 2 | 8 | 3 4 5 1 1 6 4 5 | scale steps | 4 | 1, 4, 7, 10 | 1 | exact 4 |
| 3 | 8 | 1 1 6 4 5 6 7 1 | scale steps | 4 | 1, 4, 7, 10 | 1 | exact 4 |
| 4 | 8 | 6 4 5 6 7 1 1 1 | scale steps | 4 | 1, 4, 7, 10 | 1 | exact 4 |
| 5 | 8 | 6 7 1 1 1 4 3 4 | scale steps | 4 | 2, 5, 8, 11 | 1 | exact 4 |
| 6 | 8 | 1 1 4 3 4 5 4 3 | scale steps | 4 | 2, 5, 8, 11 | 1 | exact 4 |
| 7 | 8 | 4 5 4 3 2 3 4 3 | scale steps | 4 | 2, 5, 8, 11 | 1 | exact 4 |
| 8 | 8 | 3 2 3 4 3 2 1 7 | scale steps | 4 | 2, 5, 8, 11 | 1 | exact 2, fragmented 2 |
| 9 | 8 | 4 3 2 1 7 1 2 3 | scale steps | 2 | 3, 9 | 1 | exact 2 |
| 10 | 8 | 7 1 2 3 1 3 2 5 | scale steps | 2 | 3, 9 | 1 | exact 2 |
| 11 | 8 | 3 1 3 2 5 1 2 3 | scale steps | 2 | 3, 9 | 1 | exact 2 |
| 12 | 8 | 2 5 1 2 3 4 5 1 | scale steps | 2 | 3, 9 | 1 | exact 2 |
| 13 | 8 | 3 2 3 4 3 2 1 2 | scale steps | 2 | 5, 11 | 1 | exact 2 |
| 14 | 8 | 4 3 2 1 2 3 2 1 | scale steps | 2 | 6, 12 | 1 | exact 2 |
| 15 | 7 | 2 3 2 1 7 1 1 | scale steps | 6 | 6, 2, 5, 8, 11, 12 | 1 | exact 2, varied 4 |
| 16 | 8 | 6 4 5 6 4 5 1 2 | scale steps | 2 | 13, 19 | 2 | exact 2 |
| 17 | 8 | 6 4 5 1 2 3 1 4 | scale steps | 2 | 13, 19 | 2 | exact 2 |
| 18 | 8 | 1 2 3 1 4 2 3 4 | scale steps | 2 | 13, 19 | 2 | exact 2 |
| 19 | 8 | 4 2 3 4 1 7 6 7 | scale steps | 2 | 13, 19 | 2 | exact 2 |
| 20 | 8 | 4 1 7 6 7 5 5 6 | scale steps | 2 | 14, 20 | 2 | exact 2 |
| 21 | 8 | 6 7 5 5 6 7 1 2 | scale steps | 2 | 14, 20 | 2 | exact 2 |
| 22 | 8 | 5 6 7 1 2 3 4 3 | scale steps | 2 | 14, 20 | 2 | exact 2 |
| 23 | 7 | 5 6 7 1 2 3 4 | scale steps | 4 | 14, 17, 20, 23 | 2 | exact 2, real-transposed 2 |
| 24 | 8 | 2 3 4 3 2 3 5 7 | scale steps | 2 | 14, 20 | 2 | exact 2 |
| 25 | 8 | 3 2 3 5 7 1 1 b7 | scale steps | 2 | 15, 21 | 2 | exact 2 |
| 26 | 8 | 3 5 7 1 1 b7 1 4 | scale steps | 2 | 15, 21 | 2 | exact 2 |
| 27 | 8 | 7 1 1 b7 1 4 3 4 | scale steps | 2 | 15, 21 | 2 | exact 2 |
| 28 | 8 | 1 b7 1 4 3 4 2 4 | scale steps | 2 | 15, 21 | 2 | exact 2 |
| 29 | 8 | 1 4 3 4 2 4 3 4 | scale steps | 2 | 16, 22 | 2 | exact 2 |
| 30 | 8 | 4 2 4 3 4 1 b7 6 | scale steps | 2 | 16, 22 | 2 | exact 2 |
| 31 | 8 | 4 3 4 1 b7 6 5 4 | scale steps | 2 | 16, 22 | 2 | exact 2 |
| 32 | 8 | 1 b7 6 5 4 3 4 5 | scale steps | 2 | 16, 22 | 2 | exact 2 |
| 33 | 8 | 3 2 1 7 1 2 5 6 | scale steps | 2 | 17, 23 | 2 | exact 2 |
| 34 | 8 | 7 1 2 5 6 7 1 2 | scale steps | 2 | 17, 23 | 2 | exact 2 |
| 35 | 4 | 4 3 4 3 | scale steps | 2 | 18, 24 | 2 | exact 2 |
| 36 | 6 | 3 5 1 7 1 1 | scale steps | 2 | 18, 24 | 2 | exact 2 |

## Melody

The library's melody adapter read the soprano of each phrase as a line of notes: the soprano notes that start in the phrase. Range is the lowest and highest note and the semitones between them; Climax is the first note at the highest pitch, with how many notes sound it when it is not one; Contour counts the intervals between consecutive notes that go up, go down and repeat, and how many of them are steps and leaps.

| Phrase | Measures | Notes | Range | Climax | Up | Down | Same | Steps | Leaps |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 12 | 71 | F#4 to G5 (13) | G5, m. 2, beat 1.5, 4 notes | 33 | 26 | 11 | 34 | 25 |
| 2 | mm. 13 to 24 | 78 | F#4 to B5 (17) | B5, m. 13, beat 1, 4 notes | 30 | 34 | 10 | 38 | 26 |

### Leaps

A leap is an interval of a third or larger. The library's rule asks a leap of a fourth or larger to be followed by a step in the opposite direction: such a leap is Recovered when one follows, Unrecovered when a step the same way, a repeated note or another leap does, and Not judged when nothing follows it; a third asks nothing (Not required). Consecutive counts the pairs of leaps in a row the library's rule finds fault with.

| Phrase | Leaps | Recovered | Unrecovered | Not required | Not judged | Consecutive |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 25 | 7 | 10 | 8 | 0 | 0 |
| 2 | 26 | 2 | 14 | 10 | 0 | 2 |

### Non-chord tones

Each melody note is set against the chord of the segment it starts in, as the library's chord identification reads it: a Chord tone is one of the chord's tones, a Non-chord tone is one it left over and named, and Unclassified notes are in a segment with no chord or were left unnamed. By type counts the non-chord tones under the library's names.

| Phrase | Notes | Chord tones | Non-chord tones | Unclassified | By type |
| --- | --- | --- | --- | --- | --- |
| 1 | 71 | 41 | 22 | 8 | passing 4, neighbor 8, appoggiatura 4, escape-tone 6 |
| 2 | 78 | 42 | 18 | 18 | passing 2, neighbor 10, anticipation 2, escape-tone 4 |

### Tendencies

The library's melodic tendencies are soft preferences a well-formed line usually shows: Pitch proximity (steps outnumber leaps, small leaps outnumber large ones), Step declination (descending steps outnumber ascending ones), Step inertia (the line continues in its direction at least as often as it reverses), Melodic regression (extreme notes turn back toward the middle) and Melodic arch (the line rises to a climax in its middle half and ends below it); a phrase either shows a tendency or does not. The seventh degree column counts the notes the library labels the seventh degree that another note follows, judged by its leading tone rule: a leading tone goes up to the tonic by step, and a seventh a whole step below the tonic does not. Followed is the count that did, Broken the count that did not, with where each broken one stands.

| Phrase | Pitch proximity | Step declination | Step inertia | Melodic regression | Melodic arch | Seventh degree followed | Seventh degree broken |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | does not hold | does not hold | does not hold | does not hold | does not hold | 4 | 2 (m. 3, beat 2; m. 9, beat 2) |
| 2 | does not hold | holds | does not hold | does not hold | does not hold | 8 | 4 (m. 15, beat 1; m. 15, beat 2; m. 21, beat 1; m. 21, beat 2) |

### Development

Each family's occurrences in time order, as the measure they start in and the kind the library names against the family's prototype, which is the original.

- **Family 1:** m1 original > m4 exact > m7 exact > m10 exact
- **Family 2:** m1 original > m4 exact > m7 exact > m10 exact
- **Family 3:** m1 original > m4 exact > m7 exact > m10 exact
- **Family 4:** m1 original > m4 exact > m7 exact > m10 exact
- **Family 5:** m2 original > m5 exact > m8 exact > m11 exact
- **Family 6:** m2 original > m5 exact > m8 exact > m11 exact
- **Family 7:** m2 original > m5 exact > m8 exact > m11 exact
- **Family 8:** m2 original > m5 fragmented > m8 exact > m11 fragmented
- **Family 9:** m3 original > m9 exact
- **Family 10:** m3 original > m9 exact
- **Family 11:** m3 original > m9 exact
- **Family 12:** m3 original > m9 exact
- **Family 13:** m5 original > m11 exact
- **Family 14:** m6 original > m12 exact
- **Family 15:** m2 varied > m5 varied > m6 original > m8 varied > m11 varied > m12 exact
- **Family 16:** m13 original > m19 exact
- **Family 17:** m13 original > m19 exact
- **Family 18:** m13 original > m19 exact
- **Family 19:** m13 original > m19 exact
- **Family 20:** m14 original > m20 exact
- **Family 21:** m14 original > m20 exact
- **Family 22:** m14 original > m20 exact
- **Family 23:** m14 original > m17 real-transposed > m20 exact > m23 real-transposed
- **Family 24:** m14 original > m20 exact
- **Family 25:** m15 original > m21 exact
- **Family 26:** m15 original > m21 exact
- **Family 27:** m15 original > m21 exact
- **Family 28:** m15 original > m21 exact
- **Family 29:** m16 original > m22 exact
- **Family 30:** m16 original > m22 exact
- **Family 31:** m16 original > m22 exact
- **Family 32:** m16 original > m22 exact
- **Family 33:** m17 original > m23 exact
- **Family 34:** m17 original > m23 exact
- **Family 35:** m18 original > m24 exact
- **Family 36:** m18 original > m24 exact

## Progression patterns

13 recurring progressions of two to six chords, counted over the library's Roman numerals: a held chord counts once, and a progression does not cross an unlabelled chord or a change of key. Ranked by count, then length, then first occurrence; the top 10 are shown. Measures are where each occurrence begins and ends.

| Rank | Progression | Count | Measures |
| --- | --- | --- | --- |
| 1 | I > IV | 5 | m. 1, m. 4, mm. 6 to 7, m. 10, mm. 23 to 24 |
| 2 | I > IV > vi | 4 | mm. 1 to 2, mm. 4 to 5, mm. 6 to 8, mm. 10 to 11 |
| 3 | vi > ii | 4 | m. 2, m. 8, m. 16, m. 22 |
| 4 | V > I | 3 | mm. 14 to 15, mm. 17 to 19, mm. 20 to 21 |
| 5 | I > IV > vi > ii > I > iii7 | 2 | mm. 1 to 3, mm. 6 to 9 |
| 6 | IV > vi > ii > I > iii7 > I | 2 | mm. 1 to 4, mm. 7 to 10 |
| 7 | vi > ii > I > iii7 > I > IV | 2 | mm. 2 to 4, mm. 8 to 10 |
| 8 | ii > I > iii7 > I > IV > vi | 2 | mm. 2 to 5, mm. 8 to 11 |
| 9 | I > iii7 > I > IV > vi > vii° | 2 | mm. 2 to 5, mm. 8 to 11 |
| 10 | iii7 > I > IV > vi > vii° > I | 2 | mm. 3 to 7, mm. 9 to 12 |

## Harmonic rhythm

How often the harmony changes. A change is a chord whose base Roman numeral or key differs from the chord before it, counted in the measure it starts in. Changes lists the measures of each row in order; Mean is changes per measure over the row.

| Measures | Changes | Mean |
| --- | --- | --- |
| mm. 1 to 4 | 2, 3, 1, 2 | 2 |
| mm. 5 to 8 | 2, 1, 1, 3 | 1.75 |
| mm. 9 to 12 | 1, 2, 2, 1 | 1.5 |
| mm. 13 to 16 | 3, 1, 1, 4 | 2.25 |
| mm. 17 to 20 | 2, 1, 2, 1 | 1.5 |
| mm. 21 to 24 | 1, 4, 2, 1 | 2 |

**Mean:** 1.83 changes per measure over 24 measures.

### Approach to cadences

Changes per measure over the two measures before each cadence's arrival (Approach), against the two measures before those (Before). Faster is yes when the approach changes harmony more often.

| Cadence | Arrival | Before | Approach | Faster |
| --- | --- | --- | --- | --- |
| 1 | m. 18 | 1 | 3 | yes |

## Rhythm and meter

**Meter fit:** 4/4, scale 1, no pickup (fit 0.88); no better reading found

### Density

The library's density profile of each phrase, for all its notes and for each line that has a note in it (the soprano and bass of the outer voices, and Other for the rest). A note is in a phrase when its onset is; a chord is one attack. Attacks per beat is read measure by measure over the beats the phrase spans in it, as a mean and the largest; Mean gap is the mean distance between attacks in whole notes; On the beat is the share of attacks exactly on a beat. Durations lists the notes' lengths in whole notes, shortest first, with how many notes have each. The patterns that recur are the motive families' rhythms (see Motives).

| Phrase | Measures | Line | Notes | Attacks | Per beat | Max per beat | Mean gap | On the beat | Durations |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | mm. 1 to 12 | all | 220 | 158 | 3.29 | 3.75 | 0.08 | 29.1% | 1/64 (8), 1/32 (8), 1/16 (96), 1/8 (81), 1/4 (27) |
| 1 | mm. 1 to 12 | soprano | 71 | 71 | 1.48 | 1.5 | 0.17 | 40.8% | 1/32 (4), 1/16 (30), 1/8 (33), 1/4 (4) |
| 1 | mm. 1 to 12 | bass | 60 | 60 | 1.25 | 1.5 | 0.2 | 50.0% | 1/16 (6), 1/8 (37), 1/4 (17) |
| 1 | mm. 1 to 12 | other | 89 | 83 | 1.73 | 2.25 | 0.14 | 22.9% | 1/64 (8), 1/32 (4), 1/16 (60), 1/8 (11), 1/4 (6) |
| 2 | mm. 13 to 24 | all | 210 | 138 | 2.88 | 3.25 | 0.09 | 34.8% | 1/64 (2), 1/32 (4), 1/16 (74), 5/64 (2), 1/8 (108), 1/4 (14), 3/8 (6) |
| 2 | mm. 13 to 24 | soprano | 78 | 78 | 1.62 | 2 | 0.15 | 33.3% | 1/32 (2), 1/16 (30), 1/8 (44), 1/4 (2) |
| 2 | mm. 13 to 24 | bass | 74 | 74 | 1.54 | 2 | 0.16 | 51.4% | 1/8 (60), 1/4 (8), 3/8 (6) |
| 2 | mm. 13 to 24 | other | 58 | 56 | 1.17 | 1.75 | 0.21 | 32.1% | 1/64 (2), 1/32 (2), 1/16 (44), 5/64 (2), 1/8 (4), 1/4 (4) |

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
| 1 | mm. 1 to 12 | 4 | 1, 5, 9 | 3 regular | none | 1 |
| 2 | mm. 13 to 24 | 4 | 13, 17, 21, 25 | 3 regular, 1 short | m. 18 | 0.46 |

### Tuplets and borrowed divisions

No tuplets were found.

## Texture

The library's texture adapter read every measure of the piece from the voices the kernel separated in each track or part, classified it, and merged consecutive measures of the same texture into passages. Category is the main texture (homorhythm and melody and accompaniment are homophony). Voices is the most sounding at once, Synchrony the share of attack times where the voices attack together, Pattern the score of a repeated cell (0 to 1: evidence for an accompaniment figure, not a name for it). Confidence is the texture's margin from its thresholds times the voice separation's.

| Measures | Texture | Category | Voices | Synchrony | Pattern | Confidence |
| --- | --- | --- | --- | --- | --- | --- |
| mm. 1 to 2 | melody and accompaniment | homophony | 4 | 0.27 | 0.12 | 0.51 |
| mm. 3 to 4 | polyphony | polyphony | 2 | 0.41 | 0.13 | 0.24 |
| mm. 5 to 8 | melody and accompaniment | homophony | 4 | 0.35 | 0.17 | 0.4 |
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

## Voice leading

The kernel separated the notes of the whole piece into voices (voice 0 is the top) and the library's counterpoint adapter classified how each pair of voices moves from one chord segment to the next. Parallel motion keeps the interval type, similar motion changes it; a voice that rests in either segment gives no motion. Faults are the library's: parallel fifths and octaves between any pair of voices, direct fifths and octaves in the outer pair, and a seventh or leading tone left unresolved when its chord changes (voice 0 is the top).

4 voices over 48 chord segments, 12 motions between consecutive segments: 1 contrary, 0 similar, 7 parallel, 2 oblique, 2 static.

| Voices | Contrary | Similar | Parallel | Oblique | Static |
| --- | --- | --- | --- | --- | --- |
| 0 and 1 | 0 | 0 | 2 | 0 | 0 |
| 0 and 2 | 1 | 0 | 5 | 0 | 1 |
| 1 and 2 | 0 | 0 | 0 | 2 | 1 |

0 parallel fifths, 0 parallel octaves, 0 direct fifths or octaves in the outer voices, 2 unresolved sevenths, 0 unresolved leading tones.

| Location | Fault | Voices |
| --- | --- | --- |
| m. 3, beat 4 to m. 4, beat 4 | seventh-resolves-down | 0 |
| m. 9, beat 4 to m. 10, beat 4 | seventh-resolves-down | 0 |

2 chords with three or more voices sounding: the root is doubled in 0, the third in 0, the fifth in 2, the seventh in 0, no member doubled in 0. Doubling is counted, not judged. Spacing is judged by the library for the 2 chords with four voices all sounding: 0 too wide.

## Schemas and sequences

The library's galant-schemas adapter matched its schema prototypes against the outer voices: the soprano and bass degree of each chord in its local key, with the chord's Roman numeral and the strength of its beat. Each match lists its stages by the measure they start in and the soprano and bass degrees found there (b is lowered, # raised; a stage with no soprano reads "-" and matches on the bass alone). Frame is the key the stages are read in, as a degree of the home key (1 is the home key). Confidence is the library's: the share of stages whose voices match, times the metric fit, times one minus the mean uncertainty of the chords.

No galant schema was found.

### Sequences

The library's scale-harmonization adapter found a model of one or more chords repeated, transposed by a constant diatonic interval, at least three times. Kind is the library's name for it when its rules for that pattern hold over whole units (rule-backed); the cycle of fifths has no rule in the library yet, so it is found from the Roman numerals alone. Units is how often the model sounds and, in brackets, how many chords it has; Steps the bass's diatonic steps from one unit to the next (up is positive). Stages read as in the schemas, in the home key.

| Measures | Kind | Rule-backed | Direction | Units (chords each) | Steps | Stages (soprano/bass) | Confidence |
| --- | --- | --- | --- | --- | --- | --- | --- |
| mm. 16 to 17 | parallel-chords | yes | descending | 3 (1) | -1 | m. 16 5/3, m. 16 4/2, m. 17 3/1 | 0.36 |
| mm. 22 to 23 | parallel-chords | yes | descending | 3 (1) | -1 | m. 22 5/3, m. 22 4/2, m. 23 3/1 | 0.32 |

## Markings

The tempo and dynamics markings the score notates, read with the library's standard-markings rules. A tempo holds until the next one, a dynamic until the next in its part. Group and the loudness comparison are the library's; a text it does not recognise has none. Wedges are not read, and articulations are not imported.

The score has no tempo marking.

The score has no dynamics marking.
