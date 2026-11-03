# random-motive command

```sh
ursatz-composer random-motive --mode <major|minor> --meter <n/m> [--key <key>] [--form <full|rhythm|contour>] [--role <melodic|accompaniment>] [--style <file>] [--style-without <category>]... [--count <n>] [--seed <n>] [--json]
```

Makes random motives from a mode and a meter. Every motive keeps the hard melodic rules of standard counterpoint, the ones motive invention keeps, and the same options and seed always give the same motives.

The length, pickup, number of notes, rhythm and range are not options. They are drawn inside defaults named at the top of `src/motive/motive_random.c`, which are meant to be tuned there:

| Default | Value |
| --- | --- |
| Length | 1 or 2 bars, after a pickup of half a beat or one beat in one motive of four |
| Notes | 2 to 12 |
| Rhythm | the meter's beat unit, its half and its double; a note stays in its bar, and a note of a beat or more starts on a beat |
| Frame | chord tones of I on every beat of the meter and on the first and last note: a walk on the tonic triad, a third most often, now and then a repeated pitch or a move of a fifth or sixth, which is followed by a third back; all within a twelfth, C3 to C6 |
| Opening | degree 1, 3 or 5 |
| Between | each note off the beat, between two frame notes, is drawn as none (another chord tone of I) or one of passing, neighbor, appoggiatura and escape tone, with equal weight; when the type drawn cannot be made between its two frame notes, the note is a chord tone |

The motive is heard in I throughout, so suspension, retardation and anticipation (they need a chord change), double neighbor and changing tone (they need two non-chord notes side by side) and pedal are never drawn. A motive is kept only when its tone labels (see `analysis` below) read as it was drawn: the beats are chord tones, and each decorated note has the type drawn and resolves.

The command is a wrapper of the [composer API](api.md): it reads its arguments into a request, calls
`composer_api_random_motive` and prints the response JSON, or the text lines made from it. The request members have the
options' names and values (`mode`, `meter`, `key`, `form`, `role`, `style`, `style_without`, `count`, `seed`).

**Random mode and meter.** With `--mode random` or `--meter random` the seed draws the value, by the rule the GUI used
before the Composer drew: `m = ((seed + 1) * 2654435761) mod 2^32`; the mode is major when `(m >> 8)` is even and minor when
it is odd; the meter is `4/4`, `3/4`, `2/4` or `6/8` for `(m >> 16) mod 4` of 0, 1, 2 and 3. The JSON output then has
`"drawn": {"mode", "meter"}` (only what was drawn) and the text output a `Drawn:` line.

## Options

| Option | Meaning |
| --- | --- |
| `--mode <mode>` | `major`, `minor` or `random`. Required. `random` is drawn from the seed (below). Church modes are not planned. |
| `--meter <n/m>` | Beats over a beat unit, as in `4/4` or `6/8`, or `random`. Required. `random` is drawn from the seed (below). |
| `--key <key>` | For example `"D minor"`. Its mode must be `--mode`. Without it the motive is made in C major or A minor and carries that key, so it can be stored. |
| `--form <form>` | `full` (pitches and rhythm, the default), `rhythm` (the rhythm of a full motive) or `contour` (its contour, in quarter notes with no pickup). These are read as the [motive command](cli-motive.md) reads them. |
| `--role <role>` | `melodic` (the default) or `accompaniment`: the role every motive of the run is given. |
| `--style <file>` | A style model file. For each motive, 8 candidates are made and the one closest to the style is kept (ties broken by the seed), by the style's closeness on the candidate's melody labels: all of them for a full motive, the durations for a rhythm, the contour for a contour. |
| `--style-without <category>` | Read the style without this category (repeatable, at most 8; needs `--style`; an unknown name is exit 1). |
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

Each motive is the document of `motive --json` (`motive` and `analysis`, with `notes_text`, `degrees_text` and the rest of the analysis), without `rendered`. The `analysis` carries `base_harmony` (`I`, `given` false: it is proposed from the frame), `tones` (the role of each note, and for a non-chord tone its type and the note it resolves to) and `warnings`, as `motive --json` does.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage or an invalid argument, or a style file that cannot be read. |
| 2 | No motive could be made. |
