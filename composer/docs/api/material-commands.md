# Material command entries

Material command entries of the Composer API: progression, counterpoint, develop and grow. Back to the [docs index](../README.md).

## The material commands: `progression`, `counterpoint`, `develop`, `grow`

The commands that make material from a few options are entries too, so every command of the CLI is now a wrapper. The
options of each command are members of the request with the same names (a dash is an underscore: `keep_contour`,
`first_cadence`); a number may also be a string of digits, so a command line value goes in as it was typed. A motive is a
[motive request](motives.md#motive-requests) (`input`, `meter`, `key`, `pickup`, `harmony`, `features`, `role`), its file read by
the command: a MIDI file goes through `composer_api_motive_from_midi` first and its motive is the `json` input. Files are
never written by the API: the plan is a plan object (`"plans": true` puts it in each entry of the document, and `plan_json`
is the first), `musicxml` is rendered in memory (in each entry, and `musicxml` of the response for the first), and the MIDI
file of the first is `midi`; the MIDI file of another is `composer_api_export` of its plan. A refused option is the member of
the same name with its path.

### `composer_api_progression(request_json, job, out)`

A chord progression for one phrase, with a bass line ([cli-progression.md](../cli-progression.md)).

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

### `composer_api_counterpoint(request_json, job, out)`

A counter-melody against a line in strict species ([cli-counterpoint.md](../cli-counterpoint.md)). The request is the motive
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

### `composer_api_develop(request_json, job, out)`

Motives related to one motive, best first ([cli-develop.md](../cli-develop.md)). The request is the motive request with:

| Member | Value |
|---|---|
| `op` | required: `restate`, `invert`, `retrograde`, `interval`, `fragment`, `liquidate`, `augment` or `diminish` |
| `seed`, `suggest` | 0 to 4294967295 (default 0); 1 to 16 (default 5) |
| `by`, `degree`, `tonal`, `axis`, `interval`, `amount`, `keep_contour`, `position`, `length`, `min`, `drop`, `keep_rhythm`, `ratio` | the options of the operations; one the operation does not take is refused with its path |
| `midi`, `musicxml`, `tempo`, `plain_midi` | the files of the source and the best candidate; `plan_json` is their plan |

`json`: `{"source": "<note list>", "operation", "seed", "candidates": [{"index", "label", "score_numerator",
"score_denominator", "text"}]}`. A development that can be made from nothing is `COMPOSER_ERR_UNSATISFIABLE`. Progress once
(`"develop"`). Schemas: `develop.request.schema.json`, `develop.response.schema.json`.

### `composer_api_grow(request_json, job, out)`

A theme grown from a motive ([cli-grow.md](../cli-grow.md)). The request is the motive request, or, in place of `input`,
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
