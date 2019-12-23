# io

Layer L9 (`music-io`). Path `src/io/`, tests `tests/io/`. Depends on `identity`, `core`, `time`, `pitch`, and `events` only. Must not depend on `containers` (L4) or anything above it, and must not depend on `theory`, `rules`, `transform`, `analysis`, `generation`, `frameworks/`, or `music-analysis`.

## Provides

`MIDIImporter`: parses a Standard MIDI File (SMF) type 0 or type 1 buffer into a flat, time-ordered sequence of canonical `NoteEvent`s (L3). This is intentionally the only thing in this module -- no renderer, no other importer format, and no `DigitalArtifact` type are scaffolded here, even though they are related Registry items for this layer.

## Notes

- `MIDIImporter` is built from three internal pieces: `smf_reader` (byte-level SMF header/track chunk parsing), `midi_event_decoder` (walks one track's decoded bytes and extracts note-on/note-off events only, including running status), and `midi_importer` itself (matches note-on/note-off pairs, converts to `NoteEvent`, flattens and time-orders across tracks). Only `midi_importer.h` is a public entry point; `smf_reader` and `midi_event_decoder` are implementation detail.
- **Fixed-tempo assumption.** Tempo meta-events are not read (see Deferred below). `MusicalTime`/`Duration` are produced in seconds under an assumed fixed tempo of 120 BPM -- the MIDI spec's own default when no tempo meta-event is present -- combined with the header's ticks-per-quarter-note (PPQN) division field: `seconds_per_tick = 60 / (120 * PPQN) = 1 / (2 * PPQN)`. This stays an exact rational (Law 19): a tick position/duration becomes `numerator = ticks`, `denominator = 2 * PPQN`. A file with an actual tempo map will therefore produce timing that does not match its real-world playback duration; this is a deliberate scope cut, not a bug.
- SMPTE-format division (the header's division field with its top bit set) is rejected as malformed rather than decoded -- consistent with "fail cleanly on anything not a well-formed SMF 0/1 file" rather than attempting best-effort support.
- Note-on/note-off pairing is done independently per track (MIDI convention: a note-on and its matching note-off always live in the same track), matching each note-on to the earliest following unmatched note-off with the same channel and note number. An unmatched note-on at the end of a track (no corresponding note-off) is treated as a malformed file and fails cleanly, rather than being inferred as ending at track end.
- **Provenance.** This is likely the first real, populated (non-test) use of `ProvenanceReference` (`core`, L1) in this codebase. No `ProvenanceActivity`/`ProvenanceAgent` type exists yet anywhere in the codebase -- that is legitimate future work, scoped separately, not a silent addition here. Instead, `MIDIImporter` constructs one stable `EntityID`, `"midi-importer-v1"`, and every `NoteEvent` it produces points its `ProvenanceReference` at that same id.
- Pitch conversion goes through the existing `PitchNumericValue` → `PitchIdentity` path (`pitch_numeric_value_create` + `pitch_numeric_value_to_identity`), under one importer-local `TuningSystem` labeled `"MIDI note number"` -- a plain linear space where a `PitchNumericValue` position of N means "MIDI note number N," with no 12-TET or A440 assumption baked in beyond what the MIDI note number itself already is. No parallel pitch-conversion path was introduced.
- **Numeric pitch output.** `midi_importer_import` also returns the `PitchNumericValue` it computed for each `NoteEvent`, via a second output array (`out_pitch_values`) index-paired with the `NoteEvent` array (same count, same order). This exists because `PitchIdentity`'s conversion is intentionally one-directional (see `pitch/pitch_numeric_value.h`) -- there is no way to recover a numeric pitch from a `NoteEvent`'s opaque `PitchIdentity` after the fact, so a caller needing numeric pitch data (e.g. for chord/key analysis) has no source for it other than this importer exposing the value it already computed. The caller owns the returned array and every `PitchNumericValue` in it (destroy each with `pitch_numeric_value_destroy`, then free the array). This does not weaken or bypass `PitchIdentity`'s opacity guarantee in any way -- it is a plain by-product of this importer's own MIDI-note-number tuning system, exposed only for the `NoteEvent`s this importer itself produced.

## Deferred

Deliberate scope cuts for `MIDIImporter`, not gaps to be filled incidentally:

- Tempo map / tempo change meta-events are not read. See the fixed-tempo assumption above.
- Meter/time signature changes are not read.
- Controller (CC), pedal, program change, pitch bend, and aftertouch events are ignored, not errored on.
- Per-track `Voice` assignment is not attempted. Multiple tracks (SMF type 1) are flattened into one time-ordered sequence instead.
- Malformed/unusual header recovery is not attempted. Anything that is not a well-formed SMF type 0 or type 1 file fails cleanly with a defined error, rather than being best-effort recovered.
- SMF type 2 files are rejected as unsupported, not parsed.

Out of scope entirely for this pass, not missing: renderers, other importer formats, `DigitalArtifact`, and any other L9 Registry item besides `MIDIImporter`.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time` and `pitch` (L2), and `events` (L3) only.
