# The piece request

The JSON file `ursatz-composer piece` reads ([cli-piece.md](cli-piece.md), R-173). It holds the structure of a looping game
track (an intro, a body, a turnaround and an ending, [the game loop](piece-structure.md)) and the brief. Schema
`ursatz-piece-request`, version 1 (the game loop) or 2 (any built-in form, [below](#a-piece-of-another-form-schema-version-2)).
Back to the [docs index](README.md).

```json
{"schema": "ursatz-piece-request", "schema_version": 1,
 "key": "D", "mode": "minor", "meter": "4/4",
 "motive": null,
 "sections": [
  {"id": "intro", "part": "intro", "letter": "I", "beats": 8, "pickup": "0", "cadence": null, "fill": {"kind": "empty"}},
  {"id": "a", "part": "body", "letter": "A", "beats": 32, "pickup": "0", "cadence": null,
   "fill": {"kind": "motif", "motive": {"degrees": "1:e 3:e 5:e 3:e 1:e 3:e 5:e 1':e", "pickup": "0", "role": "melodic"},
            "forms": [], "into": "sentence", "hybrid": null}},
  {"id": "b", "part": "body", "letter": "B", "beats": 16, "pickup": "0", "cadence": null,
   "fill": {"kind": "motif", "motive": {"degrees": "5:e 4:e 3:e 2:e 4:e 3:e 2:e 1:e"}, "forms": [], "into": null, "hybrid": null}},
  {"id": "t", "part": "turnaround", "letter": "T", "beats": 8, "pickup": "0", "cadence": null, "fill": {"kind": "empty"}}],
 "accompaniment": null, "voicing": null,
 "style": "/abs/path/style.json", "style_weight": "1/2", "without": [], "tempo": null}
```

## The brief

| Member | Meaning |
| --- | --- |
| `key`, `mode` | The tonic (or a key like `"D minor"`) and `minor`, `major` or `style`. `style` takes the style's most common mode and needs a `style`. |
| `meter` | `2/4`, `3/4`, `4/4` or `6/8`. |
| `motive` | `null`, or the piece's basic motive (a degree list as in the [theme request](theme-request.md#motives), melodic). With `null` the basic motive is the own motive of the first motif section, else one invented. It may be as long as the basic idea of every theme section: 2 bars when all are 8 bars, else 1. A longer one is refused at `/motive`. |
| `accompaniment`, `voicing` | As in the theme request, on every section: `{"pattern": "any" or "block" or "arpeggiated" or "alberti" or "oom-pah"}`, or `"satb"` or `"chorale"`, not both. The pattern `motive` is not available in a piece. |
| `style`, `style_weight`, `without`, `tempo` | As in the theme request. The tempo goes into the MIDI and MusicXML files of both versions. |

## Sections

`sections` lists the sections in time order, grouped by part: the intro, then the body, the turnaround, the ending. At most 32.
The body is required; the others may be left out, and a request without an ending gets one of 2 empty bars closing in a PAC
(`id` `end`).

| Member | Meaning |
| --- | --- |
| `id` | A name of 1 to 15 characters, different for each section. |
| `part` | `intro`, `body`, `turnaround` or `ending`. |
| `letter` | The form letter shown in the MIDI marker; the id's first letter, capitalized, when left out. |
| `beats` | The length: a whole number of beats of the meter, 1 or more. |
| `pickup` | Beats of anacrusis before the section, as a string or number (`"0"`, `"1"`, `"1/2"`); shorter than a bar. It is not part of `beats`. |
| `cadence` | `null` for the default (an HC for the last section of the intro and of the loop, a PAC for the last of the ending, an IAC for the others), or `PAC`, `IAC`, `HC`, `DC` or `PC`. |
| `fill` | What fills the section: see below. `null` or left out is `{"kind": "empty"}`. |

A section of whole bars needs room for its cadence and a last chord of a whole bar: 2 bars at least. A section of one bar
cannot be composed (the structure stage leaves it open). Sections that do not start or end on a barline, or are shorter than
a bar, are composed as free phrases ([piece-structure.md](piece-structure.md)). Any section that is not the loop's end can be
of any length that composes; a sentence at the loop's end (nothing after it but the seam) is the hardest to compose.

### Fills

| `kind` | Members | Meaning |
| --- | --- | --- |
| `empty` | | Nothing of its own: made from the piece's basic motive. |
| `motif` | `motive`, `forms`, `into`, `hybrid` | A motive of its own (`null`: the basic motive), saved forms of it for the motives stage (as in the theme request, at most 64 in the request), and for a section of 4 or 8 whole bars the theme it is built into: `into` `sentence` (default), `period` or `hybrid` (with `hybrid` as `--hybrid` writes it). Any other length is a free phrase. |
| `theme` | `plan` | A plan the `theme` or `grow` command wrote: its realized lanes are Given to the section, moved and transposed to it. The theme must be as long as the section, in the same meter and mode. In the request file of the `piece` command `plan` is the path of the plan file; in a request to the [API](api/theme-and-piece.md#composer_api_piecerequest_json-job-out) it is the plan object itself, with an optional `source` (the name shown as the section's source), and a path is refused. The command reads the file into the request. |
| `progression` | `numerals`, `cells` | Roman numerals separated by spaces (`"i iv V i"`), `cells` harmony cells each (a bar when left out; a cell is half a bar in 4/4). The harmony stage fills the rest of the section. |

## A piece of another form (schema version 2)

`"schema_version": 2` names a built-in form in `form` (`binary`, `rounded-binary`, `ternary`, `variations`, `loop`,
`layered` or `game-loop`; [the API serves them](api/vocabulary-and-validation.md#forms-composer_api_formsout)). Version 1 is
the game loop and refuses a `form` at `/request/form`; a version 2 request without `form`, or with another name, is refused
there too, with the built-in names. The brief (`key`, `mode`, `meter`, `motive`, `accompaniment`, `style`, `style_weight`,
`without`, `tempo`) is as above. The rest depends on the form:

| Form | Members |
| --- | --- |
| `game-loop` | `sections` and `voicing` as above. `bars`, `count` and `modulate` are refused at their paths. A version 2 game loop request answers as the version 1 request does. |
| a form with sizes | `bars` (0 to 512) and `count` (0 to 64) pick the size (none: the form's default; a size the form has no such of is refused at `/request/bars`, or `/request/count` when only `count` was given, with the sizes it has); `modulate` plans the keys of the sections from the seed. `sections` and `voicing` are refused at their paths: the sections are the size's. |

A form with sizes has the one version `piece`, no loop and no ending (`loop` is `null`, and `ending_musicxml` is refused). Its
sections are those of its size, with the roles, returns and motives the [form document](forms.md) gives, and no fill of their
own yet. The motive of the piece, when given, is the basic motive. Each seed that gets stuck is passed over and the next is
tried (up to 16), with the plan built again for each, so a modulating piece plans its keys again.

## Errors

The `piece` command and the [composer API](api/theme-and-piece.md#composer_api_piecerequest_json-job-out) read the request the same way: the command
reads the file (and the plan file of each theme fill) into the API request, and a member that is not valid is refused with its
path in the request (`/request/sections/<i>/cadence`) in the API and as a message and exit 1 in the command. The saved forms
and motives of a motif fill are inline in both.

A request that does not read is exit 1: not JSON, another schema, no body, parts out of order, a repeated id, a bad pickup, a
cadence that is none of the five, an unknown fill kind, degrees that do not read, a theme plan that cannot be read or does not
fit, a progression that cannot be read or runs past its section.
