#include <ursatz_analyzer/midi_pipeline.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/midi_importer.h"
#include "io/midi_key_signature.h"
#include "export/score_staves.h"
#include "pipeline/gm_names.h"

static const char *midi_import_error_message(MidiImportResult result) {
    switch (result) {
        case MIDI_IMPORT_ERR_NULL_BUFFER:
            return "internal error: null buffer passed to importer";
        case MIDI_IMPORT_ERR_TRUNCATED:
            return "MIDI file is truncated";
        case MIDI_IMPORT_ERR_BAD_HEADER:
            return "MIDI file has a malformed header";
        case MIDI_IMPORT_ERR_UNSUPPORTED_FORMAT:
            return "MIDI file is not SMF type 0 or type 1";
        case MIDI_IMPORT_ERR_UNSUPPORTED_DIVISION:
            return "MIDI file uses an unsupported (SMPTE) time division";
        case MIDI_IMPORT_ERR_BAD_TRACK:
            return "MIDI file contains a malformed track (bad event bytes, unmatched note-on, or unrecognized "
                   "status byte)";
        case MIDI_IMPORT_ERR_ALLOC:
            return "out of memory while importing MIDI file";
        default:
            return "unknown MIDI import error";
    }
}

/* A part of a MIDI file: the notes of one channel of one track. */
typedef struct {
    size_t track;
    int channel;
    long long first_tick;
    size_t first_event;
} PartKey;

#define PERCUSSION_CHANNEL 9

/* The tick of a note's onset: the position in whole notes times four quarters times the file's division. */
static long long onset_tick(const NoteEvent *event, unsigned division) {
    const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(event));
    long long numerator = (long long)musical_time_numerator(onset) * 4 * (long long)division;
    long long denominator = (long long)musical_time_denominator(onset);
    return denominator > 0 ? numerator / denominator : 0;
}

/* The program a part starts with: the last program change of its track and channel at or before its first note, 0
   (the General MIDI default) when there is none, -1 for the percussion channel. */
static int part_program(const MidiImport *import, const PartKey *key) {
    if (key->channel == PERCUSSION_CHANNEL) {
        return -1;
    }
    int program = 0;
    if (key->track < import->track_count) {
        const MidiTrackInfo *track = &import->tracks[key->track];
        for (size_t i = 0; i < track->program_count; i++) {
            const MidiProgramChange *change = &track->programs[i];
            if (change->channel == key->channel && (long long)change->tick <= key->first_tick) {
                program = change->program;
            }
        }
    }
    return program;
}

/* The part name: the track name when the track has one and only one part, else the track name and the instrument
   name, else the instrument name. NULL on allocation failure. */
static char *part_name(const MidiImport *import, const PartKey *keys, size_t key_count, size_t part, int program) {
    const char *track_name = keys[part].track < import->track_count ? import->tracks[keys[part].track].name : NULL;
    if (track_name && track_name[0] == '\0') {
        track_name = NULL;
    }
    const char *instrument = keys[part].channel == PERCUSSION_CHANNEL ? "Percussion" : gm_instrument_name(program);
    size_t track_parts = 0;
    for (size_t i = 0; i < key_count; i++) {
        track_parts += keys[i].track == keys[part].track;
    }
    if (track_name && track_parts == 1) {
        return strdup(track_name);
    }
    if (!track_name) {
        return strdup(instrument ? instrument : "");
    }
    size_t size = strlen(track_name) + 3 + strlen(instrument ? instrument : "") + 1;
    char *name = malloc(size);
    if (name) {
        snprintf(name, size, "%s - %s", track_name, instrument ? instrument : "");
    }
    return name;
}

/* Parts in order of first appearance in the file: by track, then by the tick of the first note. */
static int part_key_compare(const void *a, const void *b) {
    const PartKey *x = a;
    const PartKey *y = b;
    if (x->track != y->track) {
        return x->track < y->track ? -1 : 1;
    }
    if (x->first_tick != y->first_tick) {
        return x->first_tick < y->first_tick ? -1 : 1;
    }
    return x->first_event < y->first_event ? -1 : (x->first_event > y->first_event ? 1 : 0);
}

static void parts_destroy(ImportedPart *parts, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(parts[i].id);
        free(parts[i].name);
    }
    free(parts);
}

/* One part per distinct (track, channel) that has a note, in order of first appearance: its id, name and program.
   Writes the part of each note to streams (import->count entries). false on allocation failure, with source
   untouched. */
static bool build_parts(const MidiImport *import, ImportedSource *source, size_t *streams) {
    size_t count = import->count;
    PartKey *keys = calloc(count > 0 ? count : 1, sizeof *keys);
    ImportedPart *parts = calloc(count > 0 ? count : 1, sizeof *parts);
    size_t key_count = 0;
    bool ok = keys && parts;
    for (size_t i = 0; ok && i < count; i++) {
        size_t track = import->note_tracks[i];
        int channel = import->note_channels[i];
        long long tick = onset_tick(import->events[i], import->division);
        size_t part = 0;
        while (part < key_count && !(keys[part].track == track && keys[part].channel == channel)) {
            part++;
        }
        if (part == key_count) {
            keys[key_count++] = (PartKey){track, channel, tick, i};
        } else if (tick < keys[part].first_tick) {
            keys[part].first_tick = tick;
        }
    }
    if (ok) {
        qsort(keys, key_count, sizeof *keys, part_key_compare);
        for (size_t i = 0; i < count; i++) {
            size_t part = 0;
            while (keys[part].track != import->note_tracks[i] || keys[part].channel != import->note_channels[i]) {
                part++;
            }
            streams[i] = part;
        }
    }
    for (size_t part = 0; ok && part < key_count; part++) {
        int program = part_program(import, &keys[part]);
        char id[32];
        snprintf(id, sizeof id, "P%zu", part + 1);
        parts[part].id = strdup(id);
        parts[part].name = part_name(import, keys, key_count, part, program);
        parts[part].staff_count = score_program_uses_grand_staff(program) ? 2 : 1;
        parts[part].midi_program = program;
        ok = parts[part].id && parts[part].name;
    }
    if (ok) {
        source->parts = parts;
        source->part_count = key_count;
    } else {
        parts_destroy(parts, key_count);
    }
    free(keys);
    return ok;
}

/* The part (track and channel) each imported note was read from, as ImportedNotes.streams and source.part_index, with
   source.parts and the staff of each note. NULL on failure. */
static size_t *note_parts(const MidiImport *import, ImportedSource *source) {
    size_t count = import->count;
    size_t *streams = calloc(count > 0 ? count : 1, sizeof *streams);
    if (!streams || !build_parts(import, source, streams)) {
        free(streams);
        return NULL;
    }
    if (source->part_count == 0) {
        return streams;
    }
    size_t *part_index = calloc(count > 0 ? count : 1, sizeof *part_index);
    int *part_staff_count = malloc(source->part_count * sizeof *part_staff_count);
    int *staff = malloc((count > 0 ? count : 1) * sizeof *staff);
    for (size_t i = 0; part_staff_count && i < source->part_count; i++) {
        part_staff_count[i] = source->parts[i].staff_count;
    }
    if (part_index) {
        memcpy(part_index, streams, count * sizeof *part_index);
    }
    if (!part_index || !part_staff_count || !staff ||
        score_staves_assign((NoteEvent *const *)import->events, import->pitch_values, count, part_index,
                            part_staff_count, source->part_count, staff) != SCORE_STAVES_OK) {
        free(part_staff_count);
        free(staff);
        free(part_index);
        parts_destroy(source->parts, source->part_count);
        *source = (ImportedSource){.format = INPUT_FORMAT_MIDI};
        free(streams);
        return NULL;
    }
    free(part_staff_count);
    source->part_index = part_index;
    source->staff = staff;
    return streams;
}

/* The opening tempo of the file, when its first set-tempo event is at tick 0 (a defaulted region is not a declared
   tempo). */
static void read_opening_tempo(const TempoMap *tempo_map, ImportedSource *source) {
    if (tempo_map && !tempo_map_is_defaulted(tempo_map) && tempo_map_region_count(tempo_map) > 0) {
        const TempoMapRegion *region = tempo_map_region(tempo_map, 0);
        unsigned long microseconds = tempo_map_region_microseconds_per_quarter(region);
        if (microseconds > 0 && !tempo_map_region_is_defaulted(region) &&
            musical_time_numerator(tempo_map_region_start(region)) == 0) {
            source->tempo_declared = true;
            source->initial_quarters_per_minute = 60000000.0 / (double)microseconds;
        }
    }
}

/* The first key signature of the file in track then byte order, as midi_key_signature_scan reports it. */
static MidiKeySignature first_key_signature(const MidiKeySignatureEvents *events) {
    MidiKeySignature first = {false, 0, false};
    const MidiKeySignatureEvent *best = NULL;
    for (size_t i = 0; i < events->count; i++) {
        const MidiKeySignatureEvent *event = &events->events[i];
        if (!best || event->track_index < best->track_index ||
            (event->track_index == best->track_index && event->tick < best->tick)) {
            best = event;
        }
    }
    if (best) {
        first = (MidiKeySignature){true, best->sharps_flats, best->mode_byte == 1};
    }
    return first;
}

MidiPipelineResult midi_pipeline_import_file(const char *path, ImportedNotes *out) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Error: cannot read file '%s'\n", path);
        return MIDI_PIPELINE_ERR_CANNOT_READ_FILE;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "Error: cannot read file '%s'\n", path);
        return MIDI_PIPELINE_ERR_CANNOT_READ_FILE;
    }
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        fprintf(stderr, "Error: cannot read file '%s'\n", path);
        return MIDI_PIPELINE_ERR_CANNOT_READ_FILE;
    }
    rewind(file);

    unsigned char *buffer = malloc((size_t)size > 0 ? (size_t)size : 1);
    if (!buffer) {
        fclose(file);
        fprintf(stderr, "Error: out of memory reading '%s'\n", path);
        return MIDI_PIPELINE_ERR_CANNOT_READ_FILE;
    }

    size_t read_count = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    if (read_count != (size_t)size) {
        free(buffer);
        fprintf(stderr, "Error: cannot read file '%s'\n", path);
        return MIDI_PIPELINE_ERR_CANNOT_READ_FILE;
    }

    MidiImport import;
    MidiImportResult import_result = midi_importer_import_complete(buffer, (size_t)size, &import);
    free(buffer);
    if (import_result != MIDI_IMPORT_OK) {
        fprintf(stderr, "Error: failed to import '%s': %s\n", path, midi_import_error_message(import_result));
        return MIDI_PIPELINE_ERR_IMPORT_FAILED;
    }

    ImportedSource source = {.format = INPUT_FORMAT_MIDI};
    size_t *streams = note_parts(&import, &source);
    if (!streams) {
        midi_import_destroy(&import);
        fprintf(stderr, "Error: failed to import '%s': %s\n", path,
                midi_import_error_message(MIDI_IMPORT_ERR_ALLOC));
        return MIDI_PIPELINE_ERR_IMPORT_FAILED;
    }
    read_opening_tempo(import.tempo.tempo_map, &source);

    out->events = import.events;
    out->pitch_values = import.pitch_values;
    out->count = import.count;
    out->streams = streams;
    out->key_signature = first_key_signature(&import.key_signatures);
    out->key_signature_events = import.key_signatures;
    out->ticks_per_quarter = import.division;
    out->meter = import.meter;
    out->source = source;
    out->tempo_map = import.tempo.tempo_map;
    for (size_t i = 0; i < import.meter.conflict_count; i++) {
        fprintf(stderr, "Warning: '%s' declares different time signatures at tick %llu; the last one is used\n",
                path, (unsigned long long)import.meter.conflict_ticks[i]);
    }
    /* The notes, meter, key signatures and tempo map now belong to out. */
    import.events = NULL;
    import.pitch_values = NULL;
    import.count = 0;
    import.meter = (MidiMeterImport){NULL, NULL, NULL, 0};
    import.key_signatures = (MidiKeySignatureEvents){NULL, 0};
    import.tempo.tempo_map = NULL;
    midi_import_destroy(&import);
    return MIDI_PIPELINE_OK;
}
