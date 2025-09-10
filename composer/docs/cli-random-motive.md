# random-motive command

```sh
ursatz-composer random-motive --mode <major|minor> --meter <n/m> [--key <key>] [--form <full|rhythm|contour>] [--role <melodic|accompaniment>] [--style <file>] [--count <n>] [--seed <n>] [--json]
```

Makes random motives from a mode and a meter. Every motive keeps the hard melodic rules of standard counterpoint, the ones motive invention keeps, and the same options and seed always give the same motives.

The length, pickup, number of notes, rhythm and range are not options. They are drawn inside defaults named at the top of `src/motive/motive_random.c`, which are meant to be tuned there:

| Default | Value |
| --- | --- |
| Length | 1 or 2 bars, after a pickup of half a beat or one beat in one motive of four |
| Notes | 2 to 12 |
| Rhythm | the meter's beat unit, its half and its double; a note stays in its bar, and a note of a beat or more starts on a beat |
| Pitch | steps preferred, now and then a repeated pitch or a leap up to a sixth; a leap over a fourth is followed by a step the other way; all within a twelfth, C3 to C6 |
| Opening | degree 1, 3 or 5 |

## Options

| Option | Meaning |
| --- | --- |
| `--mode <mode>` | `major` or `minor`. Required. Church modes are not planned. |
| `--meter <n/m>` | Beats over a beat unit, as in `4/4` or `6/8`. Required. |
| `--key <key>` | For example `"D minor"`. Its mode must be `--mode`. Without it the motive is made in C major or A minor and carries that key, so it can be stored. |
| `--form <form>` | `full` (pitches and rhythm, the default), `rhythm` (the rhythm of a full motive) or `contour` (its contour, in quarter notes with no pickup). These are read as the [motive command](cli-motive.md) reads them. |
| `--role <role>` | `melodic` (the default) or `accompaniment`: the role every motive of the run is given. |
| `--style <file>` | A style model file. For each motive, 8 candidates are made and the one closest to the style is kept (ties broken by the seed), by the style's closeness on the candidate's melody labels: all of them for a full motive, the durations for a rhythm, the contour for a contour. |
| `--count <n>` | How many motives, 1 to 16, 1 by default. The motives of one run are all different. |
| `--seed <n>` | A whole number from 0. By default from the clock. The seed is printed. |
| `--json` | Print as JSON (below). |

## Output

Text prints the seed, then one block per motive: its note list (a rhythm or contour for those forms) and, for a full motive, its degrees.

```
Seed: 7

Motive 1: C5:e G5:q F5:h D5:q A5:e G5:e F5:q
  degrees: 3:e 7:q 6:h 4:q 1':e 7:e 6:q
```

JSON:

```json
{
  "seed": 7,
  "motives": [
    {"motive": {}, "analysis": {}}
  ]
}
```

Each motive is the document of `motive --json` (`motive` and `analysis`, with `notes_text`, `degrees_text` and the rest of the analysis), without `rendered`.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage or an invalid argument, or a style file that cannot be read. |
| 2 | No motive could be made. |
