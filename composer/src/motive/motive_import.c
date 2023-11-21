#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/midi_importer.h"
#include "io/midi_key_signature.h"
#include "io/musicxml_importer.h"
#include "pitch/pitch_numeric_value.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"

#include "motive/motive_internal.h"

/*
 * MIDI and MusicXML go through Ursatz's importers. A motive is one voice, so a file whose notes sound together, or
 * whose notes are in more than one MusicXML part, is refused as polyphonic: no voice is chosen for the user and none
 * is dropped.
 */

/* Fills the notes of motive from the importers' time-ordered events. part_index is NULL when the format has none. */
static MotiveResult notes_from_events(NoteEvent *const *events, PitchNumericValue *const *values, size_t count,
                                      const size_t *part_index, Motive *motive, MotiveError *error) {
    if (count == 0) {
        motive_set_error(error, "", "the file has no notes");
        return MOTIVE_ERR_EMPTY;
    }
    if (count > MOTIVE_MAX_NOTES) {
        motive_set_error(error, "", "the file has %zu notes; the most is %d", count, MOTIVE_MAX_NOTES);
        return MOTIVE_ERR_TOO_MANY_NOTES;
    }
    PlanTime end = motive_time(0, 1);
    for (size_t i = 0; i < count; i++) {
        char location[MOTIVE_ERROR_LOCATION_MAX];
        if (part_index && part_index[i] != part_index[0]) {
            snprintf(location, sizeof(location), "note %zu", i + 1);
            motive_set_error(error, location,
                             "the notes are in more than one part (parts %zu and %zu); a motive is one voice",
                             part_index[0] + 1, part_index[i] + 1);
            return MOTIVE_ERR_POLYPHONIC;
        }
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        const Duration *duration = note_event_duration(events[i]);
        if (pitch_numeric_value_position_denominator(values[i]) != 1) {
            snprintf(location, sizeof(location), "note %zu", i + 1);
            motive_set_error(error, location, "the pitch is not a whole MIDI note number");
            return MOTIVE_ERR_VALUE;
        }
        PlanNote note;
        note.midi_pitch = (int)pitch_numeric_value_position_numerator(values[i]);
        note.offset = motive_time(musical_time_numerator(position), musical_time_denominator(position));
        note.duration = motive_time(duration_numerator(duration), duration_denominator(duration));
        if (plan_time_compare(note.offset, end) < 0) {
            snprintf(location, sizeof(location), "at %ld/%lu (note %zu)", note.offset.num, note.offset.den, i + 1);
            motive_set_error(error, location,
                             "the note sounds while the note before it still sounds; a motive is one voice");
            return MOTIVE_ERR_POLYPHONIC;
        }
        end = motive_note_end(&note);
        motive->notes[i] = note;
    }
    motive->note_count = count;
    return MOTIVE_OK;
}

/* The meter of the first region of the map. */
static void meter_from_map(const MeterMap *map, MotiveDocument *document) {
    const TimeSignature *signature = meter_map_region_time_signature(meter_map_region(map, 0));
    document->has_meter = true;
    document->meter_declared = !meter_map_is_defaulted(map);
    document->meter.beats_per_bar = (int)time_signature_top_number(signature);
    document->meter.beat_unit = (int)time_signature_bottom_number(signature);
    document->meter.bars_per_hyperbar = 0;
}

static const char *midi_problem(MidiImportResult result) {
    switch (result) {
    case MIDI_IMPORT_ERR_NULL_BUFFER:
        return "no data";
    case MIDI_IMPORT_ERR_TRUNCATED:
        return "the file is cut short";
    case MIDI_IMPORT_ERR_BAD_HEADER:
        return "not a Standard MIDI File (no valid header)";
    case MIDI_IMPORT_ERR_UNSUPPORTED_FORMAT:
        return "only MIDI file types 0 and 1 are read";
    case MIDI_IMPORT_ERR_UNSUPPORTED_DIVISION:
        return "SMPTE time division is not read";
    case MIDI_IMPORT_ERR_BAD_TRACK:
        return "a track is malformed";
    default:
        return "out of memory";
    }
}

MotiveResult motive_read_midi(const unsigned char *data, size_t size, Motive *motive, MotiveDocument *document,
                              MotiveError *error) {
    NoteEvent **events = NULL;
    PitchNumericValue **values = NULL;
    size_t count = 0;
    MidiMeterImport meter;
    memset(&meter, 0, sizeof(meter));
    MidiImportResult imported = midi_importer_import_with_meter(data, size, &events, &values, &count, &meter);
    if (imported != MIDI_IMPORT_OK) {
        motive_set_error(error, "midi", "%s", midi_problem(imported));
        return imported == MIDI_IMPORT_ERR_ALLOC ? MOTIVE_ERR_ALLOC : MOTIVE_ERR_IMPORT;
    }
    MotiveResult result = notes_from_events(events, values, count, NULL, motive, error);
    if (result == MOTIVE_OK) {
        meter_from_map(meter.meter_map, document);
        MidiKeySignature signature;
        memset(&signature, 0, sizeof(signature));
        if (midi_key_signature_scan(data, size, &signature) == MIDI_KEY_SIGNATURE_OK && signature.found) {
            document->has_key = true;
            document->key = motive_key_from_signature(signature.sharps_flats,
                                                      signature.is_minor ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR);
        }
    }
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(values[i]);
    }
    free(events);
    free(values);
    midi_meter_import_destroy(&meter);
    return result;
}

/*
 * Ursatz's XML parser takes a first byte of 0xFE or 0xFF as the start of UTF-16 and crashes when nothing is left to
 * convert, so bytes that cannot be UTF-16 text are refused here first.
 */
static bool musicxml_bytes_are_safe(const char *data, size_t size) {
    const unsigned char *start = (const unsigned char *)data;
    size_t length = size;
    if (length >= 3 && start[0] == 0xEF && start[1] == 0xBB && start[2] == 0xBF) {
        start += 3;
        length -= 3;
    }
    while (length > 0 && isspace(start[length - 1])) {
        length--;
    }
    if (length == 0 || (start[0] != 0xFE && start[0] != 0xFF)) {
        return true;
    }
    bool mark = length >= 2 && ((start[0] == 0xFF && start[1] == 0xFE) || (start[0] == 0xFE && start[1] == 0xFF));
    return mark && length >= 4;
}

MotiveResult motive_read_musicxml(const char *data, size_t size, Motive *motive, MotiveDocument *document,
                                  MotiveError *error) {
    if (!musicxml_bytes_are_safe(data, size)) {
        motive_set_error(error, "musicxml", "malformed XML: not a UTF-16 document with a byte order mark");
        return MOTIVE_ERR_IMPORT;
    }
    MusicXmlImport imported;
    memset(&imported, 0, sizeof(imported));
    MusicXmlImportStatus status = musicxml_import_buffer(data, size, &imported);
    if (status != MUSICXML_IMPORT_OK) {
        motive_set_error(error, "musicxml", "%s", imported.error_message);
        musicxml_import_destroy(&imported);
        return status == MUSICXML_IMPORT_ERR_ALLOC ? MOTIVE_ERR_ALLOC : MOTIVE_ERR_IMPORT;
    }
    MotiveResult result =
        notes_from_events(imported.events, imported.pitch_values, imported.count, imported.part_index, motive, error);
    if (result == MOTIVE_OK) {
        meter_from_map(imported.meter_map, document);
        for (size_t i = 0; i < imported.key_signature_count; i++) {
            const MusicXmlKeySignatureEvent *key = &imported.key_signatures[i];
            if (key->mode_hint == MUSICXML_MODE_HINT_UNKNOWN) {
                continue;
            }
            document->has_key = true;
            document->key = motive_key_from_signature(
                key->sharps_flats, key->mode_hint == MUSICXML_MODE_HINT_MINOR ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR);
            break;
        }
    }
    musicxml_import_destroy(&imported);
    return result;
}
