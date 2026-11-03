# The Composer API

Everything an application asks of the Composer is a versioned in-process C API (R-219). The one public header is
`include/ursatz_composer/composer_api.h`. A request is JSON text, a response is a `ComposerResponse`; no Composer type, no
cJSON type and no file path (except style models and corpus directories) crosses it. The CLI is a client of the same
functions: it reads its arguments into a request, calls the API and prints the response (R-267).

This page covers the core, export and the motive entries. Each later branch of phase P26 adds its entries here and its JSON Schemas
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

## Tests

`tests/test_api_core.c`, `tests/test_api_motive.c` (each GUI motive shape from a request, the response equal to the CLI
snapshot) and `tests/test_job_cancel.c`; `tests/validate_api_schema.cmake` validates every response the
tests write against its schema when python3 with `jsonschema` is present.
