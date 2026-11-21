# Versions, vocabulary and validation entries

Core versions, vocabulary and validation entries of the Composer API. Back to the [docs index](../README.md).

## `composer_api_versions(out)`

`json`: `{"composer_version", "api_version", "producer_key", "composition_key", "plan_format_version"}` (the plan format is 6, `composition_key` the key of the code that composes, both API 3.1), the versions the
Composer was built with (the Analyzer's, and its producer key, held in `producer_key` since API 4.0, are the ones a plan or
an analysis is stale against). Schema: `schema/api/versions.schema.json`.

## Vocabularies: `composer_api_vocabulary(context, out)`

The words an application offers and the requests accept, in the order an application shows them, so it holds no table of its own (R-270: where the Composer and an application differ, the Composer's list wins). No request. `json`:

| Member | Entries |
|---|---|
| `meters` | `2/4 3/4 4/4 6/8`: `{"value", "label", "beats", "chord_onset_step": "1/2", "second_chord_beat"}`. A theme chord starts on any half beat of a bar and, where the bar takes two chords, the second on `second_chord_beat` (4/4: 2, 6/8: 3; absent for 2/4 and 3/4). |
| `materials`, `piece_materials` | `{"value": <material id>, "label": <key word>}` for the materials the Composer composes in: `omt.material.major` (`major`) and `omt.material.natural-minor` (`minor`); `piece_materials` adds `{"value": "style", "label": "From the brief"}` (API 2.0; they were `modes` and `piece_modes`). |
| `keys` | the 24 keys as the Composer spells a tonic: C, Db, D, Eb, E, F, F#, G, Ab, A, Bb, B major, then C, C#, D, D#, E, F, F#, G, G#, A, Bb, B minor. |
| `pitch_class_names` | `{"value": 0 to 11, "names": {<material id>: <spelling>}}`: the tonic of each pitch class in each of the two materials (API 2.0; the entries were `{"value", "major", "minor"}`). |
| `cadences`, `theme_cadences` | `PAC IAC HC DC PC` with their names; a theme ends in `PAC`, `IAC` or `HC`. |
| `pickups` | `{"<meter>": ["0", "1/4", "1/2", "1", "3/2", "2", "3"]}`: the pickups offered in beats, each a number a reader accepts in that meter (shorter than a bar). Any other length on the meter's grid shorter than a bar is accepted too: a multiple of 1/32 of a whole note, or a triplet of the beat or the division (1/6 and 1/12 in 4/4). |
| `motive_forms`, `roles`, `functions`, `distances` | `full rhythm contour`; `melodic accompaniment`; `beginning continuation contrast cadential`; `close medium far`. |
| `theme_slots`, `theme_ideas`, `theme_motive_categories`, `theme_pools` | the four slots, each with `required` (API 1.13: true for `basic` alone, the slot every theme needs; an application reads it instead of naming the slot); the ten ideas, each with its `slot`; the style categories a theme can read without (`motive-degrees`, `motive-intervals`, `motive-rhythm`, `motive-nct-figure`); the pools `approved-and-unflagged`, `approved`, `none` (the Composer reads no pool: they are the application's rule for which saved motives it offers, listed so the application keeps no table). |
| `theme_bars`, `theme_bars_regular`, `theme_extensions`, `theme_first_cadences`, `style_weights` | the bars a theme has (`3` to `16`, API 1.18; a length that no construction makes from the motif is refused at `/bars`); the lengths that need no construction (`[4, 8]`, numbers); the extensions of a construction, `{"value", "label", "ideas", "varied"}`: `repeat` (Internal repetition, `varied` true, with the ideas it can follow), `stretch` (Stretched cadence, with its ideas), `addition` (Addition after the cadence) and `evaded` (Evaded cadence), the last two with no ideas; the cadences a period's first phrase ends in (`IAC`, `HC`, in the order of `cadences`); the style weights offered (`1/4` Light, `1/2` Normal, `1` Strong; any fraction the reader accepts is still accepted). Theme modes are the existing `piece_modes`. |
| `theme_beginnings`, `theme_endings` | `{"value", "label", "ideas": [<theme_ideas value>, ...]}`: `presentation antecedent cbi` and `continuation consequent cadential`, each with its ideas in order. A hybrid is written `<beginning>+<ending>`; these are the words its reader takes. |
| `accompaniment_patterns` | `any block arpeggiated alberti oom-pah motive`. |
| `piece_functions` | `[{"name", "label", "parts"}]` (API 1.23, R-510): the functions a game loop section can take as its request `function`: `transition` (`Transition`, parts `intro`, `body`) and `coda` (`Coda`, part `ending`). Its entries have `name`, not `value`: each carries the parts it can be in. |
| `schemas` | `[{"value", "label"}]`, the galant schemas a section's `harmony` may name: those that make a chord of each stage in the major and the natural minor (R-610). |
| `relations` | `[{"id", "name", "family"}]`, every relation (K6) of the knowledge snapshot, `family` `tonal`, `modal`, `neo-riemannian`, `set` or `serial`: the words a section's `region` may name (R-610, [piece-request.md](../piece-request.md)). |
| `piece_parts`, `piece_intos`, `fill_kinds` | `intro body turnaround ending`; `sentence period hybrid`, where an archetype made of exactly one pairing has `beginning` and `ending`, the values of `theme_beginnings` and `theme_endings` (API 1.13: `sentence` is `presentation` and `continuation`, `period` is `antecedent` and `consequent`, `hybrid` has neither); `empty motif theme` (the old `progression` fill is read as a section's harmony, not offered). |
| `tone_labels` | `chord` and the ten non-chord tone types the analysis writes, each with an `abbreviation` (`CT`, `PT`, `NT`, ...). |
| `variant_operations` | what `composer_api_variant_operations` gives. |
| `preferences` | the default settings document, `composer_preferences_write` of the defaults (API 1.11; 1.12 adds the nine listening settings of the `motif` group, 1.17 the L3 settings of both groups, 1.19 the L4 settings of the `melody` and `development` groups, [preferences.md](../preferences.md)): the starting point of a listening session's settings. |
| `preference_keys` | since 4.3: `[{key, type, min, max, words, class, default}]`, one per leaf of the default `preferences` and `motif.plan` ([preferences.md](../preferences.md#the-key-and-rule-tables-r-672)); `type` `integer`, `boolean`, `ratio`, `words` or `plan`, `class` `hard` or `soft`. |
| `rules` | since 4.3: `[{name, class, key}]`, the 13 soft groups and 7 hard rules of the melody stage; `key` is the setting that holds the rule. |
| `theme_developments` | since 4.1: how far a theme may stray from its motif, `[{value, label, help}]` with the values `melody`, `balanced` and `theme` (a request without the member reads as `balanced`); the editor offers these and shows the help. |
| `lanes` | since 4.1: the lanes of a piece version a regenerate or a set of variations may name, `[{value, label}]`: `harmony`, `bass`, `melody`, `inner` and `all`. |
| `limits` | the sizes a request is held to, as numbers: `motive_notes` (64), `motive_chords` (8), `motive_features` (8), `variant_chain` (4), `variant_functions` (4), `random_motive_count` (16); since 1.1 also `tempo_min` (20), `tempo_max` (400), `piece_sections` (32), `piece_beats` (4096) and `seed_max`, an object of the largest seed each request reads: `{"variants": 4294967231, "random_motive": 4294967295, "theme": 4294967295, "piece": 4294967295}`. A limit the Composer does not hold (cells of a chord in a piece section) is not listed. The command line shows the first six in its help and holds repeated options to them.; since 1.11 also `preferences`, an object of the limits of the settings document as numbers: `weight_max` (20), `pickup_one_in_max` (64), `leap_min` (3), `leap_max` (12), `range_low_min` (36), `range_low_max` (96), `range_high_max` (108), `range_span_min` (12), `density_max` (4), `scale_max` (1000), `climax_rise_min` (1), `climax_rise_max` (12), `ratio_denominator_max` (16), `fidelity_denominator_max` (1000), `attempts_max` (256) and `keep_max` (16); since 1.12 also `accented_one_in_max` (64) and `leaps_per_motif_max` (11); since 1.15 also `long_nct_one_in_max` (64); since 1.17 also `syncopation_one_in_max` (64), `skips_per_motif_max` (11), `plan_chords_max` (4) and `plan_beat_max` (12); since 1.19 also `rhythm_repeat_bars_max` (16); since 4.1 also `theme_motives` (16), `theme_extensions` (3), `piece_variations` (8) and `harmony_cells` (8); since 4.2 also `construction_bars` (`{"min": 3, "max": 16}`, the bars a theme may have with a construction) and `construction_pickup` (`false`). |

Every list but those noted is `[{"value", "label"}]`, and every value is accepted by the reader that reads it (`tests/test_api_vocabulary.c` builds a request from each one). The lists come from the parser or table that reads them; the tables of the words that are not a library's are in `src/api/api_vocabulary.c`.

Differences from the GUI tables it replaces (Ursatz-GUI `049adaf`; the Composer's list wins):

- `keys`, `pitch_class_names`: the GUI offers one list of tonics (`C C# D Eb E F F# G Ab A Bb B`) for both materials. The Composer spells a major tonic Db (not C#) and a minor tonic C#, D# and G# (not Eb and Ab).
- `pickups`: the GUI offers `0 1/4 1/2 1 3/2 2 3` for every meter; the Composer drops the ones that are not shorter than a bar (2/4 has no `2` or `3`, 3/4 no `3`).
- `accompaniment_patterns`: the GUI has five patterns; the Composer adds `motive` (a pattern read from a motive of the accompaniment role).
- `tone_labels`: the GUI has an "Automatic" choice first (no override); the Composer lists none, a note without an override is one the request leaves out. The abbreviations are the GUI's, now served.
- `motive_forms`: no `enabled` flag; the three forms are all read.
- `cadences`, `meters`, `roles`, `piece_parts`, `piece_intos`, `fill_kinds`, `theme_slots`, `theme_pools`, `theme_motive_categories`: the same values as the GUI. Labels are the GUI's where it has one.

## Forms: `composer_api_forms(out)`

The built-in forms, in registry order (API 1.13, R-428): what the Pieces view needs to offer a form and show its sections without a table of its own. No request; quick, no job, never cancelled; `URSATZ_ERR_INTERNAL` if a built-in does not parse. `json` is `{"forms": [...]}`, each form:

| Member | Entries |
|---|---|
| `name`, `label`, `closing`, `modulates` | the form's name, its label, whether the piece ends with a cadence, and whether its key plan always leaves home whatever the request says (API 1.21, R-493; the scherzo). |
| `sections` | `"sizes"` when the sizes list the sections, `"request"` when the piece request gives them (the game loop). |
| `versions` | `{"value", "label", "loop", "after"}`: a piece of a form with a loop has `loop` ("Loop version") then `ending` ("Ending version", `after: "loop"`); any other form has `piece` ("Piece"). |
| `settings` | `modulate` (the form has regions and does not always modulate: the choice is offered), `accompaniment` (`"required"` or `"optional"`), `voicing` and `section_fills` (true for a form whose sections the request gives). |
| `regions` | the names of the keys a piece may move to. |
| `meters`, `tempo` | the meters a piece of the form may be in as `"3/4"` (`[]` for any) and its tempo in quarter notes a minute (`null` for none), as the form document gives them (API 1.20). |
| `repeats`, `parts`, `performed` | the groups of section names the form writes out twice (`[["A"], ["B", "A return"]]`), the parts of a compound form (`null`, else `[{"name", "form", "key", "motive", "again", "repeats", "bars"}]`: `bars` is the size of the part's form it is composed at or `null` for the compound size's (API 1.21, R-495; the scherzo and trio's Trio is 24), `form` is `{"value", "label"}` or `null` for an again part, `key` is `"home"` or the region, `motive` `"piece"` or `"new"`, `again` the name of the part it performs again), and whether a piece is performed in order (repeats or parts; R-485). A compound form's sizes have `bars`, `count` 0, `default` and `sections` `[]`. |
| `sizes` | `{"bars", "count", "default", "label", "sections"}` (`label`: `"3 variations, 32 bars"` or `"16 bars"`, API 4.2); a section has `labels` `{"role", "return", "keys", "figure"}`, the sentences it is shown as (API 4.2), and is `name`, `letter`, `role` and `beginning` and `ending` (each `{"value", "label"}`, `null` for a liquidation, a retransition and a coda, with `slots` (API 2.2), the plan's words and the labels of `theme_beginnings` and `theme_endings`), `bars` or `beats` (the other is null), `cadence` (`{"value", "label", "core"}`), `closed`, `keys` (`start` and `end`: `"home"` or a region), `motive` (the relation from the first motive, or null), `returns` and `variation` (null when the section returns nothing; `variation` is one of `exact`, `ornament`, `reharmonize`, `reregister`, `varied`, `figurate`, `embellish` or `setting`), `setting` (null, or the pattern of a `setting` return's new accompaniment: `"block"`, `"arpeggiated"`, `"alberti"` or `"oom-pah"`; API 1.24, R-521), `layers` (`"all"` or lane names), `pedal` (null, or `{"value": "dominant", "label": "Dominant pedal"}`: the section's presentation is written over a pedal in the bass), `slots`: `{"slot", "ideas": [...]}` for each slot its theme has, with the ideas the pairing gives it, and, only for a section with a construction (R-468; no built-in form has one), `construction`: `"auto"`, or the object a theme response gives (`halves`, `ideas`, `extensions`, `reason` `given`, `text`). | A section also has `leads_in` (true when it is held until the sections after it are written) and `connective` (`null`, else `{"value": "sometimes" or "always", "label"}`; API 1.20, R-486). It also has `figure` (`null`, else `{"kind", "rhythm", "harmony", "change_at", "climax"}` with the name strings of a figurate return's motive of variation: `kind` `"neighbor"`, `"scale"` or `"broken-chord"`, `rhythm` `"eighths"` or `"sixteenths"`, `harmony` `"kept"` or `"reharmonized"`, `change_at` the bar or `null`) and `mode` (`null`, `"parallel"` for a section in the home key's parallel mode, API 1.24, R-521, `"major"` for a return in the home tonic's major key: R-517, the `B1` of the Andante and rondo forms, or, API 1.25, R-525, `"home"` for a returning section at home in the home mode: R-524, the `B1` and `C3` of `sonatina`); API 1.22, R-501. Only the sections of `figured-variations` have a `figure` or a `"parallel"` mode. The role `elaboration` (label `Elaboration`, E of the sonatinas, API 1.25) is a role of a form with sizes like the others. A section also has `independent` and `lyric` (booleans, every section; API 1.23, R-510): `independent` is true for a transition made from a motive of its own, `lyric` for a subordinate theme whose motive is the main theme's augmented; both are false for every built-in form's sections but the lyric `S2` of `main-and-subordinate`.
| `roles`, `loop_roles`, `cadences`, `default_ending` | for a form whose sections the request gives: the roles a section may have (`{"value", "label", "required"}`, only `body` required), the roles that are the loop, the cadence a section gets when the request names none (`{"default", "last": [{"roles", "cadence"}]}`) and the section a request with no ending gets (`{"id", "letter", "role", "bars", "cadence"}`); `[]`, `[]`, null and null for a form with sizes. |

Schema: `schema/api/forms.schema.json`. A piece of any form but the game loop is not composed yet: the forms are listed here, the piece entries take them in P45 B2.

## Validation: `composer_api_validate(context, kind, request_json, out)`

Reads a request as its entry does and composes nothing, so an application can check a form and show the Composer's defaults. `kind`: `motive` (the request of `composer_api_motive`), `theme` (of `composer_api_theme`), `piece` (the piece request object of `docs/piece-request.md`, not the `{"request", ...}` wrapper of `composer_api_piece`) and `random_motive` (of `composer_api_random_motive`). `json`: `{"request": <the request with the Composer's defaults filled>}`:

- `motive`: `meter` (the motive's, when the request has none and the input declares one), `role` (`melodic`), `input.format` (`auto`) and `input.form` (`full`) when absent; `harmony` as `[{"numeral", "onset"}]`, the numerals as the library writes them (`V7`, `bVII65`) and the onsets in beats of the meter in the request syntax (`"0"`, `"3/2"`). Own harmony takes upper case numerals with an optional figure and `@onset` (`I V@3/2 V7@3`); a slash chord or a lower case numeral is refused at `/harmony`.
- `piece`: the `cadence` of every section that has none (`piece_default_cadences`: the last of the intro and of the loop `HC`, the last ending `PAC`, any other `IAC`) and the default ending section (`end`, `PAC`, an empty fill) when the request has no ending.
- `random_motive`: the `seed` (the request's, or the one read from the clock) and the `material` (an id) and `meter` a `"random"` drew from it (R-269).
- `theme`: the request as it is, once its plan is built without seeding. A request that Compose would refuse before it seeds (the pairing, the bars, the construction, the cadences, a motif that is too long or does not suit its place, an accompaniment or voicing that is not valid, a given chord that cannot arrive the cadence) is refused with `URSATZ_ERR_UNSATISFIABLE`, the reason and the path of its field: [theme-request.md](../theme-request.md#refusals-before-composing). One refusal is given at a time, the first.

A request an entry would refuse is refused the same way, with its message and path. An unknown `kind` is `URSATZ_ERR_REQUEST` with the path `/kind`. A style model named in the request is read.

## API 2.0: materials

API 2.0 (R-597) names the material of a key by its id (`omt.material.major` or `omt.material.natural-minor`, a K1 entry) where API 1 said `mode` (`"major"` or `"minor"`). A reply changes in these members, and nothing else in it does; the text members (`key`, `key_heard`, `rendered.key`, the labels) keep their words:

| Reply | 1.x | 2.0 |
| --- | --- | --- |
| `motive.key`, `analysis.key` | `{tonic_pitch_class, mode}` | `{tonic_pitch_class, material}` |
| `brief` of a theme, a piece and its variations | `{mode, from}` | `{material, from}` |
| `drawn` of a random motive | `{mode, meter}` | `{material, meter}` |
| `changes` of a section's row | `mode` | `material` (an id or `null`) |
| `schemas[]` of `list_schemas` | `modes` | `materials` |
| vocabulary | `modes`, `piece_modes`, `pitch_class_names[] {value, major, minor}` | `materials`, `piece_materials`, `pitch_class_names[] {value, names}` |

A request that names `"api_version": 1` is refused at `/api_version` (`api_version must be 2`): its client reads `mode`. A client written for 1.x reads `material` and maps the id to its word through the vocabulary's `materials` (`value` to `label`).

## API 2.1: MIDI and MusicXML layout

API 2.1 (R-599, R-601) changes the layout of the MIDI file and the MusicXML file every entry writes, and the plan the replies carry. The notes are the same (the same channel, ticks and key in the MIDI file; the same measures, onsets, durations, spellings and ties in the MusicXML), so a client that reads notes is unaffected; one that reads the layout changes:

| File | 2.0 | 2.1 |
| --- | --- | --- |
| MIDI | track 0 and one track per lane voice, named `melody`, `bass`, `inner_voice_<n>`, on channels 0, 1, 2 and so on | track 0 and one track named `Piano`, a channel for each lane voice in the order they first sound, each with a program change (the piano); `plain_midi` still leaves out the names and the section markers |
| MusicXML | one part per lane voice (`melody`, `bass`, `inner_voice_<n>`) | one part `piano` named `Piano`, of two staves when a line is on the lower staff: the melody on staff 1, the bass on staff 2, an inner voice by the mean of its notes; each note's `xml:id` is `piano-n<plan note id>` |
| `plan` (`plan_json`) | plan format 3 | plan format 4: every note has an `id`, and a `source` and `operation` where the Composer knows one; `next_note_id` ([plan-format.md](../../../library/docs/modules/plan-format.md#note-ids)) |

The GUI's playback lists a part per line as before (the Analyzer reads one stream per track and channel), and its score is one piano part on a grand staff. `composer_api_versions` and the vocabulary report `"api_version":"2.1"` and `"plan_format_version":4`. The layout is documented in [export.md](../export.md).
