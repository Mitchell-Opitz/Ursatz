# Export

A plan is written as a MIDI file or a MusicXML file. The Composer renders the plan (`render.h`) and hands the notes,
meter and key signatures to Ursatz's MIDI renderer (`midi_renderer_render_with_options`) and MusicXML exporter
(`musicxml_export_write_score`). Every command that writes `--midi` or `--musicxml` (`grow`, `fill`, `regenerate`,
`variations`, `harmonize`, `progression`, `counterpoint`, `develop`, `export`) shares these options. Back to the [docs index](README.md).

Notes are spelled in the key in force at their onset: a note of the key's scale takes its signature spelling
(`standard_tonality_key_signature_of`), any other is placed on the line of fifths, sharp in a sharp key or none, flat in a
flat key (`pitch_spelling_from_fifths`), and its octave is the one `pitch_spelling_midi_number` reads back as the MIDI
pitch. The key's signature count is `standard_tonality_key_fifths` of its tonic.

## Options

| Option | Effect |
| --- | --- |
| `--tempo <bpm>` | A tempo in the MIDI and the MusicXML file. Quarter notes per minute from 20 to 400, a whole number or a fraction like `195/2`. Default: none. |
| `--plain-midi` | MIDI without track names and without section markers (the file the Composer wrote before names and markers). |

In C these are `PlanExportOptions` (`export.h`): `tempo_numerator` and `tempo_denominator` (0 and 0 is no tempo),
`track_names` and `section_markers`. `plan_export_options_default()` is no tempo, names and markers on. Every export
function takes a `const PlanExportOptions *`; NULL means the default.

| (C only) `loop_markers`, `loop_start`, `loop_end` | MIDI: the markers `loopStart` and `loopEnd` ([below](#loop-markers)). The `piece` command sets them on the loop version. |

A plan stores no tempo. The tempo is an export option only, so the same plan can be written at any speed.

## MIDI

Format 1, 480 ticks per quarter note when every time lands on a whole tick there and otherwise the smallest division
that does (section starts included).

- **Tracks.** Track 0 is the conductor: tempo, time signature, key signature and markers. Every part is one further
  track, in the order its voice first sounds.
- **Names.** With names on, each track starts with a Sequence/Track Name (`FF 03`) at tick 0: the conductor track is
  named for the form node (`piece` when the form has no name) and the part tracks for their lane, as the MusicXML parts
  are: `melody`, `bass`, `inner_voice_<n>`.
- **Markers.** With markers on, every section that is not source material gets a Marker (`FF 06`) in the conductor track
  at its start: its form letter (`A`, `B`, `A'`) when it has one, else the node's name. A marker follows the tempo, time
  signature and key signature events at the same tick.
- **Tempo.** One region from tick 0. The tempo is written as whole microseconds per quarter note, rounded to the nearest
  (96 is 625000, 195/2 is 615385). A tempo that is zero in one part only, or does not fit 1 to 16777215 microseconds,
  is refused (`PLAN_EXPORT_ERR_NOT_REPRESENTABLE`).

### Loop markers

With `PlanExportOptions.loop_markers` the conductor track also carries a Marker `loopStart` at `loop_start` and a Marker
`loopEnd` at `loop_end` (whole notes from the start of the piece, as a `Rational`), after the section markers at the same tick.
A game reads them to play the intro once and then repeat the region between them without a gap. They are independent of the
names and section markers (`--plain-midi` keeps them) and are in the conductor track only. A time that no division up to the
renderer's limit holds is refused (`PLAN_EXPORT_ERR_NOT_REPRESENTABLE`). MusicXML has none.

## MusicXML

The parts are named `melody`, `bass`, `inner_voice_<n>`. With a tempo, part 0 carries `<sound tempo="96"/>` at the start.
The value is 60,000,000 divided by the whole microseconds written, so a tempo that is not a whole number of microseconds
reads back as its nearest value (140 bpm as 140.00014). Names and markers are MIDI only: Ursatz's MusicXML export has no
rehearsal marks.

## Tests

`tests/test_export_daw.c` (a filled rounded binary at tempo 96: same notes and tempo on re-import, names, markers at the
section starts, plain bytes equal to Ursatz's renderer with no options, MusicXML tempo) and `tests/cli_export.cmake`
(`--tempo` and `--plain-midi` on `fill`, `export`, `regenerate`, `variations`, and their errors).
