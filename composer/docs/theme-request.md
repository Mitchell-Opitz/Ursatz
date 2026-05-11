# The theme request

The JSON file `ursatz-composer theme` reads ([cli-theme.md](cli-theme.md), R-166). It holds up to four motives, the slot of
the theme each one gives the ideas of, the saved forms of those motives the theme may use, and the brief. Schema
`ursatz-theme-request`, version 1.

```json
{"schema": "ursatz-theme-request", "schema_version": 1,
 "into": "sentence", "hybrid": null, "key": "D", "mode": "minor", "meter": "4/4", "bars": 8,
 "cadence": "PAC", "first_cadence": null,
 "motives": [{"id": "A", "form": "full", "degrees": "1:q b3:e 5:e 1':h", "pickup": "0", "harmony": null,
              "features": [{"label": "a", "first": 0, "last": 1}], "role": "melodic",
              "forms": [{"item": "mot-1", "degrees": "2:q 4:e 6:e 2':h", "pickup": "0", "harmony": null, "features": [],
                         "chain": "transpose tonal=1 by=1", "functions": ["beginning"], "flag": "approved"}]}],
 "slots": {"basic": "A", "contrasting": null, "continuation": null, "cadential": null},
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

`motives` has 1 to 4 entries, each with a unique `id`. A motive is a degree list (R-135) read in the key and mode of the
request, with a `pickup` in beats, its own `harmony` as Roman numerals (or `null`), `features` (`label`, `first` and `last` note
indices) and a `role`. Only `form: "full"` is read. A slot takes a melodic motive; the accompaniment motive is of the
accompaniment role.

`forms` lists the saved forms of the motive, at most 64: `item` (the saved item's id), `degrees`, `pickup`, `harmony`,
`features`, `chain` (how the item derives, shown as saved), `functions` (any of `beginning`, `continuation`, `contrast`,
`cadential`, R-143) and `flag`, `approved` or `unflagged`. A `rejected` form is an error (exit 1): leave it out. The forms
of a motive belong to the slot that names it; the forms of a motive no slot names are not read.

## Slots

`slots` names a motive for each slot by its `id`. `basic` is required; each motive is in at most one slot, and a motive in
none gives nothing.

| Slot | Ideas it gives |
| --- | --- |
| `basic` | the basic idea, the repetition of a presentation, the consequent's basic idea |
| `contrasting` | the contrasting idea of an antecedent or a compound basic idea, the consequent's contrasting idea |
| `continuation` | fragment 1 and fragment 2 |
| `cadential` | the cadential idea of a continuation, cadential 1 and cadential 2 |

A slot the pairing has no idea for (a contrasting slot in a sentence) cannot be grown (exit 2). The motive of a slot other
than `basic` must be a full motive of two notes or more, in the key and meter, and no longer than the idea it begins: two bars
of eight for the contrasting idea and the cadential idea, one for fragment 1.
