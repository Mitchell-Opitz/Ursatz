# The explanation of a run

`<ursatz_composer/explain.h>`, `src/check/explain.c`, `fill --explain` and `grow --explain`. It reports why each
candidate was chosen or rejected. It is put together from what a run already keeps and changes no choice. Back to the [docs index](README.md).

## What it holds

| Part | Contents |
| --- | --- |
| Targets | Per stage, per target the fill loop worked on (a node, or a lane span with its lane and voice): the section letter, the bars it covers (1-based from the start of the piece, by the meter at its start), the candidate chosen, up to three runners-up, and every rejected candidate with its reason. A step no operator applied to weighed nothing and is left out. |
| Candidates | The operator, the label, the whole score and the style term in it (`style`, present only when a style was set). A runner-up also says how far it scored behind the chosen one (`behind`, 0 for a tie, which the run's random stream settled). |
| Sections | Per theme section: the form heard and the form asked for, and the Analyzer check items ([analyzer-check.md](analyzer-check.md)): each planned cadence and the key with what was heard, and any hard voice-leading fault. A section the check did not read (no `--check`, or a section it cannot read) says so. |
| Rejections | Every whole run `compose_run` threw away and started again from the plan as given, with its round, its seed and the stage reached. The reason is `stuck` (a stage left a target open), `key` or `cadence` (the plan's keys or cadences, with the checker's text) or `analyzer` (the analysis contradicted the plan, with the items that disagreed). `grow` adds the seeds the command passed over. |

The seam stage ([loop-seam.md](loop-seam.md)) is a stage of its own: the structure, motives, harmony, bass, melody and texture
targets of the held loop's end are listed under `seam`, in the order they were worked on, with the section letter of the
held section. A harmony or melody candidate that ends at the seam has the seam rules it keeps in its score, so a runner-up
can score behind the chosen one by one or more of them. The records of a stage are kept together, so the melody stage's last
attempt is the one explained, as it is elsewhere.

A scored candidate whose checker did not hold a shaping rule (R-164) names it: the candidate's reason ends in `; not held:` and
the names, which the text prints as `(not held: ...)` and the JSON gives as `not_held`. The names are `ending-reduction`, `waves`,
`central-range` and `monotony` for a melody ([melody-fill.md](melody-fill.md#the-shaping-rules)) and `root-progression` for a
harmony ([grow-harmony.md](grow-harmony.md#the-harmony)). A candidate that held them all says nothing.

Reasons are the checkers' own strings; nothing is put into words beyond them. A motive-form candidate of the motives stage
([grow-harmony.md](grow-harmony.md#motive-forms)) is labelled with its chain, its connection score in percent and its derivation (`transpose 90%:
Transposed +2 steps of the key's scale. Kept the`, cut at 63 characters), and scores 1 plus the connection. A saved form from a pool ([grow-harmony.md](grow-harmony.md#the-pool)) is labelled `pool <item> <connection>%:
<chain>`, with the item and the chain it was saved with, and scores 1 plus the connection plus 1/2 (approved) or 1/4 (unflagged).

## The summary line of a developed idea

The theme result lists each developed idea (`developments`, and the `motive developments` of the text output) with a
`summary` of one line that names the idea as the plan does, its source and its operation: `repetition = basic idea,
dominant form`, `fragment 1 = segment b, exact`, `fragment 2 = segment b, 1 step down (tonal)`, `cadential = basic idea,
segments liquidated, 2 steps`. The item also has `segment`, the label of the segment the idea is made from, or `null`.
The text output prints `<motive>: <summary>` for each (`m3: fragment 2 = segment b, 1 step down (tonal)`).

## Intended against heard

Each developed idea of the theme result also says how faithful its notes are to what the plan asked for (`fidelity`,
`minimum`, `heard`, `heard_agrees` in `developments`, API 1.10). The plan's `development` and `summary` are what was
intended: `fragment 2 = segment b, 1 step down (tonal)`. `fidelity` is the recognizability of the notes the melody holds
against the idea's source (0 to 1), and `heard` is what the recognizer finds between the two ("exact", a transposition,
"cut to the leading 2 notes", "transposed -1 step of the key's scale", "not recognized"), with whole-octave transpositions left out because the melody sits in its
register. `heard_agrees` is true when the heard relation is the intended one, so a repetition planned as exact but heard
as a narrowed interval reads `false` (the melody rules bent a note to its chord). `minimum` is the least fidelity the melody
stage accepts for the idea's role and the request's `development` setting ([grow-development.md](grow-development.md#ranking-and-refusing-by-fidelity));
an idea whose operation makes it differ from its source has minimum 0. The members are `null` for an idea with no melody to measure. The text output of `theme` does not print them.

## The style term

`FillConsidered` has `style_numerator` and `style_denominator`: the part of the score a style added (weight times how
close the candidate is to the style, `style_check.c`). The score includes it, so a score reads as the rules' part plus
the style's. They are 0/1 with no style. `fill_record_to_json` writes `style` (`{"num", "den"}`) after `score` only for
a candidate checked with a style, so a record without a style is byte for byte what it was (the test holds a hash of
one taken on main).

## In the library

- `ComposeOutcome.rejections` (`ComposeRejection`, `<ursatz_composer/rejection.h>`) is owned by the outcome;
  `GrowOutcome.rejections` is the same for `grow_run`.
- `explain_build(plan, outcome, &explanation)` for a compose run, `explain_build_grow(plan, theme, outcome, check,
  extra, extra_count, &explanation)` for a grow. An `Explanation` owns copies, so it outlives the outcome.
- `explain_to_text` gives one block per section, then each target in it by stage, then the targets outside the sections,
  then the rejected runs. `explain_to_json` gives schema `explanation`, version 1: `stages` (each with its `targets`),
  `sections` and `rejections`.

## In the CLI

`--explain <file>` on [fill](cli-fill.md) and [grow](cli-grow.md) writes JSON when the name ends in `.json`, else the
text. It works with or without `--check` and `--style`, and is written with the other outputs, all or none.

## Limits

- The records keep the last attempt of a stage (the melody stage is written up to six times), so an earlier attempt's
  candidates are not explained; the rejected runs say that a run was thrown away.
- Bars use the meter at the start of the piece, so a piece that changes meter has approximate bars after the change.
- `fill` retries the next seed when a seed is stuck; only the last seed's rejections are in its explanation.
