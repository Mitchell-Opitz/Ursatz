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

## Effect

For the 2-bar two-segment motif of `tests/test_grow_segment_fragments.c` in an 8-bar sentence, seeds 1 to 10: `melody` draws no sequence repetition and its fragments quote the motif most (share of note pairs with the motif's duration and interval 0.94, `balanced` 0.89, `theme` 0.86); every `theme` run has a sequence for fragment 2 and a sequence repetition. `tests/test_grow_development.c` asserts the order; the compose rates hold for the three settings (`test_grow_compose_rates`, `test_grow_compose_rates_fast`).
