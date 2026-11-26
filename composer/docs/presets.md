# Presets (P69c B5, R-754)

A preset is a named settings document built in to the Composer: the preferences a style of her listening sets and the accompaniment pattern it travels with. A theme or random motive request names one with `preset`; the request's own `preferences` apply over it, member by member, so a request that sets `{"blueprint": {"climax": {"m6": 2}}}` over `ff7` keeps ff7's other values and takes `m6` 2. The files are `composer/presets/<name>.json` (schema `composer/schema/preset.schema.json`, `"schema": "ursatz-preset"`, version 1), embedded at build time by `cmake/EmbedPresets.cmake`; adding one is a file and its name in `composer/src/CMakeLists.txt`.

| Name | Label | Source |
|---|---|---|
| `ff7` | Final Fantasy VII | `references.md` ref-2-ff7 (rated 5) and `taste-profile.md`, style uematsu-ff7 (rounds u1 to u15) |
| `hollow-knight` | Hollow Knight | `references.md` ref-4-hollowknight (rated 5) and `taste-profile.md`, style none |

Each preset sets the `motif` group (the durations, features, rung moves and decorations of the segments strategy, the shapes of two bars and, for ff7, `leap_recovery`, `pickup_from_accent`, `long_on_weak` and `contrast_fidelity_min`), `melody.cadence_long`, and the `blueprint` group's `sounds` and `climax` ([preferences-blueprint.md](preferences-blueprint.md)): ff7 draws ninths, suspended fourths, major sevenths and leading tones often and puts the climax in bar 6 (`m6` 0, `m7` 1); hollow-knight draws ninths and major sevenths, no leading tone and long notes off the beat, with the climax in bar 5 or 6. The accompaniment pattern is `michelle.pattern.arpeggio-eight` (ff7) and `michelle.pattern.arpeggio-wave` (hollow-knight). A preset has no `development` or `critic` member (those are for the balanced path; blueprint ranking is `blueprint.candidates`) and no `melody.range` (a generated blueprint sets the run's range from its climax and `blueprint.range`).

## In a request

- **Theme.** `"preset": "ff7"` (a string, or null for none). The theme's accompaniment is the preset's pattern when the request and its blueprint give none and no voicing is asked for; the request's own accompaniment wins. The response's `brief` has `preset` when the request names one. Settings are read in this order: the defaults, the preset, the request's `preferences`.
- **Random motive.** `"preset"` the same way; the motif and blueprint groups shape the motif ([cli-random-motive.md](../../cli/docs/cli-random-motive.md#the-blueprint-strategy-r-753)).
- **Refused.** A name that is no preset, or a value that is no string, at `/preset`: "'<x>' is not a preset; see the vocabulary's presets". The vocabulary's `presets` lists `{name, label, source}` in the order above.
- **CLI.** `ursatz listen --preset <name>` sends it with every sample of motifs and themes, and the manifest line records it as `preset` ([cli-listen.md](../../cli/docs/cli-listen.md)).

The code is `src/preferences/preferences_preset.c` (`composer_preset_count`, `composer_preset_name`, `_label`, `_source`, `composer_preset_apply`) and `composer_preferences_apply` in `preferences.c`, which applies a document over settings instead of over the defaults.
