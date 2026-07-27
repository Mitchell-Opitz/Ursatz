# MusicXmlExport

`src/export/musicxml_export.{h,c}` (the persistence entry point) and `src/export/musicxml_score.c` (the layout and writer), tests `tests/export/`. Layer L9 (`music-export`). Depends on `identity`, `time`, `pitch`, `events` and `persistence` only; the importer's types (`io/`) are not used, so the caller copies an import's members into the input struct.

## Entry points

- `musicxml_export_write(PersistenceHandle *, char **)`: unchanged signature. Loads the persisted NoteEvents and writes one part. A persistence handle holds no meter, so measures are laid from `meter_map_create_defaulted` (one 4/4 region marked defaulted) and the output carries `<miscellaneous-field name="ursatz:meter-defaulted">true</miscellaneous-field>` under `<identification><miscellaneous>`. This flag replaces the earlier `ursatz:synthetic-meter` field: the layout is no longer called synthetic, it is a default that the flag reports.
- `musicxml_export_write_score(const MusicXmlExportScore *, char **)`: the full writer. The input is borrowed and never mutated, and the output is a newly allocated MusicXML 3.1 string for the caller to `free()`. On any failure `*out_xml` is not written.

`MusicXmlExportScore` takes: events (and optional index-paired `pitch_values`, which are checked for NULL entries and not otherwise read), optional `part_index` and `staff` arrays, optional parts (id, name, staff_count), an optional `MeterMap` and `TempoMap`, optional `TempoMarking`s (with a part each), optional `Dynamics` (a part and staff each) and optional key signatures (`MusicXmlExportKeySignature`: sharps/flats, mode, position, part). With no parts there is one part `P1` whose staff count is the highest staff used.

## Layout

- **Measures come from the `MeterMap` and nowhere else.** `<measure number>` is the map's own number (a pickup is measure 0 and is written `implicit="yes"`; a short measure keeps its number; a barline belongs to the following measure). `<time>` is written in the measure where a region starts, from its `TimeSignature`. A defaulted map still writes its `<time>`; it also sets the meter-defaulted flag, and a map with any declared region does not.
- **Exact time.** `<divisions>` (per quarter note) is the least common multiple that makes every note position and duration, every barline and every marking position a whole number of divisions. Nothing is rounded. If no value up to `INT32_MAX` does, the result is `MUSICXML_EXPORT_ERR_DIVISIONS_OVERFLOW`.
- **Ties and rests.** A note that crosses a barline, or a key, tempo, marking or dynamics position of its part, is split there and tied (the importer merges such ties back). A gap inside a measure is a rest, and the first sounding voice of each measure is padded with a rest to the barline, so every measure has its full length. A measure no voice sounds in is one `<rest measure="yes"/>`, in voice 1. Notes and rests are further cut into notated values, below.
- **Parts, staves, voices.** One `<part>` per part, `<staves>` when staff_count is above 1, `<staff>` on each note, and `<backup>` between voices. A voice is a group of notes sharing a VoiceReference within one part and staff (no voice reference is one implicit voice).
- **Chords and lanes.** `musicxml_export_write_score` writes chords and overlaps; it never returns `MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES`. Each voice is first split into lanes, with its notes in start order: a note with the same start and end as a lane's last chord joins that chord; any other note goes to the lowest lane that is free at its start, and a new lane is made when none is. A chord is written as its first note plain and every other member right after it with `<chord/>` as the first child of `<note>`, the same `<duration>`, `<voice>` and `<staff>`, its own `<pitch>` and `xml:id`, and its own ties on each cut piece. Rests are never chord members. Lane 0 is the voice itself; every later lane becomes a voice of its own on the same part and staff, numbered after every voice number the part already uses, so `<voice>` is distinct per part. Lanes are a writing device, not voice separation: the notes come back from a re-import with the same onsets, durations and pitches, not the same voices. Output for input with no overlap is unchanged.
- **Persistence path.** `musicxml_export_write` still fails with `MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES` when two notes of one voice overlap, through an internal strict entry (`musicxml_score_internal.h`) of the same writer.
- **Pitch** is written from `PitchSpelling`; a note without one is `MUSICXML_EXPORT_ERR_MISSING_SPELLING`.
- **Key signatures**: `<key><fifths>` and `<mode>` (none for an unknown mode), in the event's part, in the `<attributes>` of the measure that contains it.
- **Tempo**: `<sound tempo>` (quarter notes per minute, up to 12 decimal places) in part 0 at each `TempoMap` region that is not defaulted; a defaulted map writes none. A `TempoMarking` is a `<direction>` with `<words>` and a `<metronome>` (a beat unit MusicXML names, up to four dots, and an exactly representable per-minute). A marking at a tempo region's position shares one `<direction>` with the `<sound>`, so it comes back as one marking. A marking with text only and no tempo at its position comes back as unclassified text.
- **Dynamics**: `<direction><dynamics>` at the start of the Dynamics' span, or at its event; the names MusicXML has are elements and the rest are `<other-dynamics>`.

## Notated versus played times

`MusicXmlExportScore.quantize` (`export/score_quantize.h`) is NULL by default: every note is written at its exact played start and duration, as above. When it is set, `score_quantize` turns the played times into notated ones first and the document is written from those. The events, their ids and their order are not changed, and nothing imported or analyzed is touched; the notated times exist only for the write.

- **Grid.** Per beat of the meter (the dotted beat in a compound meter such as 6/8), the grid is the straight one (`grid_denominator`, a 32nd by default) or, with `allow_triplets`, a third or a sixth of the beat. The grid with the lowest total snap error for the onsets in that beat wins; a tie goes to the straight grid and then to the coarser triplet grid. Onsets and ends snap to the nearest point of their beat's grid (a tie rounds up). An end in a beat with no onset uses the straight grid. A note is never shorter than one grid step.
- **Legato.** Within a voice group (part, staff, VoiceReference), a gap after a note's snapped end, up to `gap_absorb` times the note's own notated length and less than one beat, is absorbed: the end moves to the next onset of the voice. The notes of a chord look to the same next onset.
- **Chords and barlines.** Notes that end up with the same start and end in a voice are a chord, which the writer already handles. A snapped point never leaves its beat, so no onset or end moves across a barline by more than half a grid step.
- **Errors.** A negative position, a zero duration, a zero denominator option or an overflow is `SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE` (`MUSICXML_EXPORT_ERR_NOT_REPRESENTABLE` from the writer).
- **Not yet written.** Beams.

## Notated values

Every note and rest is written as values a player can read: after the cuts above, each piece is cut again by the beat of its measure (the bottom unit of a simple meter, three of them in a compound meter such as 6/8; the same beat `score_quantize` uses).

- **A value** is a power-of-two fraction of a whole note, whole to 128th, with up to two dots, or two thirds of an undotted one (a triplet value, which never crosses a beat). The plain reading wins when both fit.
- **Splitting.** A piece that starts off the beat stops at the next beat. One that starts on a beat may cover several beats as one value when that length is writable; in a full 4/4 measure one that starts on beat 2 stops at beat 3, and one that starts on beat 1 may go through it. Otherwise the longest run of whole beats that is writable is taken, then the first beat, then the longest writable value from the start. A length nothing can write (below a 128th, or not a whole number of divisions) is written with its exact `<duration>` and no `<type>`. Pieces of a note are tied; rests are never tied.
- **Elements.** `<duration>`, `<tie>`, `<voice>`, `<type>`, `<dot/>` per dot, `<time-modification>`, `<staff>`, `<beam>`, `<notations>` (`<tied>` and `<tuplet>`), in MusicXML order. `<divisions>` also covers the beat, so a split at a beat is a whole number of divisions. A note cut into several values keeps its `_a`, `_b` fragment ids.
- **Triplets.** A triplet value writes `<time-modification>` (3 actual, 2 normal). A run of two or more back to back inside one beat opens `<tuplet type="start"/>` on its first note or rest and closes `<tuplet type="stop"/>` on its last; chord members follow the first note.
- **Beams.** Within one voice of a measure, consecutive notes written as an eighth or shorter and back to back inside one beat group are beamed: `<beam number="n">` for n = 1 (eighth) up to 5 (128th), with `begin`, `continue` and `end`, or `forward hook` / `backward hook` on a level that only one note of the group reaches. The beat group is the beat of the meter (the dotted beat in a compound meter such as 6/8), so a beam never crosses a beat; a triplet run lies inside one beat and is one group. A rest, a longer note or a length with no `<type>` ends a group, a single note is not beamed, and a chord is beamed once, on its first note. User-set beaming and cross-staff beams are not written.
- **Measure rests.** A measure no voice sounds in, with no key, tempo, marking or dynamics position inside it, is one `<rest measure="yes"/>` with the measure's duration and no `<type>`.

## Grand staff split

`export/score_staves.h` decides which instruments use two staves and which staff each note takes. It is a separate step: the caller passes the result on as `MusicXmlExportScore.staff` with a part whose `staff_count` is 2.

- **Programs.** `score_program_uses_grand_staff` is true for General MIDI programs 0 to 8 (pianos, harpsichord, clavinet, celesta), 16 to 21 (organs, accordion) and 46 (harp), 0-based; false otherwise and for -1.
- **Assignment.** `score_staves_assign` gives staff 1 to every note of a part with staff count 1. In a part with staff count 2 the split point is middle C (MIDI 60, from the pitch value as `midi_importer` writes it): notes that start together and span more than an octave across middle C split at their largest adjacent interval (a tie goes to the gap nearest middle C), otherwise a note at or above 60 is treble (1) and below is bass (2).
- **Middle C window.** From 55 to 64 a note takes the staff of the nearer pitch of the last treble note and the last bass note that ended within the whole note before it; with no such note, or at equal distance, the middle C rule applies.
- **Errors.** `SCORE_STAVES_ERR_INCONSISTENT_INPUT` for a part index out of range, a staff count other than 1 or 2, a missing pitch value in a two-staff part, a negative position, a zero duration or an overflow.

## Errors

`MUSICXML_EXPORT_ERR_INCONSISTENT_INPUT` (a count with no array, a NULL entry, parts that do not add up), `_UNKNOWN_PART`, `_BAD_STAFF`, `_UNKNOWN_EVENT` (a Dynamics targets an event the score lacks), `_NOT_REPRESENTABLE` (a negative position, a zero duration, a key outside -7 to 7, a beat unit or per-minute MusicXML cannot write, an overflow) and `_DIVISIONS_OVERFLOW`. They were appended to the enum; the earlier values are unchanged.

## Round trip

`tests/export/test_musicxml_score_fixtures.c` and `test_musicxml_score_reference.c` import, export through `musicxml_export_write_score` and import again, and compare notes (position, duration, sounding pitch, spelling, part, staff), parts, `MeterMap` regions and measure count, `TempoMap` regions, key signatures, tempo markings and dynamics. The five reference pieces go MIDI import to MusicXML. Clementi 36 and Field 1 are two-channel files with no voices, so the test gives each hand its own staff by colouring notes greedily; the library does not do this for a caller. A map's `defaulted` flag is not compared (a defaulted region comes back as a declared one). `test_musicxml_score_overlaps.c` covers chords and overlaps: a chord, a held note under two notes, a partial overlap, lane voice numbering, Clementi 36 and Field 1 with no staff or part arrays, and `chords_backup.musicxml`; it compares count and the multiset of (onset, duration, pitch) after a re-import, not voices.

## Not exported

Articulations, slurs, lyrics, ornaments, grace notes, beams, repeats and endings, page layout, MusicXML 4.0, compressed `.mxl`, wedges, transposition (the written spelling is exported, and a transposing part's sounding pitch is not), notation inference and voice separation by musical sense, and reading meter, tempo, key or dynamics from persistence.
