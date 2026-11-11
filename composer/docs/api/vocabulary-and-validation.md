# Versions, vocabulary and validation entries

Core versions, vocabulary and validation entries of the Composer API. Back to the [docs index](../README.md).

## `composer_api_versions(out)`

`json`: `{"composer_version", "api_version", "analyzer_version", "ursatz_commit", "plan_format_version"}`, the versions the
Composer was built with (the Analyzer's, and the Ursatz commit the Analyzer was configured against, are the ones a plan or
an analysis is stale against). Schema: `schema/api/versions.schema.json`.

## Vocabularies: `composer_api_vocabulary(out)`

The words an application offers and the requests accept, in the order an application shows them, so it holds no table of its own (R-270: where the Composer and an application differ, the Composer's list wins). No request. `json`:

| Member | Entries |
|---|---|
| `meters` | `2/4 3/4 4/4 6/8`: `{"value", "label", "beats", "chord_onset_step": "1/2", "second_chord_beat"}`. A theme chord starts on any half beat of a bar and, where the bar takes two chords, the second on `second_chord_beat` (4/4: 2, 6/8: 3; absent for 2/4 and 3/4). |
| `modes`, `piece_modes` | `major`, `minor`; `piece_modes` adds `style` ("From the brief"). |
| `keys` | the 24 keys as the Composer spells a tonic: C, Db, D, Eb, E, F, F#, G, Ab, A, Bb, B major, then C, C#, D, D#, E, F, F#, G, G#, A, Bb, B minor. |
| `pitch_class_names` | `{"value": 0 to 11, "major", "minor"}`: the tonic of each pitch class in each mode. |
| `cadences`, `theme_cadences` | `PAC IAC HC DC PC` with their names; a theme ends in `PAC`, `IAC` or `HC`. |
| `pickups` | `{"<meter>": ["0", "1/4", "1/2", "1", "3/2", "2", "3"]}`: the pickups offered in beats, each a number a reader accepts in that meter (shorter than a bar). Any other multiple of 1/32 of a whole note shorter than a bar is accepted too. |
| `motive_forms`, `roles`, `functions`, `distances` | `full rhythm contour`; `melodic accompaniment`; `beginning continuation contrast cadential`; `close medium far`. |
| `theme_slots`, `theme_ideas`, `theme_motive_categories`, `theme_pools` | the four slots, each with `required` (API 1.13: true for `basic` alone, the slot every theme needs; an application reads it instead of naming the slot); the ten ideas, each with its `slot`; the style categories a theme can read without (`motive-degrees`, `motive-intervals`, `motive-rhythm`, `motive-nct-figure`); the pools `approved-and-unflagged`, `approved`, `none` (the Composer reads no pool: they are the application's rule for which saved motives it offers, listed so the application keeps no table). |
| `theme_bars`, `theme_first_cadences`, `style_weights` | the bars a theme has (`4`, `8`); the cadences a period's first phrase ends in (`IAC`, `HC`, in the order of `cadences`); the style weights offered (`1/4` Light, `1/2` Normal, `1` Strong; any fraction the reader accepts is still accepted). Theme modes are the existing `piece_modes`. |
| `theme_beginnings`, `theme_endings` | `{"value", "label", "ideas": [<theme_ideas value>, ...]}`: `presentation antecedent cbi` and `continuation consequent cadential`, each with its ideas in order. A hybrid is written `<beginning>+<ending>`; these are the words its reader takes. |
| `accompaniment_patterns` | `any block arpeggiated alberti oom-pah motive`. |
| `piece_parts`, `piece_intos`, `fill_kinds` | `intro body turnaround ending`; `sentence period hybrid`, where an archetype made of exactly one pairing has `beginning` and `ending`, the values of `theme_beginnings` and `theme_endings` (API 1.13: `sentence` is `presentation` and `continuation`, `period` is `antecedent` and `consequent`, `hybrid` has neither); `empty motif theme progression`. |
| `tone_labels` | `chord` and the ten non-chord tone types the analysis writes, each with an `abbreviation` (`CT`, `PT`, `NT`, ...). |
| `variant_operations` | what `composer_api_variant_operations` gives. |
| `preferences` | the default settings document, `composer_preferences_write` of the defaults (API 1.11; 1.12 adds the nine listening settings of the `motif` group, 1.17 the L3 settings of both groups, [preferences.md](../preferences.md)): the starting point of a listening session's settings. |
| `limits` | the sizes a request is held to, as numbers: `motive_notes` (64), `motive_chords` (8), `motive_features` (8), `variant_chain` (4), `variant_functions` (4), `random_motive_count` (16); since 1.1 also `tempo_min` (20), `tempo_max` (400), `piece_sections` (32), `piece_beats` (4096) and `seed_max`, an object of the largest seed each request reads: `{"variants": 4294967231, "random_motive": 4294967295, "theme": 4294967295, "piece": 4294967295}`. A limit the Composer does not hold (cells of a chord in a piece section) is not listed. The command line shows the first six in its help and holds repeated options to them.; since 1.11 also `preferences`, an object of the limits of the settings document as numbers: `weight_max` (20), `pickup_one_in_max` (64), `leap_min` (3), `leap_max` (12), `range_low_min` (36), `range_low_max` (96), `range_high_max` (108), `range_span_min` (12), `density_max` (4), `scale_max` (1000), `climax_rise_min` (1), `climax_rise_max` (12), `ratio_denominator_max` (16), `fidelity_denominator_max` (1000), `attempts_max` (256) and `keep_max` (16); since 1.12 also `accented_one_in_max` (64) and `leaps_per_motif_max` (11); since 1.15 also `long_nct_one_in_max` (64); since 1.17 also `syncopation_one_in_max` (64), `skips_per_motif_max` (11), `plan_chords_max` (4) and `plan_beat_max` (12). |

Every list but those noted is `[{"value", "label"}]`, and every value is accepted by the reader that reads it (`tests/test_api_vocabulary.c` builds a request from each one). The lists come from the parser or table that reads them; the tables of the words that are not a library's are in `src/api/api_vocabulary.c`.

Differences from the GUI tables it replaces (Ursatz-GUI `049adaf`; the Composer's list wins):

- `keys`, `pitch_class_names`: the GUI offers one list of tonics (`C C# D Eb E F F# G Ab A Bb B`) for both modes. The Composer spells a major tonic Db (not C#) and a minor tonic C#, D# and G# (not Eb and Ab).
- `pickups`: the GUI offers `0 1/4 1/2 1 3/2 2 3` for every meter; the Composer drops the ones that are not shorter than a bar (2/4 has no `2` or `3`, 3/4 no `3`).
- `accompaniment_patterns`: the GUI has five patterns; the Composer adds `motive` (a pattern read from a motive of the accompaniment role).
- `tone_labels`: the GUI has an "Automatic" choice first (no override); the Composer lists none, a note without an override is one the request leaves out. The abbreviations are the GUI's, now served.
- `motive_forms`: no `enabled` flag; the three forms are all read.
- `cadences`, `meters`, `roles`, `piece_parts`, `piece_intos`, `fill_kinds`, `theme_slots`, `theme_pools`, `theme_motive_categories`: the same values as the GUI. Labels are the GUI's where it has one.

## Forms: `composer_api_forms(out)`

The built-in forms, in registry order (API 1.13, R-428): what the Pieces view needs to offer a form and show its sections without a table of its own. No request; quick, no job, never cancelled; `COMPOSER_ERR_INTERNAL` if a built-in does not parse. `json` is `{"forms": [...]}`, each form:

| Member | Entries |
|---|---|
| `name`, `label`, `closing` | the form's name, its label, and whether the piece ends with a cadence. |
| `sections` | `"sizes"` when the sizes list the sections, `"request"` when the piece request gives them (the game loop). |
| `versions` | `{"value", "label", "loop", "after"}`: a piece of a form with a loop has `loop` ("Loop version") then `ending` ("Ending version", `after: "loop"`); any other form has `piece` ("Piece"). |
| `settings` | `modulate` (the form has regions), `accompaniment` (`"required"` or `"optional"`), `voicing` and `section_fills` (true for a form whose sections the request gives). |
| `regions` | the names of the keys a piece may move to. |
| `sizes` | `{"bars", "count", "default", "sections"}`; a section is `name`, `letter`, `role` and `beginning` and `ending` (each `{"value", "label"}`, the plan's words and the labels of `theme_beginnings` and `theme_endings`), `bars` or `beats` (the other is null), `cadence` (`{"value", "label", "core"}`), `closed`, `keys` (`start` and `end`: `"home"` or a region), `motive` (the relation from the first motive, or null), `returns` and `variation` (null when the section returns nothing), `layers` (`"all"` or lane names), `pedal` (null, or `{"value": "dominant", "label": "Dominant pedal"}`: the section's presentation is written over a pedal in the bass) and `slots`: `{"slot", "ideas": [...]}` for each slot its theme has, with the ideas the pairing gives it. |
| `roles`, `loop_roles`, `cadences`, `default_ending` | for a form whose sections the request gives: the roles a section may have (`{"value", "label", "required"}`, only `body` required), the roles that are the loop, the cadence a section gets when the request names none (`{"default", "last": [{"roles", "cadence"}]}`) and the section a request with no ending gets (`{"id", "letter", "role", "bars", "cadence"}`); `[]`, `[]`, null and null for a form with sizes. |

Schema: `schema/api/forms.schema.json`. A piece of any form but the game loop is not composed yet: the forms are listed here, the piece entries take them in P45 B2.

## Validation: `composer_api_validate(kind, request_json, out)`

Reads a request as its entry does and composes nothing, so an application can check a form and show the Composer's defaults. `kind`: `motive` (the request of `composer_api_motive`), `theme` (of `composer_api_theme`), `piece` (the piece request object of `docs/piece-request.md`, not the `{"request", ...}` wrapper of `composer_api_piece`) and `random_motive` (of `composer_api_random_motive`). `json`: `{"request": <the request with the Composer's defaults filled>}`:

- `motive`: `meter` (the motive's, when the request has none and the input declares one), `role` (`melodic`), `input.format` (`auto`) and `input.form` (`full`) when absent; `harmony` as `[{"numeral", "onset"}]`, the numerals as the library writes them (`V7`, `bVII65`) and the onsets in beats of the meter in the request syntax (`"0"`, `"3/2"`). Own harmony takes upper case numerals with an optional figure and `@onset` (`I V@3/2 V7@3`); a slash chord or a lower case numeral is refused at `/harmony`.
- `piece`: the `cadence` of every section that has none (`piece_default_cadences`: the last of the intro and of the loop `HC`, the last ending `PAC`, any other `IAC`) and the default ending section (`end`, `PAC`, an empty fill) when the request has no ending.
- `random_motive`: the `seed` (the request's, or the one read from the clock) and the `mode` and `meter` a `"random"` drew from it (R-269).
- `theme`: the request as it is, once its plan is built without seeding. A request that Compose would refuse before it seeds (the pairing, the bars, the cadences, a motif that is too long or does not suit its place, an accompaniment or voicing that is not valid, a given chord that cannot arrive the cadence) is refused with `COMPOSER_ERR_UNSATISFIABLE`, the reason and the path of its field: [theme-request.md](../theme-request.md#refusals-before-composing). One refusal is given at a time, the first.

A request an entry would refuse is refused the same way, with its message and path. An unknown `kind` is `COMPOSER_ERR_REQUEST` with the path `/kind`. A style model named in the request is read.
