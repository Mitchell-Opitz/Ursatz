# The theme request

The JSON file `ursatz-composer theme` reads ([cli-theme.md](cli-theme.md), R-166, R-206). It holds up to 16 motives, the slot
of the theme and the single ideas each one gives, and the brief. Schema `ursatz-theme-request`, version 2: a version 1
request is an error (`the request must have "schema_version": 2`). Back to the [docs index](README.md).

```json
{"schema": "ursatz-theme-request", "schema_version": 2,
 "into": "sentence", "hybrid": null, "key": "D", "mode": "minor", "meter": "4/4", "bars": 8,
 "cadence": "PAC", "first_cadence": null,
 "motives": [{"id": "A", "form": "full", "degrees": "1:q b3:e 5:e 1':h", "pickup": "0", "harmony": null,
              "features": [{"label": "a", "first": 0, "last": 1}], "role": "melodic"},
             {"id": "R", "form": "full", "degrees": "5:e 4:e 3:e 2:e 1:h", "pickup": "0", "harmony": null,
              "features": [], "role": "melodic"}],
 "slots": {"basic": "A", "contrasting": null, "continuation": null, "cadential": null},
 "ideas": {"repetition": "R"},
 "chords": [{"bar": 0, "text": "I"}, {"bar": 1, "text": "IV V@2"}],
 "accompaniment": null,
 "voicing": null,
 "style": "/abs/path/style.json", "style_weight": "1/2", "without": ["motive-nct-figure"],
 "tempo": null}
```

## The brief

| Member | Meaning |
| --- | --- |
| `into`, `hybrid` | `sentence`, `period` or `hybrid`; with `hybrid`, the pairing as `--hybrid` writes it (`antecedent+continuation`). |
| `key`, `mode` | The tonic (or a key like `"D minor"`) and `minor`, `major` or `style`. `style` takes the mode from the style's `mode` category and needs a `style`. |
| `meter`, `bars` | `2/4`, `3/4`, `4/4` or `6/8`; 4 or 8 bars (default 8). |
| `cadence`, `first_cadence` | As the options of [grow](cli-grow.md): PAC (default), IAC or HC; the weak cadence of an antecedent. |
| `accompaniment` | `null`, `{"pattern": "any" or "block" or "arpeggiated" or "alberti" or "oom-pah"}`, or `{"pattern": "motive", "motive": {"degrees", "pickup", "form", "role": "accompaniment"}}`, a saved accompaniment motive as the pattern. |
| `voicing` | `null`, `"satb"` (alto and tenor under the grown melody) or `"chorale"` (four parts, the melody written too). Not with an accompaniment. |
| `style`, `style_weight`, `without` | An absolute path to a style model file, how strongly it is preferred, and the categories to read it without (the per-item switch for the motive categories, R-162). `style_weight` and `without` are read only with a `style`. A style gives the mode (`mode: "style"`), the tempo and the note density (R-163). |
| `tempo` | `null`, or quarter notes per minute as a string or number. It wins over the style's. |

## Motives

`motives` has 1 to 16 entries, each with a unique `id`. A motive is a degree list (R-135) read in the key and mode of the
request, with a `pickup` in beats, its own `harmony` as Roman numerals (or `null`), `features` (`label`, `first` and `last` note
indices) and a `role`. Only `form: "full"` is read. A slot or an idea takes a melodic motive; the accompaniment motive is of the
accompaniment role.

A motive may also have `tone_overrides`, `{"<n>": "<role>"}`: the role of note `<n>` (1 from the first), `chord` or a non-chord tone
(`passing`, `neighbor`, `appoggiatura`, `suspension`, `retardation`, `escape`, `anticipation`, `double-neighbor`,
`changing-tone`, `pedal`), as the `motive` command's `--tone` writes it. It replaces the role the motive's base harmony gives the
note, and the plan keeps the result as the motive's tones ([melody-fill.md](melody-fill.md#tone-roles)). A note out of range, a
role that is not one, or a motive with no key and pitches to label exits 1, naming the motive (`motive 'A': tone_overrides "9":
...`). A piece request's motives read it too.

`forms` is not read (R-207): a motive with a `forms` member is an error (exit 1, `saved forms are no longer read; leave "forms"
out`). The forms of the ideas are invented by the motives stage.

## Slots

`slots` names a motive for each slot by its `id`. `basic` is required. A motive may be in several slots, and a motive in none
gives nothing.

| Slot | Ideas it gives |
| --- | --- |
| `basic` | the basic idea, the repetition of a presentation, the consequent's basic idea |
| `contrasting` | the contrasting idea of an antecedent or a compound basic idea, the consequent's contrasting idea |
| `continuation` | fragment 1 and fragment 2 |
| `cadential` | the cadential idea of a continuation, cadential 1 and cadential 2 |

A slot the pairing has no idea for (a contrasting slot in a sentence) cannot be grown (exit 2). The motive of a slot other
than `basic` must be a full motive of two notes or more, in the key and meter, and no longer than the idea it begins: two bars
of eight for the contrasting idea and the cadential idea, one for fragment 1. A motif as long as its idea is that idea, placed
as written; a shorter one is grown to the idea ([grow.md](grow.md#how-a-motif-fills-its-idea)). The basic motif may be as long
as the basic idea: 2 bars of an 8-bar theme, 1 of a 4-bar one. A longer one is refused at `/slots/basic` with "the motif is 3
bars; the basic idea of a 8-bar theme is 2".

## Ideas

`ideas` gives a single idea a motive of its own: an object from the idea's name to a motive `id` (or `null` for none). The
names are `basic idea`, `repetition`, `contrasting idea`, `consequent basic idea`, `consequent contrasting idea`,
`fragment 1`, `fragment 2`, `cadential`, `cadential 1` and `cadential 2`. The idea is made from its motive exactly, in place of
the slot's motive for that one idea; the other ideas of the slot are made as before. An idea with no entry is made from the
motive of its slot. The same motive may be given to several ideas and slots.

An unknown idea name, an id the request does not have, or an idea the pairing has not (a `repetition` in a period) is an error
(exit 1) naming it. The basic idea is the basic slot's: `basic idea` may be named only with the motive of the basic slot.
The motive of an idea must fit it as a slot motive does (exit 2 when it does not).

## Chords

`chords` gives chords of the theme (R-208): a list of `{"bar", "text"}`, at most one entry for a bar. `bar` counts from 0 inside
the theme's bars. `text` is the own-harmony syntax of a motive ([cli-motive.md](cli-motive.md)) relative to the bar: Roman
numerals in the theme's key separated by spaces, each with an optional `@<beats>` onset from the start of the bar in beats of
the beat unit (`I V@2`). At most 2 chords; the first is at 0 unless it is written with an onset, the onsets rise, each is a
multiple of half a beat and less than a bar. Each chord lasts to the next chord of its bar, or the end of the bar. The
numerals are diatonic (no `b` or `#`).

The chords are Given: the harmony stage fills only what they leave open, the bass and the melody are written over them, and
none of them is changed. The harmony and bass operators write chords on the cells of the meter (one for a bar of 2/4 or 3/4,
two for 4/4 or 6/8), so an onset must fall on one: in 4/4 a second chord starts at `@2`, in 6/8 at `@3`, and a bar of 2/4 or
3/4 takes one chord. Any other onset, such as the "and" of beat 2 (`@3/2`), is an error (exit 1), not shifted to a cell.

A bad `bar`, text, onset or count exits 1 naming the bar (`chords bar 2: ...`). Chords that leave a stage with a target it
cannot write (a last bar `V` under a perfect cadence, a gap between two Given chords nothing connects) exit 2 with `the given
chords in bar <n> do not allow the <cadence>`, `<n>` counted from 0 as `bar` is. The JSON output has a `chords` member, the
list as read.

## In the API

[`composer_api_theme`](api/theme-and-piece.md#composer_api_themerequest_json-job-out) reads this request as a JSON text. The motives are inline:
each motive object carries its content in `degrees`, and a motive that names a file (`path`, `file`) is refused with the path
`/motives/<n>/path`. The `theme` command reads the same request from a file; the file holds the motives inline too, so a
request file is the request text of the API. The options of the command (`--seed`, `--suggest`, `--check`, `--out`, `--midi`,
`--musicxml`, `--plain-midi`, `--explain`) are members of the API request (`seed`, `suggest`, `check`, `midi`, `musicxml`,
`plain_midi`, `explain`), and the command writes the files from the response. `blend`, `blend_weight` and `blend_weights`
take the second style model of `grow`'s `--blend`.
