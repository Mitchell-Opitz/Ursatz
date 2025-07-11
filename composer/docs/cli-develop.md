# develop command

```sh
ursatz-composer develop <motive-input> --op <name> [--seed <n>] [--suggest <n>] [--meter <n/m>] [--key <key>] [options]
```

Makes motives related to the one in `<motive-input>` and prints them, best first, each as a note list
with its recognizability score. The best one can also be written, after the source, as MIDI or
MusicXML. The command runs the motivic development operators (`<ursatz_composer/motive_develop.h>`)
on a small plan: the source is Given, and an open motive node after it asks for the development.

The input is loaded as for the [motive command](cli-motive.md): a MIDI file, a MusicXML file, a note
list or a motive JSON file, with `--meter`, `--key` and `--format` meaning the same.

## Operations

`--op` is required.

| Operation | Makes | Options |
| --- | --- | --- |
| `restate` | The exact repetition and transpositions of it. | `--by <n>`, `--degree <n>`, `--tonal` |
| `invert` | The motive mirrored about a pitch. | `--axis <pitch>`, `--tonal` |
| `retrograde` | The notes in reverse order, each with its duration. | none |
| `interval` | One interval, or every one, widened or narrowed. | `--interval <n>`, `--amount <n>`, `--keep-contour` |
| `fragment` | A run of the notes. | `--position <where>`, `--length <n>`, `--min <n>` |
| `liquidate` | The motive with its weakest notes dropped. | `--drop <n>`, `--keep-rhythm`, `--min <n>` |
| `augment` | Every onset and duration longer by a ratio. | `--ratio <n:m>` |
| `diminish` | Every onset and duration shorter by a ratio. | `--ratio <n:m>` |

An option another operation owns is an error (`--tonal does not apply to retrograde`). What an option
leaves open, the operation chooses from the seed: the same input, options and seed always give the
same output, and other seeds give other choices. `--suggest <n>` (1 to 16, 5 by default) is how many
candidates are printed.

| Option | Meaning |
| --- | --- |
| `--by <n>` | Transpose by n semitones; with `--tonal`, by n steps of the key's scale. |
| `--degree <n>` | Transpose in the scale so the first note is on degree n (1 to 7), by the nearest way, up to three steps either way. Needs a key. |
| `--tonal` | Move in steps of the key's scale instead of semitones. Needs a key. |
| `--axis <pitch>` | The pitch mirrored about, `D4` or a MIDI number. The first note by default. |
| `--interval <n>` | The 1-based interval to change. Every interval by default. |
| `--amount <n>` | Semitones to widen the interval by; negative narrows it. |
| `--keep-contour` | Refuse a result in which an interval changes direction. |
| `--position <where>` | `leading`, `trailing` or `middle`. |
| `--length <n>` | Notes in the fragment. |
| `--drop <n>` | Notes to drop. |
| `--keep-rhythm` | The notes left keep their onsets; a dropped note is a rest. Without it they follow one another from 0. |
| `--min <n>` | The fewest notes a fragment or a liquidation may have. 2 by default. |
| `--ratio <n:m>` | The factor, each part 1 to 16. Augmentation needs it above 1 and diminution below. 2:1 and 1:2 by default. |

### How each is made

The real transformations call Ursatz's transforms: transposition, inversion, retrograde, intervallic
manipulation, augmentation and diminution (durations), fragmentation and contraction. Ursatz has no
diatonic transformation, so `--tonal` is worked out here from the key's scale, the same steps the
motive analysis counts. A pitch outside the scale keeps its place, raised, in its step.

- A development that is the same as its source is not offered, except the exact repetition.
- A development that would put a pitch outside MIDI 0 to 127 is not offered.
- Fragments start at 0, so they open on their first note.
- Liquidation drops the weakest note first: a note off the beat, then a beat that is not the downbeat,
  then a downbeat; of two equals, the later one.
- Interval change leaves a repeated note alone. The notes after a changed interval move with it, so
  later intervals keep their size.
- Augmentation and diminution must come out writable in the meter: every duration a plain or dotted
  value (`w h q e s t`, up to two dots), every onset a multiple of a thirty-second, and, when the
  source is a whole number of beats long, the result too. Otherwise that ratio is not offered; a
  motive is never bent to fit.
- A motive of one note can be repeated, moved, mirrored about another pitch, augmented and
  diminished. It has no intervals to change, nothing to fragment or drop, and its retrograde is itself.

## Score

Each candidate carries a recognizability score from 0 to 1, an exact fraction. It is a weighted mean
of four parts, each from 0 to 1, with weights 3, 2, 2 and 1:

| Part | Weight | Is |
| --- | --- | --- |
| contour | 3 | The longest run of neighboring intervals whose directions the two share, over the intervals of the longer. |
| intervals | 2 | The intervals the two share by interval class (octaves folded, a size and its complement the same), over the intervals of the longer. |
| rhythm | 2 | The longest run of neighboring ratios of inter-onset intervals the two share, over the ratios of the longer. A rhythm made twice as fast or slow still scores 1. |
| length | 1 | The notes of the shorter over the notes of the longer. |

The candidates are ranked by it, best first; equal scores keep the order they were proposed in. A
mirrored motive shares no contour, so it scores lower than a transposed one. The weights are
configurable in the library (`MotiveDevelopContext`), not on the command line.

## Output

```
Source: D4:q F4:e A4:e D5:h
Operation: restate (seed 0)
1. transpose +3 semitones  score 1/1 (100%)
   F4:q G#4:e C5:e F5:h
```

Pitches are written with sharps, since the spelling is not known. A length no one value writes is
two tied notes (`C4:q~ C4:s`); a rest is split into rests.

`--midi <file>` and `--musicxml <file>` write the source followed by the best candidate, with the
meter and, when there is one, the key. Only the best candidate is written. `--tempo <bpm>` writes a tempo in both
files and `--plain-midi` leaves the track names and section markers out of the MIDI ([export.md](export.md)).

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage or an invalid argument. |
| 2 | The input cannot be loaded, or nothing can be developed from it as asked. |
| 3 | An output file could not be written. |

Errors are printed on stderr as `Error: ...`.
