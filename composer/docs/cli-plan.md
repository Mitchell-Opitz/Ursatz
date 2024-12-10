# plan command

```sh
ursatz-composer plan --form <name> --key <K> [--mode major|minor] --meter <n/m> [--bars <n>] [--count <n>] [--modulate] [--motive <file>] [--accompaniment <pattern>] [--texture <name>] [--given-melody <file> | --given-progression "<numerals>" | --given-bass <file> [--at <L>[:<bar>]] [--cells <n>]]... [--seed <n>] --out <plan.json> [--format text|json]
```

Plans a whole short piece from a form template and writes the plan as JSON. Nothing is composed yet: the
sections, their keys and cadences, the sections that return an earlier one and the motive are in the plan, every
section open for [`fill`](cli-fill.md) to realize. It builds a request for `piece_build_plan`
(`<ursatz_composer/piece.h>`, see [piece.md](piece.md)).

`--form`, `--key`, `--meter` and `--out` are required; `--form` may be left out when there is given material.

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
| `--given-melody <file>` | A phrase to place in the piece, from a MIDI file, a MusicXML file, a note list or a motive JSON file, of any length up to 64 notes (the motive limit of one bar is not asked). `fill` never changes it. Repeatable, up to 8 `--given-*` in all. See [middle-out.md](middle-out.md). |
| `--given-progression "<I IV V I>"` | Roman numerals, one chord each, to place in the piece, separated by spaces, commas or bars. A numeral the harmony operators write, or an applied chord. |
| `--given-bass <file>` | A bass line to place, read as `--given-melody` is, with no rests: its notes fill its bars. |
| `--at <L>[:<bar>]` | Goes after the `--given-*` it places: the letter of the section (`B`, `A'`, as the plan prints it) and the bar of it, from 1 (the default), where the material starts. With no `--form`, the default is `B:1`. |
| `--cells <n>` | After a `--given-progression`: the cells each chord lasts (a bar has 1 in 2/4 and 3/4, 2 in 4/4 and 6/8). The default is a bar. |
| `--seed <n>` | Whole number from 0. By default one is derived from the clock. The seed chooses the keys and cadences of `--modulate`; the same options and seed give the same plan. The seed that fills the plan is given to `fill`. |
| `--out <plan.json>` | Where the plan is written, as `plan_write_json_file` writes it. |
| `--format <text\|json>` | How the plan is printed on stdout. Text by default. |

## Given material

`--given-melody`, `--given-progression` and `--given-bass` place material in the plan from a bar of a section, as
Given lane spans (`<ursatz_composer/given.h>`, [middle-out.md](middle-out.md)). Each is followed by its `--at`, which
names the section by its letter and the bar of it (from 1) where the material starts; a progression may also be
followed by `--cells`. The material is read after the plan is made, so it is refused if it would cross the section's
end, cross a change of key, or overlap Given material of the same kind.

With given material and no `--form`, the plan is a ternary form of 24 bars, the given material goes in B unless its
`--at` says otherwise, and the output says so (`no --form: a ternary form of 24 bars, the given material in B`).
The plan takes a bar more of material for each `--given-melody`, for the motive an idea with a Given melody inside it
is made from.

```sh
ursatz-composer plan --key C --meter 4/4 --given-melody phrase.txt --seed 1 --out piece.json
ursatz-composer plan --form rounded-binary --key C --meter 4/4 --given-progression "I IV V I" --at A:1 --cells 2 --out piece.json
```

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

With given material a line follows for each (`given melody phrase.txt at B:1`, `given progression "I IV V I" at A:1,
2 cells a chord`). With `--format json` the same facts are one object: `form`, `bars`, `key`, `meter`, `seed`, `modulate`, `motive`
(the file or `null`), `accompaniment` (the pattern or `null`), `sections` (each with `letter`, `bars`, `key`,
`cadences`, `pairing` and `returns`) and `file`, and, with given material, `given`: a list of `kind`, `source`,
`section`, `bar` and, for a progression, `cells`.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | The plan was written. |
| 1 | Usage or an invalid argument: a missing required option, an unknown form, a size the form does not have (the message lists the sizes it has), a bad key, mode, meter, seed or format, or an accompaniment the meter cannot have; a `--given-*` with no `--at` (when there is a form), an `--at` or `--cells` out of place, a section letter the piece does not have, a bar that is not a whole number from 1. |
| 2 | The request cannot be satisfied: the motive cannot be loaded, does not fit in one bar of the meter, or has fewer than two notes; given material cannot be loaded or placed (past the section's end, across a change of key, over other given material, notes or numerals that cannot be placed). |
| 3 | The plan could not be written. Nothing is left behind: the file is written beside its name and renamed. |
