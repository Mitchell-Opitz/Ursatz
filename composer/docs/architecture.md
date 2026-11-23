# Composer architecture

What the Ursatz-Composer is, how its parts fit and where each is documented. Back to the [docs index](README.md).

## Purpose

- A seeded, reproducible composing partner: a piece is a plan that is filled top-down, bottom-up or middle-out.
- It holds plan structure and composing decisions only; theory comes from the Ursatz frameworks and what a listener hears comes from the library chain (`library/chain`, R-618).
- Every result carries its provenance: the seed, the inputs and the generators that produced it.
- It is a C11 static library (`ursatz_composer`) behind one public header, plus a CLI that is a client of the same API.

## Where it sits

| Direction | Part | How it is linked |
|---|---|---|
| Below | The library in [`library/`](../../library/docs/architecture.md) (kernel and frameworks) | target `ursatz`, linked PRIVATE |
| Sibling | The Analyzer in [`analyzer/`](../../analyzer/docs/architecture.md) | none: the Composer links no Analyzer target and includes no Analyzer header (R-620); only the CLIs and the GUI call both |
| Vendored | cJSON 1.7.18 | `third_party/cjson`, added by the root before the parts |
| Above | The GUI in [`gui/`](../../gui/docs/architecture.md) | links `ursatz_composer` and includes only `composer_api.h` |

All four are built by the root `CMakeLists.txt`, in that order. The Composer hears through `library/chain` (`ursatz_chain`).

## Module map

| Directory | What it holds | Doc |
|---|---|---|
| `include/ursatz_composer/` | The one public header, `composer_api.h` | [api.md](api.md) |
| `src/api/` | The composer API behind the header, one file for each request kind | [api.md](api.md) |
| (`library/plan/`) | The piece plan is the library's (R-625): `Plan`, its lanes, states, section descriptions, form record, constraints (`plan_constraint_kinds.c`: one row per kind holds its name, validity, writer and reader), edits (`plan_clone`, `plan_lane_span_reopen`), the producer stamp and reader hooks of the JSON codec. The Composer writes plans with `composer_plan_producer` and reads them with `composer_plan_read_hooks` | [plan.md](../../library/docs/modules/plan.md), [plan-format.md](../../library/docs/modules/plan-format.md) |
| `src/<module>/` | One folder for each module of the layers below (R-636) | [structure.md](structure.md), [Layers](#layers) |
| `forms/` | The built-in forms, one JSON document each, embedded at build time by `cmake/EmbedForms.cmake` | [forms.md](forms.md) |
| (`cli/` of the repository) | The command line: the `ursatz` program, which calls the Composer through the workshop | [cli docs](../../cli/docs/README.md) |
| `schema/` | The plan schema and the API request and response schemas | [plan-format.md](../../library/docs/modules/plan-format.md) |

The full table with every directory is in [structure.md](structure.md).

## Layers

The Composer's sources are 18 modules, one folder each under `src/` (R-636). A module includes only the ones below it; `cmake/layer-table.txt` lists each module's `uses` row and the test `layer_table` fails on an include upward or between siblings that the row does not allow. Every module also uses the library (`model`, `theory`, `realization`, `plan`, `reading`, `analysis`, `chain`, `io`) and cJSON. The plan is the library's; the hearing gives readings (`check/hearing.c`: a reading for each heard span, over the performer notes of the rendered work, R-633).

| Module | Holds | May use |
|---|---|---|
| `util` | shared helpers: positions, shaping bits, jobs, JSON, the composition key (`composer.c`) | the library |
| `structure` | plan queries (`plan_util`), request parsing, locks and dependents, fill targets, the form documents and the form record, theme shapes | `util` |
| `fill` | the fill loop, its operators, seeds, candidates, suggestions and records | `structure`, `util` |
| `render` | rendering a plan to events, making a work of it, MIDI and MusicXML | `structure`, `util` |
| `style` | style models, blending, labels | `fill`, `structure`, `util` |
| `harmony` | the harmony operators, the chord masks, the seam hearing | the five above |
| `motive` | motives, their development, random motives, the variant registry | `harmony` and the five above |
| `melody` | the melody operator | `harmony`, `motive` and the five above |
| `texture` | the accompaniment and four-part voicing operators | `harmony`, `melody` and the five above |
| `counterpoint` | the counter-melody operator | `harmony`, `texture` and the five above |
| `material` | the returns, variations and figures of a section, given material, connectives, the seam | `harmony`, `melody` and the five above |
| `grow` | growing a theme from a motive | `harmony`, `motive`, `melody`, `material` and the five above |
| `check` | hearing (readings), the checks (`analyzer_check`, form, key and return hearing), the style check and measure | `grow`, `harmony`, `melody`, `motive`, `texture` and the five above |
| `critic` | the critic ([critic.md](critic.md), R-719): the melody line of a candidate, eight evaluators of her listening notes and their score | `motive` and the five above |
| `piece` | the compose run, the piece builders, performing, regenerating, the stages of `grow` | `check`, the operators, `grow` and the five above |
| `candidates` | candidates and variations of a plan | `piece` and the five above |
| `explain` | the explanation of a run | `piece`, `check`, `grow` and the five above |
| `request` | the API entries and the preferences document, with its key table (`composer_preferences_keys`: one row per setting, hard or soft) and rule table (`composer_preferences_rules`), which the vocabulary serves as `preference_keys` and `rules` and the workshop reads at open to resolve settings | every module and `composer_api.h` |

A header in `src/include/ursatz_composer/` belongs to the module whose files implement it. The five lowest modules (`util`, `structure`, `fill`, `render`, `style`) are the core; `harmony` to `material` are the operators.

## Public API

All entries are in `include/ursatz_composer/composer_api.h`; the entry index is [api.md](api.md#entries).

| Header | Route or entry | What it does | Doc |
|---|---|---|---|
| `composer_api.h` | `composer_api_versions`, `composer_api_vocabulary`, `composer_api_validate` | Versions, the vocabularies and limits, and request validation without composing | [api/vocabulary-and-validation.md](api/vocabulary-and-validation.md) |
| `composer_api.h` | `composer_api_export`, `composer_api_motive`, `composer_api_motive_from_midi`, `composer_api_motive_render` | Export a plan, analyze and render a motive | [api/motives.md](api/motives.md) |
| `composer_api.h` | `composer_api_variant_operations`, `composer_api_variants`, `composer_api_recognize`, `composer_api_random_motive` | Variants, recognition and random motives | [api/motives.md](api/motives.md) |
| `composer_api.h` | `composer_api_theme`, `composer_api_piece`, `composer_api_piece_variations` | Grow a theme, compose a piece, vary its lanes | [api/theme-and-piece.md](api/theme-and-piece.md) |
| `composer_api.h` | `composer_api_plan`, `composer_api_fill`, `composer_api_lock`, `composer_api_regenerate`, `composer_api_variations`, `composer_api_harmonize` | The plan workflow | [api/plan-workflow.md](api/plan-workflow.md) |
| `composer_api.h` | `composer_api_progression`, `composer_api_counterpoint`, `composer_api_develop`, `composer_api_grow` | The material commands | [api/material-commands.md](api/material-commands.md) |
| CLI | `export`, `progression`, `motive`, `develop`, `variants`, `recognize`, `random-motive`, `counterpoint`, `grow`, `theme`, `piece`, `plan`, `fill`, `lock`, `regenerate`, `variations`, `harmonize` | One command for each entry, a client of the API | [cli.md](../../cli/docs/README.md) |

## Version constants and schemas

| Name | File | Current value | Doc |
|---|---|---|---|
| Composer version | `CMakeLists.txt` (`project(UrsatzComposer VERSION ...)`) | 0.4.1 | [changelog.md](changelog.md) |
| API version | `include/ursatz_composer/composer_api.h` (`COMPOSER_API_VERSION_MAJOR`, `_MINOR`) | 2.1 | [api.md](api.md#versioning) |
| Plan format version | `library/plan/plan_json.h` (`URSATZ_PLAN_FORMAT_VERSION`) | 5 | [plan-format.md](../../library/docs/modules/plan-format.md#format-versions) |
| Plan schema | `library/plan/schema/plan.v6.schema.json` | plan format 6 | [plan-format.md](../../library/docs/modules/plan-format.md) |
| API schemas | `schema/api/*.schema.json` | API 2.1 | [api.md](api.md) |
| Motive format | the motive document has no version member | none | [motive-format.md](motive-format.md) |
| Theme request schema | `docs/theme-request.md` (`schema_version`) | 2 | [theme-request.md](theme-request.md) |
| Vocabulary and limits | `composer_api_vocabulary`, member `limits` (`seed_max`, `tempo_min`, `tempo_max`, `piece_sections`, `piece_beats`, `preferences`) | `seed_max`: variants 4294967231, others 4294967295 | [api/vocabulary-and-validation.md](api/vocabulary-and-validation.md) |

## What it calls below

| Entry used | From which repo | For what |
|---|---|---|
| `imported_score_view` (`<io/imported_score.h>`), `analysis_chain_run_score`, `analysis_chain_result` (`<chain/analysis_chain.h>`), `chain_result_voice_faults` (`<chain/chain_result.h>`), `chain_style_distance` (`<chain/chain_style.h>`) | [library chain](../../library/chain/README.md) | Hearing one render of the plan: cadences, key regions, form, faults, style distance |
| `analysis_chain_producer_key` (`<chain/analysis_chain.h>`) | [library chain](../../library/chain/README.md) | The producer key written into a plan and returned by `composer_api_versions` as `ursatz_commit` (R-620) |
| `theory/style/style_labels.h`, `style_compare.h`, `style_model_file.h`, `style_reference.h` | library theory (`library/theory/style`) | Style models the harmony and melody stages prefer, their category names and the distance of a heard piece from one |
| `<standard-harmony/...>`, `<standard-counterpoint/...>`, `<standard-voicing/...>`, `<standard-texture/...>`, `<standard-tonality/...>`, `<standard-form/...>` | [Ursatz](https://github.com/Mitchell-Opitz/Ursatz/blob/main/docs/architecture.md) | Theory rules: phrase model, melodic moves, voicing, scales, phrase form |
| `<analysis/motive_sketch.h>`, `<analysis/motive_relation.h>` | [Ursatz](https://github.com/Mitchell-Opitz/Ursatz/blob/main/docs/architecture.md) | The motive value and the relation steps that make a motive from another |
| `<realization/work.h>`, `<realization/movement.h>`, `<realization/piano_reduction.h>`, `<io/movement_export.h>`, `<theory/knowledge/instrument.h>` | [Ursatz](https://github.com/Mitchell-Opitz/Ursatz/blob/main/library/docs/modules/realization.md) | A rendered plan as a work of one movement, its lines assigned to one piano part (the K12 piano and solo-piano ensemble of the call's knowledge snapshot), and its MIDI and MusicXML (`render/movement_build.c`, `render/export.c`) |
| `<core/rational.h>`, `<time/...>`, `<pitch/...>`, `<semantics/...>`, `<io/midi_importer.h>` | [Ursatz](https://github.com/Mitchell-Opitz/Ursatz/blob/main/docs/architecture.md) | Exact time, pitch spelling, cadence and key types, MIDI import |

The rules for what comes from which library are in [building.md](building.md#library-primitives).

## Data flow

One `composer_api_theme` request, from the reply's side:

1. `composer_api_theme` parses the JSON text and checks `schema` and `schema_version`; a bad member is refused with its JSON path ([theme-request.md](theme-request.md)).
2. The request is read into motives, slots, ideas, chords and the brief.
3. `grow_build_plan_checked` plans the theme's structure (sentence, period or hybrid) as a piece plan with one section ([grow.md](grow.md)).
4. `grow_run` runs a compose run over that section: the stages write the motives, harmony, bass and melody, each through the fill loop with its seed, retries and records ([fill-loop.md](fill-loop.md), [grow-harmony.md](grow-harmony.md)).
5. With `check` set, the Analyzer check renders the plan once and hears the section through the library chain (`analysis_chain_run_score`); a planned cadence or key not heard, or a hard voice-leading fault, rejects the run ([analyzer-check.md](analyzer-check.md)).
6. `form_hear_in` reports the form heard in the theme, kept in the outcome without rejecting ([form-hear.md](form-hear.md), [grow-hearing.md](grow-hearing.md)).
7. The plan is rendered for MIDI and MusicXML when the request asks: the render becomes a work of one movement whose lines are all assigned to one piano part (R-599), and the library's movement export writes the file ([export.md](export.md)).
8. The reply is a `ComposerResponse`: the theme document in `json`, the plan, the MIDI bytes and the MusicXML text; the caller frees it with `composer_response_destroy` ([api.md](api.md#memory)).

## The chord context (R-728)

A chord's tones are read from the knowledge snapshot's chord types, not from tables of the Composer's own. `ComposerChords` (`harmony_chords.h`) holds the snapshot and the common-practice materials resolved from it (`composer_chords_init`). `compose_run` builds one per run from `ComposeConfig.knowledge` and `api_progression` one per call from its `ComposerContext`. Each operator is registered with it as the last argument (`harmony_phrase_register`, `harmony_bass_register`, `melody_fill_register`, `accompaniment_register`, `voicing_register`, `form_return_register` and the rest), and `composer_register_operator` keeps it, with the operator's params, as the context the registry frees. The readers an operator calls (the phrase request, the bass, skeleton, melody, accompaniment and voicing contexts, the heard phrase) carry it as `composer_chords`; NULL means the library's fallback tables, which the tests pin to the entries. `harmony_diatonic_intervals`, `melody_chord_mask`, `harmony_chord_bass_pc` and `accomp_chord_tones` are the readers of a chord's semitones and tones. A `Motive` carries no snapshot, so `motive_analyze` reads the fallback tables.

## Finding things

Every doc is listed in [docs/README.md](README.md).
