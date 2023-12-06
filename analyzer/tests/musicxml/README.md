# MusicXML test fixtures

## Reference pieces

`bach-114.musicxml`, `clementi-36.musicxml`, `field-1.musicxml`, `field-5.musicxml` and
`mozart-545.musicxml` are the MusicXML versions of the five reference pieces. They are the same
pieces as the MIDI files in Ursatz's `tests/fixtures/framework-v1-corpus`, and
`test_musicxml_midi_agreement` compares the analysis of each with the analysis of its MIDI file.

Provenance: derived from those MIDI files. Each file was imported with Ursatz's MIDIImporter (notes,
the MIDI time signatures as a MeterMap, the MIDI key signature events) and written with
`musicxml_export_write_score` from Ursatz main at `543ba31`, one part per piece (the export is newer
than the commit the submodule is pinned to; the fixtures are plain files, so the pin does not matter
to them). Three consequences:

- Meter and key signatures are the MIDI file's. A MIDI file with no time signature is written with a
  4/4 the exporter marks as defaulted, which the importer reads as notated; Bach 114 has no key
  signature, so its MusicXML has none either.
- Pitch spelling is the MIDI importer's, written out, so these files do not exercise enharmonic
  spelling. The `spelling-*` fixtures below do.
- Where notes of one voice overlap in time (Clementi 36, Field 1), which one `<voice>` cannot hold,
  the overlapping notes were put on separate staves of the one part.

Licence: the compositions are in the public domain (Bach, Mozart, Clementi and Field all died before
1900). The MIDI files carry no licence statement in the Ursatz repository, and these files are
mechanical conversions of them, so they are under whatever terms the MIDI files are.

## Hand-written fixtures

Written for this repository (same licence as the repository). Each is a few measures of block chords.

- `pickup.musicxml`: C major, 4/4, a one-beat pickup (an implicit measure 0) and four measures
  I - IV - V - I.
- `two-parts.musicxml`: three parts (Soprano, Alto, Bass), one note per measure each.
- `spelling-flat.musicxml`, `spelling-sharp.musicxml`: the same sounding I - IV - V - I, written in
  D-flat major and in C-sharp major.
- `pickup.mxl`: `pickup.musicxml` compressed (a zip with `META-INF/container.xml`).

## Error fixtures

- `malformed.musicxml`: not well-formed XML (cut off).
- `timewise.musicxml`: a `score-timewise` document.
- `empty-score.musicxml`: valid, but its one measure is a rest.
- `not-music.xml`: well-formed XML that is not a score.
