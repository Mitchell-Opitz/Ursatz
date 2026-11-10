# random-motive command

The `random-motive` command of the Composer command line, with its options and output. Back to the [docs index](README.md).

```sh
ursatz-composer random-motive --mode <major|minor> --meter <n/m> [--key <key>] [--form <full|rhythm|contour>] [--strategy <segments|drawn>] [--role <melodic|accompaniment>] [--style <file>] [--style-without <category>]... [--count <n>] [--seed <n>] [--json]
```

Makes random motives from a mode and a meter. Every motive keeps the hard melodic rules of standard counterpoint, the ones motive invention keeps, and the same options and seed always give the same motives.

There are two strategies, `--strategy segments` (the default) and `--strategy drawn`. The length, pickup, number of notes, rhythm and range are not options for either. They are drawn inside defaults named at the top of the strategy's file, which are meant to be tuned there: `src/motive/motive_random_segments.c` for `segments` and `src/motive/motive_random.c` for `drawn`. The segments strategy's draws (bars, pickup, shapes, chord plans, features, leap, rung moves, decorations, note lengths) are settings, in `MotiveRandomPreferences` (`motive_random_preferences_default` has the defaults); see [preferences.md](preferences.md#the-motif-group). The steps both share (the chord-tone frame, the non-chord tone decorations, the form and the style) are in `src/motive/motive_random_common.c`.

## The segments strategy

A motive is made of segments, as a theme starts: a first cell, its repeat on the next chord, a contrasting cell. The steps:

1. **Bars and pickup.** 1 or 2 bars with equal weight, after a single pickup note (half a beat or a beat) in one motive of four. In a meter with an odd beat count (3/4, 3/8, 9/8) the motive is 2 bars and every cell is one bar. A meter of 2 beats is 2 bars too: a half-bar cell of one beat cannot hold a feature, so the three-cell shape is not drawn there either.
2. **Shape.** One bar, cells of half a bar: `a a'` 3, `a b` 2. Two bars, cells of one bar: `a a'` 3, `a b` 2; and `a a' b` 2 (two half-bar cells then one bar; even beat counts only). The pickup note belongs to segment `a`, and is not part of `a`'s rhythm when `a'` repeats it.
3. **Harmony plan**, one chord per segment, set as the motive's own harmony (a chord on each segment's first beat). Two segments: `I V` 3, `I IV` 1, `I I` 1. Three: `I V I` 2, `I IV I` 1.
4. **Segment a.** A rhythm cell from the durations of the drawn strategy (half a beat, a beat, two beats) that fills the cell, with one distinctive feature drawn with equal weight: a dotted figure (a dotted beat and the half beat that completes it), a leap of a fourth to a sixth between two frame notes, or a repeated note. A feature that cannot fit the cell (a dotted figure in a cell that must end on a beat) gives way to the next.
5. **Segment a'.** a's rhythm, with its frame (the rungs of a's walk on the chord tones) moved to the next chord's tones, so it keeps a's contour; moved to start near the end of `a`, and on the same chord (`I I`) by at least one rung.
6. **Segment b.** A new cell whose duration sequence differs from a's, with no required feature, starting on a chord tone near the note before.
7. **Frame and decoration.** Chord tones of the segment's chord on its first note and on every on-beat note; every other note is a non-chord tone (passing, neighbor, appoggiatura or escape tone, each resolving) or else a chord tone. In minor the third of V is raised.
8. **Ending.** Rhythmic reduction in the last segment: its last note is at least a beat long, on a chord tone of the last chord, made by joining the cell's last notes (when the last segment is `a'`, `a`'s own last note is already at least a beat).
9. **Checks.** The motive keeps the melodic rules and the range and pitch limits of the drawn strategy, and its tone labels must read as drawn, with the plan as its given base harmony. Up to 256 candidates are drawn for each motive; a candidate draws its bars, shape and plan once and tries its notes up to 8 times within them, so a shape's weight is not changed by how often it fails the rules.

The segments are the motive's `features`, lettered `a` to `h` in order of first appearance, a later segment with an earlier one's rhythm taking that letter with a `'` for each earlier occurrence. `form` and `style` work as for `drawn`: a rhythm or contour motive keeps the segments and the harmony of the full one.

## The drawn strategy

The earlier strategy, kept as it was:

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
options' names and values (`mode`, `meter`, `key`, `form`, `strategy`, `role`, `style`, `style_without`, `count`, `seed`).

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
| `--strategy <name>` | `segments` (the default) or `drawn`: how the motive is made (above). |
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

Each motive is the document of `motive --json` (`motive` and `analysis`, with `notes_text`, `degrees_text` and the rest of the analysis), without `rendered`. The `analysis` carries `base_harmony` (the plan, `given` true, for `segments`; `I`, `given` false, proposed from the frame, for `drawn`), `segments` (the motive's own for `segments`, proposed for `drawn`), `tones` (the role of each note, and for a non-chord tone its type and the note it resolves to) and `warnings`, as `motive --json` does.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage or an invalid argument, or a style file that cannot be read. |
| 2 | No motive could be made. |
