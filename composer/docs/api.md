# The Composer API

Everything an application asks of the Composer is a versioned in-process C API (R-219). The one public header is
`include/ursatz_composer/composer_api.h`. A request is JSON text, a response is a `ComposerResponse`; no Composer type, no
cJSON type and no file path (except style models and corpus directories) crosses it. The CLI is a client of the same
functions: it reads its arguments into a request, calls the API and prints the response (R-267).

This page covers the core, export, the motive entries, variants, recognize, random motives and themes. Each later branch of phase P26 adds its entries here and its JSON Schemas
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

## Tests

`tests/test_api_core.c`, `tests/test_api_motive.c` (each GUI motive shape from a request, the response equal to the CLI
snapshot), `tests/test_api_variants.c` (the operation list, variants, recognize and random motives against their
snapshots, the random draws for seeds 0, 1, 2 and 4294967295, refusals with their paths, cancel) and
`tests/test_api_theme.c` (every `theme-*` snapshot case from its request, with the document, the plan, the MIDI file and the MusicXML equal to the snapshots; cancel; two threads running two cases each; refusals with their paths) and
`tests/test_job_cancel.c`; `tests/validate_api_schema.cmake` validates every response the
tests write against its schema when python3 with `jsonschema` is present.
