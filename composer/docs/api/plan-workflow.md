# Plan workflow entries

Plan workflow entries of the Composer API: plan, fill, lock, regenerate, variations and harmonize. Back to the [docs index](../README.md).

## The plan workflow: `plan`, `fill`, `lock`, `regenerate`, `variations`, `harmonize`

The commands that take a plan apart and put it together again are entries too (R-219), so the command line is a wrapper
everywhere. A plan is a plan object in the request (`"plan"`), not a path; the command reads the file and writes the
plan, MIDI and MusicXML files the response holds. Options that are a path in the command line stay one only where
they name a style model, a corpus directory or a passage to look for in a corpus. A member that is a number may also be
a string of digits. Where a command line option is refused, the entry refuses the member of the same name (`seed`,
`style_weight`) with its path.

### `composer_api_plan(request_json, job, out)`

Plans a whole short piece from a form template ([cli-plan.md](../cli-plan.md)); nothing is composed.

| Member | Value |
|---|---|
| `form`, `key`, `mode`, `meter`, `bars`, `count`, `modulate` | the options of the command. `key` is a tonic with `mode`, or tonic and mode ("D minor"). With `given` and no `form` the form is a binary form of 16 bars and the given material goes in section B |
| `motive` | `{"input": {"format", "text"}, "source": "<name>"}`: a motive request ([Motive requests](motives.md#motive-requests)) read in the plan's meter and key; `source` is the name shown for it (the command gives the file) |
| `corpus`, `like` | a corpus directory and a passage file in it to look for: the motive is the first bar of a similar passage (`motive_source` in the result). `corpus` needs `like` or a `like_progression` given; `like` and `motive` are exclusive |
| `accompaniment`, `texture` | as the options |
| `seed` | 0 to 4294967295, default from the clock |
| `style`, `style_weight`, `style_without`, `blend`, `blend_weight`, `blend_weights` | a style model path and its options; the plan keeps the paths |
| `given` | a list of `{"kind": "melody"\|"bass"\|"progression"\|"like_progression", "motive": {...} (melody, bass), "numerals": "I IV V I" (progression kinds), "at": "B:3" (the section's letter, then the bar), "cells", "source"}`; at most 8. `at` is needed unless the form is defaulted |

`json`: what `plan --format json` prints, without `file`: `{"form", "bars", "key", "meter", "seed", "modulate", "motive",
"motive_source", "accompaniment", "sections": [{"letter", "bars", "key", "cadences", "pairing", "returns"}], "given",
"style", "blend"}`, with the members the text output reads: `accompaniment_texture`, `defaulted_form` and `notices`
(strings: stale documents of the corpus). `plan_json` is the plan. A request the form has no size for, a motive that does
not fit a bar, a passage no corpus entry matches, or material that does not fit its section is refused with the path of
the member (`/form`, `/bars`, `/like`, `/given/<i>`, `/given/<i>/at`). Schemas: `plan.request.schema.json`,
`plan.response.schema.json`.

### `composer_api_fill(request_json, job, out)`

Realizes a plan ([cli-fill.md](../cli-fill.md)): every open theme section is composed in stages from the seeds `seed`,
`seed + 1`, ..., at most 16, each from the plan as it was given.

| Member | Value |
|---|---|
| `plan` | the plan object (required). An Analyzer analysis document is refused with the path `/plan` |
| `seed`, `check`, `candidates` | as the options (`candidates`: 1 to 32; the best is kept and every one is listed) |
| `style`, `style_weight`, `style_without`, `blend`, `blend_weight`, `blend_weights` | replace the plan's own style; without them the plan's is used |
| `midi`, `musicxml`, `plain_midi`, `tempo` | the files to make, as for `composer_api_export` |
| `record` | `true`: the records of the stages as the text of the `--record` file, in `record` of the document |
| `explain` | `"text"` or `"json"`: the explanation as the string `explanation` of the document |

`json`: what `fill --format json` prints, without `files`: `{"seed", "realized", "candidates", "sections": [{"letter",
"heard", "heard_as_requested", "cadences"}], "analyzer", "style_distance", "returns": {"planned", "heard", "pairs"},
"given"}`; `candidates_tried` is added for the text output. `realized` is `true` for a plan that had nothing to fill. A
section outside the scope of the hearing has `"heard": null` and `"in_scope": false`. `given` is present when the plan
has Given material and says whether the run left it as it was; a run that changed it is `COMPOSER_ERR_UNSATISFIABLE`.
`plan_json`, `midi` and `musicxml` are the realized plan and its files.

A plan no seed completes is `COMPOSER_ERR_UNSATISFIABLE` with the message `the plan could not be completed from seeds <a> to
<b>: stuck at the <stage> stage`, and **`json` is set** to `{"stuck": {"first", "last", "stage", "key_rejected", "open":
[{"what", "reason"}], "more"}}`: the targets the run left open (at most 24, `more` counts the rest) and why. A cancel is
`COMPOSER_ERR_CANCELLED`. Progress is reported for each seed (`"seed"`) and each compose stage. Schemas:
`fill.request.schema.json`, `fill.response.schema.json`.

### `composer_api_lock(request_json, job, out)`

Locks a node, or one lane of it, as Given ([cli-regenerate.md](../cli-regenerate.md#lock)). The node is `"node"` (an id of the
plan) or `"section"` (a form letter), exactly one, with an optional `"lane"` (`harmony`, `bass`, `melody`, `inner`). `json`:
`{"locked": {"nodes", "spans"}, "what": "section A"}`, the nodes and lane spans that are Given now; `plan_json` is the plan.
A node with a span still open is `COMPOSER_ERR_UNSATISFIABLE`; a node or section the plan does not have has the path `/node`
or `/section`. Schemas: `lock.request.schema.json`, `lock.response.schema.json`.

### `composer_api_regenerate(request_json, job, out)`

Writes one node of a filled plan again and what depends on it ([cli-regenerate.md](../cli-regenerate.md)). Members: `plan`,
the node as for `lock`, `seed`, and `midi`, `musicxml`, `plain_midi`, `tempo`. `json` is the document of `fill` with
`"reopened": {"sections": ["B"], "spans": {"harmony", "bass", "melody", "inner_voice"}}` after `realized`: what was
reopened. A locked or unwritable node is `COMPOSER_ERR_UNSATISFIABLE` with the reason; a stuck run is reported as for `fill`.
Schemas: `regenerate.request.schema.json`, `regenerate.response.schema.json`.

### `composer_api_variations(request_json, job, out)` and `composer_api_harmonize(request_json, job, out)`

Several distinct versions of a node ([cli-variations.md](../cli-variations.md)): the node is written again from the seeds
`seed`, `seed + 1`, ..., at most 4 times `count` of them, and a version is kept when it sounds different from the plan and
from the ones kept before. Members: `plan`, the node as for `lock` (`harmonize`: `section` only, whose whole melody is
Given), `count` (1 to 16, required), `seed`, and `musicxml`, `plain_midi`, `tempo`. `json`: `{"variations": [{"index", "seed",
"plan": <plan object>, "musicxml", "sections": [...]}], "kept", "requested", "seeds_tried"}`; the MIDI file of a version is
`composer_api_export` of its plan. No version that differs from the plan is `COMPOSER_ERR_UNSATISFIABLE`. Schemas:
`variations.request.schema.json`, `variations.response.schema.json` (both entries).
