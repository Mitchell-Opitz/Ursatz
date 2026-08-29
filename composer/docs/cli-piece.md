# piece command

```sh
ursatz-composer piece <request.json> [--seed <n>] [--check] [--out <loop-plan.json>] [--ending-out <ending-plan.json>] [--midi <loop.mid>] [--musicxml <loop.musicxml>] [--ending-midi <file>] [--ending-musicxml <file>] [--plain-midi] [--explain <file>] [--format text|json]
```

Composes a looping game track from a request ([piece-request.md](piece-request.md)): an intro, a body, a turnaround and an
ending, each made of sections of any length in beats. Two versions come out (R-172):

- **loop**: the intro, the body and the turnaround. The loop is the body and the turnaround; its end is composed last against
  a copy of the body's first bar, so that it leads back ([loop-seam.md](loop-seam.md));
- **ending**: the intro and the body exactly as the loop has them (every lane Given), then the ending, composed from seed + 20.
  The ending's harmony hears the body's last chord. The turnaround and the seam are left out.

The two versions are the same note for note up to the body's end.

The seed, `--check`, `--out` and the other options are `theme`'s ([cli-theme.md](cli-theme.md)). The seeds from `--seed`
are tried one after another, up to 16, and a seed whose loop or ending is stuck (or, with `--check`, contradicted by the
Analyzer) is passed over. `--check` runs `analyzer_check_piece` on each version ([analyzer-check.md](analyzer-check.md#the-whole-piece)).

| Option | Effect |
| --- | --- |
| `--out`, `--ending-out` | The plan of each version as JSON. |
| `--midi`, `--ending-midi` | Each version as a MIDI file with the tempo of the brief. The loop's file has the markers `loopStart` and `loopEnd` in the conductor track ([export.md](export.md#loop-markers)); the ending's has none. |
| `--musicxml`, `--ending-musicxml` | Each version as MusicXML. |
| `--plain-midi` | No track names and no section markers. The loop markers stay. |
| `--explain <file>` | Why each candidate was chosen or rejected, in both versions: text with a heading for each, or with a `.json` name `{"loop": ..., "ending": ...}` ([explain.md](explain.md)). |

## Output

Text: a `piece in <key>` line, the brief, the loop's region (`loop from <a> to <b> (whole notes)`), then for each version one
line for each section (`<id> <part> <letter> start <time>, <n> beats, cadence <type> (heard <type>), <fill> fill (<source>)`), the
score, the seam rules held (loop only), the number of planned items the Analyzer agreed with (with `--check`) and the style
distance (with a style).

JSON: `{"schema": "ursatz-piece", "seed", "key", "meter", "brief", "loop": {"start", "end"}, "versions": {"loop": V, "ending": V},
"files": [...]}`, times as `"n/d"` whole notes, and each version V
`{"plan", "sections", "score", "seam", "analyzer_check", "style_distance"}`:

- `plan`: the path written with `--out` / `--ending-out`, else `null`;
- `sections`: `{"id", "part", "letter", "start", "beats", "cadence", "heard", "fill", "source"}`; `cadence` is the planned type,
  `heard` the cadence the Analyzer heard at the section's end (`null` without `--check`), `source` where the material came
  from (`basic motive`, `own motive`, the theme plan's path, the numerals);
- `score`: `{"numerator", "denominator"}`; `seam`: the four rules and how many held, for the loop, else `null`;
- `analyzer_check`: `{"planned", "agreed", "contradicts", "items"}` or `null`; `style_distance`: or `null`.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage, or a request that does not read ([piece-request.md](piece-request.md#errors)). |
| 2 | The request cannot be satisfied: no seed of the 16 tried composes both versions (the stage the last was stuck in is named), a motive that does not fit in a bar. |
| 3 | An output file could not be written. All files are written or none. |
