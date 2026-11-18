#ifndef MUSIC_IO_MIDI_IMPORTER_H
#define MUSIC_IO_MIDI_IMPORTER_H

#include <stddef.h>
#include <stdint.h>

#include "model/events/note_event.h"
#include "io/midi_key_signature.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/tempo_map.h"

/*
 * MIDIImporter (L9, music-io): parses a Standard MIDI File (SMF) type 0
 * or type 1 buffer into a flat, time-ordered sequence of canonical
 * NoteEvents (L3). See docs/modules/io.md for the exact scope cut: this
 * is a deliberately minimal, explicitly partial importer, not the full
 * L9 spec.
 *
 * NOTE IDS. A note's EntityID is "midi-note-<n>", n the note's index after the notes are sorted into time order. It is
 * positional: a MIDI file holds no note identity, so the id is not stable across edits of the file (a note added early
 * renumbers the notes after it). Code that needs ids to survive an edit reads a MusicXML export, which keeps the xml:id
 * the export wrote (io/musicxml_importer.h).
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
 * Meter. Part of the complete import below; the importer reads no meter on its own.
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
    MusicalTime end_time;
    uint64_t *conflict_ticks; /* owned; NULL when conflict_count is 0 */
    size_t conflict_count;
} MidiMeterImport;

/* Destroys the MeterMap and the conflict list. NULL is a no-op. */
void midi_meter_import_destroy(MidiMeterImport *meter);

/*
 * Tempo. Part of the complete import below. Note times stay tempo-independent: positions and
 * durations are whole-note fractions from ticks and the header's division alone, and reading the
 * tempo does not change them. The TempoMap is a separate answer to "how many seconds is that?", not
 * an input to the notes.
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

/* Destroys the TempoMap and the conflict list. NULL is a no-op. */
void midi_tempo_import_destroy(MidiTempoImport *tempo);

/*
 * Complete import: one call that parses the file once and returns everything the importer reads --
 * notes, meter, tempo, key signatures, and the tracks with their names and program changes. The file's
 * header is parsed once and every track is walked once. smf_reader.h and midi_event_decoder.h stay
 * implementation details of this module.
 *
 * events and pitch_values are index-paired and count long, in time order (ties broken by original event
 * order). Caller owns every NoteEvent (note_event_destroy) and every PitchNumericValue
 * (pitch_numeric_value_destroy) and both arrays; midi_import_destroy does it all. pitch_values[i] is the
 * PitchNumericValue the importer computed for events[i]'s pitch before converting it into the opaque
 * PitchIdentity stored on the NoteEvent (the conversion is one-directional). note_tracks[i] and
 * note_channels[i] are index-paired with events[i]: the index of the MTrk chunk the note came from and
 * its MIDI channel (0-15). Both are NULL when count is 0.
 *
 * meter is the file's time signatures and tempo its set-tempo events. key_signatures is every key
 * signature meta-event, in time order. format (0 or 1) and division (ticks per quarter note) are the
 * file header's.
 *
 * tracks has one entry per MTrk chunk, in file order (track_count == the header's chunk count, NULL
 * when 0). name is the text of the first track name meta-event (0xFF 0x03) in the chunk, NULL when
 * there is none. programs are the chunk's program changes (status 0xC0) in byte order, each with its
 * channel, program number (0-127), tick and the exact MusicalTime of that tick (tick / (4 * division),
 * as note positions are).
 *
 * Fails cleanly (no partial output, nothing leaks) on anything not a well-formed SMF 0/1 file: bad or
 * truncated header, SMF type 2, SMPTE-format division or a division of 0, truncated track data, or
 * malformed event bytes. A file whose tempo times do not fit a TempoMap fails with MIDI_IMPORT_ERR_ALLOC.
 * On any failure *out is untouched.
 */
typedef struct {
    uint64_t tick;
    MusicalTime time;
    uint8_t channel; /* 0-15 */
    uint8_t program; /* 0-127 */
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
