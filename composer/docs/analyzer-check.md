# The Analyzer check

`<ursatz_composer/analyzer_check.h>`, `src/check/analyzer_check.c`, and `compose_candidates` in
`<ursatz_composer/compose.h>`. R-080: the Analyzer is the contradiction gate of a composed section, not a score.

A realized section is rendered with every lane (`plan_render_range`, `<ursatz_composer/render.h>`: only the notes that
start in the section, with times measured from its start), analyzed in memory by the Ursatz Analyzer
(`analyzer_analyze_fragment`, `<ursatz_analyzer/composer_api.h>`) in the section's meter and in the key the plan has at
its start, and what the Analyzer hears is compared with what the plan asked for.

## What is compared

`analyzer_check_section(plan, section, &check)` gives an `AnalyzerCheck`. Its items, in order:

| Kind | Planned | Heard | Agreed when |
| --- | --- | --- | --- |
| cadence | each CADENCE constraint of the section: the first at its last bar, a second (an antecedent's weak cadence) at the last bar of the first half | the Analyzer's cadences that arrive in that bar (the bar the arrival chord starts in) | a planned PAC or IAC: any authentic cadence is heard there (PAC, IAC or unspecified); any other type: that type |
| key | the key lane at the section's start | the first key region | tonic and mode are equal |
| violation | nothing | a hard voice-leading fault (parallel fifths or octaves) | never: it is a contradiction |

A violation is listed only when it involves a note the plan wrote, one in a span that is not Given. A fault between
Given notes is the user's own and is not held against the plan. The soft faults (direct fifths and octaves, unresolved
sevenths and leading tones) are the Analyzer's to report; the check does not use them.

`planned` counts the cadences and the key, `agreed` those heard as planned, and `contradicts` is true when
`agreed < planned` or a hard violation was found. A section the check cannot read (a lane still open, no notes, a length
that is not whole bars, no meter or key over it) returns an error and is not held against the plan.

## The whole piece

`analyzer_check_piece(plan, &out)` (R-172, R-173) checks a piece structure plan as one fragment. The plan is rendered whole
(`plan_render`, so nothing past the piece's end: not the seam, not the material), analyzed in its first meter and key, and
sections need not be whole bars (the per-section check above stays as it was for the other commands). The planned items are,
for each section with a piece part, its first `CADENCE` constraint, and the key.

- A cadence is heard when an Analyzer cadence of its type (any authentic cadence for a PAC or IAC) either ends within one beat
  of the section's end (where the chord of its arrival gives way to the next) or has its arrival begin in the section's last
  bar: a tonic that arrives in the last bar and carries on into the next section ends a bar or more after it.
- A planned HC leads on, so it is also heard when any cadence arrives within a beat of the section's end (the dominant resolving
  into what follows). The Analyzer cannot hear a cadence with nothing after it, so a cadence at the end of the piece is not
  held against the plan when the Analyzer hears none there; a cadence of another type there is.
- A hard voice-leading fault involving a note the plan wrote is listed as an item, as in a section, but does not make the piece
  contradict the plan: the checkers do not yet keep parallels out of the short intro and turnaround, and a whole piece would
  pass almost no seed. `contradicts` is true when `agreed < planned`.

`compose_run` with `analyzer_check` uses it for a structure plan: `ComposeOutcome.analyzer_checks` holds that one check
(`analyzer_check_count` is 1), and the rejected runs name it `piece`. The `piece` command runs it on both versions.

## In `compose_run`

`ComposeConfig.analyzer_check` is false by default, so a run without it is what it always was (the P4 baselines and
byte-identical runs hold). With it, after a complete run has passed the key and cadence checks, every theme section is
checked. A run with a contradiction is rejected like a key-rejected one: `ComposeOutcome.analyzer_rejected`,
and the run starts again from the plan as given with the next round's seeds, up to the same twelve rounds. The checks
are kept in `ComposeOutcome.analyzer_checks` (one per theme section) with their totals `analyzer_planned` and
`analyzer_agreed`, and the compose score adds `agreed` to its numerator and `planned` to its denominator.

`form_hear`, `return_hear`, `voice_hear` and the texture hearing are unchanged: they stay scores, and the Analyzer check
is the only gate. A single fill-loop candidate is never sent to the Analyzer (too slow, one analysis a candidate): the
check is at section level only.

## Candidates

`compose_candidates(plan, config, count, &out)` runs `compose_run` on `count` fresh copies of the plan (a JSON snapshot;
the plan given is not changed) with seeds `seed`, `seed + 1`, and so on, keeps the complete runs that were not rejected,
and ranks them: by agreement share (`agreed / planned`, 1 when nothing was planned, as when the check is off), then by
the compose score, then by the lower seed. Each candidate keeps its plan, its outcome and its seed, and `agreed`,
`planned` and `score` as ranked, so a caller can list them after taking the best one's plan and outcome. The same inputs
give the same order.

## Measured

Seeds 1 to 10, composed with `analyzer_check` (`tests/test_analyzer_check.c`, `planned`, label `slow`). Every accepted
run has every planned cadence and its key heard as planned; the table is how many of the ten complete, with the count
without the check beside it. A minor in 3/4 is where the Analyzer most often hears a cadence elsewhere than planned
(the final cadence one bar early, or a half cadence in place of an authentic one).

| Form | C major 4/4 | A minor 3/4 |
| --- | --- | --- |
| sentence, 8 bars | 9 (10) | 9 (10) |
| period, 8 bars | 10 (10) | 1 (10) |
| rounded binary, 24 bars | 10 (10) | 0 (0) |

The rounded binary in A minor 3/4 does not complete without the check either (stuck at the harmony stage, see
[piece.md](piece.md)). The test asserts these counts as baselines (R-075): it fails only below them.

## Command line

`fill --check` composes with `analyzer_check`; `fill --candidates <n>` (1 to 32) runs `compose_candidates` and writes the
best candidate where `--out`, `--midi` and `--musicxml` point, listing every candidate (seed, agreement, score) in the
result; `grow --check` passes over a seed whose theme the Analyzer contradicts and prints the seed that was kept. See
[cli-fill.md](cli-fill.md) and [cli-grow.md](cli-grow.md). The check costs one analysis for each theme section
and round: twelve candidates of a rounded binary in C major took about 8 seconds with the check and 1.4 without.
