# ursatz critique

Judges candidates with the critic (R-721, [composer/docs/critic.md](../../composer/docs/critic.md)). Back to the [docs index](README.md).

```
ursatz critique <file>... [--settings <file>] [--format text|json]
ursatz critique --rated <file> [--settings <file>] [--format text|json]
```

## Candidate files

A file is one of:

- a **listen sample**: the `.json` that `ursatz listen` writes. A motif sample holds its `motive`; a theme sample holds a plan only when it was made with `listen --plans` (`themes[0].plan`), and without one `critique` refuses it and says so;
- a **plan document** (a theme's or a piece's), or
- a **motive document**.

The files of one call, 1 to 16, are one **set**: the critic judges each with the others beside it, so `one_rhythm` can tell that several open with one rhythm. `--settings` is a preferences document ([preferences.md](../../composer/docs/preferences.md)) sent as `preferences`; its `critic` group sets the weights, the evaluators that count the other way and nothing else the critic uses.

Text, one block for each file:

```
motifs/01.json  score -19/14
  bar - beat -  leap_load  skips and leaps make 6/7 of the moves, more than 1/2
  bar 2 beat -  open_ending  the last note (bar 2) is no longer than the line's mean note
```

`score` is minus the weighted sum of the evaluators' penalties (0 is nothing found). Each observation line is `bar <b> beat <x>  <rule>  <message>`, with `-` where it has no place. A theme with a node named `theme` also has the observations of the Composer's hearing of it against its plan (`cadence_heard`, `theme_heard`, `contrast_heard`). `--format json` prints the Composer's response unchanged ([api](../../composer/docs/critic.md#the-critique-entry)).

## A rated set

`--rated <file>` reads `ursatz-rated-set` version 1:

```json
{"schema": "ursatz-rated-set", "schema_version": 1, "pairs": [
  {"label": "9-74", "a": "9-74-a/01.json", "b": "9-74-b/01.json", "rating_a": 2.5, "rating_b": 3.5, "set_a": "9-a", "set_b": "9-b"}]}
```

The paths are relative to the file, the ratings 1 to 5 in halves. A set label names the candidates judged together (one round's side): the command sends one request for each label, with the files of its pairs in order of first appearance. For each pair it prints her ratings, the critic's scores and whether the critic and today's ranking chose the one she rated higher:

```
9      74  2.5 3.5  critic -1.278 -1.250 match   today tie
critic: match 19  miss 9  tie 1  of 29
today: match 2  miss 0  tie 27  of 29
```

A verdict is `match` (the higher score is the one she rated higher), `miss`, `tie` (equal scores) or `unrated` (equal ratings; not counted in the totals). Today's ranking is a theme's own `score` when both files are themes, else a tie. `phases/P68-measure` in Claude Master holds the rated set of P68 and `make.sh`, which makes its files again.

## Exit status

0 it ran; 1 usage, an invalid option or an unreadable file; 2 the Composer refused a candidate (its message and path are printed).
