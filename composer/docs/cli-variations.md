# variations and harmonize commands

The `variations` and `harmonize` commands of the Composer command line, with options and output. Back to the [docs index](README.md).

```sh
ursatz-composer variations <plan.json> (--node <id> | --section <letter>) [--lane harmony|bass|melody|inner] --count <n> [--seed <n>] --out-dir <dir> [--midi] [--musicxml] [--tempo <bpm>] [--plain-midi] [--format text|json]
ursatz-composer harmonize <plan.json> --section <letter> --count <n> [--seed <n>] --out-dir <dir> [--midi] [--musicxml] [--tempo <bpm>] [--plain-midi] [--format text|json]
```

Take a filled plan (from [`fill`](cli-fill.md)) and write several distinct versions of one part of it. `variations`
writes a node, or one lane of it, again with other seeds, as [`regenerate`](cli-regenerate.md) does, and keeps the
versions that sound different. `harmonize` writes the harmony of a section again over a melody that is given. The plan
file is never changed; every version is a plan file of its own. See [regenerate.md](regenerate.md) for how the versions
are chosen.

`variations` and `harmonize` read the plan file into a request for `composer_api_variations` and `composer_api_harmonize` ([api.md](api.md)). The command writes each version's plan, its MIDI file (`composer_api_export` over the plan) and its MusicXML from the response.

## Options

`--node`, `--section` and `--lane` address the target exactly as `regenerate` does ([cli-regenerate.md](cli-regenerate.md)).
`harmonize` takes `--section` only: it works on the harmony lane of one section.

| Option | Meaning |
| --- | --- |
| `--count <n>` | How many versions to keep, 1 to 16 (required). |
| `--seed <n>` | Whole number from 0 for the first seed tried. Default: from the clock. The seed of every version is printed. |
| `--out-dir <dir>` | Where the versions are written (required). The directory is created (one level). |
| `--midi` | Also write `variation-<i>.mid` for every version. |
| `--musicxml` | Also write `variation-<i>.musicxml` for every version. |
| `--tempo <bpm>` | Write a tempo in the MIDI and MusicXML files: quarter notes per minute from 20 to 400, a whole number or a fraction like `195/2`. Default: none. See [export.md](export.md). |
| `--plain-midi` | MIDI without track names and section markers. See [export.md](export.md). |
| `--format <text\|json>` | How to print the result (default: text). |

Every version is written as `<out-dir>/variation-<i>.json` (the plan, `i` from 1), and with `--midi` and `--musicxml` as
`variation-<i>.mid` and `variation-<i>.musicxml`. A plan file is read by every other command, so a version can be
locked, regenerated, exported or varied again.

## Output

Text: one line for each version, the seed that made it and what was heard in each section, then how many were kept.

```
$ ursatz-composer variations piece.json --section B --count 4 --seed 3 --out-dir variations --midi
variation 1  seed 3  A heard as sentence, cadences perfect-authentic; B heard as hybrid, cadences half; A' heard as sentence, cadences perfect-authentic
variation 2  seed 4  A heard as sentence, ...
kept 4 of 4, 4 seeds tried
```

JSON: `midi` and `musicxml` are `null` when not asked for; the paths are as the command was given them (relative to the
working directory). `sections` is the array `fill --format json` prints.

```json
{"variations": [{"index": 1, "seed": 3, "plan": "variations/variation-1.json", "midi": "variations/variation-1.mid",
                 "musicxml": null, "sections": [{"letter": "A", "heard": "heard as sentence",
                 "heard_as_requested": true, "cadences": ["perfect-authentic"]}]}],
 "kept": 1, "requested": 4, "seeds_tried": 16}
```

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | At least one version was kept. `kept` may be less than `requested`. |
| 1 | Usage or an invalid argument. |
| 2 | None was kept, the plan cannot be read, the node is not found, is Given or cannot be regenerated, or (`harmonize`) the section has no given melody. Nothing is written. |
| 3 | An output file could not be written. |

## harmonize

The whole melody of the section must be Given: a plan made with `plan --given-melody <file> --at <letter>` where the
melody fills every bar of the section, then filled. The melody is never changed; the harmony, the bass and the voices
between are written again, and a version is kept only when its chords differ from the plan's and from the versions kept
before. A section with melody the Composer wrote itself is refused (exit 2): a melody written over new chords is a
variation of it, which `variations --lane harmony` gives.

```sh
ursatz-composer plan --form ternary --key C --meter 4/4 --given-melody tests/fixtures/melody_c_major_8_bars.txt --at B --seed 1 --out piece.json
ursatz-composer fill piece.json --seed 1 --out piece.json
ursatz-composer harmonize piece.json --section B --count 4 --seed 1 --out-dir harmonizations --midi
```
