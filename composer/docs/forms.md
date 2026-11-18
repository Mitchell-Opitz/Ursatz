# Forms as documents

A form is a JSON document (R-416). It says which sections a short piece has, how long each is, which keys the piece may
move to, which cadences close the sections, which sections return an earlier one, and which checks of the compose run
apply. One builder, `form_build_plan` (`<ursatz_composer/piece.h>`), makes the plan of every form from its document, so a
new form is a file in `forms/` and a line in `src/CMakeLists.txt`, not new code. The game loop is one of the forms, its
sections given by the piece request ([The game loop](#the-game-loop)). The documents are read by
`form_spec_parse` (`<ursatz_composer/form_spec.h>`); `schema/form.v2.schema.json` describes them; its sections are the form use of the section description of the plan group ([library plan](../../library/docs/modules/plan.md), `library/plan/schema/section.schema.json`). Back to the
[docs index](README.md); pieces and the compose run are in [piece.md](piece.md).

## The document

```json
{
  "schema": "ursatz-form",
  "version": 2,
  "name": "rounded-binary",
  "label": "Rounded binary",
  "closing": true,
  "regions": [{"name": "away", "major": ["omt.relation.dominant"], "minor": ["omt.relation.mediant"]}],
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
| `schema`, `version` | `"ursatz-form"`, `2`. A version 1 document is refused at `/version` with `form documents are version 2`. | required |
| `name` | The form's name, `[a-z0-9-]`, 1 to 31 characters. The plan's form node is named so. | required |
| `label` | A name for people. | required |
| `plan_name` | The plan's form node name when it is not `name`. | `name` |
| `closing` | The piece ends with a cadence. With `modulate`, the cadences across the sections are planned (`piece_plan_cadences`) and checked (`piece_check_cadences`). | `false` |
| `modulates` | The key plan leaves home whatever the request says (R-493): `draw_keys` makes its home draw and does not stay. Needs `regions`; refused in a form whose sections the request gives. | `false` |
| `accompaniment` | `"optional"`, or `"required"`: every section has an accompaniment, pattern `any` when the request has none (layered). | `"optional"` |
| `regions` | The keys a piece may move to: a `name` and the `major` and `minor` lists of relations of the knowledge snapshot (K6, `omt.relation.parallel`, `supertonic`, `mediant`, `subdominant`, `dominant`, `submediant` and `subtonic`, each by id) the key is reached by from the home key, up to 8 of each; one is drawn from the list for the home key's mode and resolved by the relation entry (R-596, R-606). At most 4 regions. A region is not named `home`. | `[]` |
| `checks` | Which of the compose run's checks run: `"sections"` (each theme section heard as its shape), `"returns"`, `"keys"`, `"cadences"`, `"seam"`. | every check |
| `meters` | The meters a piece of the form may be in: 1 to 4 of `"2/4"`, `"3/4"`, `"4/4"`, `"6/8"`, each once. A request in another meter is refused (`form_spec_meter_allowed`). Refused in a form whose sections the request gives. The minuet, the trio and the minuet and trio list `"3/4"`. | any supported meter |
| `tempo` | Quarter notes a minute (30 to 300) when the request and its style give none; the MIDI and MusicXML carry it. Refused in a form whose sections the request gives. The minuet, the trio and the minuet and trio give 112. | none |
| `repeats` | The groups of sections a form repeats (up to 16 groups, R-515), each a list of section names that are consecutive and in order in every size, the groups in order and not overlapping: `[["A"], ["B", "A return"]]` (R-485, [form-performance.md](form-performance.md)). The piece is composed once and performed with each group written out twice in place (A A B A1 B A1). Refused at `/repeats/<i>/<j>` for a name a size does not have, a group that is not consecutive and in order, and groups that overlap or are out of order; at `/repeats` in a form whose sections the request gives. The minuet and the trio use it ([minuet.md](minuet.md)); [tests/fixtures/forms/repeat-form.json](../tests/fixtures/forms/repeat-form.json) composes in C major 4/4 (5 of 5 seeds). | none |
| `parts` | A compound form (R-485, [form-performance.md](form-performance.md)): 1 to 4 parts, each `{"name", "form", "key", "motive", "again", "repeats", "bars"}`. `name` is unique among the parts and prefixes the part's sections after the first part (`Trio A`). `form` is a built-in form with sizes that is not itself compound, with a size of each of the form's `bars`; the part is composed as a piece of that form. `key` is `"home"` or a region of the form: the part's home key, drawn from the region (a region's `omt.relation.parallel` is the home key's parallel key, the other mode). `motive` is `"piece"` (the piece's motive) or `"new"` (a motive of its own, invented). `again` names an earlier part that is not itself an `again` part and then takes no `form`, `key` or `motive`: the part performs it once more. `repeats` is whether the part's repeats are written out: `true`, and `false` for an `again` part (the minuet da capo). `bars` (R-495) is the size of the part's form it is composed at, in place of the compound size's bars (0 or absent: the compound size's); the form must have a size of those bars, the compound's `bars` then need not be a size of the part's form, and an `again` part takes no `bars` (it performs its source at the source's). A compound form's `sizes` have `bars` (and `default`) and no `sections` or `count`: each part is composed at its form's size of those bars, or at its own `bars`. Refused at `/parts/<i>/<member>` for an unknown member, a name that is not a built-in form with that size, an unknown region, an `again` naming a later, unknown or `again` part a member an `again` part does not take (`form`, `key`, `motive`, `bars`) and a `bars` the part's form has no size of; at `/parts` for more than 4 parts or in a form whose sections the request gives; at `/repeats` with `repeats`; at `/sizes/<i>/sections` with sections. The minuet and trio uses it ([minuet.md](minuet.md)); [tests/fixtures/forms/parts-form.json](../tests/fixtures/forms/parts-form.json) composes in C major 4/4 (10 of 10 seeds). | none |
| `sizes` | The sizes the form comes in, at most 8. Each has `bars` (the sum of its sections, checked), an optional `count` (the variations or layers), at most one `"default": true` and exactly one is required, and `sections` (at most 16, R-515). Two sizes may not have the same `bars` and `count`. | required |
| `sections` | `"request"`: the piece request gives the sections (the game loop). A form with this has no `sizes`, and has `roles`, `loop` and `cadences`. | absent |
| `roles` | A form whose sections the request gives: the roles a section may have, in the order `intro`, `body`, `turnaround`, `ending`, each once, and including `body` (the reader refuses other roles for such a form until P45). | required with `"sections": "request"`, else refused |
| `loop` | `{"roles": [...]}`: the roles that are the loop, each one of `roles`. The loop's end is held for the seam stage and a seam bar follows it; the ending version replaces the loop's last role with the ending. | required with `"sections": "request"`, else refused |
| `cadences` | `{"default": name, "last": [{"roles": [...], "cadence": name}, ...]}`: the cadence of a section the request gives none. `last` is applied in order (at most 4 rules): the rule whose roles hold the section as the last section of one of them gives its cadence, else `default`. Names are cadence vocabulary names whose core is a PAC, an IAC or an HC. | required with `"sections": "request"`, else refused |
| `default_ending` | `{"id", "letter", "role", "bars", "cadence"}`: the section a request with no ending gets; `role` is `ending`. | none |

The members of a section of a size are in [form-sections.md](form-sections.md); the built-in forms are in [forms-builtin.md](forms-builtin.md).

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
lists them, from the region's relations by the home mode. Each section then starts and ends in the key its `keys` name.
`piece_plan_cadences` reads `closing` and each section's `closed` ([piece.md](piece.md#keys-and-cadences)).

## Checks

`compose_run` makes the checks the form's `checks` names: hearing each theme section as its shape (`hear`), hearing the
returns, checking the keys and the cadences of a plan with several keys, and hearing the seam. `ComposeConfig.form` gives
the form; NULL is the form the plan records (`plan_form_record`: its `checks`, and `closing`, which the cadence check reads with each
section's `closed`), and a plan with no record, such as a theme or a fill plan, runs every check it has the parts for. A check never changes what is
written: the key check only rejects a finished run. With a check off, its result is empty (`returns_planned` is 0 with
`returns` off).

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
| `test_form_plans_templates` | The plan of every built-in form at every size, in C major 4/4 and A minor 3/4, with an invented motive, without `modulate` and with it and seeds 1 to 5, hashes as `tests/fixtures/forms/template-plans.txt` records them (R-417). `update-goldens` writes the file. | |
| `test_form_plans_pieces` | The loop plan of each piece request in `tests/fixtures/piece` (loop and ending, progression fill, structure only, theme fill), hashes as `tests/fixtures/forms/piece-plans.txt` records them (R-417). The ending plan is pinned by the `piece-loop-and-ending.*` snapshots. `update-goldens` writes the file. | |
| `test_form_spec` | Every built-in parses and is as the table says; each refusal of the reader with its path; `form_spec_size` for every size, count and bad size of each built-in; the game loop document, the refusals of its members and the default cadences for each layout of parts. | |
| `test_piece_structure_*`, `test_loop_seam_*`, `test_piece_fills_*` | The game loop's checks, defaults, plans and composed runs, built as a `PieceRequest` of the form `game-loop`. | |
| `test_form_spec_custom` | A form added as a document alone composes (seeds 1 to 10); with `checks` set to `sections` alone the same seed writes the same plan and hears no return. | |
| `test_form_templates_*` | The plan's structure at every size against a table written in the test, and the compose sweeps ([piece.md](piece-tests.md#tests-and-measured-results)); the minuet and the trio run in 3/4. | `compose` and `heard` are `slow` |
