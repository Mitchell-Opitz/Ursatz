# io

Layer L9 (`music-io`). Path `src/io/`, tests `tests/io/`. Depends on `identity`, `core`, `time`, `pitch`, and `events` only. Must not depend on `containers` (L4) or anything above it, and must not depend on `theory`, `rules`, `transform`, `analysis`, `generation`, `frameworks/`, or `music-analysis`.

## Provides

`MIDIImporter`: parses a Standard MIDI File (SMF) type 0 or type 1 buffer into a flat, time-ordered sequence of canonical `NoteEvent`s (L3). This is intentionally the only thing in this module -- no renderer, no other importer format, and no `DigitalArtifact` type are scaffolded here, even though they are related Registry items for this layer.

## Notes (in progress)

This module is under active construction across several commits; each landing commit updates this section as scope is filled in.

## Deferred

Deliberate scope cuts for `MIDIImporter`, not gaps to be filled incidentally:

- Tempo map / tempo change meta-events are not read. A fixed default tempo is assumed for `MusicalTime`/`Duration` conversion (see the tempo assumption note once the importer lands).
- Meter/time signature changes are not read.
- Controller (CC), pedal, program change, pitch bend, and aftertouch events are ignored, not errored on.
- Per-track `Voice` assignment is not attempted. Multiple tracks (SMF type 1) are flattened into one time-ordered sequence instead.
- Malformed/unusual header recovery is not attempted. Anything that is not a well-formed SMF type 0 or type 1 file fails cleanly with a defined error, rather than being best-effort recovered.
- SMF type 2 files are rejected as unsupported, not parsed.

Out of scope entirely for this pass, not missing: renderers, other importer formats, `DigitalArtifact`, and any other L9 Registry item besides `MIDIImporter`.

## Dependencies

Depends on `identity` (L0), `core` (L1), `time` and `pitch` (L2), and `events` (L3) only.
