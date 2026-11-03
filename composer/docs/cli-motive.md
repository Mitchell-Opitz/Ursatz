# motive command

```sh
ursatz-composer motive <input> [--meter <n/m>] [--key <key>] [--format <auto|midi|musicxml|notes|degrees|rhythm|contour|json>] [--form <full|rhythm|contour>] [--pickup <beats>] [--harmony "<text>"] [--feature <label:first-last>]... [--role <melodic|accompaniment>] [--tone <n>=<role>]... [--render-key <key>] [--midi <file>] [--musicxml <file>] [--tempo <bpm>] [--json]
```

Loads a motive, a short cell of notes (one bar or less, as a rule), and prints its analysis: the
intervals, the contour, the rhythm, the range, the scale degrees and the chords it implies. The motive
can be attached to a plan as Given material with `motive_attach` (`<ursatz_composer/motive.h>`); this
command reads and reports, and with `--render-key`, `--midi` or `--musicxml` it also renders the motive
(below).

The command is a wrapper of the [composer API](api.md): it reads its arguments and the input file into a request, calls
`composer_api_motive` (or `composer_api_motive_from_midi` for a MIDI file, and `composer_api_motive_render` with
`--render-key`, `--midi` or `--musicxml`), writes the MIDI and MusicXML the response holds to the paths given and prints the
response JSON, or the text lines made from it. The request members have the options' names and values (`meter`, `key`,
`pickup`, `harmony`, `features`, `role`, `tones`, `render_key`, `tempo`).

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

## Harmony, features and role

A motive can carry its own harmony, labelled features and a role.

| Flag | Meaning |
| --- | --- |
| `--harmony "<text>"` | The motive's own chords: Roman numerals with their onsets in beats, `"I V6@2 I@3"`. Up to 8. An empty text clears the chords the input has. |
| `--feature <label:first-last>` | A labelled run of notes by 0-based index, inclusive: `a:0-2`, or `turn:3` for one note. The label is 1 to 8 characters. Repeat the flag for more, up to 8, each label different. |
| `--role <role>` | `melodic` (the default) or `accompaniment`. |

Each one replaces what the input says (a JSON document may carry `harmony`, `features` and `role`).

Chords. A chord is an upper case numeral `I` to `VII`, with `b` or `#` before it for a lowered or raised root, and a figure after it: none for a triad in root position, `6` and `64` for its inversions, `7`, `65`, `43` and `42` for a seventh chord and its inversions. `@` and the onset in beats of the meter's beat unit, counted from the start of the motive, a whole number or a fraction (`@1/2`), says when it starts. The first chord needs no onset (it starts at 0); the others do, and the onsets rise. Every chord starts inside the motive. A motive without chords of its own uses its best implied chord, one chord at 0, wherever a relation needs chords.

The onsets are in beats of the motive's meter, which a MIDI or MusicXML file may declare, so with such a file the file is read twice, as for `--pickup`. A text that is not a chord is exit status 1 and names the token and column (`token 2 (column 4)`); a chord that starts outside the motive, or a feature outside its notes, is exit status 2 and names the place (`harmony[1]`, `features[0]`).

The text output lists `Own harmony:`, the features and the role when the motive has them. The `--json` document has them in `motive` and `harmony_text` in `analysis` (see [plan-format.md](plan-format.md#motive-analysis)).

The same three options are read by [variants](cli-variants.md) and [grow](cli-grow.md); [random-motive](cli-random-motive.md) takes `--role`.

## Base harmony and tones

Every note of a full motive with a key is heard against a base harmony, and has a harmonic role.

The base harmony is the motive's own harmony when it has one (`given`). A motive has at most 2 chords in a bar: more is exit status 1, `at most 2 harmonies per bar`. Without its own harmony the base harmony is proposed, bar by bar, from the triads of the key (`I V IV vi ii iii vii`):

- The triad with the most beat notes (notes that start on a beat of the meter) as chord tones wins. A tie goes to `I`, then to the earlier triad in that order.
- A second chord comes in a bar only when the best triad leaves two or more beat notes as non-chord tones and a triad starting on a later beat of the bar makes at least two of them chord tones. The triad that gains the most wins, the earlier beat and then the earlier triad on a tie.
- A bar with no note keeps the chord before it, and a chord equal to the one before it is not repeated.

A rhythm, a contour and a motive without a key have no base harmony.

Tones. Each note is a chord tone, a non-chord tone of one of ten types (`passing`, `neighbor`, `appoggiatura`, `suspension`, `retardation`, `escape`, `anticipation`, `double-neighbor`, `changing-tone`, `pedal`) or unclassified, by the rules of the library's motive tone roles. A non-chord or unclassified note with no chord tone to move to is unresolved.

| Flag | Meaning |
| --- | --- |
| `--tone <n>=<role>` | Labels note `n` (1-based) by hand: `chord`, or a type name above. Repeat the flag for more notes; the later label of a note wins. A bad note, a bad role, or a motive with no base harmony is exit status 1. |

Resolution is worked out for the label you give a note; a label on one note never changes how another is labelled.

The text output has a `Base harmony:` line (`I V@2 (proposed)`, `VI (given)`, `none`), a `Tones:` line (`1 A5 chord, 2 B5 passing, ...`, `none`) and a `Warning: Note <n> (<name>) does not resolve` line for each unresolved note. The `analysis` in `--json` has `base_harmony` (`text` in the own-harmony syntax and `given`, or `null`), `tones` (one object per note: `note` 1-based, `role` `chord`, `non-chord` or `unclassified`, `type` or `null`, `resolves_to` 1-based or `null`, `unresolved`, `overridden`) and `warnings`. [variants](cli-variants.md) gives the same fields in every variant's `motive`.

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

## Rendering

`--render-key <key>` hears the motive in another key: each note keeps its degree of the scale and its
alteration, and sounds on that degree of the new key (degree 1 in octave 0, MIDI 60 to 71 for the tonic's
pitch class). A D minor motive `1:q b3:e 5:e 1':h` in G minor is `G4 Bb4 D5 G5`. The mode may change; the
degrees are counted in the new mode's scale. The default is the motive's own key. A motive with no key can
only be rendered as it is (rhythm and contour forms sound on C major's degrees); `--render-key` on one is
an error.

| Flag | Meaning |
| --- | --- |
| `--midi <file>` | Write the rendered motive as a MIDI file. |
| `--musicxml <file>` | Write it as a MusicXML file. |
| `--tempo <bpm>` | The tempo of the files, 20 to 400 quarter notes per minute, as in `export`. |

The files are one voice (a part named `melody`), as long as the motive, with its meter and, when it has
one, the key it is rendered in. A pickup is written as a short first meter, its own length (a pickup of
one beat in 3/4 is a 1/4 meter), and then the motive's meter from the first barline. In MusicXML the
first bar is `<measure number="0" implicit="yes">`; the full bars are numbered from 1. A pickup whose
length is not a meter (1/3 of a beat) cannot be written. Library: `motive_in_key` in
`<ursatz_composer/motive.h>`, `motive_export_midi` and `motive_export_musicxml` in
`<ursatz_composer/motive_render.h>`.

With `--json`, a render option adds a top-level `rendered` object next to `motive` and `analysis`:

```json
"rendered": {"key": "G minor", "notes": [{"midi_pitch": 67, "offset": {"num": 0, "den": 1}, "duration": {"num": 1, "den": 4}}]}
```

`key` is `null` for a motive with no key. Offsets are in whole notes from the motive's start, which is the
start of its pickup. The `analysis` is still the motive's own, in the key it was given.

## Exit status

| Status | Meaning |
| --- | --- |
| 0 | Success. |
| 1 | Usage or an invalid argument. |
| 2 | The input cannot be loaded as a motive, or cannot be moved to `--render-key`. The message gives the reason and where: a token and column in a note list, a field path in JSON, a position in a file. |
| 3 | A `--midi` or `--musicxml` file cannot be written. |

Errors are printed on stderr as `Error: ...`; nothing is printed on stdout.
