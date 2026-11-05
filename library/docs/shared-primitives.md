# Shared primitives

The small public helpers that every layer above reuses instead of writing its own copy. [Docs index](README.md)

Small public helpers that every layer above reuses instead of writing its own copy.

- `core/string_copy.h`: `ursatz_string_copy`.
- `core/rational.h`: `Rational`, a signed exact rational on 64-bit integers, always in lowest terms. Every operation is checked and reports `RATIONAL_ERR_OVERFLOW`; nothing wraps. `time/time_rational.h` bridges it to `MusicalTime`, `Duration` and `PerformanceTime`.
- `time/meter_arithmetic.h`: exact measure and beat functions over a `MeterMap` (measure of a time, measure bounds, beat length and count, beat of a time, snap to beat, on beat), on `Rational`. The standard-form, standard-meter and standard-texture time helpers wrap it. `time_span_is_valid` in `time/time_span.h` is the shared span check.
- `pitch/pitch_arithmetic.h`: letter semitone and index, letter offset, pitch class, MIDI number, line-of-fifths position, spelling names and parsing. Theory-neutral; every framework's letter tables sit on it. Key, mode and degree functions are `frameworks/standard-tonality/tonal_key.h`.
- `io/midi_importer.h`: `midi_importer_import_complete` parses an SMF once and returns notes, meter, tempo, key signatures, and every track's name and program changes in one `MidiImport`. The three older import entry points are wrappers over it. `smf_reader.h` and `midi_event_decoder.h` are internal to `io`.
- `frameworks/standard-tonality/roman_numeral_grammar.h`: the Roman numeral grammar. `standard_tonality_numeral_parse` reads every numeral form (alteration, case, quality mark, figure, applied target), `standard_tonality_numeral_format` writes the canonical spelling, `standard_tonality_numeral_root_semitone` gives the root through an applied target. The older numeral entry points are wrappers on it. standard-harmony keeps its own label writer, checked by a test.
- `semantics/analysis_note.h`, `analysis_segment.h`, `analysis_key.h`, `voice_grid.h`: the neutral analysis input types (`AnalysisNote`, `AnalysisSegment`, `AnalysisKey`, `AnalysisVoiceGrid`) that framework adapters accept, so a caller no longer writes the converter. `semantics/` includes `interpretation/key.h` for `KeyStructureMode` (allowlisted in its architecture test), and `io/` may include `semantics/analysis_key.h`. `standard-counterpoint/slot_note.h`: the slot note and rule result the three counterpoint styles share. See `docs/modules/semantics.md`.
