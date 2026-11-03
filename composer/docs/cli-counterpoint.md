# counterpoint command

```sh
ursatz-composer counterpoint <line> --species <1-4> [--above|--below] --key <key> --meter <n/m> [--mode <major|minor>] [--seed <n>] [--suggest <n>] [--out <file.json>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--plain-midi] [--format text|json]
```

Writes a counter-melody against `<line>` in the first, second, third or fourth species and checks it with
standard-counterpoint ([counterpoint.md](counterpoint.md)). The line is loaded as for the [motive command](cli-motive.md):
a MIDI file, a MusicXML file, a note list or a motive JSON file. It may be longer than a bar. `--key` and `--meter` are
required and are given to the loader.

The command is a wrapper of the [composer API](api.md): it reads its arguments and the line into a request (a MIDI file through `composer_api_motive_from_midi` first), calls `composer_api_counterpoint`, writes the plan, MIDI and MusicXML files the response holds, all or none, and prints the counter-melodies from the response document alone. `--above` is `"position": "above"`.

## Options

| Option | Meaning |
| --- | --- |
| `--species <1-4>` | 1 note against 1, 2 against 1, 4 against 1, or syncopated half notes tied over the barline. Species 2 to 4 need a cantus firmus: one whole note a bar, three bars or more; species 3 a meter of four beats. |
| `--below` | The line is the melody and the counter-melody sings below it. The default. |
| `--above` | The line is the bass and the counter-melody sings above it. |
| `--key <key>` | Tonic and mode in one argument (`"D minor"`), or a tonic with `--mode`. |
| `--seed <n>` | Whole number from 0. By default one is derived from the clock; the seed used is printed. |
| `--suggest <n>` | 1 to 16 different counter-melodies. With files, number k goes to `<name>-k.<extension>`. |
| `--out <file.json>` | Write the plan as JSON. |
| `--midi <file>`, `--musicxml <file>` | Write the line and the counter-melody as two parts. |
| `--tempo <bpm>` | Write a tempo in the MIDI and MusicXML files: quarter notes per minute from 20 to 400, a whole number or a fraction like `195/2`. Default: none. See [export.md](export.md). |
| `--plain-midi` | MIDI without track names and section markers. See [export.md](export.md). |
| `--format <text\|json>` | How to print the result: text by default. |

## Output

The text gives the line and the counter-melody as note lists, the rule groups standard-counterpoint asked of the result
with a verdict each (`held`, or `BROKEN`; `held` with `(some advice not followed)` when a soft rule is broken), and the
seed. The JSON has the species, position, key and meter, and for each result the seed, both parts as notes, the groups and
the files written.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Written. |
| 1 | Usage or an invalid argument. |
| 2 | The request cannot be satisfied: the line cannot be loaded, or `cannot be written as asked: <reason>` (the line is not one the species can be written against, or no seed in the first 16 gives a counter-melody that holds). |
| 3 | An output file could not be written; none is left behind. |
