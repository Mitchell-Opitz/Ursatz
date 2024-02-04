#include <ursatz_analyzer/musicxml_pipeline.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/musicxml_importer.h"

static bool is_compressed(FILE *file) {
    unsigned char head[4];
    size_t count = fread(head, 1, sizeof head, file);
    rewind(file);
    return count == sizeof head && memcmp(head, "PK\x03\x04", 4) == 0;
}

/* The end of the last note, in whole notes: what the meter's measure count is taken to. NULL on failure. */
static MusicalTime *last_note_end(NoteEvent *const *events, size_t count) {
    MusicalTime *latest = NULL;
    for (size_t i = 0; i < count; i++) {
        MusicalTime *end = NULL;
        if (musical_time_add(temporal_anchor_position(note_event_temporal_anchor(events[i])),
                             note_event_duration(events[i]), &end) != MUSICAL_TIME_OK) {
            musical_time_destroy(latest);
            return NULL;
        }
        if (!latest || musical_time_numerator(end) * (long)musical_time_denominator(latest) >
                           musical_time_numerator(latest) * (long)musical_time_denominator(end)) {
            musical_time_destroy(latest);
            latest = end;
        } else {
            musical_time_destroy(end);
        }
    }
    if (!latest && musical_time_create(0, 1, &latest) != MUSICAL_TIME_OK) {
        return NULL;
    }
    return latest;
}

static MidiModeHint mode_hint_of(MusicXmlModeHint hint) {
    switch (hint) {
        case MUSICXML_MODE_HINT_MAJOR:
            return MIDI_MODE_HINT_MAJOR;
        case MUSICXML_MODE_HINT_MINOR:
            return MIDI_MODE_HINT_MINOR;
        default:
            return MIDI_MODE_HINT_UNKNOWN;
    }
}

/* Moves the importer's key signature events into the shape the pipeline reads; false on allocation failure. */
static bool take_key_signatures(MusicXmlImport *import, MidiKeySignatureEvents *events, MidiKeySignature *first) {
    *events = (MidiKeySignatureEvents){NULL, 0};
    *first = (MidiKeySignature){false, 0, false};
    if (import->key_signature_count == 0) {
        return true;
    }
    MidiKeySignatureEvent *array = calloc(import->key_signature_count, sizeof(MidiKeySignatureEvent));
    if (!array) {
        return false;
    }
    for (size_t i = 0; i < import->key_signature_count; i++) {
        MusicXmlKeySignatureEvent *source = &import->key_signatures[i];
        array[i].key_signature = source->key_signature;
        array[i].mode_hint = mode_hint_of(source->mode_hint);
        array[i].position = source->position;
        array[i].track_index = source->part_index;
        array[i].sharps_flats = source->sharps_flats;
        array[i].mode_byte = source->mode_hint == MUSICXML_MODE_HINT_MINOR ? 1 : 0;
    }
    first->found = true;
    first->sharps_flats = array[0].sharps_flats;
    first->is_minor = array[0].mode_hint == MIDI_MODE_HINT_MINOR;
    events->events = array;
    events->count = import->key_signature_count;
    import->key_signature_count = 0; /* the events are now owned by the pipeline's array */
    return true;
}

static bool copy_parts(const MusicXmlImport *import, ImportedSource *source) {
    if (import->part_count == 0) {
        return true;
    }
    source->parts = calloc(import->part_count, sizeof(ImportedPart));
    if (!source->parts) {
        return false;
    }
    for (size_t i = 0; i < import->part_count; i++) {
        source->parts[i].id = strdup(import->parts[i].id);
        source->parts[i].name = strdup(import->parts[i].name);
        source->parts[i].staff_count = import->parts[i].staff_count;
        source->part_count = i + 1;
        if (!source->parts[i].id || !source->parts[i].name) {
            return false;
        }
    }
    return true;
}

static const char *status_description(MusicXmlImportStatus status) {
    switch (status) {
        case MUSICXML_IMPORT_ERR_IO:
            return "cannot read the file";
        case MUSICXML_IMPORT_ERR_TOO_LARGE:
            return "the file is too large";
        case MUSICXML_IMPORT_ERR_MALFORMED_XML:
            return "the file is not well-formed XML";
        case MUSICXML_IMPORT_ERR_WRONG_ROOT:
            return "the file is not a score-partwise MusicXML document";
        case MUSICXML_IMPORT_ERR_ALLOC:
            return "out of memory";
        default:
            return "the score cannot be read";
    }
}

MusicXmlPipelineResult musicxml_pipeline_import_file(const char *path, ImportedNotes *out) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Error: cannot read file '%s'\n", path);
        return MUSICXML_PIPELINE_ERR_CANNOT_READ_FILE;
    }
    bool compressed = is_compressed(file);
    fclose(file);
    if (compressed) {
        fprintf(stderr,
                "Error: failed to import '%s': compressed MusicXML (.mxl) is not supported by the Ursatz importer; "
                "export the score as uncompressed .musicxml\n",
                path);
        return MUSICXML_PIPELINE_ERR_COMPRESSED;
    }

    MusicXmlImport import;
    memset(&import, 0, sizeof(import));
    MusicXmlImportStatus status = musicxml_import_file(path, &import);
    if (status != MUSICXML_IMPORT_OK) {
        fprintf(stderr, "Error: failed to import '%s': %s (%s)\n", path, status_description(status),
                import.error_message);
        musicxml_import_destroy(&import);
        return status == MUSICXML_IMPORT_ERR_IO ? MUSICXML_PIPELINE_ERR_CANNOT_READ_FILE
                                                : MUSICXML_PIPELINE_ERR_IMPORT_FAILED;
    }
    if (import.count == 0) {
        fprintf(stderr, "Error: failed to import '%s': the score contains no notes\n", path);
        musicxml_import_destroy(&import);
        return MUSICXML_PIPELINE_ERR_EMPTY_SCORE;
    }
    for (size_t i = 0; i < import.warnings.message_count; i++) {
        fprintf(stderr, "Warning: '%s': %s\n", path, import.warnings.messages[i]);
    }
    if (import.warnings.count > import.warnings.message_count) {
        fprintf(stderr, "Warning: '%s': and %zu more\n", path, import.warnings.count - import.warnings.message_count);
    }

    ImportedNotes notes;
    memset(&notes, 0, sizeof(notes));
    notes.source.format = INPUT_FORMAT_MUSICXML;
    notes.source.meter_notated = true;
    notes.source.key_notated = true;
    notes.source.spelling_notated = true;
    notes.source.tempo_marking_count = import.tempo_marking_count;
    notes.source.dynamics_count = import.dynamics_count;
    notes.source.warning_count = import.warnings.count;
    if (import.tempo_map && !tempo_map_is_defaulted(import.tempo_map) && tempo_map_region_count(import.tempo_map) > 0) {
        unsigned long microseconds = tempo_map_region_microseconds_per_quarter(tempo_map_region(import.tempo_map, 0));
        if (microseconds > 0) {
            notes.source.tempo_declared = true;
            notes.source.initial_quarters_per_minute = 60000000.0 / (double)microseconds;
        }
    }

    MusicalTime *end_time = last_note_end(import.events, import.count);
    bool ok = end_time && take_key_signatures(&import, &notes.key_signature_events, &notes.key_signature) &&
              copy_parts(&import, &notes.source);
    if (!ok) {
        musical_time_destroy(end_time);
        midi_key_signature_events_destroy(&notes.key_signature_events);
        imported_notes_destroy(&notes);
        musicxml_import_destroy(&import);
        fprintf(stderr, "Error: out of memory importing '%s'\n", path);
        return MUSICXML_PIPELINE_ERR_IMPORT_FAILED;
    }

    notes.events = import.events;
    notes.pitch_values = import.pitch_values;
    notes.count = import.count;
    notes.source.part_index = import.part_index;
    notes.source.staff = import.staff;
    notes.meter = (MidiMeterImport){import.meter_map, end_time, NULL, 0};
    import.events = NULL;
    import.pitch_values = NULL;
    import.count = 0;
    import.part_index = NULL;
    import.staff = NULL;
    import.meter_map = NULL;
    musicxml_import_destroy(&import);

    *out = notes;
    return MUSICXML_PIPELINE_OK;
}
