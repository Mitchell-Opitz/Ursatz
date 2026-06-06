#include <ursatz_analyzer/midi_pipeline.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/midi_importer.h"
#include "io/midi_key_signature.h"
#include "io/smf_reader.h"

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

/* A note-on of the file with the track chunk and channel it is in. */
typedef struct {
    long long tick;
    int pitch;
    size_t track;
    int channel;
    bool used;
} TrackNoteOn;

/* A program change (0xC0) of the file. */
typedef struct {
    long long tick;
    size_t track;
    int channel;
    int program;
} ProgramChange;

/* What the track chunks say beyond their note-ons: the first track name (meta 0x03) of each track, NULL when it has
   none, and every program change in file order. */
typedef struct {
    char **track_names; /* owned; one entry per track */
    size_t track_count;
    ProgramChange *programs;
    size_t program_count;
    size_t program_capacity;
} MidiFileInfo;

static void midi_file_info_destroy(MidiFileInfo *info) {
    for (size_t t = 0; t < info->track_count; t++) {
        free(info->track_names[t]);
    }
    free(info->track_names);
    free(info->programs);
    *info = (MidiFileInfo){0};
}

/* Reads a variable-length quantity at data[at], within end; returns the bytes used, 0 when it runs past end. */
static size_t read_vlq(const unsigned char *data, size_t at, size_t end, unsigned long *value) {
    *value = 0;
    size_t used = 0;
    unsigned char c;
    do {
        if (at + used >= end || used >= 5) {
            return 0;
        }
        c = data[at + used];
        *value = (*value << 7) | (c & 0x7f);
        used++;
    } while ((c & 0x80) != 0);
    return used;
}

static bool add_program_change(MidiFileInfo *info, ProgramChange change) {
    if (info->program_count == info->program_capacity) {
        size_t capacity = info->program_capacity > 0 ? info->program_capacity * 2 : 16;
        ProgramChange *grown = realloc(info->programs, capacity * sizeof *grown);
        if (!grown) {
            return false;
        }
        info->programs = grown;
        info->program_capacity = capacity;
    }
    info->programs[info->program_count++] = change;
    return true;
}

/* The note-ons of every track chunk (running status honoured), ported from Ursatz's tests/integration/texture_util.h
   texture_read_note_ons as a rewrite with error returns, with each track's first name and every program change in
   info (empty on failure). false on a malformed track or allocation failure. */
static bool read_note_ons(const SmfTrack *tracks, size_t track_count, TrackNoteOn **out, size_t *out_count,
                          MidiFileInfo *info) {
    size_t capacity = 1024;
    size_t count = 0;
    *info = (MidiFileInfo){0};
    TrackNoteOn *ons = malloc(capacity * sizeof *ons);
    info->track_names = calloc(track_count > 0 ? track_count : 1, sizeof *info->track_names);
    if (!ons || !info->track_names) {
        free(ons);
        midi_file_info_destroy(info);
        return false;
    }
    info->track_count = track_count;
    for (size_t t = 0; t < track_count; t++) {
        const unsigned char *data = tracks[t].data;
        size_t end = tracks[t].size;
        size_t p = 0;
        long long tick = 0;
        unsigned char running = 0;
        while (p < end) {
            unsigned long delta = 0;
            size_t used = read_vlq(data, p, end, &delta);
            if (used == 0 || p + used >= end) {
                free(ons);
                midi_file_info_destroy(info);
                return false;
            }
            p += used;
            tick += (long long)delta;
            unsigned char status = data[p];
            if (status == 0xff || status == 0xf0 || status == 0xf7) {
                unsigned long length = 0;
                unsigned char meta_type = (status == 0xff && p + 1 < end) ? data[p + 1] : 0;
                p += status == 0xff ? 2 : 1;
                used = p < end ? read_vlq(data, p, end, &length) : 0;
                if (used == 0 || p + used + length > end) {
                    free(ons);
                    return false;
                }
                if (meta_type == 0x03 && !info->track_names[t]) {
                    info->track_names[t] = malloc(length + 1);
                    if (!info->track_names[t]) {
                        free(ons);
                        midi_file_info_destroy(info);
                        return false;
                    }
                    memcpy(info->track_names[t], data + p + used, length);
                    info->track_names[t][length] = '\0';
                }
                p += used + length;
                continue;
            }
            if ((status & 0x80) != 0) {
                running = status;
                p++;
            } else {
                status = running;
            }
            unsigned char kind = status & 0xf0;
            size_t data_bytes = (kind == 0xc0 || kind == 0xd0) ? 1 : 2;
            if (kind < 0x80 || p + data_bytes > end) {
                free(ons);
                midi_file_info_destroy(info);
                return false;
            }
            if (kind == 0x90 && data[p + 1] > 0) {
                if (count == capacity) {
                    capacity *= 2;
                    TrackNoteOn *grown = realloc(ons, capacity * sizeof *ons);
                    if (!grown) {
                        free(ons);
                        midi_file_info_destroy(info);
                        return false;
                    }
                    ons = grown;
                }
                ons[count++] = (TrackNoteOn){tick, data[p], t, status & 0x0f, false};
            } else if (kind == 0xc0 &&
                       !add_program_change(info, (ProgramChange){tick, t, status & 0x0f, data[p]})) {
                free(ons);
                midi_file_info_destroy(info);
                return false;
            }
            p += data_bytes;
        }
    }
    *out = ons;
    *out_count = count;
    return true;
}

/* The track chunk each imported note was read from, matched to the file's note-ons by tick and pitch. A note with no
   match (the importer and this reader disagree) is in track 0. NULL on failure. */
static size_t *note_streams(const unsigned char *buffer, size_t size, const NoteEvent *const *events,
                            PitchNumericValue *const *pitch_values, size_t count) {
    size_t *streams = calloc(count > 0 ? count : 1, sizeof *streams);
    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    if (!streams || smf_reader_parse(buffer, size, &header, &tracks, &track_count) != SMF_READER_OK) {
        free(streams);
        return NULL;
    }
    TrackNoteOn *ons = NULL;
    size_t on_count = 0;
    MidiFileInfo info;
    bool ok = read_note_ons(tracks, track_count, &ons, &on_count, &info);
    free(tracks);
    if (!ok) {
        free(streams);
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        long long numerator = (long long)musical_time_numerator(onset) * 4 * (long long)header.division;
        long long denominator = (long long)musical_time_denominator(onset);
        if (denominator <= 0 || numerator % denominator != 0) {
            continue;
        }
        long long tick = numerator / denominator;
        int pitch = (int)pitch_numeric_value_position_numerator(pitch_values[i]);
        for (size_t k = 0; k < on_count; k++) {
            if (!ons[k].used && ons[k].tick == tick && ons[k].pitch == pitch) {
                ons[k].used = true;
                streams[i] = ons[k].track;
                break;
            }
        }
    }
    free(ons);
    midi_file_info_destroy(&info);
    return streams;
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

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiMeterImport meter = {NULL, NULL, NULL, 0};
    MidiImportResult import_result =
        midi_importer_import_with_meter(buffer, (size_t)size, &events, &pitch_values, &count, &meter);

    MidiKeySignature key_signature = {false, 0, false};
    MidiKeySignatureEvents key_signature_events = {NULL, 0};
    unsigned ticks_per_quarter = 0;
    if (import_result == MIDI_IMPORT_OK) {
        SmfHeader header;
        SmfTrack *tracks = NULL;
        size_t track_count = 0;
        if (smf_reader_parse(buffer, (size_t)size, &header, &tracks, &track_count) == SMF_READER_OK) {
            ticks_per_quarter = header.division;
            free(tracks);
        }

        /* A malformed key signature meta-event doesn't fail an
           otherwise-valid import -- it just means no key signature is
           reported, same as a file that never had one. */
        if (midi_key_signature_scan(buffer, (size_t)size, &key_signature) != MIDI_KEY_SIGNATURE_OK) {
            key_signature = (MidiKeySignature){false, 0, false};
        }
        if (midi_key_signature_scan_all(buffer, (size_t)size, &key_signature_events) !=
            MIDI_KEY_SIGNATURE_SCAN_ALL_OK) {
            key_signature_events = (MidiKeySignatureEvents){NULL, 0};
        }
    }
    size_t *streams = NULL;
    if (import_result == MIDI_IMPORT_OK) {
        streams = note_streams(buffer, (size_t)size, (const NoteEvent *const *)events, pitch_values, count);
        if (!streams) {
            import_result = MIDI_IMPORT_ERR_ALLOC;
            for (size_t i = 0; i < count; i++) {
                note_event_destroy(events[i]);
                pitch_numeric_value_destroy(pitch_values[i]);
            }
            free(events);
            free(pitch_values);
            midi_meter_import_destroy(&meter);
        }
    }
    free(buffer);

    if (import_result != MIDI_IMPORT_OK) {
        midi_key_signature_events_destroy(&key_signature_events);
        fprintf(stderr, "Error: failed to import '%s': %s\n", path, midi_import_error_message(import_result));
        return MIDI_PIPELINE_ERR_IMPORT_FAILED;
    }

    out->events = events;
    out->pitch_values = pitch_values;
    out->count = count;
    out->streams = streams;
    out->key_signature = key_signature;
    out->key_signature_events = key_signature_events;
    out->ticks_per_quarter = ticks_per_quarter;
    out->meter = meter;
    out->source = (ImportedSource){.format = INPUT_FORMAT_MIDI};
    for (size_t i = 0; i < meter.conflict_count; i++) {
        fprintf(stderr, "Warning: '%s' declares different time signatures at tick %llu; the last one is used\n",
                path, (unsigned long long)meter.conflict_ticks[i]);
    }
    return MIDI_PIPELINE_OK;
}
