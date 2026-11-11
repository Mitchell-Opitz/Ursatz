# Composer architecture

What the Ursatz-Composer is, how its parts fit and where each is documented. Back to the [docs index](README.md).

## Purpose

- A seeded, reproducible composing partner: a piece is a plan that is filled top-down, bottom-up or middle-out.
- It holds plan structure and composing decisions only; theory comes from the Ursatz frameworks and what a listener hears comes from the Analyzer.
- Every result carries its provenance: the seed, the inputs and the generators that produced it.
- It is a C11 static library (`ursatz_composer`) behind one public header, plus a CLI that is a client of the same API.

## Where it sits

| Direction | Repo | How it is pinned |
|---|---|---|
| Below | [Ursatz](https://github.com/Mitchell-Opitz/Ursatz/blob/main/docs/architecture.md) (kernel and frameworks) | git submodule `external/ursatz` (`49a5523`), linked PRIVATE |
| Below | [Ursatz-Analyzer](https://github.com/Mitchell-Opitz/Ursatz-Analyzer/blob/main/docs/architecture.md) (hearing, styles, corpus) | git submodule `external/ursatz-analyzer` (`4374060`), linked PRIVATE |
| Vendored | cJSON 1.7.18 | `external/cjson`, added before the submodules |
| Above | [Ursatz-GUI](https://github.com/Mitchell-Opitz/Ursatz-GUI/blob/main/docs/architecture.md) | links `ursatz_composer` and includes only `composer_api.h` |

The pins are checked by `scripts/check-pins.sh` (see [building.md](building.md#submodules)).

## Module map

| Directory | What it holds | Doc |
|---|---|---|
| `include/ursatz_composer/` | The one public header, `composer_api.h` | [api.md](api.md) |
| `src/api/` | The composer API behind the header, one file for each request kind | [api.md](api.md) |
| `src/plan/` | The piece plan, its lanes, states, constraints and JSON | [plan-format.md](plan-format.md) |
| `src/render/` | Rendering a plan to NoteEvents, MIDI and MusicXML | [export.md](export.md) |
| `src/fill/` | The fill loop, its operators, seeds and records | [fill-loop.md](fill-loop.md) |
| `src/harmony/` | The harmony operators and hearing cadences | [grow-harmony.md](grow-harmony.md) |
| `src/melody/` | The melody operator | [melody-fill.md](melody-fill.md) |
| `src/preferences/` | The preferences document and the settings it holds | [preferences.md](preferences.md) |
| `src/motive/` | Motives, their analysis, development and the variant registry | [motive-format.md](motive-format.md), [variant-registry.md](variant-registry.md) |
| `src/form/` | Hearing the phrase form through standard-form; the form documents: `form_spec.c` reads them, `form_builtin.c` serves the built-in ones | [form-hear.md](form-hear.md), [forms.md](forms.md) |
| `forms/` | The built-in forms, one JSON document each, embedded at build time by `cmake/EmbedForms.cmake` | [forms.md](forms.md) |
| `src/counterpoint/` | The counter-melody operator, species 1 to 4 | [counterpoint.md](counterpoint.md) |
| `src/texture/` | The accompaniment and four-part voicing operators | [accompaniment.md](accompaniment.md), [voicing.md](voicing.md) |
| `src/piece/` | The one piece builder (`form_build.c`; `form_build_request.c` lays out the sections of a form the request gives, the game loop), keys and cadences from a form, the piece plan and the compose run | [piece.md](piece.md) |
| `src/grow/` | Growing a theme from a motive; `theme_shape.c` is the table of what a theme of each beginning and ending is made of (ideas, slots, roles, places, lengths) | [grow.md](grow.md) |
| `src/check/` | Hearing a render through the Analyzer, the check loop, the explanation | [analyzer-check.md](analyzer-check.md), [explain.md](explain.md) |
| `src/style/` | Style models, blending, style checks, seeding from a corpus | [style.md](style.md) |
| `src/util/` | Private helpers the sources share | [structure.md](structure.md) |
| `cli/` | The `ursatz-composer` command | [cli.md](cli.md) |
| `schema/` | The plan schema and the API request and response schemas | [plan-format.md](plan-format.md) |

The full table with every directory is in [structure.md](structure.md).

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
| CLI | `export`, `progression`, `motive`, `develop`, `variants`, `recognize`, `random-motive`, `counterpoint`, `grow`, `theme`, `piece`, `plan`, `fill`, `lock`, `regenerate`, `variations`, `harmonize` | One command for each entry, a client of the API | [cli.md](cli.md) |

## Version constants and schemas

| Name | File | Current value | Doc |
|---|---|---|---|
| Composer version | `CMakeLists.txt` (`project(UrsatzComposer VERSION ...)`) | 0.4.1 | [changelog.md](changelog.md) |
| API version | `include/ursatz_composer/composer_api.h` (`COMPOSER_API_VERSION_MAJOR`, `_MINOR`) | 1.17 | [api.md](api.md#versioning) |
| Plan format version | `src/include/ursatz_composer/plan_json.h` (`URSATZ_COMPOSER_PLAN_FORMAT_VERSION`) | 2 | [plan-format.md](plan-format.md#format-versions) |
| Plan schema | `schema/plan.v2.schema.json` | plan format 2 | [plan-format.md](plan-format.md) |
| API schemas | `schema/api/*.schema.json` | API 1.17 | [api.md](api.md) |
| Motive format | the motive document has no version member | none | [motive-format.md](motive-format.md) |
| Theme request schema | `docs/theme-request.md` (`schema_version`) | 2 | [theme-request.md](theme-request.md) |
| Vocabulary and limits | `composer_api_vocabulary`, member `limits` (`seed_max`, `tempo_min`, `tempo_max`, `piece_sections`, `piece_beats`, `preferences`) | `seed_max`: variants 4294967231, others 4294967295 | [api/vocabulary-and-validation.md](api/vocabulary-and-validation.md) |

## What it calls below

| Entry used | From which repo | For what |
|---|---|---|
| `analyzer_hear`, `analyzer_result_report`, `analyzer_result_violations`, `analyzer_result_chain` (`<ursatz_analyzer/composer_api.h>`) | [Ursatz-Analyzer](https://github.com/Mitchell-Opitz/Ursatz-Analyzer/blob/main/docs/composer-api.md) | Hearing one render of the plan: cadences, key regions, form, faults, style distance |
| `analyzer_version`, `analyzer_ursatz_commit` (`<ursatz_analyzer/version.h>`) | [Ursatz-Analyzer](https://github.com/Mitchell-Opitz/Ursatz-Analyzer/blob/main/docs/composer-api.md) | The versions written into a plan and returned by `composer_api_versions` |
| `analyzer_chord_sequence_free`, similar material and corpus readers (`<ursatz_analyzer/similar_material.h>`) | [Ursatz-Analyzer](https://github.com/Mitchell-Opitz/Ursatz-Analyzer/blob/main/docs/composer-api.md) | Seeding a piece from a corpus; staleness of analyzed documents |
| Style profile and labels (`<ursatz_analyzer/style_profile.h>`, `style_labels.h`) | [Ursatz-Analyzer](https://github.com/Mitchell-Opitz/Ursatz-Analyzer/blob/main/docs/composer-api.md) | Style models the harmony and melody stages prefer |
| `<standard-harmony/...>`, `<standard-counterpoint/...>`, `<standard-voicing/...>`, `<standard-texture/...>`, `<standard-tonality/...>`, `<standard-form/...>` | [Ursatz](https://github.com/Mitchell-Opitz/Ursatz/blob/main/docs/architecture.md) | Theory rules: phrase model, melodic moves, voicing, scales, phrase form |
| `<analysis/motive_sketch.h>`, `<analysis/motive_relation.h>` | [Ursatz](https://github.com/Mitchell-Opitz/Ursatz/blob/main/docs/architecture.md) | The motive value and the relation steps that make a motive from another |
| `<core/rational.h>`, `<time/...>`, `<pitch/...>`, `<semantics/...>`, `<io/midi_importer.h>` | [Ursatz](https://github.com/Mitchell-Opitz/Ursatz/blob/main/docs/architecture.md) | Exact time, pitch spelling, cadence and key types, MIDI import |

The rules for what comes from which library are in [building.md](building.md#library-primitives).

## Data flow

One `composer_api_theme` request, from the reply's side:

1. `composer_api_theme` parses the JSON text and checks `schema` and `schema_version`; a bad member is refused with its JSON path ([theme-request.md](theme-request.md)).
2. The request is read into motives, slots, ideas, chords and the brief.
3. `grow_build_plan` plans the theme's structure (sentence, period or hybrid) as a piece plan with one section ([grow.md](grow.md)).
4. `grow_run` runs a compose run over that section: the stages write the motives, harmony, bass and melody, each through the fill loop with its seed, retries and records ([fill-loop.md](fill-loop.md), [grow-harmony.md](grow-harmony.md)).
5. With `check` set, the Analyzer check renders the plan once and hears the section through `analyzer_hear`; a planned cadence or key not heard, or a hard voice-leading fault, rejects the run ([analyzer-check.md](analyzer-check.md)).
6. `form_hear` reports the form heard in the theme, kept in the outcome without rejecting ([form-hear.md](form-hear.md), [grow-hearing.md](grow-hearing.md)).
7. The plan is rendered for MIDI and MusicXML when the request asks ([export.md](export.md)).
8. The reply is a `ComposerResponse`: the theme document in `json`, the plan, the MIDI bytes and the MusicXML text; the caller frees it with `composer_response_destroy` ([api.md](api.md#memory)).

## Finding things

Every doc is listed in [docs/README.md](README.md).
