# analyze command

The `analyze` command: the Ursatz Analyzer's reading of a MIDI or MusicXML file, as a document or as text. Back to the [cli docs](README.md).

```sh
ursatz analyze <input> [--input-format <format>] [--meter <n/m>] [--value-scale <n or n/d>] [--pickup <beats>] [--layers <dir>] [--model-plan <file>]
ursatz analyze <input> --view text [--measures <first>-<last>] [--title <text>] [--input-format <format>] [--meter <n/m>] [--value-scale <n or n/d>] [--pickup <beats>]
```

Without `--view`, the command writes one file next to the input, `<stem>.analysis.json`: the analysis document, the form, phrases, cadences, harmony, keys and motives the Analyzer hears. See [analysis-document.md](../../analyzer/docs/analysis-document.md) and the [schema](../../analyzer/schema/analysis-document.schema.json). It runs the workshop's `analyze` tool in the command's own process.

With `--view text` it prints the measure-addressed text export of the piece to standard output and writes nothing: the notes of each measure and the claims of the analysis, in the text a person can paste into a conversation to say "measure 27 is flagged as X, I read it as Y". See [text-export.md](../../analyzer/docs/text-export.md).

## Options

| Option | Meaning |
| --- | --- |
| `--input-format <format>` | `auto` (the default), `midi` or `musicxml`; see [input.md](../../analyzer/docs/input.md#input-formats) |
| `--meter <n/m>`, `--value-scale <n or n/d>`, `--pickup <beats>` | a notation reading for a file whose meter or note values are not what it means: `--meter 3/4`, `--value-scale 2` or `1/2`, `--pickup 1/4` (shorter than one measure of the meter); see [input.md](../../analyzer/docs/input.md#notation-readings) |
| `--layers <dir>` | also writes the layers of the analysis: `realization.json` and `readings/analyzer.json` |
| `--model-plan <file>` | also writes the minimal plan of the Analyzer's reading: its sections, keys and chords |
| `--view text` | prints the text export instead; `--layers` and `--model-plan` do not go with it |
| `--measures <first>-<last>` | with `--view text`: the measures to export as written in the score, inclusive (a pickup measure is number 0) |
| `--title <text>` | with `--view text`: the title of the export |

The notation readings apply to the one input file, not to corpus runs.

## Exit status

0 success; 1 usage, an input that cannot be read, a notation reading that cannot be applied or an analysis that failed; 2 an output file (the layers, the model plan) could not be written.

```sh
ursatz analyze song.mid
ursatz analyze song.mid --view text --measures 1-4
```
