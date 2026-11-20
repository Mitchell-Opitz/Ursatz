# Theme and piece entries

Theme, piece and piece variations entries of the Composer API. Back to the [docs index](../README.md).

## `composer_api_theme(context, request_json, job, out)`

Grows a theme from the theme request v2 of [theme-request.md](../theme-request.md) (`"schema": "ursatz-theme-request"`,
`"schema_version": 2`). Its motives are inline: each motive object carries its content in `degrees`, a motive with a `path`
or a `file` member is refused with `URSATZ_ERR_REQUEST` and the path `/motives/<n>/path` (the caller reads files). A
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
| `preferences` | the settings document of [preferences.md](../preferences.md) (API 1.12): its `melody` group and the `development` fidelity minimums reach the grow; from API 1.17 its `development` group's `continuation` settings and `basic_as_written` reach the grow too; from API 1.19 so do `melody.closure`, `rhythm_repeat_bars`, `leaps`, `register` and `development.contrast`, and its `motif` group shapes the theme's drawn rhythms and new material (and nothing else, because the motives come from the request). A bad member is refused with its path under `/preferences` |
| `midi`, `musicxml` | `true`: the first theme's MIDI file (`midi`, `midi_size`) and MusicXML (`musicxml`), rendered once from the same `RenderedPlan` |
| `plain_midi` | `true`: no track names and no section markers in the MIDI file |
| `layers` | `true`: the first theme's `realization` and `reading` in the document ([layers](#layers)) |
| `plans` | `true`: each theme's plan as an object, `themes[k].plan` (the CLI writes the plans, MIDI files and MusicXML of the themes after the first from it, through `composer_api_export`) |
| `explain` | `"text"` or `"json"`: why each candidate was chosen or rejected, as a string, `themes[k].explanation` |

A request takes `bars` 3 to 16 and a `construction` (API 1.18, [theme-request.md](../theme-request.md)): `null`, or the bars of each idea and its extensions. A refusal of either carries the grow refusal's path under `/bars` or `/construction`.

The request has no `corpus` member: a theme is grown from its motives only.

`json`: what `theme --format json` prints, without the `files` it wrote: `{"key", "meter", "bars", "into", "pairing",
"cadence", "first_cadence", "construction", "accompaniment", "texture", "voicing", "tempo": {"qpm", "from"}, "brief": {"tempo", "material",
"density", "without"}, "chords": [{"bar", "text"}], "themes": [{"seed", "structure", "construction", "chords", "developments", "slots",
"score", "requested", "heard", "heard_as_requested", "heard_text", "style_distance"}], "preferences"}`, the last when the request had the member (API 1.11: the full document with the defaults filled in). `construction` (API 1.18): at the root the construction as the request gave it, `{"ideas", "extensions"}` (only when it gave one); in each theme `null` for a theme without one, else `{"halves": [h1, h2], "ideas": [{"idea", "bars", "first_bar"}], "extensions": [{"kind", "idea", "bars", "first_bar", "varied"}], "reason": "given"|"motive"|"length", "text"}`, bars counted from 1: `halves` the two halves' bars (the second with the idea an evaded cadence brings back), `ideas` in order with a stretched idea's stretch counted in its bars, an extension's `bars` and `first_bar` explicit (a stretch spans its idea; `idea` is `null` for an addition and an evaded cadence), and `text` the sentence of [grow-construction.md](../grow-construction.md#choosing-a-construction) that says why the theme is as long as it is. `plan_json`: the plan of the first
theme. Each item of `developments` is `{"motive", "idea", "development", "segment", "summary", "fidelity", "minimum", "heard", "heard_agrees"}`: `segment` is the label of the motif's segment the idea is made from or `null`, `summary` one line naming the idea, its source and its operation (API 1.8, [explain.md](../explain.md#the-summary-line-of-a-developed-idea)). `fidelity` is how close the notes the melody holds are to the idea's source, from 0 to 1 (three decimals); `minimum` is the least the melody stage accepts for the idea's role under the development setting, 0 for an idea whose operation makes it differ from its source (a liquidation, a fragment by notes, an inversion), which is ranked and never refused; `heard` is the relation the recognizer hears between the two, in words (`"exact"`, `"transposed -1 step of the key's scale"`, `"not recognized"`), and `heard_agrees` whether it is the one the operation intended. All four are `null` for an idea with no melody to measure (API 1.10, [explain.md](../explain.md#intended-against-heard)). The basic idea is not an item. Progress is reported once for each seed tried (`"grow"`) and for each compose stage of the run; a cancel is
`URSATZ_ERR_CANCELLED` with no document and no plan. A request no seed can write is `URSATZ_ERR_UNSATISFIABLE`
(`the given chords in bar <n> do not allow the PAC`, `no theme could be grown for ...`). The same request and seed give the
same theme on any thread; the entry keeps no state, so requests may run at the same time. Schemas:
`theme.request.schema.json`, `theme.response.schema.json`.

## `composer_api_piece(context, request_json, job, out)`

Composes a looping game track from the piece request of [piece-request.md](../piece-request.md) (`"schema":
"ursatz-piece-request"`, `"schema_version": 1`, the game loop, or `2` with a `form`: any built-in form, API 1.14; the `preferences` document gained the L2 motif members in 1.15), or writes
one section of the plan of an earlier piece again, and builds the ending version from the loop (a form with sizes has the one
version `piece`). The request is an object around the piece request, which carries no path but the style
model (R-267):

| Member | Value |
|---|---|
| `request` | the piece request object, as the file holds it, except that a section filled with a theme carries `"plan": <plan object>` (a path is refused with `URSATZ_ERR_REQUEST` and the path `/request/sections/<i>/fill/plan`; the caller reads the file) and may carry `"source": "<name>"`, the name shown as the section's source in the result (default `theme plan`) |
| `from` | the plan object of an earlier piece (its first member of `plans`, or `plan_json`); absent to compose a piece |
| `regenerate` | with `from`: `{"section": "<section id>", "lanes": ["bass", ...]}`; the section of a form with sizes is the name of a section of the plan (`B`, `variation 2`), else refused at `/regenerate/section`. `lanes` holds `harmony`, `bass`, `melody`, `inner`; none, or `all`, is the whole section. A section filled with a theme needs `harmony` or `bass`. A `null` or absent `section` (or no `regenerate`) reopens nothing: the loop plan is kept and the ending version is built again |
| `seed` | 0 to 4294967295 (a number or a string of digits), default from the clock; the seeds after it are tried (up to 16) when one gets stuck |
| `check` | `true`: the Analyzer analyzes both versions, and a seed whose analysis contradicts the plan is passed over |
| `style` | a style model file path; replaces the request's `style` |
| `midi`, `musicxml` | `true`: the loop version's MIDI file (`midi`, `midi_size`, with the `loopStart` and `loopEnd` markers) and MusicXML (`musicxml`) |
| `ending_musicxml` | `true`: the ending version's MusicXML as the string `ending_musicxml` of the result (refused at `/ending_musicxml` for a form with no ending version) |
| `musicxml_versions` | `true`: the MusicXML of every version after the first as `musicxml_versions: {"<name>": "<MusicXML>"}` (`{}` for a form with one version) |
| `plain_midi` | `true`: no track names and no section markers in the MIDI file |
| `layers` | `true`: the loop version's `realization` and `reading` in the document ([layers](#layers)) |
| `explain` | `"text"` or `"json"`: why each candidate was chosen or rejected in both versions, as the string `explanation` |

The request has no `corpus` member: a piece reads no corpus, and `corpus` is refused with the path `/corpus`.

`json`: what `piece --format json` prints, without the `files` it wrote: `{"schema": "ursatz-piece", "seed", "key", "meter",
"brief", "loop": {"start", "end"}, "regenerated", "versions": {"loop": V, "ending": V}}` (the versions are `piece` alone for a form with sizes, and `loop` is `null` when the first has no loop region; `regenerated` only with `from`: `null`,
or `{"section", "lane"}` with `all`, a lane, or the lanes joined with `+`), each version V `{"plan", "sections", "score",
"seam", "analyzer_check", "style_distance"}` as in [cli-piece.md](../../../cli/docs/cli-piece.md#output). `V.plan` is `null`: the plans are in
`"plans": {"loop": <plan object>, "ending": <plan object>}` (by the version names), and `plan_json` is the first version's plan. A section a regenerate left
as it was (a theme section out of the scope of the hearing, [regenerate.md](../regenerate.md)) has `"in_scope": false`; a
complete loop plan has no open theme section, so only a plan with open sections reports one; a regenerate of a form with sizes marks every section it did not write. A row of a game loop is `{"id", "part", "function", "letter", "start", "beats", "cadence", "heard", "fill", "source", "region", "harmony"}`; a row of a form with sizes is `{"id", "role", "letter", "start", "beats", "cadence", "heard", "returns", "motive", "key_heard", "key", "region", "harmony", "pedal", "changes", "figure", "function", "part", "repeat_of", "upbeat", "connective"}` (R-610: `harmony` the section's harmony as the request gave it, `"i iv V i (2 cells each)"`, `"schema prinner"` or `"bar 2: V"`, `null` for none, and a progression fill answers `fill` `"empty"` with its harmony; `region` the region as the request asked for it (`"home"`, a key as written, `"mediant of A"`) or the form names it, `null` for none; a request's `sections` are the game loop's, or for a form with sizes the overlays of [piece-request.md](../piece-request.md), refused at their paths; the API stays 2.2. API 1.23, R-510: `function` `null` for a section with no section function, else `{"kind": "transition" | "retransition" | "subordinate" | "coda", "independent", "lyric", "text"}`, in a row of either kind; `text` `"a transition grown from the main theme, liquidating to the upbeat chord"` (`"an independent transition with its own motif, ..."`, with `", modulating to G major"` before the comma clause when the transition ends in another key, the key as `key` writes it), `"a retransition over the dominant pedal, ending on the main theme's head"`, `"a subordinate theme"` (`", lyric"` when lyric) or `"a coda of 4, 2 and 2 bars, each closing on the tonic"`; a performed row takes the function of the section it performs; a game loop request section takes `function` `"transition"` or `"coda"`, see [piece-request.md](../piece-request.md); the API is 1.25; API 1.22, R-501: `figure` `null` for a section without a figure or one that was not copied, else `{"kind": "neighbor" | "scale" | "broken-chord", "rhythm": "eighths" | "sixteenths", "harmony": "kept" | "reharmonized", "change_at": <bar> | null, "climax", "raised", "mode": "parallel" | null, "skeleton": ["E5", "D5", ...], "text"}`: `skeleton` the principal tone of each beat cell of the composed section (16 tones for 8 bars of 2/4), each as a note name and octave with C4 middle C and spelled with flats in a flat key; `raised` true when the climax octave was applied (a climax that does not fit the melody range stays down); `text` `"neighbor tones in sixteenths over the skeleton E5 D5 C5 ..."` with `, reharmonized`, `, root changed at bar 3`, `, in the parallel mode` and `, an octave up` as they apply and the first eight tones; a performed row takes the figure of the section it performs, a game loop row has none; the `changes.text` of a figurate return says `melody: figured over the skeleton` in place of the notes added; API 1.21, R-493: `key` the key the plan gives the section, the key lane's span under its start, `"G major"`, `null` when the plan has none; a performed row takes the key of the composed section it performs, a part's row its part's plan, so a scherzo's P, S1 and S2 in C major are `"G major"`, `"F major"` and `"C major"`; API 1.20, R-485, R-486: `part` the name of the part of a compound form the section is in, `"Minuet"`, `"Trio"` or `"Minuet da capo"`, else `null`; `repeat_of` the name of the performed section a written-out repeat plays again, `"A"` for `A (repeat)` and for `Minuet da capo A`, `null` the first time; `upbeat` `{"chord", "next"}` for a section that leads into the next, its last chord and the next section's first, else `null`; `connective` `true` or `false` for a section with a connective, whether it was written, else `null`; a performed form has one row for each performed section, its `id` the source section's, and the document gains `"performed": {"bars"}` after `loop`, `null` for a form with sizes that is not performed, absent for the game loop); either gains `"construction_text"` (R-469, the sentence that says why the theme is as long as it is, as the theme response's `construction.text` gives it) when its section has a construction, and has no such member otherwise, so a piece of 4 and 8-bar themes answers as before; API 1.18 stays the same version (the section's name, role, letter, start, length in beats, planned and heard cadence, the section it returns and how its motive is made, or `null`; `heard` is the Analyzer check's, or without a check the last cadence the compose run heard in the section's last bar; `key_heard` the key heard at the section's start over the whole piece, `"C major"` (the Analyzer often hears a key other than the plan's, which is `key`); `pedal` `"dominant"` when the section's first half was written over a pedal; `changes` `null` unless the section returns one, else `{"copied", "variation", "setting", "material", "melody_added", "chords_changed", "chords", "cadence_from", "cadence_to", "text"}` (API 1.25, R-525: `material` the id of the material (`omt.material.major` or `omt.material.natural-minor`) for a copy across modes, the material the copy is mapped into (a `home` return in a minor piece, a figurate return in the parallel mode), else `null` (API 2.0; it was `mode`, `"major"` or `"minor"`), and `text` then has the part `mode: mapped into minor` before the harmony part; API 1.24, R-521: `variation` one of `exact`, `ornament`, `reharmonize`, `reregister`, `varied`, `figurate`, `embellish` or `setting`; `setting` `null` unless the section's RETURN is a `setting`, else the pattern of its new accompaniment, `"block"`, `"arpeggiated"`, `"alberti"` or `"oom-pah"`, as the plan carries it after the build's substitution, and `text` then has the part `accompaniment: arpeggiated (new setting)` before the harmony part; the notes an `embellish` return adds are the `melody: N notes added` part), the last members of the numbers and numerals `null` when the section was composed anew, and `text` the sentence for a person). The MIDI file of the ending
version is `composer_api_export` of `plans.ending`; its MusicXML comes with `ending_musicxml` because the `xml:id` of
its notes carry the span ids of the plan in memory, which a plan file renumbers.

A form with `repeats` (R-485, [form-performance.md](../form-performance.md)) is composed once and performed in order, written out: its `plans` member, plan file, MIDI file and MusicXML are the performed plan (A A B A1 B A1), and the hearing members are the composed plan's. A row of a performed version is one for each performed section, its `start` and `beats` the performed section's and every other member its source's, so a repeat's row is its source's at its own start. `from` (regenerate) and the variations entry refuse such a form with `URSATZ_ERR_REQUEST` at `/request/form`: `regenerate and variations of the <label> form are Later: it is composed whole`. A form with no `repeats` answers as before.

Progress is reported once for each seed tried (`"seed"`) and for each compose stage of each section; a cancel is
`URSATZ_ERR_CANCELLED` with no document and no plan. Refusals have the path of the member at fault (`/request/key`,
`/request/sections/<i>/beats`, `/regenerate/section`, `/regenerate/lanes`, `/from`, `/seed`). A piece no seed can compose,
a motive that does not fit its bar, a request that no seed can satisfy is `URSATZ_ERR_UNSATISFIABLE` (`no piece could be
composed from seeds <a> to <b> (the last was stuck in the loop version, seam stage ...)`). The cadence of a section without
one is the game loop document's default (an HC at the end of the intro and of the loop, a PAC at the end of the
ending, an IAC elsewhere), and a request without an ending gets the default ending. The same request and seed give the same
piece on any thread; the entry keeps no state. Schemas: `piece.request.schema.json`, `piece.response.schema.json`.

## `composer_api_piece_variations(context, request_json, job, out)`

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
"kept", "requested", "seeds_tried"}` (the version of a form with sizes is under `piece` and has no `loop_markers`). `V.plan` is the plan object of the variation. The MIDI file of a variation is
`composer_api_export` of its plan with its `loop_markers` as `loop`. Progress is reported once for each seed tried
(`"variations"`) and for each compose stage; a cancel is `URSATZ_ERR_CANCELLED`. No version that differs from the plan is
`URSATZ_ERR_UNSATISFIABLE`. Refusals name `/from`, `/variations/section`, `/variations/lanes`, `/variations/count`.
Schemas: `piece_variations.request.schema.json`, `piece_variations.response.schema.json`.

## Layers

The member `layers` (API 3.2, R-634; a boolean, off by default) of the theme, grow, piece, fill, regenerate, develop, counterpoint and
progression requests adds two members to the document of the first result (the first theme, the loop version, the first
progression or counter-melody, the realized plan): `realization`, the work its MIDI and MusicXML are written from
(`realization.v1`, [schema](../../../library/realization/schema/realization.v1.schema.json)), and `reading`, the Composer's hearing of
the plan over that work (`reading.v1`, [schema](../../../library/reading/schema/reading.v1.schema.json)), whose `target.digest` is the
realization's digest and whose evidence names the work's performer notes (`piano-n<plan note id>`). Without it the response is as it was.
Variations refuse it (`/layers`). The command line writes the two members and the plan with `--layers <dir>`
([cli-theme.md](../../../cli/docs/cli-theme.md)).
