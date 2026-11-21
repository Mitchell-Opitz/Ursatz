# plan

[Docs index](../README.md): the Ursatz docs by topic.

Path `library/plan/`, target `ursatz_plan`, on `model`, `theory` and cJSON (the layer table row `uses plan model theory cjson`). The plan (R-625) and the description of a section. The plan is a tree of nodes (form, section, phrase, subphrase, idea, motive) over a piece's time, with lanes (meter, region, harmony, bass, melody, inner voices), constraints and states; the description of a section is shared by form documents, plan section nodes and piece requests (R-605): its value, its vocabulary, the rules about a section itself and its JSON codec. Sources include headers by their path from `library/` (`plan/<file>.h`). The headers the Composer includes are staged; `plan_internal.h`, `plan_json_internal.h` and `plan_constraint_kinds.h` are the group's own.

## Provides

| Header | Provides |
|---|---|
| `plan.h` | `Plan` (`plan_create`, `plan_destroy`), `NodeId`, `PlanResult`, `PlanSpan`, `PlanNodeKind`, the nodes (`plan_add_node`, `plan_remove_subtree`, the `plan_node_*` readers), the analyzer version and the form record (`PlanFormRecord`) |
| `plan_lanes.h` | `PlanLaneKind`, `PlanMeter`, `PlanHarmony`, `PlanNote` (with its id, source and operation), `PlanLanePayload`, `PlanKeyRelation`, the lane spans (`plan_add_lane_span`, `plan_lane_span_resize`, `plan_lane_find`, `plan_note_by_id`) |
| `plan_constraint.h` | `PlanConstraint` and its kinds, `PlanMotiveRef`, the section description of a node (`plan_node_section`, `plan_node_set_section`) and the constraint lists of nodes and lane spans |
| `plan_state.h` | `PlanState` (Constrained, Given, Realized), the transitions and `plan_lane_span_write` |
| `plan_edit.h` | `plan_clone` and `plan_copy_into` (a plan as a JSON round trip gives it, without the text), `plan_lane_span_reopen` (the one way out of Given), `plan_node_remove_leaf`, `plan_lane_span_remove`, `plan_lane_span_remove_owned` |
| `plan_json.h` | The file is `plan_json.c` (the document, the metadata, the reader base and the states) with `plan_json_constraints.c` (the constraints, the keys and the sections, current and legacy forms, the form record), `plan_json_nodes.c` (the node tree) and `plan_json_lanes.c` (the lane spans and their notes), sharing `plan_json_internal.h`; the reader sits on `CommonJsonReader` (`common/json.h`). `plan_to_json` with a `PlanProducer` (the program, its version, its build keys and the hash of its knowledge, which the caller gives), `plan_from_json` and `plan_from_json_resolved` with `PlanReadHooks` (a program's say in which materials it supports and how a plan of a format before 5 gets its form record), `plan_resolve_key_spans`; the format is [plan-format.md](plan-format.md) |
| `plan_common.h` | The helpers the plan's files share: the lane names, the cadence names, `plan_meter_signature`, `plan_meter_grid`, `plan_meter_pickup_on_grid`, `plan_meter_bar_length`, `plan_key_valid`, `plan_key_material_id`, `plan_key_word` |
| `section.h` | `SectionDescription`, the vocabulary (`SectionRole`, `SectionBeginning`, `SectionEnding`, `SectionIdea`, `SectionConstruction`, `SectionReturnVariation`, `SectionSetting`, `SectionModeShift`, `SectionConnective`, `SectionFigure`, `SectionFunction`, `SectionLane`, `SectionRegion`, `SectionHarmony`), a `*_name` and `*_find` pair for each, `section_description_init`, `section_description_check`, `section_description_weight` |
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

`section_description_weight` is the number of constraints the members used to be (a pairing was two, a game loop part, the seam, source material and a liquidation one each): the Composer's fill picks the open node with the most constraints first, and a section node counts its weight with them so the order stays as it was.

## The codec

`section_from_json(object, use, skip, skip_count, out, error)` reads an object for a use. `skip` names members the caller reads itself (`fill` and `part` in a request). An unknown member is `unknown member "<name>"`, a member of another use `"<name>" is not given here`; then the description is checked. `section_to_json(description, use)` writes the members in table order, only those set and allowed for the use.

The codec has one static table, one row per member with its name, its uses, its reader and its writer, and nothing else in the file names a member. The same members are the properties of `$defs.section` in `library/plan/schema/section.schema.json`; the test `plan_section` fails when the two differ. The fragment also holds the profiles `form_section`, `plan_section`, `request_section` and `overlay`; the form, plan and request schemas reference them with `$ref`, and `tests/support/validate_schema.py --ref <schema.json>` resolves such a reference when a test validates a document.

A cadence is a kind name (`perfect-authentic`, `medial-caesura`, kept as written) or PAC, IAC, HC, DC or PC in any case. A pickup is a whole number or a fraction `"1/2"`. A region is `"home"`, a form region name, `{"tonic", "material"}` or `{"relation", "anchor"}` with the anchor `home` (default), `previous` or a section name.

## The plan

The Composer writes the plan; the Analyzer's reading becomes one later in P62 (B14). The plan holds no program of its own: a caller stamps what it writes (`plan_to_json(plan, &producer, &error)`) and a reader that needs a program's rules passes hooks (`plan_from_json(knowledge, text, hooks, &plan, &error)`; NULL hooks support every material and migrate nothing). `plan_clone` renumbers the live nodes and lane spans from 1, as writing and reading a plan does; `plan_copy_into` replaces a plan's contents with a clone (what a retry of a failed composing round restores). Spans and nodes are edited through the public functions of the headers; only the group's files include `plan_internal.h`.

## Deferred

The Composer reads forms, plans and requests through the group. `tools/ci/reach-allow.txt` lists the functions no application reaches yet with the branch that adds the caller.
