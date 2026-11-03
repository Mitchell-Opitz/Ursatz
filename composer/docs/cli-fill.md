# fill command

```sh
ursatz-composer fill <plan.json> [--seed <n>] [--check] [--candidates <n>] [--style <file>] [--style-weight <n/d>] [--style-without <category>]... [--blend <file>] [--blend-weight <n/d> | --blend-weights <dimension>=<n/d>,...] [--out <plan.json>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi] [--record <file.json>] [--explain <file>] [--format text|json]
```

Realizes a plan: it runs `compose_run` (`<ursatz_composer/compose.h>`, see [piece.md](piece.md)) over the plan, which
fills every open theme section in stages (invent, structure, motives, harmony, bass, melody, texture and the returns),
and writes the piece.

The command reads the plan file into a request for `composer_api_fill` ([api.md](api.md)) and writes the plan, MIDI, MusicXML, records and explanation the response holds, all or none. The text it prints is made from the response document alone; a plan no seed completes is the response `stuck` report, printed as the lines below.

## Input

Any plan file: one from [`plan`](cli-plan.md), or one `grow --out` wrote. A plan with no open theme section (the
theme of `grow --out` is written already) is not an error: nothing is composed, and the files asked for are written
from the plan as it is. The output then says `nothing to fill: the plan is already realized`, and the records file
holds an empty array.

An Ursatz-Analyzer analysis document is not a plan. It describes a piece that is already written, so it is refused,
with a message that names `plan_from_analyzer_result` (`<ursatz_composer/analysis_load.h>`), which builds a plan from an
Analyzer result for the library to use. The document is recognized by the Analyzer (`analyzer_document_status` is not
`ANALYZER_DOCUMENT_UNREADABLE`); the Composer never reads its content. A plan file of format version 1 or 2 opens
([plan-format.md](plan-format.md#format-versions)); `--out` writes version 2.

## Options

| Option | Meaning |
| --- | --- |
| `--seed <n>` | Whole number from 0. By default one is derived from the clock. The same plan and seed give the same output, byte for byte. |
| `--check` | Analyze every theme section with the Ursatz Analyzer and reject a run whose analysis contradicts the plan: a planned cadence not heard at its bar, the key not heard, a hard voice-leading fault in material the plan wrote. See [analyzer-check.md](analyzer-check.md). Off by default; with it the result says how many cadences and keys were heard as planned. |
| `--candidates <n>` | Compose `n` candidates (1 to 32) from the seeds `--seed`, `--seed + 1`, ... and write the best: the most cadences and keys heard as planned, then the best score, then the lower seed. Every candidate is listed with its seed, its agreement and its score. Not combined with the seed retries: if no candidate completes the command fails. |
| `--style <file>` | Prefer the harmony and melody closest to a style: a style model file the Ursatz Analyzer wrote (`--style-profile`). Overrides the plan's own style. The Analyzer's distance from the style is printed with the result, per dimension. See [style.md](style.md). |
| `--style-weight <n/d>` | How strongly the style is preferred (default `1/1`). Overrides the plan's weight; needs a style from here or the plan. |
| `--style-without <category>` | Read the style without this category (repeatable, at most 8). Replaces the plan's own list when given; needs a style from here or the plan; an unknown name is exit 1. See [style.md](style.md). |
| `--blend <file>` | A second style model file, blended with the style (`--style`, or the plan's style). The share of each of the two in each dimension is set by the weights below. See [style.md](style.md). |
| `--blend-weight <n/d>` | The second style's share in every dimension, from 0 (only the first) to 1 (only the second); default `1/2`. Needs `--blend`. |
| `--blend-weights <dimension>=<n/d>,...` | The share per dimension: `harmony`, `rhythm`, `melody`, `form`. A dimension not named takes `--blend-weight`, else `1/2`. Needs `--blend`. |
| `--out <plan.json>` | The realized plan as JSON. |
| `--midi <file>`, `--musicxml <file>` | The piece, one part for each voice. The source material is not part of the piece, and a layered form sounds as its layers say. |
| `--tempo <bpm>` | Write a tempo in the MIDI and MusicXML files: quarter notes per minute from 20 to 400, a whole number or a fraction like `195/2`. Default: none. See [export.md](export.md). |
| `--plain-midi` | MIDI without track names and section markers. See [export.md](export.md). |
| `--record <file.json>` | The records of the stages as one JSON array. Each element is `{"stage": <name>, "record": <fill record>}`, the record being the `fill_record_to_json` document of that stage (the seed, the attempts, the candidates and why each was chosen or rejected). A stage that did not run has no element. |
| `--explain <file>` | Why each candidate was chosen or rejected, put together from the records, the Analyzer check and the rejected runs: JSON when the file name ends in `.json`, else text. Works with or without `--check` and `--style`. See [explain.md](explain.md). |
| `--format <text\|json>` | How the result is printed on stdout. Text by default. |

## Seeds

The seed is always printed. A run can get stuck, and each seed starts again from the plan as it was read; the next
seeds are tried, up to 16, and the seed that worked is printed. Giving that seed again gives the same piece.

## Output

In text: the seed used; for each section the form heard in it and the cadences heard (the form is what standard-form
hears in the written music, which need not be the form planned: `heard as hybrid, not period` is reported, not an
error); the thematic returns heard against the ones planned, with the similarity of each pair; then the files written.

```
seed 5

sections (form and cadences heard)
  A   heard as hybrid, not period, cadences imperfect-authentic, perfect-authentic
  B   heard as hybrid, not sentence, cadences perfect-authentic
  A   heard as hybrid, not period, cadences imperfect-authentic, perfect-authentic

returns heard 1 of 1 planned
  A resembles A (similarity 1.00)
```

A section in which no theme is heard (a theme of 4 bars is seldom heard) is reported as `no theme heard`.

A plan with given material ([middle-out.md](middle-out.md), placed with `plan --given-melody` and the like) is filled
around it. Before the run `fill` copies the Given spans the user placed, and after it compares them with the plan, span
by span and byte for byte (the pitches and the times of the notes, the numerals of the chords). The result lists each
run of them and says that they are unchanged:

```
given material, unchanged
  melody in B from bar 1, 8 bars
  progression in A from bar 1, 4 chords
```

A run that changed them (it must not) is refused with exit status 2 and nothing is written. In JSON the list is
`given`: `{"unchanged": true, "runs": [{"kind", "section", "bar", "bars" or "chords", "unchanged"}]}`, present only when
the plan has given material.

With `--check` the result also says `analyzer check: 6 of 6 planned cadences and keys heard as planned` (an accepted run
always has every one heard). With `--candidates` it lists the candidates, best first, before the result:

```
candidates (3 of 12 completed, best first)
  seed 13  agreement 6/6  score 17/22  written
  seed 4  agreement 6/6  score 16/22
  seed 14  agreement 6/6  score 16/22
```

With `--format json` the result is one object: `seed`, `realized` (true for a plan that had nothing to fill),
`candidates` (with `--candidates`: each `seed`, `agreed`, `planned`, `score` and `written`), `sections` (each with
`letter`, `heard`, `heard_as_requested` and `cadences`), `analyzer` (with `--check`: `planned` and `agreed`), `returns`
(`planned`, `heard` and the `pairs` with `later`, `earlier` and `similarity`) and `files`.

## A plan that cannot be completed

When no seed completes the plan, nothing is written and the command lists, on stderr, the stage it was stuck at and every
target still open with the reason it could not be filled:

```
Error: the plan could not be completed from seeds 1 to 16: stuck at the harmony stage
  open: bass lane of basic idea: no operator applies
  open: harmony lane of continuation: every candidate was rejected
```

`no operator applies` is a target a later stage would have filled; `every candidate was rejected` is the target the
stage could not fill. A plan can also be rejected after it is complete, when the keys or cadences heard do not fit
the plan; the message says so.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | The piece was written. |
| 1 | Usage or an invalid argument. |
| 2 | The plan cannot be read (missing, not JSON, not a plan, or an analysis document) or cannot be completed from any of the seeds. |
| 3 | An output file could not be written. Nothing is left behind: every file is written beside its name and all are renamed only when all were written. |
