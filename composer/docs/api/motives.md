# Motive entries

Motive entries of the Composer API: core export, motive requests, motive, MIDI, render, variants, recognize and random motive. Back to the [docs index](../README.md).

## `composer_api_export(request_json, job, out)`

Renders a plan once and exports it, as `ursatz-composer export` does.

| Member | Value |
|---|---|
| `plan` | the plan object (docs/plan-format.md) |
| `midi` | `true` to fill `midi` and `midi_size`; default `false` |
| `musicxml` | `true` to fill `musicxml`; default `false` |
| `tempo` | quarter notes per minute from 20 to 400, a whole number or a string like `"195/2"`; `null` or absent for none |
| `plain_midi` | `true` to leave out the MIDI track names and section markers; default `false` |
| `plan_file` | `true` to fill `plan_json` with the plan as the library writes plan files (docs/plan-format.md), which is how a client saves a plan object it holds; with no `midi` and no `musicxml` nothing is rendered. Default `false` |
| `loop` | `{"start": "2/1", "end": "14/1"}`, times in whole notes from the start of the plan: the `loopStart` and `loopEnd` markers of the MIDI file ([export.md](../export.md#loop-markers)); the end must come after the start |

`json`: `{"skipped": [{"span", "lane", "reason"}]}`, the lane spans the render left out, with the reasons the CLI prints on
stderr (`"not Given or Realized"`, `"source material"`, `"not in the layer"`, `"seam"`, `"lane is not rendered yet"`).
A plan that does not read is `COMPOSER_ERR_REQUEST` with the path of the member (`/plan/format`); a plan with no Given or
Realized note is `COMPOSER_ERR_REQUEST` with `/plan`. Schema: `schema/api/export.schema.json`.

## Motive requests

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

Key and meter text. Every entry reads a `key` and a `meter` with one parser (`request_parse_key`, `request_parse_meter`): a key is a tonic letter, an optional `#` or `b`, a space and `major` or `minor` in any case (`"D minor"`, `"F# major"`); a meter is whole numbers only, 1 to 64 beats over a beat unit of 1, 2, 4, 8 or 16 (`"3/4"`, `"6/8"`), with no sign and no white space.

## `composer_api_motive(request_json, job, out)`

Analyzes a motive, as `ursatz-composer motive <input> --json` does. `json`: the document that command prints,
`{"motive", "analysis"}`: the motive in the motive-document format and its analysis (intervals, contour, rhythm, range,
scale degrees, implied harmony, base harmony, the segments and the tone labels with their warnings). `analysis.segments` is `{"given": bool, "items": [{"label", "first", "last"}]}`: the motive's own `features` when it has any, otherwise a proposal (API 1.2; see [motive-format.md](../motive-format.md#segments)); every response that carries a motive analysis has it. Schema:
`schema/api/motive.response.schema.json`.

## `composer_api_motive_from_midi(bytes, size, request_json, job, out)`

The same for a motive that is a Standard MIDI File (type 0 or 1), read in memory. The request has the members above
without `input`. A buffer that is not a MIDI file, or holds notes sounding together, is `COMPOSER_ERR_UNSATISFIABLE`.

## `composer_api_motive_render(request_json, job, out)`

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

## `composer_api_variant_operations(out)`

The variant operations of the registry, as `ursatz-composer variants --list-ops --json` prints them: for each its `name`,
`label`, `category`, `citation`, `relation`, `params` (`name`, `kind`, `min`, `max`, `description`, `optional`) and `forms`.
Schema: `schema/api/variant_operations.schema.json`.

## `composer_api_variants(request_json, job, out)`

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

## `composer_api_recognize(request_json, job, out)`

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

## `composer_api_random_motive(request_json, job, out)`

Random motives that keep the hard melodic rules of standard counterpoint; their length, pickup, number of notes, rhythm and
range are drawn from defaults in the Composer.

| Member | Value |
|---|---|
| `mode` | `"major"`, `"minor"` or `"random"`. Required |
| `meter` | `"4/4"`, `"3/4"`, `"2/4"`, `"6/8"` or any meter, or `"random"`. Required |
| `key` | `"D minor"`; its mode must be the mode (`/key`). Default C major or A minor |
| `form` | `"full"` (default), `"rhythm"` or `"contour"` |
| `strategy` | `"segments"` (default) or `"drawn"`: how the motive is made (below). Anything else is a refusal with `/strategy` |
| `role` | `"melodic"` or `"accompaniment"` |
| `style` | a style model file: 8 candidates are drawn for each motive and the closest to it kept. A file that cannot be read is `COMPOSER_ERR_UNSATISFIABLE` with `/style` |
| `style_without` | up to 8 style categories read without; needs `style` |
| `count` | 1 to 16, default 1 |
| `seed` | 0 to 4294967295, default from the clock |

**Strategy.** `"segments"` builds each motive the way a motive is made, from segments (Schoenberg's motives): one to three
segments, `a`, its repeat `a'` on the next chord and a new `b`, with one chord on each segment from a harmony plan (`I V`,
`I IV`, `I I`, `I V I`, `I IV I`) and an ending that slows down on a chord tone of the last chord. The motive comes with its
harmony and its segments as its own, so in the response `analysis.base_harmony.given` and `analysis.segments.given` are
true and the motive's `harmony` and `features` are set; a rhythm or contour motive keeps them too. `"drawn"` is the earlier
strategy: a rhythm drawn note by note over a frame of chord tones of I, with no harmony or features of its own (the harmony
and segments in the analysis are proposed). The vocabulary serves the names as `random_motive_strategies`.

**Random mode and meter (R-269).** `"random"` is drawn from the seed by the rule the GUI used before the Composer drew, so a
GUI seed draws what it drew: `m = ((seed + 1) * 2654435761) mod 2^32`; the mode is major when `(m >> 8)` is even and
minor when it is odd; the meter is `"4/4"`, `"3/4"`, `"2/4"` or `"6/8"` for `(m >> 16) mod 4` of 0, 1, 2 and 3. Seed 0 draws
minor 6/8, seed 1 and seed 2 minor 2/4, and seed 4294967295 (where `seed + 1` wraps to 0) major 4/4. The response says what
was drawn.

`json`: `{"seed", "motives": [<the motive entry's document>]}` and `"drawn": {"mode", "meter"}`, with only the members that
were drawn, when something was. The same request and seed give the same motives. Schemas:
`random_motive.request.schema.json`, `random_motive.response.schema.json`.
