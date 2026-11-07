# Changelog

The Composer versions and what each changed. Back to the [docs index](README.md).

## Unreleased (phase P36)

Every development operation of a theme produces the idea's notes and chords from the motive (R-329, R-330, B1). One function, `grow_develop_motive` (`src/grow/grow_develop.c`), develops a source motive by a plan ref through the variant registry; the structure stage's checks and the motives stage both use it, and the motives stage offers exactly that motive for every slot except a contrasting idea, which keeps the pool and the operations as a fallback and has its ref rewritten to the step applied. A tonal transposition now moves the motive's chords with its notes. Each developed motive node carries its own `MOTIVE_TONES` (its chords and the tone role of each note), also when a motive operator wrote it. A contrasting idea is drawn in the key's scale when the motif is. The API version does not change; seeded theme and piece output does, and the snapshots, hashes and baselines changed are named in the phase notes. See [grow-development.md](grow-development.md#what-sounds), [grow-harmony.md](grow-harmony.md#motive-forms) and [variant-registry.md](variant-registry.md).

## Unreleased (phase P35)

Melody-fill realizes a motive's notes instead of re-deriving them (R-326 to R-328, R-333 to R-339): the melody voice's range is C4 to C6 (84, was 79), each idea says its motive's pitches moved by one octave and bent to its chords only as they ask (a clash the NonChordTone framework labels a figure is kept, escape tones and retardations included), and only the notes after the motive are generated, under the arch. `MelodyFillParams.tolerance` is gone. A motive no strict attempt composes is composed relaxed. The API version does not change; seeded theme and piece output does, and the snapshots, hashes and baselines changed are named in the phase notes. Fixed on the way: the structure stage reserves a bar for every motive it makes, so a section regenerated in a full material no longer fails to apply. See [melody-fill.md](melody-fill.md).

## Unreleased (phase P34)

A theme request takes `"development": "melody" | "balanced" | "theme"` (ch. XI, default `balanced`): how far the presentation and the continuation stray from the motif. It sets the weights of the repetition (exact, dominant form, sequence), of fragment 1's segment, of fragment 2 (sequence or next segment) and the steps of a liquidation; `balanced` is the weights of before. `melody` brings the exact repetition back and keeps a liquidation to one step; `theme` favours the sequence and segment `b` and liquidates in two steps or more. The API is 1.9: the brief of the result gains `"development"` and the explanation names it. A wrong value is a request error at `/development`. See [grow-development.md](grow-development.md).

A continuation develops the motif through its segments (ch. VIII). Fragment 1 is a segment of the motif (its `features`, or the proposed segments when it has none), fragment 2 is that segment one step down in sequence or the next segment, and the cadential idea, or the two ideas of a cadential ending, liquidate the segments step by step until a residue fits and can take the cadence; the segments are Given motive nodes in the material, and the plan's `motive_ref` records them (`"segment"`, `"steps"`, both optional). The API is 1.8: each item of the theme result's `developments` gains `"segment"` (the label or `null`) and `"summary"` (one line naming the idea, its source and its operation), the text output prints the summary, and the plan JSON gains the two members. Seeded theme output changes (new draws, two more bars of material for a continuation or a cadential ending); the snapshots and expectations changed are named in the phase notes. See [grow.md](grow.md#choices).

A presentation repeats its basic idea in one of three ways, drawn per seed (ch. V): exact, in its dominant form, or in sequence one step up with its chord a step up (I to ii, i to iio). `GrowThemeSpec.repetition` (`GrowRepetition`) replaces `statement_response`, the weights are `REPEAT_WEIGHTS` in `grow_beginning.c` (2, 1, 1), and `describe_ref` names them `restated, exact`, `dominant form` and `sequence, 1 step up (tonal)`. The API is 1.7 (a new value of the theme result's `development` text only). Seeded theme output changes for presentations (new draws); the snapshots and expectations changed are named in the phase notes. A sequence is the transposition by one step and writes its chords whole, so it is not drawn over Given material in the first half; the share of 8-bar sentences heard as a sentence fell from 75% to 50% ([grow-hearing.md](grow-hearing.md)). See [grow.md](grow.md#choices).

## Unreleased (phase P33)

The compose rates are measured and held by a test: every buildable pairing at 4 and 8 bars composes on at least 8 of 10 start seeds for a 2-bar 3/4 and a 1-bar 4/4 reference motif, and none needed a fix. The API stays 1.6. See [grow.md](grow.md#compose-rates).

The API is 1.6: `composer_api_validate("theme", ...)` builds the plan of the request without seeding and refuses exactly what Compose refuses before it seeds, each refusal with the path of its field (`/bars`, `/hybrid`, `/cadence`, `/first_cadence`, `/slots/<slot>`, `/ideas/<name>`, `/accompaniment`, `/voicing`, `/slots/basic` for a motif that is too long). A given chord in the last bar of the theme (or of an antecedent) that cannot arrive the cadence is refused at `/chords/<n>` with `chords bar 7: V cannot end a PAC; the cadence needs a tonic chord in root position`; Compose gives the same message and path instead of the generic text, and the "given chords in bar" message after seeding has the path `/chords`. In C, `grow_build_plan_checked` gives a `GrowRefusal` (result, path, message). See [theme-request.md](theme-request.md#refusals-before-composing).

The API was 1.5 before: a basic motif as long as the presentation or the antecedent of a theme (4 bars of an 8-bar theme, 2 of a 4-bar one) fills that part whole, placed as written, with no repetition or contrasting idea of its own; the rest of the theme is made from the motif's first idea-length bars. A contrasting slot or the motive of the repetition or contrasting idea beside it is refused at `/slots/contrasting` or `/ideas/<name>`: "the basic motif fills the whole presentation; leave the contrasting idea out". Any other motif longer than the basic idea is refused at `/slots/basic` with "the motif is 3 bars; a 8-bar theme takes a motif of up to 2 bars, or of 4 for its presentation or antecedent" (this replaces the theme message of 1.4). See [grow.md](grow.md#how-a-motif-fills-its-idea).

The API was 1.4 before: a motif as long as the idea it fills is that idea, placed as written, so a 2-bar motif is the basic idea of an 8-bar theme or of a piece of 8-bar sections (a shorter one is grown to the idea as before). The basic, slot and idea motives and the motive of a piece or plan may be as long as their idea; a longer one is refused with "the motif is 3 bars; the basic idea of a 8-bar theme is 2", at `/slots/basic` for a theme request and `/motive` for a piece or plan request. A section's drawn motive may be as long as its idea. The snapshot `piece-loop-and-ending` (stdout, plan and MIDI) changes because a drawn section motive of 2 bars is no longer refused. See [grow.md](grow.md#how-a-motif-fills-its-idea).

## Unreleased (phase P32)

The API is 1.3: `composer_api_random_motive` takes `strategy`, `"segments"` (the default: motives built from a harmony plan and repeated segments, returned with their harmony and segments as their own) or `"drawn"` (the earlier strategy, unchanged); `composer_api_vocabulary` serves `random_motive_strategies`. The CLI takes `random-motive --strategy`. See [api/motives.md](api/motives.md) and [cli-random-motive.md](cli-random-motive.md).

The API was 1.2 before: every motive analysis document (`composer_api_motive`, `composer_api_motive_from_midi`, `composer_api_motive_render`, `composer_api_random_motive` and the variants) has `analysis.segments`, `{"given", "items"}`: the motive's own features when it has any, otherwise a proposal from its bars and rhythmic groups. See [motive-format.md](motive-format.md#segments).

## Version 0.4.1 (phase P28)

`composer_api.h` is the only public header; every other header is private to the Composer (`src/include/`) and its dependencies are linked PRIVATE. The API is unchanged (1.1).

## Version 0.4.0 (phase P27)

The Composer builds as a subproject of an application that already defines `cjson`, `ursatz` and `ursatz_analyzer`, and
`URSATZ_COMPOSER_BUILD_CLI` turns the command off. The API is 1.1: `composer_api_vocabulary` also serves `theme_bars`,
`theme_first_cadences`, `theme_beginnings`, `theme_endings`, `style_weights` and, in `limits`, `tempo_min`, `tempo_max`,
`seed_max`, `piece_sections` and `piece_beats`. See [api.md](api.md).

## Version 0.3.0 (phase P26)

Everything an application asks of the Composer is the composer API, 1.0 (R-219); the CLI is a client of it and nothing else:
`cli/` includes only `composer_api.h`, checked by `scripts/check-cli-api.sh`. The API gained the `limits` of
`composer_api_vocabulary` and the `plan_file` member of `composer_api_export`. See [api.md](api.md).

## Version 0.2.0 (phase P25)

The Composer holds plan structure and composing decisions only, on library values, and hears only through the Analyzer.
The public types and the plan format changed:

- Plans use the library's meter, key, harmony, motive and time values; motives are `MotiveSketch` values with relation steps
  (`step` in plan files; old `transform` names still read). Plan format version 2 is the Composer's own, with its own schema.
- Chord numerals, key names and note spelling follow the library (`viio`, `D#` minor).
- Melodic and harmonic rules, harmonic rhythm cells and the seam rules come from the library's framework checks.
- What a listener finds in the written music is the Analyzer's: one `analyzer_hear` per section span over one render per run.
- The corpus seed reads chords through the Analyzer's chord sequence reader.
- `plan_regenerate_all` hears and checks only the sections it reopened (`ComposeConfig.scope`, `ComposeHeard.in_scope`);
  a section outside the scope prints as `kept` in the output of `regenerate` and `piece --regenerate`.
