# The explanation of a run

`<ursatz_composer/explain.h>`, `src/check/explain.c`, `fill --explain` and `grow --explain`. It reports why each
candidate was chosen or rejected. It is put together from what a run already keeps and changes no choice.

## What it holds

| Part | Contents |
| --- | --- |
| Targets | Per stage, per target the fill loop worked on (a node, or a lane span with its lane and voice): the section letter, the bars it covers (1-based from the start of the piece, by the meter at its start), the candidate chosen, up to three runners-up, and every rejected candidate with its reason. A step no operator applied to weighed nothing and is left out. |
| Candidates | The operator, the label, the whole score and the style term in it (`style`, present only when a style was set). A runner-up also says how far it scored behind the chosen one (`behind`, 0 for a tie, which the run's random stream settled). |
| Sections | Per theme section: the form heard and the form asked for, and the Analyzer check items ([analyzer-check.md](analyzer-check.md)): each planned cadence and the key with what was heard, and any hard voice-leading fault. A section the check did not read (no `--check`, or a section it cannot read) says so. |
| Rejections | Every whole run `compose_run` threw away and started again from the plan as given, with its round, its seed and the stage reached. The reason is `stuck` (a stage left a target open), `key` or `cadence` (the plan's keys or cadences, with the checker's text) or `analyzer` (the analysis contradicted the plan, with the items that disagreed). `grow` adds the seeds the command passed over. |

Reasons are the checkers' own strings; nothing is put into words beyond them. A motive-form candidate of the motives stage
([grow.md](grow.md#motive-forms)) is labelled with its chain, its connection score in percent and its derivation (`transpose 90%:
Transposed +2 steps of the key's scale. Kept the`, cut at 63 characters), and scores 1 plus the connection.

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
