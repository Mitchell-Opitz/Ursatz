# Forms as documents

A form is a JSON document (R-416). It says which sections a short piece has, how long each is, which keys the piece may
move to, which cadences close the sections, which sections return an earlier one, and which checks of the compose run
apply. One builder, `form_build_plan` (`<ursatz_composer/piece.h>`), makes the plan of every form from its document, so a
new form is a file in `forms/` and a line in `src/CMakeLists.txt`, not new code. The game loop is one of the forms, its
sections given by the piece request ([The game loop](#the-game-loop)). The documents are read by
`form_spec_parse` (`<ursatz_composer/form_spec.h>`); `schema/form.v1.schema.json` describes them. Back to the
[docs index](README.md); pieces and the compose run are in [piece.md](piece.md).

## The document

```json
{
  "schema": "ursatz-form",
  "version": 1,
  "name": "rounded-binary",
  "label": "Rounded binary",
  "closing": true,
  "regions": [{"name": "away", "major": [5], "minor": [3]}],
  "sizes": [
    {"bars": 16, "default": true, "sections": [
      {"name": "A", "letter": "A", "role": "theme", "bars": 8, "beginning": "presentation", "ending": "continuation",
       "cadence": "perfect-authentic", "closed": true, "keys": {"end": "away"}},
      {"name": "B", "letter": "B", "role": "contrast", "bars": 4, "beginning": "compound_basic_idea", "ending": "continuation",
       "cadence": "half", "motive": "invert tonal=1", "keys": {"start": "away"}},
      {"name": "A'", "letter": "A'", "role": "return", "bars": 4, "beginning": "presentation", "ending": "continuation",
       "cadence": "perfect-authentic", "returns": "A", "variation": "exact"}
    ]},
    {"bars": 24, "sections": ["... the same three at 8 bars each ..."]}
  ]
}
```

### The form

| Member | Meaning | Default |
| --- | --- | --- |
| `schema`, `version` | `"ursatz-form"`, `1`. | required |
| `name` | The form's name, `[a-z0-9-]`, 1 to 31 characters. The plan's form node is named so. | required |
| `label` | A name for people. | required |
| `plan_name` | The plan's form node name when it is not `name`. | `name` |
| `closing` | The piece ends with a cadence. With `modulate`, the cadences across the sections are planned (`piece_plan_cadences`) and checked (`piece_check_cadences`). | `false` |
| `accompaniment` | `"optional"`, or `"required"`: every section has an accompaniment, pattern `any` when the request has none (layered). | `"optional"` |
| `regions` | The keys a piece may move to: a `name` and the `major` and `minor` degrees of the home key a key is drawn from (`harmony_related_key_of`), 1 to 7, up to 8 of each. At most 4 regions. A region is not named `home`. | `[]` |
| `checks` | Which of the compose run's checks run: `"sections"` (each theme section heard as its shape), `"returns"`, `"keys"`, `"cadences"`, `"seam"`. | every check |
| `sizes` | The sizes the form comes in, at most 8. Each has `bars` (the sum of its sections, checked), an optional `count` (the variations or layers), at most one `"default": true` and exactly one is required, and `sections` (at most 9). Two sizes may not have the same `bars` and `count`. | required |
| `sections` | `"request"`: the piece request gives the sections (the game loop). A form with this has no `sizes`, and has `roles`, `loop` and `cadences`. | absent |
| `roles` | A form whose sections the request gives: the roles a section may have, in the order `intro`, `body`, `turnaround`, `ending`, each once, and including `body` (the reader refuses other roles for such a form until P45). | required with `"sections": "request"`, else refused |
| `loop` | `{"roles": [...]}`: the roles that are the loop, each one of `roles`. The loop's end is held for the seam stage and a seam bar follows it; the ending version replaces the loop's last role with the ending. | required with `"sections": "request"`, else refused |
| `cadences` | `{"default": name, "last": [{"roles": [...], "cadence": name}, ...]}`: the cadence of a section the request gives none. `last` is applied in order (at most 4 rules): the rule whose roles hold the section as the last section of one of them gives its cadence, else `default`. Names are cadence vocabulary names whose core is a PAC, an IAC or an HC. | required with `"sections": "request"`, else refused |
| `default_ending` | `{"id", "letter", "role", "bars", "cadence"}`: the section a request with no ending gets; `role` is `ending`. | none |

### A section of a size

| Member | Meaning | Default |
| --- | --- | --- |
| `name` | The section node's name, unique in the size, 1 to 31 characters. | required |
| `letter` | Its form letter, 1 to 3 characters. | required |
| `role` | One of `theme`, `contrast`, `return`, `variation`, `layer`, `intro`, `body`, `turnaround`, `ending` (the role table in `form_spec.c`; later phases add rows). | required |
| `bars` or `beats` | Its length, whole bars or whole beats of the meter: exactly one. Until P47 a section of a sized form must be a theme grow-theme builds (a theme-like role, 4 or 8 whole bars), else the build is `PIECE_ERR_BAD_REQUEST`. | required |
| `beginning`, `ending` | The theme shape, by the plan names: `presentation`, `antecedent`, `compound_basic_idea`; `continuation`, `consequent`, `cadential` ([grow.md](grow.md)). | required |
| `cadence` | A name of the standard-harmony cadence vocabulary (`harmony_cadence_kind_find`). The plan holds its core type, which must be a PAC, an IAC or an HC. | required |
| `closed` | It may close in the home key with a PAC before the piece ends. | `false` |
| `keys` | `{"start", "end"}`: `"home"` or the name of a region. | both `"home"` |
| `motive` | How its motive is made from m0, as `motive_relation_step_text` writes it (`"invert tonal=1"`, `"diminish"`, `"interval-change"`). | m0 itself |
| `returns`, `variation` | The name of an earlier section it returns, and how it varies it (`exact`, `ornament`, `reharmonize`, `reregister`). A returning section with no `motive` takes its source's. `variation` needs `returns`. | none; `exact` |
| `layers` | The lanes that sound in it: `"all"`, or a list of `bass`, `inner_voice`, `melody`. | all, no LAYERS constraint |
| `pedal` | `"dominant"`: its presentation is written over a pedal on the dominant (R-446). Refused for any other value, and for a section whose `beginning` is not `presentation` (`/sizes/<i>/sections/<j>/pedal`). The section gets a `pedal` constraint; grow keeps it only when the section starts in the home key with no Given material in its first half. No built-in form uses it yet. | none, no PEDAL constraint |

The reader refuses anything it does not know, naming the member by its JSON pointer (`FormSpecError.path`, for example
`/sizes/0/sections/2/returns`): an unknown member at any level, a wrong type, a missing required member, `schema` or
`version` not as above, a name outside `[a-z0-9-]{1,31}`, an unknown role, beginning, ending, cadence name, region,
variation, check or lane, a cadence whose core is not a PAC, an IAC or an HC, `bars` and `beats` together or neither, a
section name repeated in a size, `returns` naming a later or unknown section, `variation` without `returns`, two default
sizes or none, a size's `bars` not the sum of its sections, two sizes of the same `bars` and `count`, and more sizes,
sections or regions than the limits (`FORM_SPEC_MAX_SIZES`, `FORM_SPEC_MAX_SECTIONS`, `FORM_SPEC_MAX_REGIONS`). For a form
whose sections the request gives it also refuses `roles` that are not the four parts in order (`/roles/<i>`) or have no
body, a `loop` or cadence rule role that is not one of the form's `roles` (`/loop/roles/<i>`,
`/cadences/last/<i>/roles/<j>`), a missing `roles`, `loop` or `cadences`, and `roles`, `loop`, `cadences` or
`default_ending` in a form that has sizes.

## Choosing a size

`form_spec_size(form, bars, count)` gives the size a request asks for:

- `bars` 0 and `count` 0: the default size.
- A `count` only for a form whose sizes have counts. It picks the size with that count, and `bars`, when given, must
  match.
- `bars` alone picks the size with those bars.

Any other request has no size and `piece_build_plan` returns `PIECE_ERR_BAD_SIZE`.

## Keys and cadences

`piece_plan_keys` draws the keys from the document. A piece with a section that starts or ends in a region modulates in
two seeds of three: it draws `fill_rng_below(rng, 3)` (0 stays home), then one key for each region in the order the form
lists them, from the region's degrees by the home mode. Each section then starts and ends in the key its `keys` name.
`piece_plan_cadences` reads `closing` and each section's `closed` ([piece.md](piece.md#keys-and-cadences)).

## Checks

`compose_run` makes the checks the form's `checks` names: hearing each theme section as its shape (`hear`), hearing the
returns, checking the keys and the cadences of a plan with several keys, and hearing the seam. `ComposeConfig.form` gives
the form; NULL is the built-in form the plan's root is named for (`form_spec_builtin_for_plan`), and a plan of no
built-in form, such as a theme or a fill plan, runs every check it has the parts for. A check never changes what is
written: the key check only rejects a finished run. With a check off, its result is empty (`returns_planned` is 0 with
`returns` off).

## The built-in forms

`forms/<name>.json`, embedded into the library at build time by `cmake/EmbedForms.cmake` (one byte array for each file in
`COMPOSER_FORM_DOCUMENTS`). `form_spec_builtin_count`, `form_spec_builtin_name`, `form_spec_builtin_document` and
`form_spec_builtin` list and read them in registry order, which is the order of `COMPOSER_FORMS` in `src/CMakeLists.txt`.
No built-in names `checks`, so each runs every check. The game loop is the last, and the only one whose sections the
request gives.

| Form | Sizes (default marked) | Sections | Regions and keys |
| --- | --- | --- | --- |
| `binary` | 8, **16** | A, B: presentation + continuation, PAC, half the bars each; B's motive is m0 inverted | `away` V (major) or III (minor): A ends in it, B starts in it. Closing. |
| `rounded-binary` | **16**, 24 | A 8 (presentation + continuation, PAC, closed), B 4 or 8 (compound basic idea + continuation, HC, inverted motive), A' 4 or 8 (returns A, exact, PAC) | `away` as binary. Closing. |
| `ternary` | 12, **24** | A (antecedent + consequent, PAC, closed), B (presentation + continuation, PAC, inverted motive), A return (returns A, exact), a third of the bars each | `away` IV, vi or V (major) or III, iv or VI (minor): B ends in it. Closing. |
| `variations` | 16 to 72, count 1 to 8, count **3** | The theme (antecedent + consequent, PAC) and n variations of 8 bars, each returning the theme, every section closed; variation k is `ornament` with a diminished motive (k odd) or `reharmonize` with an interval-changed motive (k even) | `away` vi (major) or III (minor): with three variations or more, the section at index `(count + 1) / 2` (the theme is 0) starts and ends in it. Closing. |
| `loop` | **8**, 16 | 8: one section (presentation + continuation, HC) so the end leads back to the first bar; 16: A (IAC), B (HC, inverted motive) | None. |
| `layered` | 16, 24, **32**, count 2 to 4, count **4** | Layer 1 (presentation + continuation, HC), each later layer returning it, exact. The masks build up: layer 1 the bass, layer 2 the bass and the accompaniment, layer 3 adds the melody, the last layer everything. `accompaniment` is `"required"`. | None. |
| `game-loop` | none: the request gives the sections | Intro, body, turnaround and ending sections of any length in beats ([The game loop](#the-game-loop)); `plan_name` is `structure` | None. Not closing. |

## Adding a form

1. Write `forms/<name>.json` (the file's name is the form's `name`).
2. Add `<name>` to `COMPOSER_FORMS` in `src/CMakeLists.txt`.
3. A piece of the form is `piece_build_plan` with `PieceRequest.form` set to the name (a form that comes in sizes), and through the API a
   piece request of `"schema_version": 2` with `"form": "<name>"` composes, regenerates and varies it with no further code
   ([piece-request.md](piece-request.md#a-piece-of-another-form-schema-version-2)). A form that is not built in is
   built with `form_build_plan(form, request, &plan)` from a parsed document and composed with `ComposeConfig.form` set:
   `tests/fixtures/forms/bar-form.json` (A, A' returning A, B in 24 bars) is composed that way by `test_form_spec custom`,
   and no C code names it.

The API serves the forms (`composer_api_forms`, API 1.13, [api/vocabulary-and-validation.md](api/vocabulary-and-validation.md#forms-composer_api_formsout)):
their sizes, sections, cadences, keys and theme slots. The piece entries compose any of them from a piece request of version 2
(API 1.14); a form with sizes has the one version `piece`, and the game loop keeps its loop and ending versions.

## The game loop

The game loop (`forms/game-loop.json`, R-419) is the form whose sections the piece request gives, not the document: an
intro, a body, a turnaround and an ending of any length in beats. Its document has `"sections": "request"` and the members
`roles`, `loop`, `cadences` and `default_ending` of the table above. `form_build_plan` builds it as it builds every other
form (`form_build_request.c` lays out the request's sections), `piece_build_ending` builds the ending version, and
`piece_request_check`, `piece_default_cadences` and `piece_default_ending` check a request and apply the document's
cadences and ending. [piece-structure.md](piece-structure.md) has the request, the fills, the plan and what does not
compose. A form whose sections the request gives takes no sizes and no `piece_plan_keys`: the game loop never modulates.

## Tests

| Test | What it checks | Label |
| --- | --- | --- |
| `test_form_plans_templates` | The plan of every built-in form at every size, in C major 4/4 and A minor 3/4, with an invented motive, without `modulate` and with it and seeds 1 to 5, hashes as `tests/fixtures/forms/template-plans.txt` records them (R-417). `test_form_plans record` writes the file. | |
| `test_form_plans_pieces` | The loop plan of each piece request in `tests/fixtures/piece` (loop and ending, progression fill, structure only, theme fill), hashes as `tests/fixtures/forms/piece-plans.txt` records them (R-417). The ending plan is pinned by the `piece-loop-and-ending.*` snapshots. `test_form_plans record-pieces` writes the file. | |
| `test_form_spec` | Every built-in parses and is as the table says; each refusal of the reader with its path; `form_spec_size` for every size, count and bad size of each built-in; the game loop document, the refusals of its members and the default cadences for each layout of parts. | |
| `test_piece_structure_*`, `test_loop_seam_*`, `test_piece_fills_*` | The game loop's checks, defaults, plans and composed runs, built as a `PieceRequest` of the form `game-loop`. | |
| `test_form_spec_custom` | A form added as a document alone composes (seeds 1 to 10); with `checks` set to `sections` alone the same seed writes the same plan and hears no return. | |
| `test_form_templates_*` | The plan's structure at every size against a table written in the test, and the compose sweeps ([piece.md](piece.md#tests-and-measured-results)). | `compose` and `heard` are `slow` |
