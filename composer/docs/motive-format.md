# Motive documents

The motive document and its motive analysis, as the plan format embeds them (see the [plan format](plan-format.md)). Back to the [docs index](README.md).

## Motive documents

A motive is a short cell of notes the user supplies (`<ursatz_composer/motive.h>`). It is read from MIDI,
MusicXML, a note list or JSON, and attaches to a plan as a Given motive node with a Given melody lane span
(`motive_attach`). The JSON below is the motive on its own and the form `motive_from_json` reads and
`motive_to_json` writes. It uses the plan's vocabulary: the meter and key are payloads of the meter and key
lanes, and `notes` is the note payload of a melody lane, with times as in the conventions above.

| Key | Content |
| --- | --- |
| `meter` | Optional. `beats_per_bar` (1 to 64), `beat_unit` (1, 2, 4, 8 or 16) and optionally `bars_per_hyperbar`. |
| `key` | Optional. `tonic_pitch_class` (0 to 11) and `mode` (`major` or `minor`). |
| `notes` | A non-empty array of at most 64 `{"midi_pitch": 0 to 127, "offset": <time>, "duration": <time>}`. `offset` is measured from the start of the motive, so a gap between notes is a rest and a first note after 0 is a leading rest. Notes are in order and do not overlap. A tie is one note as long as both. |
| `form` | Optional, `full` (the default), `rhythm` or `contour`. A rhythm keeps only the durations and onsets (every note sounds on one pitch); a contour only the heights of the notes in relation to each other (even quarters). Written only when it is not `full`. |
| `pickup` | Optional. A time in whole notes: the length before the first barline, shorter than one bar. The notes still start at 0; bars and beats are counted from the barline. Written only when it is not 0. |
| `harmony` | Optional. The motive's own chords as text, `"I V6@2 I@3"`: upper case Roman numerals (`b` or `#` before one for an altered root; figures `6`, `64`, `7`, `65`, `43`, `42`) and `@` and the onset in beats of the beat unit from the start of the motive, a whole number or a fraction. The first chord needs no onset. At most 8 chords, onsets rising and inside the motive. Written only when the motive has chords. |
| `features` | Optional. Up to 8 labelled runs of notes, `{"label": 1 to 8 characters, "first": note index, "last": note index}`, 0-based and inclusive, labels different. Written only when the motive has features. |
| `role` | Optional, `melodic` (the default) or `accompaniment`. Written only when it is not `melodic`. |

The motive ends where its last note ends. Unknown fields are ignored; a value that is not allowed is an error
that names the field, for example `notes[2].duration.den`.

```json
{
  "meter": {"beats_per_bar": 4, "beat_unit": 4, "bars_per_hyperbar": 0},
  "key": {"tonic_pitch_class": 2, "mode": "minor"},
  "notes": [
    {"midi_pitch": 62, "offset": {"num": 0, "den": 1}, "duration": {"num": 1, "den": 4}},
    {"midi_pitch": 65, "offset": {"num": 1, "den": 4}, "duration": {"num": 1, "den": 8}},
    {"midi_pitch": 69, "offset": {"num": 3, "den": 8}, "duration": {"num": 1, "den": 8}},
    {"midi_pitch": 74, "offset": {"num": 1, "den": 2}, "duration": {"num": 1, "den": 2}}
  ]
}
```

The meter is the file's own for MIDI and MusicXML (4/4 when a file has none), and a meter given by the caller
that differs from a declared one is an error. A note list has no meter and needs one from the caller. The key is
the caller's, else the input's (a key signature with a major or minor mode, or `key` above); otherwise it is
inferred when the pitches settle on one key, and is left out when they do not. A key that was inferred is not
written, since reading the motive infers it again.

### Motive analysis

`motive_analysis_to_json` writes the motive and its analysis together, as an object with a `motive` member (the
document above) and an `analysis` member (and, from `motive --render-key`, `--midi` or `--musicxml`, a `rendered` member: `{"key", "notes": [{"midi_pitch", "offset", "duration"}]}`, see [cli-motive.md](cli-motive.md#rendering)):

| Key | Content |
| --- | --- |
| `form`, `pickup` | The motive's form, and its pickup in beats of the beat unit as text: `"0"`, `"1"`, `"1/2"`. |
| `notes_text`, `degrees_text` | The notes as a note list (`D4:q F4:e`), and as a degree list (`1:q 3:e`, `null` without a key). |
| `rhythm_text`, `contour_text` | Only for the form: the rhythm list (`x:q x:e`) of a `rhythm` motive, the contour list (`0 2 1 3`) of a `contour` motive. |
| `note_count` | The number of notes. |
| `length`, `bars` | The motive's length in whole notes, and as a number of bars of its meter. Times. |
| `key` | `null` without a key. Otherwise `tonic_pitch_class`, `mode` and `source`: `given` or `inferred`. |
| `intervals` | `null` for a rhythm or a contour. Otherwise one per pair of neighboring notes: `semitones` (signed) and `steps` (signed diatonic steps in the key, `null` without a key). |
| `contour` | `null` for a rhythm. Otherwise `shape` (`single note`, `level`, `ascending`, `descending`, `arch`, `valley` or `wave`) and `directions`, one of `up`, `down` and `same` per interval. |
| `rhythm` | `null` for a contour. Otherwise one per note: `onset`, `duration`, `bar` (the first full bar is 0, and a pickup's notes are in bar -1), `beat` (the position in the bar in beats of the beat unit, 0 is the downbeat; a time), `on_beat`, `dotted` (the duration is 3 or 7 over a power of two) and `rest_before` (a time). |
| `range` | `null` for a rhythm or a contour. Otherwise `lowest` and `highest` MIDI pitch, and `semitones` between them. |
| `scale_degrees` | `null` without a key, and for a rhythm or a contour. Otherwise `start`, `end` and `all` (one per note), each `{"degree": 1 to 7, "alteration": 0 or 1}`; 1 is a raised degree. |
| `harmony_text` | Only for a motive with chords of its own: the chords as text, as `harmony` above. |
| `implied_harmony` | `key_known`, and `candidates`, best fit first: `roman_numeral`, `function`, `fit` (the share of the motive that is chord tones, 0 to 1, to four places), `chord_tones` and `non_chord_tones`. Empty without a key, and for a rhythm or a contour. |
| `base_harmony` | `null` without a key, and for a rhythm or a contour. Otherwise `text` (the base harmony as `harmony` above) and `given` (true when it is the motive's own harmony, false when it was proposed from the notes; at most 2 chords in a bar). |
| `tones` | One object per note, `[]` when `base_harmony` is `null`: `note` (1-based), `role` (`chord`, `non-chord` or `unclassified`), `type` (`passing`, `neighbor`, `appoggiatura`, `suspension`, `retardation`, `escape`, `anticipation`, `double-neighbor`, `changing-tone` or `pedal` for a non-chord tone, else `null`), `resolves_to` (1-based note or `null`), `unresolved` and `overridden` (the label came from `--tone`). |
| `warnings` | One string per unresolved note: `Note 3 (B5) does not resolve`. |

For the motive above, `scale_degrees` gives `1 3 5 1`, the contour is `ascending` and the intervals are `+3`, `+4`
and `+5` semitones, `+2`, `+2` and `+3` steps. The best implied chord is `i`, with `fit` 1.

The analysis is deterministic: the same motive gives the same document, byte for byte.
