# The Composer API

Everything an application asks of the Composer is a versioned in-process C API (R-219). The one public header is
`include/ursatz_composer/composer_api.h`. A request is JSON text, a response is a `ComposerResponse`; no Composer type, no
cJSON type and no file path (except style models and corpus directories) crosses it. The CLI is a client of the same
functions: it reads its arguments into a request, calls the API and prints the response (R-267).

This page covers the core, export, the motive entries, variants, recognize, random motives, themes and pieces. Each later branch of phase P26 adds its entries here and its JSON Schemas
under `schema/api/` (draft 2020-12).

## Versioning

`COMPOSER_API_VERSION_MAJOR` and `COMPOSER_API_VERSION_MINOR`; `composer_api_version()` gives `"<major>.<minor>"`. The
minor version rises by one with every entry added; 1.0 is the first stable API (R-268), and from then a request or
response member is only added, never changed or removed, within a major version.

Every request may carry `"api_version": <major>`. A different major is refused with `COMPOSER_ERR_REQUEST` and the path
`/api_version`. A request without it is taken as the major of the library.

## Status

Every entry returns a `ComposerStatus`. The CLI maps it to its exit code.

| Status | Meaning | CLI exit |
|---|---|---|
| `COMPOSER_OK` | the response is filled | 0 |
| `COMPOSER_ERR_REQUEST` | the request is malformed or asks for something invalid | 1 |
| `COMPOSER_ERR_UNSATISFIABLE` | the request is valid but nothing satisfies it (a stuck run) | 2 |
| `COMPOSER_ERR_CANCELLED` | the job's `cancelled` callback returned true | none; the CLI has no cancel |
| `COMPOSER_ERR_ALLOC` | out of memory | 1 |
| `COMPOSER_ERR_INTERNAL` | a failure that is not the request's | 1 |

`COMPOSER_ERR_ALLOC` and `COMPOSER_ERR_INTERNAL` are 1 unless a command did otherwise before. A failure to write a file is
the CLI's own (the API writes none) and stays 3 where it was 3 (`export` has always given 1). `composer_status_name`
gives `"ok"`, `"request"`, `"unsatisfiable"`, `"cancelled"`, `"alloc"` and `"internal"`.

On any failure `ComposerResponse.error` holds `message` and `path`, the JSON Pointer of the request member at fault
(`"/tempo"`, `"/plan/form/children/1"`), or `""` when no member is at fault. Nothing else is set.

## Memory

An entry zeroes the `ComposerResponse` first and fills only the members it produces: `json`, `plan_json`, `midi` and
`midi_size`, `musicxml`. The caller owns them and frees them all with `composer_response_destroy`, which zeroes the
response; it is safe on a zeroed response and on one already destroyed. Strings are NUL-terminated; `midi` is `midi_size`
bytes. A request string is only read during the call.

## Jobs

`const ComposerJob *job` carries progress and cancel for the entries that take long. A NULL job, or a NULL member, means no
reports and no cancel.

- `progress(user, stage, done, total)` is called at the start of each stage and once per target of a fill loop. The stages
  are the compose stages (`"invent"`, `"structure"`, `"motives"`, `"harmony"`, `"bass"`, `"melody"`, `"texture"`,
  `"returns"`, `"seam"`, as `done` the stage's index of `total` = 9), `"fill"` (`done` targets attempted of the iteration
  limit; a fill loop that starts again, as the melody does from the next seed, begins at 0), `"grow"` (one candidate) and
  `"export"`.
- `cancelled(user)` is asked at the same points. When it returns true the run stops there, frees what it built and the
  entry returns `COMPOSER_ERR_CANCELLED`. The plan the run was filling is left as far as it got and is not returned.
- Both are called from the calling thread only.

In the library, `FillConfig`, `ComposeConfig` and `GrowConfig` carry `const ComposerJob *job` (NULL by default); a cancel is
`FillOutcome.cancelled`, `ComposeOutcome.cancelled` and `GrowOutcome.cancelled` with the module's existing failure value
(`FILL_ERR_OPERATOR_FAILED`, `PIECE_ERR_STUCK`, `GROW_ERR_STUCK`). `compose_run` tries no further round and
`compose_candidates` no further candidate after a cancel. With no job nothing changes: the same seed gives the same plan.

## Threading

No entry keeps state between calls or shares state with another: one request per call, and a caller may run requests on
several threads at once. A job's callbacks run on the thread that called the entry.

## Entries

### `composer_api_versions(out)`

`json`: `{"composer_version", "api_version", "analyzer_version", "ursatz_commit", "plan_format_version"}`, the versions the
Composer was built with (the Analyzer's, and the Ursatz commit the Analyzer was configured against, are the ones a plan or
an analysis is stale against). Schema: `schema/api/versions.schema.json`.

### `composer_api_export(request_json, job, out)`

Renders a plan once and exports it, as `ursatz-composer export` does.

| Member | Value |
|---|---|
| `plan` | the plan object (docs/plan-format.md) |
| `midi` | `true` to fill `midi` and `midi_size`; default `false` |
| `musicxml` | `true` to fill `musicxml`; default `false` |
| `tempo` | quarter notes per minute from 20 to 400, a whole number or a string like `"195/2"`; `null` or absent for none |
| `plain_midi` | `true` to leave out the MIDI track names and section markers; default `false` |
| `loop` | `{"start": "2/1", "end": "14/1"}`, times in whole notes from the start of the plan: the `loopStart` and `loopEnd` markers of the MIDI file ([export.md](export.md#loop-markers)); the end must come after the start |

`json`: `{"skipped": [{"span", "lane", "reason"}]}`, the lane spans the render left out, with the reasons the CLI prints on
stderr (`"not Given or Realized"`, `"source material"`, `"not in the layer"`, `"seam"`, `"lane is not rendered yet"`).
A plan that does not read is `COMPOSER_ERR_REQUEST` with the path of the member (`/plan/format`); a plan with no Given or
Realized note is `COMPOSER_ERR_REQUEST` with `/plan`. Schema: `schema/api/export.schema.json`.

### Motive requests

The three motive entries read the same request. Only `input` (with its `text`) is required; `composer_api_motive_from_midi`
takes the bytes of a MIDI file instead of `input`.

| Member | Value |
|---|---|
| `input.text` | the motive as text: a note list, a degree list, a rhythm, a contour, a motive document (JSON) or a MusicXML document |
| `input.format` | `"auto"` (default; the text is detected as the CLI detects a file), `"notes"`, `"degrees"`, `"rhythm"`, `"contour"`, `"json"` or `"musicxml"` |
| `input.form` | `"full"` (default), `"rhythm"` or `"contour"`: a rhythm or a contour is read by its own reader; a `format` that says otherwise is refused |
| `meter` | `"4/4"`: the meter, for input that has none (a note list); an input that declares another meter is `COMPOSER_ERR_UNSATISFIABLE` |
| `key` | `"D minor"`: replaces a key the input states |
| `pickup` | the beats before the first barline, counted in beats of the motive's meter: a number or a string (`1`, `"1/2"`); default 0 |
| `harmony` | the motive's own chords as Roman numerals with onsets in beats after `@` (`"I V6@2 I@3"`); `""` clears the harmony the input has |
| `features` | up to 8 labelled runs of notes by 0-based index: `["a:0-2", "b:3-4"]`; a label is used once |
| `role` | `"melodic"` (default) or `"accompaniment"` |
| `tones` | tone labels by 1-based note: `["3=passing", "2=chord"]`; the roles are `chord`, `passing`, `neighbor`, `appoggiatura`, `suspension`, `retardation`, `escape`, `anticipation`, `double-neighbor`, `changing-tone` and `pedal`; needs a motive with a key and pitches |

A member that is not valid is `COMPOSER_ERR_REQUEST` with its path (`/meter`, `/harmony`, `/features/1`, `/tones/0`,
`/input/format`). Text that is not a motive of the kind asked for, or a pickup, chord or feature the motive cannot hold,
is `COMPOSER_ERR_UNSATISFIABLE` with the path `/input/text` (`""` for MIDI bytes) and the library's message and location.
Schema: `schema/api/motive.request.schema.json`.

### `composer_api_motive(request_json, job, out)`

Analyzes a motive, as `ursatz-composer motive <input> --json` does. `json`: the document that command prints,
`{"motive", "analysis"}`: the motive in the motive-document format and its analysis (intervals, contour, rhythm, range,
scale degrees, implied harmony, base harmony and the tone labels with their warnings). Schema:
`schema/api/motive.response.schema.json`.

### `composer_api_motive_from_midi(bytes, size, request_json, job, out)`

The same for a motive that is a Standard MIDI File (type 0 or 1), read in memory. The request has the members above
without `input`. A buffer that is not a MIDI file, or holds notes sounding together, is `COMPOSER_ERR_UNSATISFIABLE`.

### `composer_api_motive_render(request_json, job, out)`

The motive request, and the motive is rendered.

| Member | Value |
|---|---|
| `render_key` | `"G minor"`: hear the motive in this key, the same degrees; default its own key. A motive with no key, or a note that leaves the MIDI range, is `COMPOSER_ERR_UNSATISFIABLE` with `/render_key` |
| `tempo` | quarter notes per minute from 20 to 400, a whole number or a string like `"195/2"` |
| `musicxml` | `true` to fill `musicxml` (a pickup is an implicit first measure); default `false` |
| `midi` | `true` to fill `midi` and `midi_size`; default `false` |

`json` is the analysis document with the top-level `rendered` object, `{"key", "notes": [{"midi_pitch", "offset",
"duration"}]}`, the motive as it sounds with offsets from its first note. A file that cannot be made is
`COMPOSER_ERR_UNSATISFIABLE` with the path `/midi` or `/musicxml`. The render entry takes text input only; the CLI, which
may read a MIDI file and render it, reads the file with `composer_api_motive_from_midi` and passes the motive document it
answers with as the `json` input of the render request.

### `composer_api_variant_operations(out)`

The variant operations of the registry, as `ursatz-composer variants --list-ops --json` prints them: for each its `name`,
`label`, `category`, `citation`, `relation`, `params` (`name`, `kind`, `min`, `max`, `description`, `optional`) and `forms`.
Schema: `schema/api/variant_operations.schema.json`.

### `composer_api_variants(request_json, job, out)`

Variants of a motive: each applies a chain of up to 4 operations, and a parameter the chain leaves open is drawn from the
seed. The request is the motive request (above, without `tones`) with these members:

| Member | Value |
|---|---|
| `basic` | a motive request with its own `input`, `pickup` and `harmony`, read in the source's `meter`, `key`, `role` and input `form`: the basic motive remoteness is measured from. Default: the source. A basic that does not read is refused with a path under `/basic` |
| `functions` | up to 4 of `"beginning"`, `"continuation"`, `"contrast"`, `"cadential"`: keep the variants that suit any of them. Up to 4 times `count` chains are drawn, so fewer may come back |
| `distance` | `"close"` (1 step, remoteness at most 1/3), `"medium"` (2 steps, over 1/3 up to 2/3) or `"far"` (3 or 4 steps, over 2/3). Combines with `functions`; not with `chain` |
| `chain` | `{"steps": [{"op", "params"}]}`: the same chain for every variant (the CLI's chain file, as a value). A step that is not valid is refused with its path, `/chain/steps/<i>/params/<name>` for a parameter |
| `each_op` | `true`: one variant for every operation that takes the motive's form, in registry order, each a one-step chain; an operation the motive refuses is listed in `skipped`. Not with `chain`, `count`, `distance` or `functions` (`/each_op`) |
| `count` | 1 to 16, default 6. Variant i uses seed + i |
| `seed` | 0 to 4294967231, default from the clock. A whole number or a string of digits |
| `detail` | `true` adds to each variant `remoteness_fraction` `[numerator, denominator]` and `leaves_harmony`, the exact figures the CLI prints as text |

`json`: `{"seed", "source", "variants": [...]}`: each variant has its `seed`, `chain`, `kind`, `kept`, `changed`,
`harmony`, `functions`, `derivation`, `recognizability`, `remoteness` and `motive` (the motive document of the motive
entry). `skipped` (`[{"op", "reason"}]`) for `each_op`. `notices` (strings, only when there are any) say which seeds were
skipped because a chain did not apply, and that fewer variants than asked for suit the functions and distance; the CLI
prints them on stderr. Nothing is made: `COMPOSER_ERR_UNSATISFIABLE`, as is a chain with a step the motive refuses
(`step 2 of the chain: ...`).

Progress is reported once for each variant tried, stage `"variants"` (`done` of `total` draws, or registry rows for
`each_op`), and a cancel is honored there with `COMPOSER_ERR_CANCELLED`. Schemas: `schema/api/variants.request.schema.json`,
`variants.response.schema.json`.

### `composer_api_recognize(request_json, job, out)`

Names the chain of operations that turns a source motive into a target.

| Member | Value |
|---|---|
| `source`, `target` | motive requests with their own `input`, `harmony` and `features`; both are read in the request's `meter`, `key` and `role` |
| `meter` | required: both motives are read in it (`/meter`) |
| `key`, `role` | as for a motive request, for both motives |
| `pickup`, `target_pickup` | the source's and the target's pickup in beats; each is named at its own path when it is not valid |
| `basic` | as for variants: remoteness is measured from it; default the source |
| `depth` | the most operations in the chain, 1 to 4 (default 4) |
| `detail` | `true` adds `distance_fraction` and `remoteness_fraction`, each `[numerator, denominator]` |

`json`: `{"found", "chain", "distance", "remoteness", "kept", "changed", "derivation"}`. A search that finds no chain is a
response with `found` false. Schemas: `recognize.request.schema.json`, `recognize.response.schema.json`.

### `composer_api_random_motive(request_json, job, out)`

Random motives that keep the hard melodic rules of standard counterpoint; their length, pickup, number of notes, rhythm and
range are drawn from defaults in the Composer.

| Member | Value |
|---|---|
| `mode` | `"major"`, `"minor"` or `"random"`. Required |
| `meter` | `"4/4"`, `"3/4"`, `"2/4"`, `"6/8"` or any meter, or `"random"`. Required |
| `key` | `"D minor"`; its mode must be the mode (`/key`). Default C major or A minor |
| `form` | `"full"` (default), `"rhythm"` or `"contour"` |
| `role` | `"melodic"` or `"accompaniment"` |
| `style` | a style model file: 8 candidates are drawn for each motive and the closest to it kept. A file that cannot be read is `COMPOSER_ERR_UNSATISFIABLE` with `/style` |
| `style_without` | up to 8 style categories read without; needs `style` |
| `count` | 1 to 16, default 1 |
| `seed` | 0 to 4294967295, default from the clock |

**Random mode and meter (R-269).** `"random"` is drawn from the seed by the rule the GUI used before the Composer drew, so a
GUI seed draws what it drew: `m = ((seed + 1) * 2654435761) mod 2^32`; the mode is major when `(m >> 8)` is even and
minor when it is odd; the meter is `"4/4"`, `"3/4"`, `"2/4"` or `"6/8"` for `(m >> 16) mod 4` of 0, 1, 2 and 3. Seed 0 draws
minor 6/8, seed 1 and seed 2 minor 2/4, and seed 4294967295 (where `seed + 1` wraps to 0) major 4/4. The response says what
was drawn.

`json`: `{"seed", "motives": [<the motive entry's document>]}` and `"drawn": {"mode", "meter"}`, with only the members that
were drawn, when something was. The same request and seed give the same motives. Schemas:
`random_motive.request.schema.json`, `random_motive.response.schema.json`.

### `composer_api_theme(request_json, job, out)`

Grows a theme from the theme request v2 of [theme-request.md](theme-request.md) (`"schema": "ursatz-theme-request"`,
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
| `midi`, `musicxml` | `true`: the first theme's MIDI file (`midi`, `midi_size`) and MusicXML (`musicxml`), rendered once from the same `RenderedPlan` |
| `plain_midi` | `true`: no track names and no section markers in the MIDI file |
| `plans` | `true`: each theme's plan as an object, `themes[k].plan` (the CLI writes the plans, MIDI files and MusicXML of the themes after the first from it, through `composer_api_export`) |
| `explain` | `"text"` or `"json"`: why each candidate was chosen or rejected, as a string, `themes[k].explanation` |

The request has no `corpus` member: a theme is grown from its motives only.

`json`: what `theme --format json` prints, without the `files` it wrote: `{"key", "meter", "bars", "into", "pairing",
"cadence", "first_cadence", "accompaniment", "texture", "voicing", "tempo": {"qpm", "from"}, "brief": {"tempo", "mode",
"density", "without"}, "chords": [{"bar", "text"}], "themes": [{"seed", "structure", "chords", "developments", "slots",
"score", "requested", "heard", "heard_as_requested", "heard_text", "style_distance"}]}`. `plan_json`: the plan of the first
theme. Progress is reported once for each seed tried (`"grow"`) and for each compose stage of the run; a cancel is
`COMPOSER_ERR_CANCELLED` with no document and no plan. A request no seed can write is `COMPOSER_ERR_UNSATISFIABLE`
(`the given chords in bar <n> do not allow the PAC`, `no theme could be grown for ...`). The same request and seed give the
same theme on any thread; the entry keeps no state, so requests may run at the same time. Schemas:
`theme.request.schema.json`, `theme.response.schema.json`.

### `composer_api_piece(request_json, job, out)`

Composes a looping game track from the piece request of [piece-request.md](piece-request.md) (`"schema":
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
"seam", "analyzer_check", "style_distance"}` as in [cli-piece.md](cli-piece.md#output). `V.plan` is `null`: the plans are in
`"plans": {"loop": <plan object>, "ending": <plan object>}`, and `plan_json` is the loop plan. A section a regenerate left
as it was (a theme section out of the scope of the hearing, [regenerate.md](regenerate.md)) has `"in_scope": false`; a
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

### `composer_api_piece_variations(request_json, job, out)`

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

### The plan workflow: `plan`, `fill`, `lock`, `regenerate`, `variations`, `harmonize`

The commands that take a plan apart and put it together again are entries too (R-219), so the command line is a wrapper
everywhere. A plan is a plan object in the request (`"plan"`), not a path; the command reads the file and writes the
plan, MIDI and MusicXML files the response holds. Options that are a path in the command line stay one only where
they name a style model, a corpus directory or a passage to look for in a corpus. A member that is a number may also be
a string of digits. Where a command line option is refused, the entry refuses the member of the same name (`seed`,
`style_weight`) with its path.

#### `composer_api_plan(request_json, job, out)`

Plans a whole short piece from a form template ([cli-plan.md](cli-plan.md)); nothing is composed.

| Member | Value |
|---|---|
| `form`, `key`, `mode`, `meter`, `bars`, `count`, `modulate` | the options of the command. `key` is a tonic with `mode`, or tonic and mode ("D minor"). With `given` and no `form` the form is a ternary form of 24 bars and the given material goes in section B |
| `motive` | `{"input": {"format", "text"}, "source": "<name>"}`: a motive request ([Motive requests](#motive-requests)) read in the plan's meter and key; `source` is the name shown for it (the command gives the file) |
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

#### `composer_api_fill(request_json, job, out)`

Realizes a plan ([cli-fill.md](cli-fill.md)): every open theme section is composed in stages from the seeds `seed`,
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

#### `composer_api_lock(request_json, job, out)`

Locks a node, or one lane of it, as Given ([cli-regenerate.md](cli-regenerate.md#lock)). The node is `"node"` (an id of the
plan) or `"section"` (a form letter), exactly one, with an optional `"lane"` (`harmony`, `bass`, `melody`, `inner`). `json`:
`{"locked": {"nodes", "spans"}, "what": "section A"}`, the nodes and lane spans that are Given now; `plan_json` is the plan.
A node with a span still open is `COMPOSER_ERR_UNSATISFIABLE`; a node or section the plan does not have has the path `/node`
or `/section`. Schemas: `lock.request.schema.json`, `lock.response.schema.json`.

#### `composer_api_regenerate(request_json, job, out)`

Writes one node of a filled plan again and what depends on it ([cli-regenerate.md](cli-regenerate.md)). Members: `plan`,
the node as for `lock`, `seed`, and `midi`, `musicxml`, `plain_midi`, `tempo`. `json` is the document of `fill` with
`"reopened": {"sections": ["B"], "spans": {"harmony", "bass", "melody", "inner_voice"}}` after `realized`: what was
reopened. A locked or unwritable node is `COMPOSER_ERR_UNSATISFIABLE` with the reason; a stuck run is reported as for `fill`.
Schemas: `regenerate.request.schema.json`, `regenerate.response.schema.json`.

#### `composer_api_variations(request_json, job, out)` and `composer_api_harmonize(request_json, job, out)`

Several distinct versions of a node ([cli-variations.md](cli-variations.md)): the node is written again from the seeds
`seed`, `seed + 1`, ..., at most 4 times `count` of them, and a version is kept when it sounds different from the plan and
from the ones kept before. Members: `plan`, the node as for `lock` (`harmonize`: `section` only, whose whole melody is
Given), `count` (1 to 16, required), `seed`, and `musicxml`, `plain_midi`, `tempo`. `json`: `{"variations": [{"index", "seed",
"plan": <plan object>, "musicxml", "sections": [...]}], "kept", "requested", "seeds_tried"}`; the MIDI file of a version is
`composer_api_export` of its plan. No version that differs from the plan is `COMPOSER_ERR_UNSATISFIABLE`. Schemas:
`variations.request.schema.json`, `variations.response.schema.json` (both entries).

## Tests

`tests/test_api_core.c`, `tests/test_api_motive.c` (each GUI motive shape from a request, the response equal to the CLI
snapshot), `tests/test_api_variants.c` (the operation list, variants, recognize and random motives against their
snapshots, the random draws for seeds 0, 1, 2 and 4294967295, refusals with their paths, cancel) and
`tests/test_api_theme.c` (every `theme-*` snapshot case from its request, with the document, the plan, the MIDI file and the MusicXML equal to the snapshots; cancel; two threads running two cases each; refusals with their paths),
`tests/test_api_piece.c` (every `piece-*` snapshot case from its request, with the document, the plans, the MIDI file and the MusicXML equal to the snapshots, the variations with their MIDI files through the export entry, the lanes of a regenerate, cancel, refusals with their paths) and
`tests/test_api_plan_workflow.c` (the plan, fill, lock, regenerate, variations and harmonize cases of the CLI snapshots from their requests, the candidates, record and explanation, the report of a stuck plan, refusals with their paths, cancel) and
`tests/test_job_cancel.c`; `tests/validate_api_schema.cmake` validates every response the
tests write against its schema when python3 with `jsonschema` is present.
