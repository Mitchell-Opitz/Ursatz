# progression command

```sh
ursatz-composer progression --key <key> --meter <n/m> --bars <n> --cadence <type> [options]
```

Writes a chord progression for one phrase, with a bass line, ending in the cadence asked for.
Output is printed on stdout and can also be written as plan JSON, MIDI or MusicXML.

## Required flags

| Flag | Meaning |
| --- | --- |
| `--key <key>` | Tonic and mode in one argument. |
| `--meter <n/m>` | `2/4`, `3/4`, `4/4` or `6/8`. |
| `--bars <n>` | Phrase length, 1 to 64. |
| `--cadence <type>` | `PAC`, `IAC`, `HC`, `DC` or `PC`. |

### Key syntax

A letter `A`-`G`, an optional `#` or `b`, whitespace, then `major` or `minor`. Quote it so the shell
passes one argument: `"D minor"`, `"F# major"`, `"Bb minor"`. Any case.

### Meter syntax

`2/4`, `3/4`, `4/4` or `6/8`. Anything else is rejected.

### Bars and cadence

Cadences: PAC perfect authentic, IAC imperfect authentic, HC half, DC deceptive, PC plagal. Any case.

A cadence needs a minimum length: 2 bars in 4/4 and 6/8, 3 bars in 2/4 and 3/4, more for a plagal
cadence. A shorter phrase exits with status 2 and names the minimum. `--sequence`, and a `--schema`
that is not an opening, add one bar at the start (a tonic chord the schema follows).

## Options

| Flag | Meaning |
| --- | --- |
| `--seed <n>` | Whole number, 0 to 4294967295. |
| `--embellish <level>` | `none` (default), `low`, `medium`, `high`. |
| `--bass`, `--no-bass` | Write a bass line (default) or only the chords. |
| `--schema <name>` | Build the start of the phrase on a galant schema. |
| `--sequence <name>` | Build a continuation on a harmonic sequence. |
| `--list-schemas` | List schema and sequence names, then exit. |
| `--suggest <n>` | Produce n alternatives, 1 to 8. |
| `--out <file.json>` | Write the plan as JSON. |
| `--midi <file>` | Write the bass line as MIDI. Needs the bass. |
| `--musicxml <file>` | Write the bass line as MusicXML. Needs the bass. |
| `--tempo <bpm>` | Write a tempo in the MIDI and MusicXML files: quarter notes per minute from 20 to 400, a whole number or a fraction like `195/2`. Default: none. See [export.md](export.md). |
| `--plain-midi` | MIDI without track names and section markers. See [export.md](export.md). |
| `--format <text\|json>` | Stdout format. Default `text`. |
| `--help` | Print usage. |

Schemas with weak stages only work in 4/4 and 6/8. Not every schema is offered; `--list-schemas`
shows the ones that are.

MIDI and MusicXML hold the bass line only; the harmony lane is not rendered.

## Seeds

The same flags and seed give byte-identical output. Without `--seed`, a seed is derived from the
clock. The seed used is always printed. If no bass line suits the progression for a seed, the next
seeds are tried (up to 16) and the one that worked is printed, so rerunning with the printed seed
reproduces the result.

## `--suggest`

Prints n alternatives. With files, alternative k is written to `<name>-k.<extension>`
(`phrase-2.json`).

## Output formats

`text`: header, one row per chord (bar, beat, chord, function, bass), then the cadence and what the
recognizer heard.

`json`: one object with `key`, `meter`, `bars`, `cadence`, `seed`, `heard`, `chords` (each with
`bar`, `beat`, `chord`, `function`, `bass`) and `files` (paths written). With `--suggest`, the
alternatives are listed too.

Files are written to a temporary name and renamed, so a failure leaves no partial file.

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage error or invalid argument. |
| 2 | The request cannot be satisfied (for example, too few bars). |
| 3 | An output file could not be written. |

A failing command prints an `Error:` line on stderr and writes no files.

## Examples

### 1. A minor-key authentic cadence

```sh
ursatz-composer progression --key "D minor" --meter 4/4 --bars 8 --cadence PAC --seed 7
```

```
key D minor, meter 4/4, 8 bars, cadence PAC
seed 7

bar  beat chord      function     bass
1    1    i          tonic        D3
3    3    iv         predominant  G3
5    1    V          dominant     A3
8    1    i          tonic        D3

cadence PAC (heard: perfect-authentic)
```

### 2. A schema, written to files

```sh
ursatz-composer progression --key "G major" --meter 4/4 --bars 8 --cadence PAC --seed 2 \
    --schema romanesca --out phrase.json --midi phrase.mid
```

```
key G major, meter 4/4, 8 bars, cadence PAC
seed 2

bar  beat chord      function     bass
1    1    I          tonic        G3
1    3    V6         dominant     F#3
2    1    vi         predominant  E3
2    3    I6         tonic        B3
3    1    I          tonic        G3
4    3    IV         predominant  C4 C3
5    3    V7         dominant     D3
8    1    I          tonic        G3

cadence PAC (heard: perfect-authentic)
wrote phrase.json
wrote phrase.mid
```

### 3. Triple meter, embellished, as JSON

```sh
ursatz-composer progression --key "A minor" --meter 3/4 --bars 6 --cadence HC --seed 5 \
    --embellish medium --format json
```

```json
{"key":"A minor","meter":"3/4","bars":6,"cadence":"HC","seed":5,"heard":"half","chords":[{"bar":1,"beat":1,"chord":"i","function":"tonic","bass":"A2 A3"},{"bar":3,"beat":1,"chord":"iv","function":"predominant","bass":"D3"},{"bar":5,"beat":1,"chord":"ii°6","function":"predominant","bass":"D3"},{"bar":6,"beat":1,"chord":"V","function":"dominant","bass":"E3"}],"files":[]}
```

### Error

```sh
ursatz-composer progression --key "D minor" --meter 4/4 --bars 1 --cadence PAC
```

```
Error: 1 bar is too short for a PAC in 4/4; at least 2 bars are needed
```

Exit status 2.
