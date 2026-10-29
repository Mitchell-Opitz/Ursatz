#ifndef MUSIC_IO_MIDI_IMPORTER_H
#define MUSIC_IO_MIDI_IMPORTER_H

#include <stddef.h>
#include <stdint.h>

#include "events/note_event.h"
#include "io/midi_key_signature.h"
#include "pitch/pitch_numeric_value.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/tempo_map.h"

/*
 * MIDIImporter (L9, music-io): parses a Standard MIDI File (SMF) type 0
 * or type 1 buffer into a flat, time-ordered sequence of canonical
 * NoteEvents (L3). See docs/modules/io.md for the exact scope cut: this
 * is a deliberately minimal, explicitly partial importer, not the full
 * L9 spec.
 */

typedef enum {
    MIDI_IMPORT_OK = 0,
    MIDI_IMPORT_ERR_NULL_BUFFER,
    MIDI_IMPORT_ERR_TRUNCATED,
    MIDI_IMPORT_ERR_BAD_HEADER,
    MIDI_IMPORT_ERR_UNSUPPORTED_FORMAT,
    MIDI_IMPORT_ERR_UNSUPPORTED_DIVISION,
    MIDI_IMPORT_ERR_BAD_TRACK,
    MIDI_IMPORT_ERR_ALLOC
} MidiImportResult;

/*
 * Parses data[0..size) as a well-formed SMF type 0 or type 1 file and
 * writes a newly allocated array of NoteEvent* to *out_events, with
 * *out_count entries, in time order (ties broken by original event
 * order). Caller owns the array and every NoteEvent in it (destroy each
 * with note_event_destroy, then free the array).
 *
 * *out_pitch_values receives a second newly allocated array, also
 * *out_count entries, index-paired with *out_events: out_pitch_values[i]
 * is the PitchNumericValue this importer computed for out_events[i]'s
 * pitch before converting it into the opaque PitchIdentity stored on the
 * NoteEvent (the conversion is one-directional and does not support
 * recovering this value from the NoteEvent afterward). Caller owns the
 * array and every PitchNumericValue in it (destroy each with
 * pitch_numeric_value_destroy, then free the array). This does not
 * change PitchIdentity's own guarantees; it only exposes a value this
 * importer already had on hand.
 *
 * Fails cleanly (no partial *out_events, no partial *out_pitch_values,
 * no crash) on anything not a well-formed SMF 0/1 file: bad/truncated
 * header, SMF type 2, SMPTE-format division, truncated track data, or
 * malformed event bytes.
 */
MidiImportResult midi_importer_import(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                       PitchNumericValue ***out_pitch_values, size_t *out_count);

/*
 * Meter (additive). midi_importer_import above is unchanged: it still returns only notes and still
 * ignores time signature meta-events, so existing callers (Ursatz-Analyzer, Ursatz-GUI) see no
 * difference. The meter is read by a new function, midi_importer_import_with_meter, rather than
 * by a field on the existing result: MidiImportResult is a bare status enum with no result struct
 * to extend, and changing midi_importer_import's signature would break its callers. The new
 * function takes the same arguments plus a MidiMeterImport, and returns the same notes.
 *
 * meter_map: the file's time signature meta-events (0xFF 0x58, every track) as a MeterMap. A file
 * with no such event gets a single 4/4 region marked defaulted. See time/meter_map.h for measure
 * numbering, the pickup rule and barline ownership.
 *
 * end_time: the time of the file's last note-off, in whole notes (tick / (4 * division)), 0 if the
 * file has no notes. Pass it to meter_map_measure_count for the number of measures the notes
 * reach.
 *
 * conflict_ticks: ticks where different time signatures shared a tick. The last in track order was
 * kept (see midi_event_decoder.h); the importer reports the tick and leaves printing to the
 * caller.
 */
typedef struct {
    MeterMap *meter_map;      /* owned */
    MusicalTime *end_time;    /* owned */
    uint64_t *conflict_ticks; /* owned; NULL when conflict_count is 0 */
    size_t conflict_count;
} MidiMeterImport;

/*
 * As midi_importer_import, and also fills *out_meter (destroy with midi_meter_import_destroy). On
 * any failure no output is written: *out_events, *out_pitch_values and *out_meter are untouched.
 */
MidiImportResult midi_importer_import_with_meter(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                                  PitchNumericValue ***out_pitch_values, size_t *out_count,
                                                  MidiMeterImport *out_meter);

/* Destroys the MeterMap, the end time and the conflict list. NULL is a no-op. */
void midi_meter_import_destroy(MidiMeterImport *meter);

/*
 * Tempo (additive, the same pattern as the meter above). midi_importer_import and
 * midi_importer_import_with_meter are unchanged. Note times stay tempo-independent: positions and
 * durations are whole-note fractions from ticks and the header's division alone, exactly as before,
 * and reading the tempo does not change them. The TempoMap is a separate answer to "how many
 * seconds is that?", not an input to the notes.
 *
 * tempo_map: the file's set-tempo meta-events (0xFF 0x51, every track) as a TempoMap, one region
 * per distinct tick. A tempo at tick 0 is the first region; with none at tick 0 the default of
 * 500,000 microseconds per quarter note (120 quarter notes per minute) applies from 0 as a
 * defaulted region, and a file with no set-tempo event is one defaulted region. See time/tempo_map.h
 * for the exact seconds mapping.
 *
 * conflict_ticks: ticks where different tempos shared a tick. The last in track order was kept (see
 * midi_event_decoder.h); the importer reports the tick and leaves printing to the caller.
 */
typedef struct {
    TempoMap *tempo_map;      /* owned */
    uint64_t *conflict_ticks; /* owned; NULL when conflict_count is 0 */
    size_t conflict_count;
} MidiTempoImport;

/*
 * As midi_importer_import, and also fills *out_tempo (destroy with midi_tempo_import_destroy). On
 * any failure no output is written: *out_events, *out_pitch_values and *out_tempo are untouched.
 * A file whose tempo times do not fit a TempoMap (the seconds would overflow) fails with
 * MIDI_IMPORT_ERR_ALLOC.
 */
MidiImportResult midi_importer_import_with_tempo(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                                  PitchNumericValue ***out_pitch_values, size_t *out_count,
                                                  MidiTempoImport *out_tempo);

/* Destroys the TempoMap and the conflict list. NULL is a no-op. */
void midi_tempo_import_destroy(MidiTempoImport *tempo);

/*
 * Complete import: one call that parses the file once and returns everything the importer reads --
 * notes, meter, tempo, key signatures, and the tracks with their names and program changes. The file's
 * header is parsed once and every track is walked once. smf_reader.h and midi_event_decoder.h stay
 * implementation details of this module.
 *
 * events, pitch_values and count are exactly what midi_importer_import returns (same order, same
 * ownership rules). note_tracks[i] and note_channels[i] are index-paired with events[i]: the index of
 * the MTrk chunk the note came from and its MIDI channel (0-15). Both are NULL when count is 0.
 *
 * meter and tempo are exactly what midi_importer_import_with_meter and midi_importer_import_with_tempo
 * return. key_signatures is exactly what midi_key_signature_scan_all returns for the same buffer
 * (every key signature, in time order). format (0 or 1) and division (ticks per quarter note) are the
 * file header's.
 *
 * tracks has one entry per MTrk chunk, in file order (track_count == the header's chunk count, NULL
 * when 0). name is the text of the first track name meta-event (0xFF 0x03) in the chunk, NULL when
 * there is none. programs are the chunk's program changes (status 0xC0) in byte order, each with its
 * channel, program number (0-127), tick and the exact MusicalTime of that tick (tick / (4 * division),
 * as note positions are).
 *
 * Fails as midi_importer_import does, and also with MIDI_IMPORT_ERR_UNSUPPORTED_DIVISION for a division
 * of 0 (no position can be formed). On any failure *out is untouched and nothing leaks. Because every
 * part is built, a file whose tempo times do not fit a TempoMap (MIDI_IMPORT_ERR_ALLOC, see
 * midi_importer_import_with_tempo) fails here.
 */
typedef struct {
    uint64_t tick;
    MusicalTime *time; /* owned */
    uint8_t channel;   /* 0-15 */
    uint8_t program;   /* 0-127 */
} MidiProgramChange;

typedef struct {
    size_t index;
    char *name; /* owned; NULL when absent */
    size_t program_count;
    MidiProgramChange *programs; /* owned; NULL when program_count is 0 */
} MidiTrackInfo;

typedef struct {
    NoteEvent **events; /* owned, with every note in it */
    PitchNumericValue **pitch_values; /* owned, index-paired with events */
    size_t count;
    size_t *note_tracks;    /* owned; the track index of each note */
    uint8_t *note_channels; /* owned; the channel of each note */
    MidiMeterImport meter;
    MidiTempoImport tempo;
    MidiKeySignatureEvents key_signatures;
    MidiTrackInfo *tracks; /* owned */
    size_t track_count;
    uint16_t format;
    uint16_t division;
} MidiImport;

MidiImportResult midi_importer_import_complete(const unsigned char *data, size_t size, MidiImport *out);

/* Destroys everything the import owns and zeroes it. NULL is a no-op; a zeroed struct is safe. */
void midi_import_destroy(MidiImport *import);

#endif
