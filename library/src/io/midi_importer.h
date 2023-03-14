#ifndef MUSIC_IO_MIDI_IMPORTER_H
#define MUSIC_IO_MIDI_IMPORTER_H

#include <stddef.h>
#include <stdint.h>

#include "events/note_event.h"
#include "pitch/pitch_numeric_value.h"
#include "time/meter_map.h"
#include "time/musical_time.h"

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

#endif
