# The Composer API

Everything an application asks of the Composer is a versioned in-process C API (R-219). The one public header is
`include/ursatz_composer/composer_api.h`. A request is JSON text, a response is a `ComposerResponse`; no Composer type, no
cJSON type and no file path (except style models and corpus directories) crosses it. The CLI is a client of the same
functions: it reads its arguments into a request, calls the API and prints the response (R-267).

This page covers every entry: the core, export, the motive entries, variants, recognize, random motives, themes, pieces, the
plan workflow, the material commands (progression, counterpoint, develop, grow), the vocabularies and request validation.
Each entry has its request and response here, an example under [Examples](#examples), and its JSON Schemas under
`schema/api/` (draft 2020-12). [Where the GUI's calls go](#what-replaces-each-gui-call) says which entry answers which call
of the GUI, and the CLI (`cli/`) includes no header but `composer_api.h` (`scripts/check-cli-api.sh`, run by CI).

A minimal client in C is in the [README](../README.md#using-the-api-from-c).

## Versioning

`COMPOSER_API_VERSION_MAJOR` and `COMPOSER_API_VERSION_MINOR`; `composer_api_version()` gives `"<major>.<minor>"`. This
is API 1.1 (1.0 was the first stable API, R-268).

**Compatibility rule.** The MAJOR version changes on any removed or changed member or entry: an entry that goes, a request
or response member that goes, changes meaning or type, or a value a member took that is refused now. The MINOR version
changes on additions: a new entry, a new optional request member, a new response member, a new value. Within one major
version a client written against 1.0 keeps working with 1.n, if it ignores response members it does not know. The Composer
version (`composer_api_versions`, the project version) is separate: it follows the releases, not the API.

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

The engine reports through the engine job hook in `src/util/job.h`: `job_step(job, stage, done, total)` calls `progress` and
returns whether the job is cancelled, and `job_cancelled(job)` asks `cancelled` alone. `fill_loop.c`, `compose_run.c`,
`grow_run.c` and the API files use these two and nothing else; `api/api_internal.h` is for `src/api/` only
(`scripts/check-layers.sh` in CI).

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
| `plan_file` | `true` to fill `plan_json` with the plan as the library writes plan files (docs/plan-format.md), which is how a client saves a plan object it holds; with no `midi` and no `musicxml` nothing is rendered. Default `false` |
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

### The material commands: `progression`, `counterpoint`, `develop`, `grow`

The commands that make material from a few options are entries too, so every command of the CLI is now a wrapper. The
options of each command are members of the request with the same names (a dash is an underscore: `keep_contour`,
`first_cadence`); a number may also be a string of digits, so a command line value goes in as it was typed. A motive is a
[motive request](#motive-requests) (`input`, `meter`, `key`, `pickup`, `harmony`, `features`, `role`), its file read by
the command: a MIDI file goes through `composer_api_motive_from_midi` first and its motive is the `json` input. Files are
never written by the API: the plan is a plan object (`"plans": true` puts it in each entry of the document, and `plan_json`
is the first), `musicxml` is rendered in memory (in each entry, and `musicxml` of the response for the first), and the MIDI
file of the first is `midi`; the MIDI file of another is `composer_api_export` of its plan. A refused option is the member of
the same name with its path.

#### `composer_api_progression(request_json, job, out)`

A chord progression for one phrase, with a bass line ([cli-progression.md](cli-progression.md)).

| Member | Value |
|---|---|
| `key`, `meter`, `bars`, `cadence` | required: `"D minor"`, `"4/4"` (2/4, 3/4, 4/4 or 6/8), 1 to 64, `PAC`, `IAC`, `HC`, `DC` or `PC` |
| `seed` | 0 to 4294967295, default from the clock. If no bass line suits the progression the next seeds are tried (up to 16) and the one that worked is the `seed` of the document |
| `embellish` | `none` (default), `low`, `medium`, `high` |
| `bass` | `false`: only the chords (not with `schema`, `sequence`, `midi` or `musicxml`) |
| `schema`, `sequence` | a galant schema or a harmonic sequence the phrase is built on |
| `suggest` | 1 to 8: that many alternatives instead of one |
| `plans`, `midi`, `musicxml`, `tempo`, `plain_midi` | the outputs, as above |
| `list_schemas` | `true` alone: the names instead of a progression |

`json`: `{"key", "meter", "bars", "cadence", "seed", "heard", "chords": [{"bar", "beat", "chord", "function", "bass"}]}`; with
`suggest`, `"alternatives": [{"heard", "chords"}]` in place of the last two; `plan` and `musicxml` in each entry when
asked, and `notices` (strings) when fewer alternatives than asked for were found. `list_schemas`: `{"schemas": [{"name",
"function", "modes"}], "sequences": [name]}`. Progress per seed tried (`"progression"`) and for each fill stage; a cancel
is `COMPOSER_ERR_CANCELLED`. A phrase too short for the cadence, a schema that cannot be written in the key and a request no
seed completes are `COMPOSER_ERR_UNSATISFIABLE`. Schemas: `progression.request.schema.json`,
`progression.response.schema.json`.

#### `composer_api_counterpoint(request_json, job, out)`

A counter-melody against a line in strict species ([cli-counterpoint.md](cli-counterpoint.md)). The request is the motive
request of the line (any length) with:

| Member | Value |
|---|---|
| `species`, `key`, `meter` | required: 1 to 4; a tonic alone with `mode` (`major` or `minor`), or tonic and mode; `"4/4"` |
| `position` | `below` (default: the line is the melody) or `above` (the line is the bass) |
| `seed`, `suggest` | 0 to 4294967295, default from the clock; 1 to 16 different counter-melodies |
| `plans`, `midi`, `musicxml`, `tempo`, `plain_midi` | the outputs, as above |

`json`: `{"species", "position", "key", "meter", "results": [{"seed", "line": [note], "counter_melody": [note], "groups":
[{"name", "hard_held", "soft_held"}]}]}` with `note` = `{"midi_pitch", "offset": {"num", "den"}, "duration": {"num",
"den"}}`, `plan` and `musicxml` in each result when asked and `notices` when fewer than `suggest` were found. A line the
species cannot be written against, or no counter-melody that holds the rules from the seeds tried (16 for each one asked
for), is `COMPOSER_ERR_UNSATISFIABLE`. Progress per seed (`"counterpoint"`). Schemas: `counterpoint.request.schema.json`,
`counterpoint.response.schema.json`.

#### `composer_api_develop(request_json, job, out)`

Motives related to one motive, best first ([cli-develop.md](cli-develop.md)). The request is the motive request with:

| Member | Value |
|---|---|
| `op` | required: `restate`, `invert`, `retrograde`, `interval`, `fragment`, `liquidate`, `augment` or `diminish` |
| `seed`, `suggest` | 0 to 4294967295 (default 0); 1 to 16 (default 5) |
| `by`, `degree`, `tonal`, `axis`, `interval`, `amount`, `keep_contour`, `position`, `length`, `min`, `drop`, `keep_rhythm`, `ratio` | the options of the operations; one the operation does not take is refused with its path |
| `midi`, `musicxml`, `tempo`, `plain_midi` | the files of the source and the best candidate; `plan_json` is their plan |

`json`: `{"source": "<note list>", "operation", "seed", "candidates": [{"index", "label", "score_numerator",
"score_denominator", "text"}]}`. A development that can be made from nothing is `COMPOSER_ERR_UNSATISFIABLE`. Progress once
(`"develop"`). Schemas: `develop.request.schema.json`, `develop.response.schema.json`.

#### `composer_api_grow(request_json, job, out)`

A theme grown from a motive ([cli-grow.md](cli-grow.md)). The request is the motive request, or, in place of `input`,
`corpus` (a corpus directory) with `like` (a passage file): the first bar of a similar passage becomes the motive and the
document reports it as `source` (not with `harmony`, `features` or `role`). The brief:

| Member | Value |
|---|---|
| `into`, `key`, `meter` | required: `sentence`, `period` or `hybrid`; the key (a tonic alone with `mode`); the meter |
| `hybrid`, `mode`, `bars`, `cadence`, `first_cadence`, `texture`, `voicing`, `accompaniment` | as the options |
| `accompaniment_motive` | a motive request of its own, read in the meter with the role `accompaniment` |
| `check` | `true`: pass over a seed the Analyzer contradicts |
| `style`, `style_weight`, `style_without`, `blend`, `blend_weight`, `blend_weights` | a style model path and its options (`style_without` is a list of at most 8 categories) |
| `seed`, `suggest` | 0 to 4294967295, default from the clock (a stuck seed passes to the next, up to 16); 1 to 16 different themes |
| `tempo`, `plain_midi`, `midi`, `musicxml`, `plans` | the outputs, as above; `musicxml` is also in each theme |
| `explain` | `text` or `json`: each theme's explanation as `explanation` |

`json`: what `grow --format json` prints, without the files, with `plan`, `musicxml` and `explanation` in each theme when
asked and `notices` (strings: stale documents of the corpus; fewer themes than asked for). Progress per compose stage and
per seed tried (`"grow"`); a cancel is `COMPOSER_ERR_CANCELLED` with no partial plan. Schemas: `grow.request.schema.json`,
`grow.response.schema.json`.

### Vocabularies: `composer_api_vocabulary(out)`

The words an application offers and the requests accept, in the order an application shows them, so it holds no table of its own (R-270: where the Composer and an application differ, the Composer's list wins). No request. `json`:

| Member | Entries |
|---|---|
| `meters` | `2/4 3/4 4/4 6/8`: `{"value", "label", "beats", "chord_onset_step": "1/2", "second_chord_beat"}`. A theme chord starts on any half beat of a bar and, where the bar takes two chords, the second on `second_chord_beat` (4/4: 2, 6/8: 3; absent for 2/4 and 3/4). |
| `modes`, `piece_modes` | `major`, `minor`; `piece_modes` adds `style` ("From the brief"). |
| `keys` | the 24 keys as the Composer spells a tonic: C, Db, D, Eb, E, F, F#, G, Ab, A, Bb, B major, then C, C#, D, D#, E, F, F#, G, G#, A, Bb, B minor. |
| `pitch_class_names` | `{"value": 0 to 11, "major", "minor"}`: the tonic of each pitch class in each mode. |
| `cadences`, `theme_cadences` | `PAC IAC HC DC PC` with their names; a theme ends in `PAC`, `IAC` or `HC`. |
| `pickups` | `{"<meter>": ["0", "1/4", "1/2", "1", "3/2", "2", "3"]}`: the pickups offered in beats, each a number a reader accepts in that meter (shorter than a bar). Any other multiple of 1/32 of a whole note shorter than a bar is accepted too. |
| `motive_forms`, `roles`, `functions`, `distances` | `full rhythm contour`; `melodic accompaniment`; `beginning continuation contrast cadential`; `close medium far`. |
| `theme_slots`, `theme_ideas`, `theme_motive_categories`, `theme_pools` | the four slots; the ten ideas, each with its `slot`; the style categories a theme can read without (`motive-degrees`, `motive-intervals`, `motive-rhythm`, `motive-nct-figure`); the pools `approved-and-unflagged`, `approved`, `none` (the Composer reads no pool: they are the application's rule for which saved motives it offers, listed so the application keeps no table). |
| `theme_bars`, `theme_first_cadences`, `style_weights` | the bars a theme has (`4`, `8`); the cadences a period's first phrase ends in (`IAC`, `HC`, in the order of `cadences`); the style weights offered (`1/4` Light, `1/2` Normal, `1` Strong; any fraction the reader accepts is still accepted). Theme modes are the existing `piece_modes`. |
| `theme_beginnings`, `theme_endings` | `{"value", "label", "ideas": [<theme_ideas value>, ...]}`: `presentation antecedent cbi` and `continuation consequent cadential`, each with its ideas in order. A hybrid is written `<beginning>+<ending>`; these are the words its reader takes. |
| `accompaniment_patterns` | `any block arpeggiated alberti oom-pah motive`. |
| `piece_parts`, `piece_intos`, `fill_kinds` | `intro body turnaround ending`; `sentence period hybrid`; `empty motif theme progression`. |
| `tone_labels` | `chord` and the ten non-chord tone types the analysis writes, each with an `abbreviation` (`CT`, `PT`, `NT`, ...). |
| `variant_operations` | what `composer_api_variant_operations` gives. |
| `limits` | the sizes a request is held to, as numbers: `motive_notes` (64), `motive_chords` (8), `motive_features` (8), `variant_chain` (4), `variant_functions` (4), `random_motive_count` (16); since 1.1 also `tempo_min` (20), `tempo_max` (400), `piece_sections` (32), `piece_beats` (4096) and `seed_max`, an object of the largest seed each request reads: `{"variants": 4294967231, "random_motive": 4294967295, "theme": 4294967295, "piece": 4294967295}`. A limit the Composer does not hold (cells of a chord in a piece section) is not listed. The command line shows the first six in its help and holds repeated options to them. |

Every list but those noted is `[{"value", "label"}]`, and every value is accepted by the reader that reads it (`tests/test_api_vocabulary.c` builds a request from each one). The lists come from the parser or table that reads them; the tables of the words that are not a library's are in `src/api/api_vocabulary.c`.

Differences from the GUI tables it replaces (Ursatz-GUI `049adaf`; the Composer's list wins):

- `keys`, `pitch_class_names`: the GUI offers one list of tonics (`C C# D Eb E F F# G Ab A Bb B`) for both modes. The Composer spells a major tonic Db (not C#) and a minor tonic C#, D# and G# (not Eb and Ab).
- `pickups`: the GUI offers `0 1/4 1/2 1 3/2 2 3` for every meter; the Composer drops the ones that are not shorter than a bar (2/4 has no `2` or `3`, 3/4 no `3`).
- `accompaniment_patterns`: the GUI has five patterns; the Composer adds `motive` (a pattern read from a motive of the accompaniment role).
- `tone_labels`: the GUI has an "Automatic" choice first (no override); the Composer lists none, a note without an override is one the request leaves out. The abbreviations are the GUI's, now served.
- `motive_forms`: no `enabled` flag; the three forms are all read.
- `cadences`, `meters`, `roles`, `piece_parts`, `piece_intos`, `fill_kinds`, `theme_slots`, `theme_pools`, `theme_motive_categories`: the same values as the GUI. Labels are the GUI's where it has one.

### Validation: `composer_api_validate(kind, request_json, out)`

Reads a request as its entry does and composes nothing, so an application can check a form and show the Composer's defaults. `kind`: `motive` (the request of `composer_api_motive`), `theme` (of `composer_api_theme`), `piece` (the piece request object of `docs/piece-request.md`, not the `{"request", ...}` wrapper of `composer_api_piece`) and `random_motive` (of `composer_api_random_motive`). `json`: `{"request": <the request with the Composer's defaults filled>}`:

- `motive`: `meter` (the motive's, when the request has none and the input declares one), `role` (`melodic`), `input.format` (`auto`) and `input.form` (`full`) when absent; `harmony` as `[{"numeral", "onset"}]`, the numerals as the library writes them (`V7`, `bVII65`) and the onsets in beats of the meter in the request syntax (`"0"`, `"3/2"`). Own harmony takes upper case numerals with an optional figure and `@onset` (`I V@3/2 V7@3`); a slash chord or a lower case numeral is refused at `/harmony`.
- `piece`: the `cadence` of every section that has none (`piece_structure_default_cadences`: the last of the intro and of the loop `HC`, the last ending `PAC`, any other `IAC`) and the default ending section (`end`, `PAC`, an empty fill) when the request has no ending.
- `random_motive`: the `seed` (the request's, or the one read from the clock) and the `mode` and `meter` a `"random"` drew from it (R-269).
- `theme`: the request as it is.

A request an entry would refuse is refused the same way, with its message and path. An unknown `kind` is `COMPOSER_ERR_REQUEST` with the path `/kind`. A style model named in the request is read.

## Error paths

An error never comes with a document, except the stuck report of `composer_api_fill` and `composer_api_regenerate`. What
the path says:

| Error | Status | `path` |
|---|---|---|
| text that is not JSON, or not an object | `COMPOSER_ERR_REQUEST` | `""` |
| `"api_version"` of another major | `COMPOSER_ERR_REQUEST` | `/api_version` |
| a member of the wrong type or value | `COMPOSER_ERR_REQUEST` | the member: `/tempo`, `/features/1`, `/input/format`, `/given/0/at` |
| a plan that does not read | `COMPOSER_ERR_REQUEST` | `/plan` and the place in the plan: `/plan/form/children/1/span` |
| a section or node the plan does not have | `COMPOSER_ERR_REQUEST` | `/section`, `/node` |
| an input that is a motive but not a usable one (a pickup the bar cannot hold, notes sounding together) | `COMPOSER_ERR_UNSATISFIABLE` | `/input/text` |
| a file the request names that cannot be read (a style, a corpus) | `COMPOSER_ERR_UNSATISFIABLE` | `/style`, `/corpus`, `/like` |
| a valid request no seed can satisfy | `COMPOSER_ERR_UNSATISFIABLE` | `""` |
| the job's `cancelled` returned true | `COMPOSER_ERR_CANCELLED` | `""` |

`message` is for a person: it names the member by its request name (`tempo must be from 20 to 400`). A client that shows the
error next to a form field uses `path`.

## Job rules

The stage each entry reports at the start (`done` of `total`), besides the compose stages of the Jobs section:

| Entry | Stage |
|---|---|
| `composer_api_export` | `export` |
| `composer_api_motive`, `composer_api_motive_from_midi`, `composer_api_motive_render` | `motive` |
| `composer_api_variants` | `variants` (one step for each variant tried) |
| `composer_api_recognize` | `recognize` |
| `composer_api_random_motive` | `random-motive` |
| `composer_api_theme`, `composer_api_grow` | `grow` (one step for each seed tried) |
| `composer_api_piece`, `composer_api_fill`, `composer_api_regenerate` | `seed` (one step for each seed tried) |
| `composer_api_variations`, `composer_api_harmonize`, `composer_api_piece_variations` | `variations` |
| `composer_api_progression` | `progression` |
| `composer_api_counterpoint` | `counterpoint` |
| `composer_api_develop` | `develop` |

`composer_api_versions`, `composer_api_variant_operations`, `composer_api_vocabulary`, `composer_api_validate`,
`composer_api_plan` and `composer_api_lock` are quick: they report nothing and never return `COMPOSER_ERR_CANCELLED`. A
cancel is honored at the next report; the entry frees what it built and returns `COMPOSER_ERR_CANCELLED` with no document
and no plan. Progress and cancel run on the thread that called the entry: a client that shows progress from another thread
hands the value over itself.

## Examples

The examples come from runs of this API (the MIDI entry and the piece variations are written from `tests/test_api_motive.c` and `tests/test_api_piece.c`); a long value is shortened with `…`. A plan object is `{…}`: the plan file of the
command line, read as JSON (docs/plan-format.md).

### Versions

```
composer_api_versions(&out)
json: {"composer_version":"0.4.0","api_version":"1.1","analyzer_version":"0.6.0","ursatz_commit":"d2b85ce…","plan_format_version":2}
```

### Export

```
request: {"plan": {…}, "midi": true, "tempo": 120}
status:  COMPOSER_OK    midi, midi_size filled
json:    {"skipped":[{"span":3,"lane":"melody","reason":"source material"}, …]}

request: {"plan": {…}, "plan_file": true}
status:  COMPOSER_OK    plan_json filled with the plan file text, nothing rendered
```

### Motive, from MIDI and render

```
request: {"input": {"format": "notes", "text": "D4:q E4:q F4:q G4:q"}, "meter": "4/4", "key": "D major"}
status:  COMPOSER_OK
json:    {"motive": {"meter": {"beats_per_bar": 4, "beat_unit": 4, …}, "key": {"tonic_pitch_class": 2, "mode": "major"},
          "notes": [{"midi_pitch": 62, "offset": {"num": 0, "den": 1}, "duration": {"num": 1, "den": 4}}, …]},
          "analysis": {"notes_text": "D4:q E4:q F4:q G4:q", "degrees_text": "1:q 2:q #2:q 4:q", …}}

composer_api_motive_from_midi(bytes, size, "{\"key\": \"D minor\"}", NULL, &out)    the same document for a MIDI file

request: the motive request above plus {"render_key": "G minor", "tempo": 90, "musicxml": true}
json:    the document with "rendered": {"key": "G minor", "notes": [{"midi_pitch", "offset", "duration"}, …]}
         musicxml filled
```

### Variants, recognize and random motives

```
composer_api_variant_operations(&out)
json: {"operations": [{"name": "retrograde", "label": …, "category": …, "params": [], "forms": ["full"], …}, …]}

request: {"input": {"format": "notes", "text": "D4:q E4:q F4:q G4:q"}, "meter": "4/4", "key": "D major", "count": 2, "seed": 1}
json:    {"seed": 1, "source": {"motive": {…}, "analysis": {…}}, "variants": [{"seed": 1, "chain": {"steps": […]}, "kept": […],
          "changed": […], "derivation": "…", "motive": {…}}, …]}

request: {"source": {"input": {"format": "notes", "text": "D4:q E4:q F4:q G4:q"}},
          "target": {"input": {"format": "notes", "text": "G4:q F4:q E4:q D4:q"}}, "meter": "4/4", "key": "D major"}
json:    {"found": true, "chain": {"steps": [{"op": "retrograde", "params": {}}]}, "distance": 0.3333, "remoteness": 0.3333,
          "kept": ["rhythm", "harmony"], "changed": ["intervals", "contour"],
          "derivation": "Put in retrograde. Kept the rhythm and the harmony; changed the intervals and the contour."}

request: {"mode": "major", "meter": "4/4", "seed": 7}
json:    {"seed": 7, "motives": [{"motive": {…}, "analysis": {…}}]}
```

### Theme and piece

```
request: a theme request v2 (theme-request.md) with "seed": 1, "suggest": 1, "midi": true
status:  COMPOSER_OK    midi, midi_size and plan_json filled
json:    {"key": "D minor", "meter": "4/4", "bars": 8, "into": "sentence", "pairing": "presentation + continuation",
          "cadence": "PAC", "themes": [{"seed": 1, "structure": […], "chords": […], "score": …}], …}

request: {"request": {"schema": "ursatz-piece-request", "schema_version": 1, "key": "D", "mode": "minor", "meter": "4/4",
          "sections": […]}, "seed": 1, "midi": true}
json:    {"schema": "ursatz-piece", "seed": 1, "key": "D minor", "loop": {"start": "2/1", "end": "16/1"},
          "versions": {"loop": {…}, "ending": {…}}, "plans": {"loop": {…}, "ending": {…}}}

request: {"request": {…}, "from": {…the loop plan…}, "variations": {"section": "a", "lanes": ["bass"], "count": 3}, "seed": 1}
json:    {"schema": "ursatz-piece-variations", "section": "a", "variations": [{"index": 1, "seed": 1, "loop": {…}, …}], "kept": 3, …}
```

### The plan workflow

```
request: {"form": "binary", "key": "C", "mode": "major", "meter": "4/4", "seed": 1}      composer_api_plan
json:    {"form": "binary", "bars": 16, "key": "C major", "sections": [{"letter": "A", "bars": 8, …}, {"letter": "B", …}], …}
         plan_json: the plan

request: {"plan": {…}, "seed": 1}                                                            composer_api_fill
json:    {"seed": 1, "realized": false, "sections": [{"letter": "A", "heard": "heard as sentence", "heard_as_requested": true,
          "cadences": ["half", "perfect-authentic"]}, …], "returns": {"planned": 0, "heard": 0, "pairs": []}}
         plan_json: the realized plan

request: {"plan": {…}, "section": "A"}                                                       composer_api_lock
json:    {"locked": {"nodes": 9, "spans": 19}, "what": "section A"}
         plan_json: the plan with the section Given

request: {"plan": {…}, "section": "A", "seed": 2}                                            composer_api_regenerate
json:    {"seed": 2, "realized": false, "reopened": {"sections": ["A"], "spans": {…}}, "sections": […], …}

request: {"plan": {…}, "section": "A", "count": 2, "seed": 1}                                composer_api_variations
json:    {"variations": [{"index": 1, "seed": 1, "plan": {…}, "sections": […]}, …], "kept": 2, "requested": 2, "seeds_tried": …}
```

`composer_api_harmonize` takes the request of `composer_api_variations` with a `section` whose melody is Given.

### The material commands

```
request: {"key": "C major", "meter": "4/4", "bars": 4, "cadence": "PAC", "seed": 1}         composer_api_progression
json:    {"key": "C major", "meter": "4/4", "bars": 4, "cadence": "PAC", "seed": 1, "heard": "perfect-authentic",
          "chords": [{"bar": 1, "beat": 1, "chord": "I", "function": "tonic", "bass": "C3"}, …]}

request: {"input": {"format": "notes", "text": "D4:w F4:w E4:w D4:w …"}, "species": 1, "key": "D", "mode": "minor",
          "meter": "4/4", "position": "above", "seed": 1}                                    composer_api_counterpoint
json:    {"species": 1, "position": "above", "key": "D minor", "meter": "4/4",
          "results": [{"seed": 1, "line": […], "counter_melody": […], "groups": […]}]}

request: {"input": {"format": "notes", "text": "D4:q E4:q F4:q G4:q"}, "meter": "4/4", "key": "D major",
          "op": "invert", "seed": 0, "suggest": 2}                                           composer_api_develop
json:    {"source": "D4:q E4:q F4:q G4:q", "operation": "invert", "seed": 0, "candidates": [{"index": 1,
          "label": "invert about the first note", "score_numerator": 5, "score_denominator": 8, "text": "D4:q C4:q B3:q A3:q"}, …]}

request: {"input": {"format": "notes", "text": "D4:q E4:q F4:q G4:q"}, "meter": "4/4", "key": "D", "mode": "major",
          "into": "sentence", "bars": 8, "cadence": "PAC", "seed": 1}                        composer_api_grow
json:    {"key": "D major", "meter": "4/4", "bars": 8, "into": "sentence", "pairing": "presentation + continuation",
          "cadence": "PAC", "themes": [{"seed": 1, "structure": […], …}], …}
```

### Vocabularies and validation

```
composer_api_vocabulary(&out)
json: {"meters": […], "modes": […], "keys": […], …, "limits": {"motive_notes": 64, "motive_chords": 8, "motive_features": 8,
       "variant_chain": 4, "variant_functions": 4, "random_motive_count": 16, "tempo_min": 20, …}}

composer_api_validate("random_motive", "{\"mode\": \"random\", \"meter\": \"random\", \"seed\": 5}", &out)
json: {"request": {"seed": 5, "mode": "major", "meter": "4/4"}}
```

## What replaces each GUI call

The GUI ran the command line once per request. Each call below becomes a call of the entry in the last column (phase P27
moves the GUI over; until then it runs the CLI at its old pin, whose interface did not change). File and line are in
Ursatz-GUI `049adaf`.

| Shape | GUI call site | CLI | Entry |
|---|---|---|---|
| version | `routes/composer.c:83` | `--version` | `composer_api_versions` |
| motive analyze (notes or form text, harmony, features, role, tone overrides) | `routes/materials.c:560` | `motive <file> --format notes\|--form <f> ... --json` | `composer_api_motive` |
| degrees to notes | `routes/materials.c:755` | `motive <file> --format degrees ... --json` | `composer_api_motive` |
| MIDI to motive | `routes/materials.c:875` | `motive <file> --format midi ... --json` | `composer_api_motive_from_midi` |
| motive render | `routes/materials.c:1200` | `motive ... --render-key --musicxml <out> --tempo --json` | `composer_api_motive_render` |
| operation list | `routes/materials.c:1909` | `variants --list-ops --json` | `composer_api_variant_operations` |
| variants | `routes/materials.c:2026` | `variants <file> ... --basic --function --distance --count --seed --json` | `composer_api_variants` |
| recognize | `routes/materials.c:2357` | `recognize <source> <target> ...` | `composer_api_recognize` |
| random motive | `routes/materials.c:3215` | `random-motive --mode --meter --key --form --role --style --count --seed --json` | `composer_api_random_motive` |
| theme | `service/theme_service.c:642` | `theme <request.json> --format json --out --midi --musicxml` | `composer_api_theme` |
| piece compose and regenerate | `service/piece_service.c:302` | `piece [--from] --request [--regenerate --lane]` | `composer_api_piece` |
| piece variations | `service/piece_service.c:961` | `piece --from --request --variations --lane --count` | `composer_api_piece_variations` |
| the GUI's vocabulary tables, cadence defaults, random draws | the tables in `materials-model.js` and the services | none | `composer_api_vocabulary`, `composer_api_validate`, and `"random"` mode and meter |

The commands the GUI does not run (`plan`, `fill`, `lock`, `regenerate`, `variations`, `harmonize`, `progression`,
`counterpoint`, `develop`, `grow`, `export`) have their entries for the same reason: the command line is a client of the API
in every command.

## Tests

`tests/test_api_core.c`, `tests/test_api_motive.c` (each GUI motive shape from a request, the response equal to the CLI
snapshot), `tests/test_api_variants.c` (the operation list, variants, recognize and random motives against their
snapshots, the random draws for seeds 0, 1, 2 and 4294967295, refusals with their paths, cancel) and
`tests/test_api_theme.c` (every `theme-*` snapshot case from its request, with the document, the plan, the MIDI file and the MusicXML equal to the snapshots; cancel; two threads running two cases each; refusals with their paths),
`tests/test_api_piece.c` (every `piece-*` snapshot case from its request, with the document, the plans, the MIDI file and the MusicXML equal to the snapshots, the variations with their MIDI files through the export entry, the lanes of a regenerate, cancel, refusals with their paths) and
`tests/test_api_plan_workflow.c` (the plan, fill, lock, regenerate, variations and harmonize cases of the CLI snapshots from their requests, the candidates, record and explanation, the report of a stuck plan, refusals with their paths, cancel),
`tests/test_api_material.c` (the progression, counterpoint and grow cases of the CLI snapshots from their requests, with the documents, the plans and the MIDI files equal to the snapshots; develop; the corpus seed; refusals with their paths; progress and cancel) and
`tests/test_api_vocabulary.c` (every vocabulary list non-empty and each value accepted by its reader, the default cadences of three piece structures, the parsed harmony, refusals with their paths) and
`tests/test_job_cancel.c`; `tests/validate_api_schema.cmake` validates every response the
tests write against its schema when python3 with `jsonschema` is present.
