# plan

[Docs index](../README.md): the Ursatz docs by topic.

Path `library/plan/`, target `ursatz_plan`, on `model`, `theory` and cJSON (the layer table row `uses plan model theory cjson`). One description of a section, shared by form documents, plan section nodes and piece requests (R-605): its value, its vocabulary, the rules about a section itself and its JSON codec. Sources include headers by their path from `library/` (`plan/<file>.h`). No header is staged yet: P60 B2 stages both when the Composer first includes them.

## Provides

| Header | Provides |
|---|---|
| `section.h` | `SectionDescription`, the vocabulary (`SectionRole`, `SectionBeginning`, `SectionEnding`, `SectionIdea`, `SectionConstruction`, `SectionReturnVariation`, `SectionSetting`, `SectionModeShift`, `SectionConnective`, `SectionFigure`, `SectionFunction`, `SectionLane`, `SectionRegion`, `SectionHarmony`), a `*_name` and `*_find` pair for each, `section_description_init`, `section_description_check` |
| `section_json.h` | `section_from_json`, `section_to_json`, `section_json_member_count`, `section_json_member_name`, `section_json_member_uses` |

## Uses

A description is used for one of four things, and the use decides which members it needs and which it may hold.

| Use | Needs | Holds |
|---|---|---|
| `SECTION_USE_FORM` | `name`, `letter`, `role` (a form role), `bars` or `beats` (not both), `cadence`, and `beginning` with `ending` exactly when the role is not liquidation, retransition or coda | no `id`, `pickup`, `first_cadence`, `connective_written`, `free_phrase`, `harmony`, `region` or `function` (the role gives the function); keys are `home` or a region of the form |
| `SECTION_USE_PLAN` | nothing | no `name`, `motive`, `bars`, `beats`, `pickup` or `region`; keys of any kind |
| `SECTION_USE_REQUEST` | `id`, `role` (intro, body, turnaround or ending), `bars` or `beats` | `id`, `role`, `letter`, `bars`, `beats`, `pickup`, `cadence`, `function` (transition or coda), `region` (home, a tonic and material, or a relation) and `harmony` |
| `SECTION_USE_OVERLAY` | `name` | `name`, `region` and `harmony` |

## The rules

`section_description_check` holds the rules about a section itself, with the messages the Composer's form reader used: a variation or a setting needs `returns`; a setting return names its setting; a figurate variation needs a figure and the reverse; mode `home`, `parallel` and `major` need what each says and no keys; a connective needs `leads_in`; a pedal needs a presentation beginning; a liquidation, retransition and coda refuse the members an operator makes itself; a transition refuses returns, variation, figure, mode and pedal; independent only with a transition, lyric only with a subordinate theme. A rule that looks at other sections (a return naming an earlier one), at a form's regions or at the meter stays with the code that holds those. A refusal names the member as a path relative to the section object, `/variation`.

## The codec

`section_from_json(object, use, skip, skip_count, out, error)` reads an object for a use. `skip` names members the caller reads itself (`fill` and `part` in a request). An unknown member is `unknown member "<name>"`, a member of another use `"<name>" is not given here`; then the description is checked. `section_to_json(description, use)` writes the members in table order, only those set and allowed for the use.

The codec has one static table, one row per member with its name, its uses, its reader and its writer, and nothing else in the file names a member. The same members are the properties of `$defs.section` in `library/plan/schema/section.schema.json`; the test `plan_section` fails when the two differ. The fragment also holds the profiles `form_section`, `plan_section`, `request_section` and `overlay`; the form, plan and request schemas reference them with `$ref`, and `tests/support/validate_schema.py --ref <schema.json>` resolves such a reference when a test validates a document.

A cadence is a kind name (`perfect-authentic`, `medial-caesura`, kept as written) or PAC, IAC, HC, DC or PC in any case. A pickup is a whole number or a fraction `"1/2"`. A region is `"home"`, a form region name, `{"tonic", "material"}` or `{"relation", "anchor"}` with the anchor `home` (default), `previous` or a section name.

## Deferred

The Composer reads forms, plans and requests through it from P60 B2 on. Until then nothing outside its tests calls it, and `tools/ci/reach-allow.txt` lists its functions with the branch that adds the caller.
