# variants command

```sh
ursatz-composer variants --list-ops [--json]
ursatz-composer variants <motive-input> [--format ...] [--form ...] [--meter <n/m>] [--key <key>] [--pickup <beats>] [--chain <file.json>] [--count <n>] [--seed <n>] [--json]
```

Lists the operations of the [variant registry](variant-registry.md), or makes variants of a motive. Each variant applies a chain of up to 4 operations. A parameter the chain leaves open is drawn from the seed, so the same motive, chain and seed always give the same variant, and a reroll is the same chain with another seed.

The input is loaded as for the [motive command](cli-motive.md): a MIDI file, a MusicXML file, a note list or a motive JSON file, with `--format`, `--form`, `--meter`, `--key` and `--pickup` meaning the same. A rhythm (`x:q x:e`) or a contour (`0 2 1 3`) is read with `--form rhythm` or `--form contour`, and only the operations that apply to that form can be used on it.

## Options

| Option | Meaning |
| --- | --- |
| `--list-ops` | Print every operation: name, label, category, citation, parameters and forms. Takes no input. |
| `--chain <file>` | A chain file (below), used for every variant. Without it, each variant gets its own random chain of 1 or 2 operations that the motive's form takes. |
| `--count <n>` | How many variants, 1 to 16, 6 by default. Variant i uses seed + i. |
| `--seed <n>` | A whole number from 0. By default from the clock. The seed is printed. |
| `--json` | Print as JSON (below). |

## Chain file

```json
{"steps": [{"op": "invert", "params": {"tonal": true}}, {"op": "augment", "params": {"ratio": "2:1"}}]}
```

`op` is an operation name from `--list-ops`. `params` is optional and holds the parameters to fix, named as in the list: a boolean for `tonal`, `keep-contour` and `keep-rhythm`, a whole number for the others (a MIDI note number for `axis`), the text `"2:1"` for `ratio` and `"leading"`, `"trailing"` or `"middle"` for `position`. A parameter not listed is open and drawn from the seed.

## Output

Text prints the source and the seed, then one block per variant: its seed and operations, its note list (a rhythm or contour for those forms) and the derivation sentence.

```
Variant 1 (seed 3): invert, augment
  A4:h F4:q D4:q A3:w
  Inverted about F4 in the key's scale, then augmented 2:1. Kept the rhythm's proportions and the harmony; changed the intervals and the contour.
```

JSON:

```json
{"seed": 3, "source": {"motive": {}, "analysis": {}},
 "variants": [{"seed": 3, "chain": {"steps": [{"op": "invert", "params": {"axis": 65, "tonal": true}, "open": ["axis"]}]},
               "kind": "motive-form", "kept": [], "changed": [], "functions": [],
               "derivation": "", "recognizability": 0.8571, "motive": {"motive": {}, "analysis": {}}}]}
```

`source` and each variant's `motive` are the document of `motive --json`, with `notes_text`, `degrees_text` and the analysis. `chain` holds every resolved parameter, and `open` names those the seed drew. `kind` is `exact` or `motive-form`; `functions` stays empty until the function tags of a later phase.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage or an invalid argument, or a chain file that cannot be read or has an unknown operation, parameter or value. |
| 2 | The input cannot be loaded, or a step of the chain is refused. The message names the step and the reason, for example `step 2 of the chain: interval-change does not apply to a contour motive`. |

With random chains, a variant that cannot be made is skipped with a note on stderr; the command fails only if none can be made.
