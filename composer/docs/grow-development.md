# Theme development

The `development` setting of a theme request (ch. XI, P34, API 1.9): how far the presentation and the continuation stray from the motif. Back to the [docs index](README.md); the request member is in [theme-request.md](theme-request.md#the-brief), the choices a theme draws in [grow.md](grow.md#choices).

`"development": "melody" | "balanced" | "theme"`, default `balanced`; anything else is a request error at `/development`. It is read by `read_brief` (`src/api/api_theme.c`), kept as `GrowSetup.development` and `GrowThemeRequest.development` (`GrowDevelopment`: `GROW_DEVELOP_BALANCED` = 0, `GROW_DEVELOP_MELODY`, `GROW_DEVELOP_THEME`), and carried to the operator in `GrowContext.development` (set from `GrowConfig.context`). The brief of the result echoes it (`"development"`), and the explanation text of a theme names it (`development: theme`). One setting for a theme: none per slot, and a piece does not pass one to its themes yet.

## The tables

One table per decision, indexed by `GrowDevelopment`, at the top of the file that draws it. `balanced` is the weights of [fragments, repetitions](grow-segments.md) before the setting existed.

| Decision (table, file) | melody | balanced | theme |
|---|---|---|---|
| Repetition: exact, dominant form, sequence (`REPEAT_WEIGHTS`, `grow_beginning.c`) | 3, 1, 0 | 2, 1, 1 | 0, 1, 2 |
| Fragment 1: a segment other than `a`, then `a` (`FRAGMENT_SEGMENT_WEIGHTS`, `grow_ending.c`) | 1, 2 | 2, 1 | 3, 1 |
| Fragment 2: sequence one step down, then the next segment (`FRAGMENT_2_WEIGHTS`, `grow_ending.c`) | 1, 3 | 3, 1 | 1, 0 |
| Liquidation steps (`LIQUIDATION_STEPS`, `grow_ending.c`) | at most 1 | as many as fit | at least 2 (a motif with 2 segments or more) |

A weight of 0 removes the option. The weights apply to the options a request allows: when every option allowed is 0 (a theme setting with a one-cell basic idea, which allows no dominant form or sequence, leaves only the exact repetition), the balanced weights are used for that draw. A `theme` fragment 2 is the sequence unless it does not fit its idea, when the next segment is used; a liquidation the steps cannot meet drops notes as before. Exact repetitions are a choice again with `melody`.

## What sounds

Every development is one function: `grow_develop_motive(source, ref, out)` (`src/grow/grow_develop.c`) turns the ref of an idea into a chain of
the [variant registry](variant-registry.md) (no step or an exact step is `exact`; a liquidation with `steps` is that many `liquidate-features`
steps; every other kind is the registry row of that relation, its parameters set from the step) and applies it with a fixed seed of 1. The structure
stage's checks (`grow_idea_develop`) and the motives stage both call it, so the plan's `exact`, `sequence` or `segment b, 1 step down` is what the
idea sounds, notes and chords. The motives stage offers exactly that motive for every slot; the contrasting idea is the only slot with other offers
([grow-harmony.md](grow-harmony.md#motive-forms)).

## Effect

For the 2-bar two-segment motif of `tests/test_grow_segment_fragments.c` in an 8-bar sentence, seeds 1 to 10: `melody` draws no sequence repetition and its fragments quote the motif most (share of note pairs with the motif's duration and interval 0.94, `balanced` 0.89, `theme` 0.86); every `theme` run has a sequence for fragment 2 and a sequence repetition. `tests/test_grow_development.c` asserts the order; the compose rates hold for the three settings (`test_grow_compose_rates`, `test_grow_compose_rates_fast`).

## Fidelity

How faithful an idea sounds to the motive it was made from (P37, R-331, R-354). The melody stage ranks and refuses its candidates by it (R-355, below); the API members come next.

Four terms. The idea's **realized notes** are the notes of its melody span whose onsets, from the span's start, fall before the length of the idea's motive node (the pickup is part of that length, R-319), read as a motive in the key and meter of the lanes over the span (`grow_idea_realized`). Its **source** is the motive the idea was made from (`grow_idea_fidelity_source`): the span's `MOTIVE_REF` names the idea's motive node, and that node's own `MOTIVE_REF` gives the source node and the ref (the basic idea, and an idea with a motive of its own, are their own source with the span's ref). A ref that names a segment makes the source that segment of m0, cut with `grow_segment_motive` from the labelled feature of the basic motive when its notes are m0's, else from the proposed segments; a liquidation by segments is made from m0 whole. The **score** is `motive_recognizability` of the realized notes against the source, the total at the default weights. **Heard** is the chain `variant_recognize_depth` finds from the source to the realized notes, searched to depth 2, written with each step's registry phrase joined by ", then ". A transposition by whole octaves is not heard, because the realized idea sits in the voice's register (R-334), and an `exact` step is the recognizer's name for no change: an empty chain is "exact", and no chain found is "not recognized".

The idea **agrees** when the chain heard is the one its ref asked for, after dropping octave transpositions and exact steps:

| The ref is | The chain heard |
|---|---|
| no step, exact, a restatement, a segment fragment from its segment, the basic idea | empty |
| a transposition (a tonal one by steps, a real one by semitones) | one transposition of the same kind by the same steps (mod 7) or semitones (mod 12); empty when the ref's own amount is whole octaves |
| a dominant form | empty, or one transposition |
| a liquidation (`steps` above 0, or a `liquidate` step) | holds `fragment`, `liquidate`, `liquidate-features` or `condense` |
| any other relation (invert, interval change, the pool forms) | holds a step of that relation |

A motive the recognizer finds no chain for never agrees. `motive_fidelity(source, realized, ref, from_segment, out)` (`src/motive/motive_fidelity.c`) is the pure measure on two motives; `grow_idea_fidelity(plan, melody, basic, out)` (`src/grow/grow_fidelity.c`) reads an idea from the plan and calls it, and `grow_idea_role(name)` gives the role an idea has by its name (presentation, continuation, cadential or contrast). The recognizer is searched, so a chain that is not an exact restatement costs about half a second in a Debug build.

`motive_fidelity_depth` is the same measure with the recognizer's search cut to a number of steps, which the melody stage uses (below).

`tests/test_motive_fidelity.c` covers the measure, `test_grow_fidelity_sentence` and `test_grow_fidelity_period` the reading from a plan and print each idea's role, score and heard, and `test_grow_fidelity_sweep` (slow) prints the lowest score per role and development setting over the four sweep motifs.

### Ranking and refusing by fidelity

The melody stage's checker is `grow_fidelity_checker` (`src/grow/grow_fidelity.c`) around the stage's own checker (and around the style checker when a style is set), for every melody attempt and retry; the melody module knows no development setting. It runs the inner checker first and a hard violation stands. For a melody-fill candidate whose target span has a `MOTIVE_REF` and whose owner idea has a role, it measures the candidate's realized notes (the notes that carry the motive, `melody_candidate_realized`) against the idea's source as above, and:

- a score under the role's **minimum** is a hard violation (unless the ref makes the idea differ from its source, below), with the reason `motive fidelity n/d under n/d (role)`;
- otherwise the score becomes `((agree * 100 + floor(100 * fidelity)) * (total + 1) + held) / (201 * (total + 1))`, where `held/total` is the inner score: agreement ranks first, then the fidelity in hundredths, then the melody rules' share. `held/total` is taken in lowest terms, and a style's part of the score stays a part of it (scaled as `held` is).

Ideas with no motive ref or no role are not scored. The minimums are the table at the top of `grow_fidelity.c` (`grow_fidelity_minimum(development, role)`):

| Role | `balanced` | `melody` | `theme` |
|---|---|---|---|
| presentation (basic idea, repetition, consequent basic idea) | 11/20 | 11/20 | 11/20 |
| continuation (fragment 1, fragment 2) | 7/20 | 7/20 | 7/20 |
| cadential (cadential, cadential 1 and 2) | 3/10 | 7/20 | 1/5 |
| contrast (contrasting idea, consequent contrasting idea) | 0 | 0 | 0 |

R-355 started them at 3/4, 1/2 and 3/10 for `balanced`, 1/10 higher for `melody` and 1/10 lower for `theme`; the compose rates (R-317) lowered them in steps of 1/20, as far as they needed. The melody rules move a note that clashes with its chord (R-328), so the best candidate of a two-bar 3/4 motif's presentation scores 9/16, and a two-note segment's fragment 3/8.

The table is a setting (`GrowFidelityTable`, `GrowConfig.fidelity`; `grow_fidelity_minimum_in(table, development, role)` reads it, NULL for this table): see [preferences.md](preferences.md#the-development-group).

A contrasting idea is meant to stray, so it is scored and ranked but never refused. So is an idea whose ref cuts the motive down itself (a fragment by notes, a liquidation, a condensation, with no segment): its score against the whole source is low by what it was asked to be. A minimum may be lowered in steps of 1/20, only as far as the compose rates need (R-317).

Measuring is the costly part (R-307). The ranking searches the recognizer one step, not two: the score is the same at any depth and most relations an idea is asked for are one step, so the agreement a search of one step misses counts as none. A cadential idea is for its cadence, so it is only refused under its minimum (by its score: its residue is liquidated in more steps than the search reaches, so its agreement is not measured) and is ranked by the melody rules alone, which put the cadence tone first. Ranking it by fidelity as well lowered the cadences heard as planned in `test_middle_out_bass` from 17 of 20 to 16. The heard text of an idea (the API's, later) is still searched to two steps. A candidate whose realized notes were measured before for the same span, in any octave, is not measured again within a stage run.

`tests/test_grow_fidelity_rank.c` covers the ranking, the choice and the refusal, and `test_grow_fidelity_sweep` (slow) asserts the minimums over the four sweep motifs for every setting, and that presentation and continuation ideas are heard as intended at least 15% and 70% of the time (measured 20 to 26% and 75%: a note the chords move is heard as an interval change, and every kept candidate of a target is moved alike, so the ranking has little to choose from).
