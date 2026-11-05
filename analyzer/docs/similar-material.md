# Similar material

The similar material search over a corpus, by motive or by progression. Back to the [docs index](README.md).

```sh
ursatz-analyzer --similar <corpus output dir> (--motive <file> | --progression "<numerals>") [--min <n/d>] [--max <n>] [--format text|json]
```

Library: `include/ursatz_analyzer/similar_material.h`. Output schema: `schema/similar-material.v1.schema.json`.

The search finds passages of an analyzed corpus that look like a motive or a progression. It reads only the analysis
documents of a corpus run (see [corpus.md](corpus.md)), never the scores. A match says how much two passages agree on
the dimensions scored; it does not say the passage is the motive.

## Queries

- `--motive <file>` is a MIDI or MusicXML file. The file is analyzed and the notes of its soprano (the highest line,
  a note held across several chord segments counting once) are the motive. It needs at least two notes.
- `--progression "<numerals>"` is Roman numerals separated by spaces or `-`: `"I IV V7 I"`, `"ii-V7-I"`. Each token must
  be a numeral of the library's grammar; an inversion figure is dropped, so `V6/5` reads `V7`, and the label is the
  grammar's canonical spelling (`viio`, `viiø7`, whatever mark the document wrote). It needs at least two.

Give exactly one of the two. `--min <n/d>` is the lowest score reported (default `1/2`, from `0/1` to `1/1`);
`--max <n>` is how many results are returned (default 10).

## Scoring

Every window of the query's length is scored with Ursatz's `similarity_score`, an exact rational in lowest terms.
The combined score is the weighted mean of the dimension scores, so the weights below sum to the divisor.

| Query | Dimension | Weight | What is compared |
| --- | --- | --- | --- |
| motive | interval | 2 | the semitone steps between soprano notes, so a transposed motive scores the same |
| motive | contour | 1 | up, down or same for each step |
| motive | rhythm | 1 | the durations as ratios of consecutive notes, so an augmented rhythm scores the same |
| progression | interval | 2 | the semitone steps between chord roots, -6 to +5 |
| progression | rhythm | 1 | the durations of the chords as ratios of consecutive chords |

A progression has no contour. Its query carries no durations, so its chords are taken to be of equal length: a window
of chords of equal length scores 1 on rhythm. The roots of the query come from its numerals: the degree in the major or
the natural minor scale (by the mode of the window's key region), with a leading `b` or `#` moving it a semitone, and
an applied chord (`V7/V`) taken as the numeral's degree of the target's own scale above the target's root: the major
scale for an uppercase target and the harmonic minor scale for a lowercase one (`viio7/ii` reads the leading tone of ii,
a raised seventh). An applied chord with more than one target (`V/V/V`) is not a numeral. The roots of a
window come from the documents' chord roots.

A window of a progression lies inside one run of labelled chords of one key region. Chords that follow each other with
the same numeral, key region and root are one chord: the joined chord sequence (`chord_sequence.h`), which the Composer
reads too, so the indices of a match and the public ones are the same by construction. The number of chords of the window that carry exactly the
query's numeral at their place is reported as `numeral_matches` and is not part of the score.

## Results

A result has the piece, its start and end (measure and beat; a beat of 2.5 is half way through beat 2), the combined
score and the score of each dimension, `numeral_matches`, and the soprano notes of the passage (pitch, and onset and
duration in whole notes from the start of the passage), so a later step can grow from them.

In each piece, of any windows that overlap only the best is kept (the earliest on a tie). Results are sorted by
score, highest first, then by piece path, then by start. Nothing reaching the minimum is not an error: the text output
is `no match in <n> pieces` and the JSON has an empty `matches` array.

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | The search ran, with or without a match. |
| 1 | Usage: a missing or conflicting flag, a bad `--min`, `--max` or `--format`, a progression that is not two or more numerals, a motive of fewer than two notes. |
| 2 | Unreadable: the corpus output folder, its index or a document, or the motive file. |

## Text output

One line per match: the score as a decimal and a fraction, the piece, the start and end, each dimension's score and,
for a progression, `numerals <matched>/<length>`.

```
0.8333 (5/6) Mozart 545.mid m.1 b.1 - m.2 b.2.5 interval 1/1 contour 1/1 rhythm 1/3
```

## Limits

- Only the soprano and the chord roots are searched, not inner voices or bass lines.
- It does not use Ursatz's `corpus_search_melodic`, which reads persisted claims; the Analyzer does not persist.
- A window is as long as the query. A passage with an extra or a missing note in the middle does not match.
