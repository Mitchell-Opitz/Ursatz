#ifndef MUSIC_IO_MIDI_IMPORTER_H
#define MUSIC_IO_MIDI_IMPORTER_H

#include <stddef.h>

#include "events/note_event.h"

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
 * Fails cleanly (no partial *out_events, no crash) on anything not a
 * well-formed SMF 0/1 file: bad/truncated header, SMF type 2, SMPTE-
 * format division, truncated track data, or malformed event bytes.
 */
MidiImportResult midi_importer_import(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                       size_t *out_count);

#endif
