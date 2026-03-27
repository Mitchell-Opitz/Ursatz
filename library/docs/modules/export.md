# MusicXmlExport

`src/export/musicxml_export.{h,c}` (the persistence entry point) and `src/export/musicxml_score.c` (the layout and writer), tests `tests/export/`. Layer L9 (`music-export`). Depends on `identity`, `time`, `pitch`, `events` and `persistence` only; the importer's types (`io/`) are not used, so the caller copies an import's members into the input struct.

## Entry points

- `musicxml_export_write(PersistenceHandle *, char **)`: unchanged signature. Loads the persisted NoteEvents and writes one part. A persistence handle holds no meter, so measures are laid from `meter_map_create_defaulted` (one 4/4 region marked defaulted) and the output carries `<miscellaneous-field name="ursatz:meter-defaulted">true</miscellaneous-field>` under `<identification><miscellaneous>`. This flag replaces the earlier `ursatz:synthetic-meter` field: the layout is no longer called synthetic, it is a default that the flag reports.
- `musicxml_export_write_score(const MusicXmlExportScore *, char **)`: the full writer. The input is borrowed and never mutated, and the output is a newly allocated MusicXML 3.1 string for the caller to `free()`. On any failure `*out_xml` is not written.

`MusicXmlExportScore` takes: events (and optional index-paired `pitch_values`, which are checked for NULL entries and not otherwise read), optional `part_index` and `staff` arrays, optional parts (id, name, staff_count), an optional `MeterMap` and `TempoMap`, optional `TempoMarking`s (with a part each), optional `Dynamics` (a part and staff each) and optional key signatures (`MusicXmlExportKeySignature`: sharps/flats, mode, position, part). With no parts there is one part `P1` whose staff count is the highest staff used.

## Layout

- **Measures come from the `MeterMap` and nowhere else.** `<measure number>` is the map's own number (a pickup is measure 0 and is written `implicit="yes"`; a short measure keeps its number; a barline belongs to the following measure). `<time>` is written in the measure where a region starts, from its `TimeSignature`. A defaulted map still writes its `<time>`; it also sets the meter-defaulted flag, and a map with any declared region does not.
- **Exact time.** `<divisions>` (per quarter note) is the least common multiple that makes every note position and duration, every barline and every marking position a whole number of divisions. Nothing is rounded. If no value up to `INT32_MAX` does, the result is `MUSICXML_EXPORT_ERR_DIVISIONS_OVERFLOW`.
- **Ties and rests.** A note that crosses a barline, or a key, tempo, marking or dynamics position of its part, is split there and tied (the importer merges such ties back). A gap inside a measure is a rest, and the first sounding voice of each measure is padded with a rest to the barline, so every measure has its full length. A measure no voice sounds in is one rest, in voice 1.
- **Parts, staves, voices.** One `<part>` per part, `<staves>` when staff_count is above 1, `<staff>` on each note, and `<backup>` between voices. A voice is a group of notes sharing a VoiceReference within one part and staff (no voice reference is one implicit voice). Overlap is checked per group: `MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES` only when two notes of one voice, part and staff overlap.
- **Pitch** is written from `PitchSpelling`; a note without one is `MUSICXML_EXPORT_ERR_MISSING_SPELLING`.
- **Key signatures**: `<key><fifths>` and `<mode>` (none for an unknown mode), in the event's part, in the `<attributes>` of the measure that contains it.
- **Tempo**: `<sound tempo>` (quarter notes per minute, up to 12 decimal places) in part 0 at each `TempoMap` region that is not defaulted; a defaulted map writes none. A `TempoMarking` is a `<direction>` with `<words>` and a `<metronome>` (a beat unit MusicXML names, up to four dots, and an exactly representable per-minute). A marking at a tempo region's position shares one `<direction>` with the `<sound>`, so it comes back as one marking. A marking with text only and no tempo at its position comes back as unclassified text.
- **Dynamics**: `<direction><dynamics>` at the start of the Dynamics' span, or at its event; the names MusicXML has are elements and the rest are `<other-dynamics>`.

## Errors

`MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT` (a count with no array, a NULL entry, parts that do not add up), `_UNKNOWN_PART`, `_BAD_STAFF`, `_UNKNOWN_EVENT` (a Dynamics targets an event the score lacks), `_NOT_REPRESENTABLE` (a negative position, a zero duration, a key outside -7 to 7, a beat unit or per-minute MusicXML cannot write, an overflow) and `_DIVISIONS_OVERFLOW`. They were appended to the enum; the earlier values are unchanged.

## Round trip

`tests/export/test_musicxml_score_fixtures.c` and `test_musicxml_score_reference.c` import, export through `musicxml_export_write_score` and import again, and compare notes (position, duration, sounding pitch, spelling, part, staff), parts, `MeterMap` regions and measure count, `TempoMap` regions, key signatures, tempo markings and dynamics. The five reference pieces go MIDI import to MusicXML. Clementi 36 and Field 1 are two-channel files with no voices, so the test gives each hand its own staff by colouring notes greedily; the library does not do this for a caller. A map's `defaulted` flag is not compared (a defaulted region comes back as a declared one).

## Not exported

Articulations, slurs, lyrics, ornaments, grace notes, tuplet brackets, repeats and endings, page layout, MusicXML 4.0, compressed `.mxl`, wedges, transposition (the written spelling is exported, and a transposing part's sounding pitch is not), chords as a single voice (two notes of one voice at one position overlap), and reading meter, tempo, key or dynamics from persistence.
