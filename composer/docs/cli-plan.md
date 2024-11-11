# plan command

```sh
ursatz-composer plan --form <name> --key <K> [--mode major|minor] --meter <n/m> [--bars <n>] [--count <n>] [--modulate] [--motive <file>] [--accompaniment <pattern>] [--texture <name>] [--seed <n>] --out <plan.json> [--format text|json]
```

Plans a whole short piece from a form template and writes the plan as JSON. Nothing is composed yet: the
sections, their keys and cadences, the sections that return an earlier one and the motive are in the plan, every
section open for [`fill`](cli-fill.md) to realize. It builds a request for `piece_build_plan`
(`<ursatz_composer/piece.h>`, see [piece.md](piece.md)).

`--form`, `--key`, `--meter` and `--out` are required.

## Options

| Option | Meaning |
| --- | --- |
| `--form <name>` | `binary`, `rounded-binary`, `ternary`, `variations`, `loop` or `layered`. See [piece.md](piece.md) for the sections of each. |
| `--key <K>` | The tonic (`D`, `F#`, `Bb`), with the mode from `--mode`, or the tonic and the mode in one argument (`"D minor"`, any case). |
| `--mode <major\|minor>` | The mode when `--key` is a tonic only. Major by default. Not with a `--key` that names its mode. |
| `--meter <n/m>` | 2/4, 3/4, 4/4 or 6/8. |
| `--bars <n>` | The size of the piece in bars. Each form has its own sizes (binary 8 or 16, rounded binary 16 or 24, ternary 12 or 24, variations 16 to 72 in steps of 8, loop 8 or 16, layered 16 to 32 in steps of 8); a size it does not have is refused with the sizes it has. The default is the form's own (`form_template_default_bars`). |
| `--count <n>` | The variations (1 to 8) of `variations` or the layers (2 to 4) of `layered`, instead of `--bars`. With both, they must agree. Refused for the other forms. |
| `--modulate` | Plan the keys and the cadences across the sections from the seed ([piece.md](piece.md)): a section may end in a closely related key, reached by a pivot chord, and the cadences follow. Without it every section is in the home key and the cadences are the form's. |
| `--motive <file>` | The motive, loaded as for the [motive command](cli-motive.md) (a MIDI file, a MusicXML file, a note list or a motive JSON file), given the key and the meter. Without it the plan asks for one to be invented when it is filled. |
| `--accompaniment <pattern>` | `block`, `arpeggiated`, `alberti` (not in 6/8), `oom-pah` or `any`. A layered form has an accompaniment in any case. See [accompaniment.md](accompaniment.md). |
| `--texture <name>` | The texture the accompaniment is to be heard as, as for [grow](cli-grow.md). Alone it implies `--accompaniment any`. |
| `--seed <n>` | Whole number from 0. By default one is derived from the clock. The seed chooses the keys and cadences of `--modulate`; the same options and seed give the same plan. The seed that fills the plan is given to `fill`. |
| `--out <plan.json>` | Where the plan is written, as `plan_write_json_file` writes it. |
| `--format <text\|json>` | How the plan is printed on stdout. Text by default. |

## Output

In text: the form with its size, the key, the meter and the seed; the motive file or `motive invented at fill`;
the accompaniment if there is one; then a line for each section with its letter, its bars, the key it ends in, the
cadence it ends in (and, in brackets, the weaker cadence its antecedent ends in), its pairing, and the section it
returns with how it varies it (`exact`, `ornament`, `reharmonize` or `reregister`):

```
ternary, 24 bars, key A minor, meter 3/4, seed 4, keys and cadences planned
motive invented at fill
sections
  A    8 bars, key A minor, cadence PAC (antecedent HC), antecedent + consequent
  B    8 bars, key D minor, cadence PAC, presentation + continuation
  A    8 bars, key A minor, cadence PAC (antecedent HC), antecedent + consequent, returns A (exact)
wrote doc_t.json
```

With `--format json` the same facts are one object: `form`, `bars`, `key`, `meter`, `seed`, `modulate`, `motive`
(the file or `null`), `accompaniment` (the pattern or `null`), `sections` (each with `letter`, `bars`, `key`,
`cadences`, `pairing` and `returns`) and `file`.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | The plan was written. |
| 1 | Usage or an invalid argument: a missing required option, an unknown form, a size the form does not have (the message lists the sizes it has), a bad key, mode, meter, seed or format, or an accompaniment the meter cannot have. |
| 2 | The request cannot be satisfied: the motive cannot be loaded, does not fit in one bar of the meter, or has fewer than two notes. |
| 3 | The plan could not be written. Nothing is left behind: the file is written beside its name and renamed. |
