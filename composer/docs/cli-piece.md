# piece command

The `piece` command of the Composer command line, with its options and output. Back to the [docs index](README.md).

```sh
ursatz-composer piece <request.json> [--seed <n>] [--check] [--out <loop-plan.json>] [--ending-out <ending-plan.json>] [--midi <loop.mid>] [--musicxml <loop.musicxml>] [--ending-midi <file>] [--ending-musicxml <file>] [--plain-midi] [--explain <file>] [--format text|json]
```

The command is a wrapper of the [composer API](api.md): it reads the request file (and the plan file of each theme fill, and
the loop plan of `--from`) into a request, calls `composer_api_piece` (or `composer_api_piece_variations` with
`--variations`), writes the plans, MIDI files, MusicXML and explanation the response holds to the paths given (the ending
version's MIDI file through `composer_api_export`), prints the result from the response JSON alone, and maps the status to
the exit status. The output below is the response's `json`; the plan paths and the `files` list are the command's.

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
  from (`basic motive`, `own motive`, the theme plan's path, the numerals); a section that `--regenerate` kept (outside the
  scope of the hearing, see [regenerate.md](regenerate.md)) also has `"in_scope": false`, and the text line ends `(kept)`
  where it would show the cadence heard;
- `score`: `{"numerator", "denominator"}`; `seam`: the four rules and how many held, for the loop, else `null`;
- `analyzer_check`: `{"planned", "agreed", "contradicts", "items"}` or `null`; `style_distance`: or `null`.

## Regenerate

```sh
ursatz-composer piece --from <loop-plan.json> --request <request.json> --regenerate <section id>|none [--lane harmony|bass|melody|inner|all] [--seed <n>] [--check] [--out <loop-plan.json>] [--ending-out <file>] [--midi <file>] [--musicxml <file>] [--ending-midi <file>] [--ending-musicxml <file>] [--plain-midi] [--explain <file>] [--format text|json]
```

Writes one section of a loop plan the command wrote again (R-173), and nothing the section does not reach. `--request` is
the request the plan was made from: it gives the structure for the ending version and the style, the tempo and the
motives. `--regenerate` names the section by the `id` of the request. Without `--lane` (or with `all`) the section is
reopened whole; with a lane, that lane of the section and what is written from it (`regenerate.h`; the bass and the melody
over new chords, the inner voices over a new bass), the rest locked. The outputs are `--out` and the other options above,
and the JSON gains `"regenerated": {"section", "lane"}`.

- **The seam.** When the section holds the body's first bar, the seam's copy of that bar is dropped and the sections the
  loop's end holds back (the turnaround) are reopened, so the seam stage copies the new bar and writes the end against it
  ([loop-seam.md](loop-seam.md)). Regenerating a held section only (the turnaround) leaves the copy as it is.
- **The ending version** is always built again from the new loop plan (seed + 20 of the seed that worked), so the two stay
  the same note for note up to the body's end. `--regenerate none` reopens nothing and only builds the ending version again;
  the loop version then has no score or seam in the output. It is for a variation chosen as the new loop plan.
- **A section filled with a theme** can be regenerated only with `--lane harmony` or `--lane bass` (exit 1 otherwise): its
  melody and inner voices are the theme's own. Its lanes are Given, and the lane asked for is reopened for this call only
  (`piece_reopen_lane`); the file keeps nothing Given that was not Given. Harmony is reopened with the bass and inner voices
  written from it, over the theme's melody (a harmonization).
- **Seeds.** As for the other options: the seeds from `--seed` are tried one after another, up to 16, and a seed that gets
  stuck in the loop or the ending is passed over.

An unknown section id is exit 1 (`the loop plan has no section '<id>'`). A section that cannot be regenerated (Given
material among its phrases, a progression fill's numerals) is exit 2.

## Variations

```sh
ursatz-composer piece --from <loop-plan.json> --request <request.json> --variations <section id> --lane harmony|bass|melody|inner|all --count <1..8> --out-dir <dir> [--seed <n>] [--check] [--plain-midi] [--format text|json]
```

Makes up to `--count` different versions of a section (R-088, R-173). The section is regenerated as above (the seam and the
loop's end included) for the seeds `--seed`, `--seed + 1`, ..., at most 4 times `--count` of them, and a version is kept when
its notes differ from the plan's and from every version kept before (the signature of `plan_signature`). A seed that gets
stuck is skipped. Fewer than `--count` may be kept; none is exit 2.

Each version is written to `--out-dir` (made if missing) as `variation-<i>.json` (the loop plan), `variation-<i>.mid` (with the
loop markers) and `variation-<i>.musicxml`, and `result.json` lists them: `{"schema": "ursatz-piece-variations", "section",
"lane", "variations": [{"index", "seed", "midi", "musicxml", "loop": V}], "kept", "requested", "seeds_tried"}` with V as above
(`plan`, `sections` with the cadence heard, `score`, `seam`, `analyzer_check`, `style_distance`). The same JSON is printed with
`--format json`. The ending version is not made: choose one, use its `variation-<i>.json` as `--from`, and run
`--regenerate none`.

Harmonizing is `--lane harmony` on a section whose melody is Given (a theme fill): the chords and the bass are new and every
melody note is kept. With any other melody the harmony is written again with the melody around it, which is allowed. The
other output options (`--out`, `--midi`, ...) do not apply and are exit 1.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage, or a request that does not read ([piece-request.md](piece-request.md#errors)). |
| 2 | The request cannot be satisfied: no seed of the 16 tried composes both versions (the stage the last was stuck in is named), a motive that does not fit in a bar. |
| 3 | An output file could not be written. All files are written or none. |
