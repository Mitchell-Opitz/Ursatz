# Theme and piece entries

Theme, piece and piece variations entries of the Composer API. Back to the [docs index](../README.md).

## `composer_api_theme(request_json, job, out)`

Grows a theme from the theme request v2 of [theme-request.md](../theme-request.md) (`"schema": "ursatz-theme-request"`,
`"schema_version": 2`). Its motives are inline: each motive object carries its content in `degrees`, a motive with a `path`
or a `file` member is refused with `COMPOSER_ERR_REQUEST` and the path `/motives/<n>/path` (the caller reads files). A
member of the brief that is not valid is refused with its path (`/into`, `/key`, `/slots/basic`, `/chords/<n>`, `/style`).
The options the `theme` command takes are members of the same object:

| Member | Value |
|---|---|
| `seed` | 0 to 4294967295 (a number or a string of digits), default from the clock; the seeds after it are tried when one gets stuck |
| `suggest` | 1 to 16 different themes, default 1 |
| `check` | `true`: the Analyzer analyzes each theme, and a theme whose analysis contradicts the plan is passed over |
| `style`, `without`, `style_weight` | as in the request; `style` is a style model file path (R-267) |
| `blend`, `blend_weight`, `blend_weights` | a second style model file path blended with `style` (`"1/2"`, `"harmony=1/2,form=0"`) |
| `tempo` | quarter notes per minute, a whole number or `"195/2"`, 20 to 400; wins over the style's |
| `preferences` | the settings document of [preferences.md](../preferences.md) (API 1.11): its `melody` group and the `development` fidelity minimums reach the grow; its `motif` group is accepted, echoed and has no effect, because the motives come from the request. A bad member is refused with its path under `/preferences` |
| `midi`, `musicxml` | `true`: the first theme's MIDI file (`midi`, `midi_size`) and MusicXML (`musicxml`), rendered once from the same `RenderedPlan` |
| `plain_midi` | `true`: no track names and no section markers in the MIDI file |
| `plans` | `true`: each theme's plan as an object, `themes[k].plan` (the CLI writes the plans, MIDI files and MusicXML of the themes after the first from it, through `composer_api_export`) |
| `explain` | `"text"` or `"json"`: why each candidate was chosen or rejected, as a string, `themes[k].explanation` |

The request has no `corpus` member: a theme is grown from its motives only.

`json`: what `theme --format json` prints, without the `files` it wrote: `{"key", "meter", "bars", "into", "pairing",
"cadence", "first_cadence", "accompaniment", "texture", "voicing", "tempo": {"qpm", "from"}, "brief": {"tempo", "mode",
"density", "without"}, "chords": [{"bar", "text"}], "themes": [{"seed", "structure", "chords", "developments", "slots",
"score", "requested", "heard", "heard_as_requested", "heard_text", "style_distance"}], "preferences"}`, the last when the request had the member (API 1.11: the full document with the defaults filled in). `plan_json`: the plan of the first
theme. Each item of `developments` is `{"motive", "idea", "development", "segment", "summary", "fidelity", "minimum", "heard", "heard_agrees"}`: `segment` is the label of the motif's segment the idea is made from or `null`, `summary` one line naming the idea, its source and its operation (API 1.8, [explain.md](../explain.md#the-summary-line-of-a-developed-idea)). `fidelity` is how close the notes the melody holds are to the idea's source, from 0 to 1 (three decimals); `minimum` is the least the melody stage accepts for the idea's role under the development setting, 0 for an idea whose operation makes it differ from its source (a liquidation, a fragment by notes, an inversion), which is ranked and never refused; `heard` is the relation the recognizer hears between the two, in words (`"exact"`, `"transposed -1 step of the key's scale"`, `"not recognized"`), and `heard_agrees` whether it is the one the operation intended. All four are `null` for an idea with no melody to measure (API 1.10, [explain.md](../explain.md#intended-against-heard)). The basic idea is not an item. Progress is reported once for each seed tried (`"grow"`) and for each compose stage of the run; a cancel is
`COMPOSER_ERR_CANCELLED` with no document and no plan. A request no seed can write is `COMPOSER_ERR_UNSATISFIABLE`
(`the given chords in bar <n> do not allow the PAC`, `no theme could be grown for ...`). The same request and seed give the
same theme on any thread; the entry keeps no state, so requests may run at the same time. Schemas:
`theme.request.schema.json`, `theme.response.schema.json`.

## `composer_api_piece(request_json, job, out)`

Composes a looping game track from the piece request of [piece-request.md](../piece-request.md) (`"schema":
"ursatz-piece-request"`, `"schema_version": 1`), or writes one section of the loop plan of an earlier piece again, and builds
the ending version from the loop. The request is an object around the piece request, which carries no path but the style
model (R-267):

| Member | Value |
|---|---|
| `request` | the piece request object, as the file holds it, except that a section filled with a theme carries `"plan": <plan object>` (a path is refused with `COMPOSER_ERR_REQUEST` and the path `/request/sections/<i>/fill/plan`; the caller reads the file) and may carry `"source": "<name>"`, the name shown as the section's source in the result (default `theme plan`) |
| `from` | the loop plan object of an earlier piece (its `plans.loop`, or `plan_json`); absent to compose a piece |
| `regenerate` | with `from`: `{"section": "<section id>", "lanes": ["bass", ...]}`. `lanes` holds `harmony`, `bass`, `melody`, `inner`; none, or `all`, is the whole section. A section filled with a theme needs `harmony` or `bass`. A `null` or absent `section` (or no `regenerate`) reopens nothing: the loop plan is kept and the ending version is built again |
| `seed` | 0 to 4294967295 (a number or a string of digits), default from the clock; the seeds after it are tried (up to 16) when one gets stuck |
| `check` | `true`: the Analyzer analyzes both versions, and a seed whose analysis contradicts the plan is passed over |
| `style` | a style model file path; replaces the request's `style` |
| `midi`, `musicxml` | `true`: the loop version's MIDI file (`midi`, `midi_size`, with the `loopStart` and `loopEnd` markers) and MusicXML (`musicxml`) |
| `ending_musicxml` | `true`: the ending version's MusicXML as the string `ending_musicxml` of the result |
| `plain_midi` | `true`: no track names and no section markers in the MIDI file |
| `explain` | `"text"` or `"json"`: why each candidate was chosen or rejected in both versions, as the string `explanation` |

The request has no `corpus` member: a piece reads no corpus, and `corpus` is refused with the path `/corpus`.

`json`: what `piece --format json` prints, without the `files` it wrote: `{"schema": "ursatz-piece", "seed", "key", "meter",
"brief", "loop": {"start", "end"}, "regenerated", "versions": {"loop": V, "ending": V}}` (`regenerated` only with `from`: `null`,
or `{"section", "lane"}` with `all`, a lane, or the lanes joined with `+`), each version V `{"plan", "sections", "score",
"seam", "analyzer_check", "style_distance"}` as in [cli-piece.md](../cli-piece.md#output). `V.plan` is `null`: the plans are in
`"plans": {"loop": <plan object>, "ending": <plan object>}`, and `plan_json` is the loop plan. A section a regenerate left
as it was (a theme section out of the scope of the hearing, [regenerate.md](../regenerate.md)) has `"in_scope": false`; a
complete loop plan has no open theme section, so only a plan with open sections reports one. The MIDI file of the ending
version is `composer_api_export` of `plans.ending`; its MusicXML comes with `ending_musicxml` because the `xml:id` of
its notes carry the span ids of the plan in memory, which a plan file renumbers.

Progress is reported once for each seed tried (`"seed"`) and for each compose stage of each section; a cancel is
`COMPOSER_ERR_CANCELLED` with no document and no plan. Refusals have the path of the member at fault (`/request/key`,
`/request/sections/<i>/beats`, `/regenerate/section`, `/regenerate/lanes`, `/from`, `/seed`). A piece no seed can compose,
a motive that does not fit its bar, a request that no seed can satisfy is `COMPOSER_ERR_UNSATISFIABLE` (`no piece could be
composed from seeds <a> to <b> (the last was stuck in the loop version, seam stage ...)`). The cadence of a section without
one is the library's `piece_structure` default (an HC at the end of the intro and of the loop, a PAC at the end of the
ending, an IAC elsewhere), and a request without an ending gets the default ending. The same request and seed give the same
piece on any thread; the entry keeps no state. Schemas: `piece.request.schema.json`, `piece.response.schema.json`.

## `composer_api_piece_variations(request_json, job, out)`

Makes up to `count` different versions of one section of a loop plan: the section is written again (as `regenerate` does,
the seam and the loop's end included) from the seeds `seed`, `seed + 1`, ..., at most 4 times `count` of them, and a version
is kept when its notes differ from the plan's and from every version kept before. A seed that gets stuck is skipped.

| Member | Value |
|---|---|
| `request`, `from`, `seed`, `check`, `style`, `plain_midi` | as for `composer_api_piece`; `from` is required |
| `variations` | `{"section": "<section id>", "lanes": [...], "count": 1 to 8}`; `lanes` as for `regenerate`, none or `all` the whole section; harmony over a Given melody (a theme fill) is a harmonization |
| `musicxml` | `true`: the MusicXML of each variation as `variations[i].musicxml` |

`json`: what `piece --variations` prints, with each version's plan inline: `{"schema": "ursatz-piece-variations",
"section", "lane", "brief", "variations": [{"index", "seed", "loop": V, "loop_markers": {"start", "end"}, "musicxml"}],
"kept", "requested", "seeds_tried"}`. `V.plan` is the plan object of the variation. The MIDI file of a variation is
`composer_api_export` of its plan with its `loop_markers` as `loop`. Progress is reported once for each seed tried
(`"variations"`) and for each compose stage; a cancel is `COMPOSER_ERR_CANCELLED`. No version that differs from the plan is
`COMPOSER_ERR_UNSATISFIABLE`. Refusals name `/from`, `/variations/section`, `/variations/lanes`, `/variations/count`.
Schemas: `piece_variations.request.schema.json`, `piece_variations.response.schema.json`.
