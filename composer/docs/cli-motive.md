# motive command

```sh
ursatz-composer motive <input> [--meter <n/m>] [--key <key>] [--format <auto|midi|musicxml|notes|degrees|rhythm|contour|json>] [--form <full|rhythm|contour>] [--pickup <beats>] [--json]
```

Loads a motive, a short cell of notes (one bar or less, as a rule), and prints its analysis: the
intervals, the contour, the rhythm, the range, the scale degrees and the chords it implies. The motive
can be attached to a plan as Given material with `motive_attach` (`<ursatz_composer/motive.h>`); this
command only reads and reports.

## Input

`<input>` is a file. `--format` says what it is; `auto` (the default) looks at the bytes: `MThd` is
MIDI, a first character of `<` is MusicXML, `{` is JSON, and anything else is a note list.

| Format | Notes |
| --- | --- |
| `midi` | A Standard MIDI File, type 0 or 1, read with Ursatz's MIDI importer. |
| `musicxml` | An uncompressed score-partwise file, read with Ursatz's MusicXML importer. Ties are merged into one note. |
| `notes` | A note list, below. |
| `degrees` | A degree list, below. Never chosen by `auto`. |
| `rhythm` | A rhythm-only list, below. Never chosen by `auto`. |
| `contour` | A contour-only list, below. Never chosen by `auto`. |
| `json` | A motive document, see [plan-format.md](plan-format.md#motive-documents). |

A motive is one voice. A MIDI or MusicXML file with notes sounding together, or with notes in more than
one MusicXML part, is refused as polyphonic and the message gives the position of the first overlap. No
voice is chosen for you and none is dropped; cut the file down to the voice you mean.

The motive starts at the start of a bar. A first note after time 0 means the motive opens with a rest.
It ends where its last note ends.

### Note list

```
D4:q F4:e A4:e D5:h
r:e G4:e C5:q. B4:e
G4:q B4:q~ B4:e A4:e G4:q
```

Tokens are separated by white space. Each is a pitch, a colon and a duration, with an optional tie mark.

| Part | Syntax |
| --- | --- |
| Pitch | A letter `A`-`G`, up to two accidentals (`#` or `b`), and an octave: `C4` is middle C (MIDI 60), `F#3`, `Bb5`, `C-1`. `r` is a rest and has no octave. |
| Duration | `w` whole, `h` half, `q` quarter, `e` eighth, `s` sixteenth, `t` thirty-second, then up to two dots: `q.` is 3/8. |
| Tie | `~` after the duration ties the note to the next token, which must be the same pitch. The two are one note as long as both. A rest cannot be tied. |
| Barline | `\|` is ignored. |

Notes follow one another from time 0. The text must end on a note, not a rest, and may have at most 64
notes.

### Degree list

```
1:q b3:e 5:e 1':h
```

The same as a note list, but each pitch is a degree of the key's scale (natural minor in a minor key).

| Part | Syntax |
| --- | --- |
| Degree | An optional `#` or `b`, then `1` to `7`: `b3`, `#4`. |
| Octave | Marks after the degree, any number: `'` up an octave, `,` down. `1'` is the tonic an octave up, `5,,` the dominant two octaves down. Octave 0 is the octave whose degree 1 is the tonic's pitch class in MIDI 60 to 71 (D is 62, G is 67). |
| Rest, duration, tie, barline | As in a note list: `r:q`, `q.`, `~`, `\|`. |

The key comes from `--key`. Without one the pitches are C major's, and the key is left to the usual
inference. `degrees_text` in the JSON output (below) is the motive written this way, and it needs a key.

### Rhythm-only list

```
x:q x:e x:e x:h
```

A note list with no pitches: `x` is a note, `r` a rest, and durations, dots, ties and `\|` are as above. The
motive has the form `rhythm`. When it is heard, every note sounds on degree 1 of the key (C4 without one).
Only the rhythm is analyzed.

### Contour-only list

```
0 2 1 3
```

Ranks separated by white space: 0 is the lowest, equal ranks are the same height, no durations. The motive
has the form `contour`. When it is heard, rank `r` sounds on degree `r + 1` of the key (C major without one),
each note a quarter. Only the contour is analyzed.

`--form rhythm` and `--form contour` read the input as a rhythm or a contour; they are the same as
`--format rhythm` and `--format contour`, and a `--format` that says otherwise is an error. `--form full`
(the default) leaves the format as it is.

## Pickup

`--pickup <beats>` is the number of beats of the meter's beat unit that come before the first barline:
`0` (the default), `1`, `1/2`. It must be shorter than a bar. The motive still starts at offset 0, but bars
and beats are counted from the first barline: the pickup's notes are in bar 0 of the text output (bar -1
in the JSON, where the first full bar is 0), so in 3/4 a note at the start of a one-beat pickup is in
bar 0, beat 3. The pickup is in beats of the file's own meter for MIDI and MusicXML.

## Meter and key

| Flag | Meaning |
| --- | --- |
| `--meter <n/m>` | 1 to 64 beats over a beat unit of 1, 2, 4, 8 or 16: `4/4`, `3/4`, `6/8`. |
| `--key <key>` | A letter `A`-`G`, an optional `#` or `b`, whitespace, then `major` or `minor`. Quote it: `"D minor"`. Any case. |

Meter. A MIDI or MusicXML file that declares a meter supplies it, and a different `--meter` is an error,
since it would move the barlines. A file with none is 4/4, as Ursatz reads it, unless `--meter` says
otherwise. A note list has no meter and needs `--meter`. A JSON document may carry one.

Key. `--key` comes first. Then a key the input states: a MIDI or MusicXML key signature that says major
or minor, or the `key` of a JSON document. Both are shown as `given`. With neither, the key is inferred
from the pitches with Ursatz's common-practice key identification, shown as `inferred`. That needs all
seven degrees of a major or natural minor scale, so most motives have no inferred key and show
`unspecified`. Without a key the output has semitone intervals only, no scale degrees and no implied
harmony.

## Output

Text, by default:

```
Notes: D4 F4 A4 D5
Meter: 4/4
Form: full
Pickup: none
Key: D minor (given)
Length: 1 bar
Intervals: +3 (+2 steps), +4 (+2 steps), +5 (+3 steps)
Contour: ascending (up, up, up)
Rhythm:
  D4   q    bar 1 beat 1
  F4   e    bar 1 beat 2
  A4   e    bar 1 beat 2+1/2 (off the beat)
  D5   h    bar 1 beat 3
Range: D4 to D5 (12 semitones)
Scale degrees: 1 3 5 1
  starts on 1, ends on 1
Implied harmony:
  1. i (tonic) fits 100%: 4 of 4 notes are chord tones
  ...
```

Intervals are signed semitones, and with a key signed diatonic steps in its scale. A pitch outside the
scale counts as the degree below it, raised (`#4`). Implied harmony ranks the triads of the key (in
minor also the one on the raised seventh degree) and V7 by the share of the motive that is their chord
tones; each note counts for its duration, and twice that on a downbeat. A chord none of the notes belong
to is left out.

A rhythm has only the `Rhythm:` section and a contour only the `Contour:` line: their pitches are made up to
sound them, so there are no intervals, range, degrees or harmony.

`--json` prints the motive and its analysis as one JSON document, described in
[plan-format.md](plan-format.md#motive-analysis). The `analysis` also gives the form, the pickup in beats
(`"1"`, `"1/2"`), `notes_text` (the pitches as a note list), `degrees_text` (as a degree list, `null` without
a key) and, for the two forms, `rhythm_text` or `contour_text`. The same motive gives the same bytes whichever format
it was read from.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage or an invalid argument. |
| 2 | The input cannot be loaded as a motive. The message gives the reason and where: a token and column in a note list, a field path in JSON, a position in a file. |

Errors are printed on stderr as `Error: ...`; nothing is printed on stdout.
