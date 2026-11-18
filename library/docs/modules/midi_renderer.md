# MIDIRenderer

[Docs index](../README.md): the Ursatz docs by topic.

`io/midi_renderer.{h,c}`, tests `tests/io/test_midi_renderer*.c`. Part of `music-io` (L9); same dependencies as the rest of the module (`identity`, `core`, `time`, `pitch`, `events`).

`midi_renderer_render_with_options` writes NoteEvents (with the index-paired PitchNumericValues `midi_importer_import_complete` returns), an optional MeterMap, an optional TempoMap and optional key signature events to a Standard MIDI File of format 1. It is the inverse of `MIDIImporter` and changes none of the importer, `smf_reader`, MeterMap or TempoMap.

## Output

- **Header.** Format 1, track count, the caller's division (ticks per quarter note, 1 to 0x7FFF; SMPTE is rejected).
- **Conductor track (track 0).** Set-tempo (`FF 51`), time signature (`FF 58`) and key signature (`FF 59`) meta-events in tick order; at an equal tick tempo, then time signature, then key signature, supplied order within a kind. Defaulted map regions are written as declared ones. With no map or key signatures supplied nothing is written: the SMF defaults (4/4, 120 bpm) apply and the renderer never invents a meter or tempo.
- **Voice tracks.** One track per distinct voice reference (by target id); notes without a voice share one track. Tracks are ordered by when their voice first sounds, then input order. Track `i` plays on channel `i - 1`, skipping channel 9 (percussion), so at most 15 voice tracks exist; more is `MIDI_RENDER_ERR_TOO_MANY_VOICES`.
- **Notes.** Note-on velocity 64 (dynamics are not modeled), note-off velocity 0. At an equal tick note-offs are written before note-ons. Running status is never used. Every track ends with End of Track.
- **Determinism.** Same inputs, same bytes. On any failure the outputs are untouched.

## Track names and markers

`midi_renderer_render_with_options` takes a `MidiRenderOptions`, or NULL for none. Every member is optional (NULL, 0).

- **`conductor_name`.** A Sequence/Track Name meta-event (`FF 03`, variable-length length, the name's bytes as given) at tick 0, first in track 0.
- **`track_names`.** `MidiTrackName { voice_id, name }`: `voice_id` is the voice reference's target id value (`entity_id_value`), NULL for the track of notes with no voice. The name is an `FF 03` at tick 0, first in that voice's track. Entries are matched by id, in any order. A name for a voice with no notes is ignored.
- **`markers`.** `MidiMarker { position, text }`: a marker meta-event (`FF 06`) in track 0 at the position's tick. At an equal tick a marker follows the tempo, time signature and key signature events, and markers keep their supplied order among themselves.
- **Errors.** A NULL `name`, `text` or `position`, or a count above 0 with a NULL array, is `MIDI_RENDER_ERR_NULL_ARG`. Two names for one voice (two NULL voice ids included), a marker position that is negative or not a whole tick, or a name or text longer than 0x0FFFFFFF bytes is `MIDI_RENDER_ERR_NOT_REPRESENTABLE`. Nothing is written on error.
- **Import.** The importer keeps ignoring both meta-events: notes, meter and tempo read back are the same with and without them (`test_midi_renderer_meta.c`).

## Performer part tracks (R-599)

`MidiRenderOptions.parts` (`MidiRenderPart { name, program, voice_ids, voice_count, pedals, pedal_count }`) groups voices into part tracks. With `part_count` 0 the renderer is exactly as above, one track per voice.

- **Tracks.** Track 0 as before, then one track per part in the given order, named `name` at tick 0 when it is not NULL (the options' `track_names` are not read). A part none of whose voices has a note still gets its (empty) track.
- **Channels.** Each voice id a part lists is one channel. Channels go to the voices of all parts together in the order they first sound (earliest start tick, then input order), skipping channel 9: the rule voice tracks follow, so a movement whose voices were tracks keeps each note's channel. More than 15 channels is `MIDI_RENDER_ERR_TOO_MANY_VOICES`.
- **Programs and pedal.** Each channel gets a program change (`0xC0 | channel`, the part's General MIDI program 0 to 127) at tick 0 after the track name. Each pedal span of a part writes controller 64 with value 127 at its start tick and value 0 at its end tick on every channel of the part.
- **Order at an equal tick** in a part track: the name, the program changes, the note-offs, controller 64 value 0, controller 64 value 127, the note-ons, otherwise input order.
- **Refused** (`MIDI_RENDER_ERR_NOT_REPRESENTABLE`): a voice listed twice, a note with no voice or whose voice no part lists, a program outside 0 to 127, a pedal whose ticks are not whole. A NULL array with a count is `MIDI_RENDER_ERR_NULL_ARG`. A same-pitch overlap is refused per channel, so two voices of one part may sound the same pitch together.

## Exactness

`tick = position * 4 * division`. A position or duration that is not a whole number of ticks is `MIDI_RENDER_ERR_NOT_REPRESENTABLE`; nothing is rounded. The same result covers: a negative position, a tick past the 28-bit delta limit, a pitch that is not an integer MIDI note number 0 to 127, two same-pitch notes overlapping in one voice track, tempo 0 or above 0xFFFFFF, a time signature top above 255, and a key signature sf outside -7 to 7.

## Round trip

The tests render four of the five framework-v1 reference pieces (Bach 114, Clementi 36, Field 5, Mozart 545) at their own division and re-import them. `Field 1` is two-channel, so its notes carry no voice and share one track where the same pitch sounds twice at once; the renderer refuses it with `MIDI_RENDER_ERR_NOT_REPRESENTABLE` (asserted) rather than guess a split. For the four: note position, pitch and duration as a multiset, meter and tempo regions, and key signatures are equal. Note ids and order are not preserved. Voice grouping is checked as the partition of notes into tracks in the written file, not through the importer, which recomputes voices from channels. A map's `defaulted` flag is not preserved.

## Out of scope

Velocity and dynamics, articulation, control events other than a part's sustain pedal, program changes other than a part's, sysex, SMPTE division, format 0 and 2, lyrics, cue points, MusicXML rehearsal marks, reading names or markers in the importer, MusicXML export, playback.
