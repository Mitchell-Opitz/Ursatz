# Changelog

The Composer versions and what each changed. Back to the [docs index](README.md).

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
