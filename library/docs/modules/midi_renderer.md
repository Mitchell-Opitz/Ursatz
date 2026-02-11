# MIDIRenderer

`src/io/midi_renderer.{h,c}`, tests `tests/io/test_midi_renderer*.c`. Part of `music-io` (L9); same dependencies as the rest of the module (`identity`, `core`, `time`, `pitch`, `events`).

`midi_renderer_render` writes NoteEvents (with the index-paired PitchNumericValues `midi_importer_import` returns), an optional MeterMap, an optional TempoMap and optional key signature events to a Standard MIDI File of format 1. It is the inverse of `MIDIImporter` and changes none of the importer, `smf_reader`, MeterMap or TempoMap.

## Output

- **Header.** Format 1, track count, the caller's division (ticks per quarter note, 1 to 0x7FFF; SMPTE is rejected).
- **Conductor track (track 0).** Set-tempo (`FF 51`), time signature (`FF 58`) and key signature (`FF 59`) meta-events in tick order; at an equal tick tempo, then time signature, then key signature, supplied order within a kind. Defaulted map regions are written as declared ones. With no map or key signatures supplied nothing is written: the SMF defaults (4/4, 120 bpm) apply and the renderer never invents a meter or tempo.
- **Voice tracks.** One track per distinct voice reference (by target id); notes without a voice share one track. Tracks are ordered by when their voice first sounds, then input order. Track `i` plays on channel `i - 1`, skipping channel 9 (percussion), so at most 15 voice tracks exist; more is `MIDI_RENDER_ERR_TOO_MANY_VOICES`.
- **Notes.** Note-on velocity 64 (dynamics are not modeled), note-off velocity 0. At an equal tick note-offs are written before note-ons. Running status is never used. Every track ends with End of Track.
- **Determinism.** Same inputs, same bytes. On any failure the outputs are untouched.

## Exactness

`tick = position * 4 * division`. A position or duration that is not a whole number of ticks is `MIDI_RENDER_ERR_NOT_REPRESENTABLE`; nothing is rounded. The same result covers: a negative position, a tick past the 28-bit delta limit, a pitch that is not an integer MIDI note number 0 to 127, two same-pitch notes overlapping in one voice track, tempo 0 or above 0xFFFFFF, a time signature top above 255, and a key signature sf outside -7 to 7.

## Round trip

The tests render the five framework-v1 reference pieces at their own division and re-import them: note position, pitch and duration as a multiset, meter and tempo regions, and key signatures are equal. Note ids and order are not preserved. Voice grouping is checked as the partition of notes into tracks in the written file, not through the importer, which recomputes voices from channels. A map's `defaulted` flag is not preserved.

## Out of scope

Velocity and dynamics, articulation, control and program events, sysex, SMPTE division, format 0 and 2, track names, MusicXML export, playback, persistence.
