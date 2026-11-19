#include "io/imported_score.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/general_midi.h"
#include "io/midi_importer.h"
#include "io/midi_key_signature.h"
#include "io/musicxml_importer.h"
#include "io/score_staves.h"
#include "model/core/provenance_reference.h"
#include "model/core/rational.h"
#include "model/core/voice_reference.h"
#include "model/identity/entity_id.h"
#include "model/pitch/tuning_system.h"
#include "model/time/duration.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/note_value.h"
#include "model/time/temporal_anchor.h"
#include "model/time/time_signature.h"

/* ---- Release ---- */

void imported_score_destroy(ImportedScore *score) {
    if (!score) {
        return;
    }
    for (size_t i = 0; i < score->count; i++) {
        if (!score->events_borrowed) {
            note_event_destroy(score->events[i]);
        }
        pitch_numeric_value_destroy(score->pitch_values[i]);
    }
    free(score->events);
    free(score->pitch_values);
    free(score->streams);
    tempo_map_destroy(score->tempo_map);
    score->tempo_map = NULL;
    if (score->meter_map_borrowed) {
        score->meter.meter_map = NULL;
    }
    midi_meter_import_destroy(&score->meter);
    midi_key_signature_events_destroy(&score->key_signature_events);
    for (size_t i = 0; i < score->source.part_count; i++) {
        free(score->source.parts[i].id);
        free(score->source.parts[i].name);
    }
    free(score->source.parts);
    free(score->source.part_index);
    free(score->source.staff);
    for (size_t i = 0; i < score->source.tempo_marking_count; i++) {
        free(score->source.tempo_markings[i].text);
    }
    free(score->source.tempo_markings);
    for (size_t i = 0; i < score->source.dynamics_count; i++) {
        free(score->source.dynamics[i].marking);
    }
    free(score->source.dynamics);
    for (size_t i = 0; i < score->source.warning_line_count; i++) {
        free(score->source.warnings[i]);
    }
    free(score->source.warnings);
    score->source = (ImportedSource){.format = IMPORTED_FORMAT_MIDI};
    score->meter = (MidiMeterImport){NULL, {{0, 1}}, NULL, 0};
    score->events = NULL;
    score->pitch_values = NULL;
    score->streams = NULL;
    score->count = 0;
    score->ticks_per_quarter = 0;
    score->events_borrowed = false;
    score->meter_map_borrowed = false;
}

/* ---- Messages and warnings ---- */

static void set_error(ImportedScoreError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof error->message, format, args);
    va_end(args);
}

/* Appends one line to the source's warnings; false on allocation failure. */
static bool warning_add(ImportedSource *source, const char *format, ...) {
    char text[512];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    char **grown = realloc(source->warnings, (source->warning_line_count + 1) * sizeof *grown);
    if (!grown) {
        return false;
    }
    source->warnings = grown;
    char *copy = strdup(text);
    if (!copy) {
        return false;
    }
    source->warnings[source->warning_line_count++] = copy;
    return true;
}

/* How messages name the input: the quoted path of a file, or "the input" for bytes. */
typedef struct {
    const char *path; /* NULL for a buffer */
} Origin;

static void origin_name(const Origin *origin, char *out, size_t size) {
    if (origin->path) {
        snprintf(out, size, "'%s'", origin->path);
    } else {
        snprintf(out, size, "the input");
    }
}

/* ---- Format ---- */

bool imported_format_parse(const char *name, ImportedFormat *out) {
    if (strcmp(name, "midi") == 0) {
        *out = IMPORTED_FORMAT_MIDI;
        return true;
    }
    if (strcmp(name, "musicxml") == 0) {
        *out = IMPORTED_FORMAT_MUSICXML;
        return true;
    }
    return false;
}

static bool extension_equals(const char *path, const char *extension) {
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) {
        return false;
    }
    dot++;
    while (*dot && *extension) {
        if (tolower((unsigned char)*dot) != *extension) {
            return false;
        }
        dot++;
        extension++;
    }
    return *dot == '\0' && *extension == '\0';
}

static ImportedFormat detect_by_extension(const char *path) {
    if (extension_equals(path, "mid") || extension_equals(path, "midi")) {
        return IMPORTED_FORMAT_MIDI;
    }
    if (extension_equals(path, "musicxml") || extension_equals(path, "xml") || extension_equals(path, "mxl")) {
        return IMPORTED_FORMAT_MUSICXML;
    }
    return IMPORTED_FORMAT_AUTO;
}

/* The format the first bytes of a document say, IMPORTED_FORMAT_AUTO when they say nothing. */
static ImportedFormat detect_by_bytes(const unsigned char *head, size_t count) {
    if (count >= 4 && memcmp(head, "MThd", 4) == 0) {
        return IMPORTED_FORMAT_MIDI;
    }
    if (count >= 4 && memcmp(head, "PK\x03\x04", 4) == 0) {
        return IMPORTED_FORMAT_MUSICXML;
    }
    size_t i = 0;
    if (count >= 3 && memcmp(head, "\xEF\xBB\xBF", 3) == 0) {
        i = 3;
    }
    while (i < count && isspace(head[i])) {
        i++;
    }
    return i < count && head[i] == '<' ? IMPORTED_FORMAT_MUSICXML : IMPORTED_FORMAT_AUTO;
}

static ImportedFormat detect_by_content(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return IMPORTED_FORMAT_AUTO;
    }
    unsigned char head[64];
    size_t count = fread(head, 1, sizeof head, file);
    fclose(file);
    return detect_by_bytes(head, count);
}

ImportedFormat imported_format_detect(const char *path) {
    ImportedFormat format = detect_by_extension(path);
    return format != IMPORTED_FORMAT_AUTO ? format : detect_by_content(path);
}

/* ---- MIDI ---- */

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
    long long numerator = onset->value.num * 4 * (long long)division;
    long long denominator = onset->value.den;
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
    const char *instrument = keys[part].channel == PERCUSSION_CHANNEL ? "Percussion" : general_midi_instrument_name(program);
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

/* One stream per distinct (track, channel) that has a note, in order of first appearance, then the streams the library
   joins as the two hands of one piano become one part. Each part has its id, name and program (the higher stream's
   for a pair) and 2 staves for a pair or a grand staff program, else 1. Writes to each note (import->count entries)
   its stream (streams), its part (part_index) and its stream's staff, 1, 2 or 0 for "decide per note" (hints).
   false on allocation or library failure, with source untouched. */
static bool build_parts(const MidiImport *import, ImportedSource *source, size_t *streams, size_t *part_index,
                        int *hints) {
    size_t count = import->count;
    PartKey *keys = calloc(count > 0 ? count : 1, sizeof *keys);
    ImportedPart *parts = calloc(count > 0 ? count : 1, sizeof *parts);
    size_t key_count = 0;
    bool ok = keys && parts;
    for (size_t i = 0; ok && i < count; i++) {
        size_t track = import->note_tracks[i];
        int channel = import->note_channels[i];
        long long tick = onset_tick(import->events[i], import->division);
        size_t stream = 0;
        while (stream < key_count && !(keys[stream].track == track && keys[stream].channel == channel)) {
            stream++;
        }
        if (stream == key_count) {
            keys[key_count++] = (PartKey){track, channel, tick, i};
        } else if (tick < keys[stream].first_tick) {
            keys[stream].first_tick = tick;
        }
    }
    if (ok) {
        qsort(keys, key_count, sizeof *keys, part_key_compare);
        for (size_t i = 0; i < count; i++) {
            size_t stream = 0;
            while (keys[stream].track != import->note_tracks[i] || keys[stream].channel != import->note_channels[i]) {
                stream++;
            }
            streams[i] = stream;
        }
    }
    ScoreStream *score_streams = calloc(key_count > 0 ? key_count : 1, sizeof *score_streams);
    size_t *part_of_stream = calloc(key_count > 0 ? key_count : 1, sizeof *part_of_stream);
    int *staff_of_stream = calloc(key_count > 0 ? key_count : 1, sizeof *staff_of_stream);
    size_t part_count = 0;
    ok = ok && score_streams && part_of_stream && staff_of_stream;
    for (size_t stream = 0; ok && stream < key_count; stream++) {
        score_streams[stream].program = part_program(import, &keys[stream]);
    }
    for (size_t i = 0; ok && i < count; i++) {
        score_streams[streams[i]].note_count++;
    }
    ok = ok && score_staves_pair_hands((NoteEvent *const *)import->events, import->pitch_values, count, streams,
                                       score_streams, key_count, part_of_stream, staff_of_stream,
                                       &part_count) == SCORE_STAVES_OK;
    for (size_t part = 0; ok && part < part_count; part++) {
        /* The stream that names the part: the higher stream of a pair (staff 1), else its only stream. */
        size_t named = key_count;
        int stream_total = 0;
        for (size_t stream = 0; stream < key_count; stream++) {
            if (part_of_stream[stream] == part) {
                stream_total++;
                if (named == key_count || staff_of_stream[stream] == 1) {
                    named = stream;
                }
            }
        }
        int program = score_streams[named].program;
        char id[32];
        snprintf(id, sizeof id, "P%zu", part + 1);
        parts[part].id = strdup(id);
        parts[part].name = part_name(import, keys, key_count, named, program);
        parts[part].staff_count = stream_total > 1 || score_program_uses_grand_staff(program) ? 2 : 1;
        parts[part].midi_program = program;
        ok = parts[part].id && parts[part].name;
    }
    for (size_t i = 0; ok && i < count; i++) {
        part_index[i] = part_of_stream[streams[i]];
        hints[i] = staff_of_stream[streams[i]];
    }
    if (ok) {
        source->parts = parts;
        source->part_count = part_count;
    } else {
        parts_destroy(parts, part_count);
    }
    free(keys);
    free(score_streams);
    free(part_of_stream);
    free(staff_of_stream);
    return ok;
}

/* The stream (track and channel) each imported note was read from, as ImportedScore.streams, with source.parts, the
   part of each note as source.part_index and the staff of each note. NULL on failure. */
static size_t *note_parts(const MidiImport *import, ImportedSource *source) {
    size_t count = import->count;
    size_t *streams = calloc(count > 0 ? count : 1, sizeof *streams);
    size_t *part_index = calloc(count > 0 ? count : 1, sizeof *part_index);
    int *hints = malloc((count > 0 ? count : 1) * sizeof *hints);
    if (!streams || !part_index || !hints || !build_parts(import, source, streams, part_index, hints)) {
        free(streams);
        free(part_index);
        free(hints);
        return NULL;
    }
    if (source->part_count == 0) {
        free(part_index);
        free(hints);
        return streams;
    }
    int *part_staff_count = malloc(source->part_count * sizeof *part_staff_count);
    int *staff = malloc((count > 0 ? count : 1) * sizeof *staff);
    for (size_t i = 0; part_staff_count && i < source->part_count; i++) {
        part_staff_count[i] = source->parts[i].staff_count;
    }
    if (!part_staff_count || !staff ||
        score_staves_assign_hinted((NoteEvent *const *)import->events, import->pitch_values, count, part_index,
                                   part_staff_count, source->part_count, hints, staff) != SCORE_STAVES_OK) {
        free(part_staff_count);
        free(staff);
        free(part_index);
        free(hints);
        parts_destroy(source->parts, source->part_count);
        *source = (ImportedSource){.format = IMPORTED_FORMAT_MIDI};
        free(streams);
        return NULL;
    }
    free(part_staff_count);
    free(hints);
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
            tempo_map_region_start(region).value.num == 0) {
            source->tempo_declared = true;
            source->initial_quarters_per_minute = 60000000.0 / (double)microseconds;
        }
    }
}

/* The first key signature of the file in track then byte order (the mode byte decides minor). */
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

/* The score of a MIDI document held in bytes. */
static ImportedScoreResult import_midi(const unsigned char *bytes, size_t size, const Origin *origin, ImportedScore *out,
                                       ImportedScoreError *error) {
    char name[400];
    origin_name(origin, name, sizeof name);
    MidiImport import;
    MidiImportResult import_result = midi_importer_import_complete(bytes, size, &import);
    if (import_result != MIDI_IMPORT_OK) {
        set_error(error, "Error: failed to import %s: %s", name, midi_import_error_message(import_result));
        return IMPORTED_SCORE_ERR_FORMAT;
    }

    ImportedSource source = {.format = IMPORTED_FORMAT_MIDI};
    size_t *streams = note_parts(&import, &source);
    if (!streams) {
        midi_import_destroy(&import);
        set_error(error, "Error: failed to import %s: %s", name, midi_import_error_message(MIDI_IMPORT_ERR_ALLOC));
        return IMPORTED_SCORE_ERR_ALLOC;
    }
    read_opening_tempo(import.tempo.tempo_map, &source);
    for (size_t i = 0; i < import.meter.conflict_count; i++) {
        if (!warning_add(&source, "Warning: %s declares different time signatures at tick %llu; the last one is used",
                         name, (unsigned long long)import.meter.conflict_ticks[i])) {
            for (size_t w = 0; w < source.warning_line_count; w++) {
                free(source.warnings[w]);
            }
            free(source.warnings);
            free(streams);
            midi_import_destroy(&import);
            set_error(error, "Error: failed to import %s: %s", name,
                      midi_import_error_message(MIDI_IMPORT_ERR_ALLOC));
            return IMPORTED_SCORE_ERR_ALLOC;
        }
    }

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
    out->events_borrowed = false;
    out->meter_map_borrowed = false;
    /* The notes, meter, key signatures and tempo map now belong to out. */
    import.events = NULL;
    import.pitch_values = NULL;
    import.count = 0;
    import.meter = (MidiMeterImport){NULL, {{0, 1}}, NULL, 0};
    import.key_signatures = (MidiKeySignatureEvents){NULL, 0};
    import.tempo.tempo_map = NULL;
    midi_import_destroy(&import);
    return IMPORTED_SCORE_OK;
}

/* ---- MusicXML ---- */

/* The end (start plus duration) of the latest-ending note, or 0/1 with no notes, written to *out. False when a sum
   overflows (*out is untouched). */
static bool notes_last_end(const NoteEvent *const *events, size_t count, MusicalTime *out) {
    MusicalTime latest = musical_time_of(rational_ratio(0, 1));
    for (size_t i = 0; i < count; i++) {
        MusicalTime end;
        if (musical_time_plus(*temporal_anchor_position(note_event_temporal_anchor(events[i])),
                              *note_event_duration(events[i]), &end) != MUSICAL_TIME_OK) {
            return false;
        }
        if (musical_time_cmp(end, latest) > 0) {
            latest = end;
        }
    }
    *out = latest;
    return true;
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
        source->parts[i].midi_program = -1;
        source->part_count = i + 1;
        if (!source->parts[i].id || !source->parts[i].name) {
            return false;
        }
    }
    return true;
}

/* Copies the importer's tempo and dynamics markings; each count is set as its entry is filled, so a failure leaves
   the source safe to destroy. false on allocation failure. */
static bool copy_markings(const MusicXmlImport *import, ImportedSource *source) {
    if (import->tempo_marking_count > 0) {
        source->tempo_markings = calloc(import->tempo_marking_count, sizeof(ImportedTempoMarking));
        if (!source->tempo_markings) {
            return false;
        }
    }
    for (size_t i = 0; i < import->tempo_marking_count; i++) {
        const TempoMarking *marking = import->tempo_markings[i];
        ImportedTempoMarking *copy = &source->tempo_markings[i];
        const MusicalTime *position = temporal_anchor_position(tempo_marking_temporal_anchor(marking));
        copy->position = position->value;
        copy->part = import->tempo_marking_part[i];
        const MetronomeMarking *metronome = tempo_marking_metronome_marking(marking);
        if (metronome) {
            copy->has_metronome = true;
            if (rational_make((long long)metronome_marking_beat_unit_numerator(metronome),
                              (long long)metronome_marking_beat_unit_denominator(metronome),
                              &copy->beat_unit) != RATIONAL_OK ||
                rational_make((long long)metronome_marking_bpm_numerator(metronome),
                              (long long)metronome_marking_bpm_denominator(metronome), &copy->bpm) != RATIONAL_OK) {
                return false;
            }
        }
        if (tempo_marking_has_text(marking) && !(copy->text = strdup(tempo_marking_text(marking)))) {
            return false;
        }
        source->tempo_marking_count = i + 1;
    }
    if (import->dynamics_count > 0) {
        source->dynamics = calloc(import->dynamics_count, sizeof(ImportedDynamic));
        if (!source->dynamics) {
            return false;
        }
    }
    for (size_t i = 0; i < import->dynamics_count; i++) {
        const Dynamics *dynamics = import->dynamics[i];
        ImportedDynamic *copy = &source->dynamics[i];
        const TimeSpan *span = dynamics_target_time_span(dynamics);
        source->dynamics_count = i + 1;
        if (!span) {
            continue; /* the importer gives every dynamic a span; an entry without a marking is skipped later */
        }
        copy->start = span->start.value;
        copy->end = span->end.value;
        copy->part = import->dynamics_part[i];
        copy->staff = import->dynamics_staff[i];
        if (!(copy->marking = strdup(dynamics_marking(dynamics)))) {
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

/* The score of a MusicXML document held in bytes. */
static ImportedScoreResult import_musicxml(const unsigned char *bytes, size_t size, const Origin *origin,
                                           ImportedScore *out, ImportedScoreError *error) {
    char name[400];
    origin_name(origin, name, sizeof name);
    if (size >= 4 && memcmp(bytes, "PK\x03\x04", 4) == 0) {
        set_error(error,
                  "Error: failed to import %s: compressed MusicXML (.mxl) is not supported by the Ursatz importer; "
                  "export the score as uncompressed .musicxml",
                  name);
        return IMPORTED_SCORE_ERR_COMPRESSED;
    }

    MusicXmlImport import;
    memset(&import, 0, sizeof(import));
    MusicXmlImportStatus status = musicxml_import_buffer((const char *)bytes, size, &import);
    if (status != MUSICXML_IMPORT_OK) {
        set_error(error, "Error: failed to import %s: %s (%s)", name, status_description(status),
                  import.error_message);
        musicxml_import_destroy(&import);
        return status == MUSICXML_IMPORT_ERR_ALLOC ? IMPORTED_SCORE_ERR_ALLOC : IMPORTED_SCORE_ERR_FORMAT;
    }
    if (import.count == 0) {
        set_error(error, "Error: failed to import %s: the score contains no notes", name);
        musicxml_import_destroy(&import);
        return IMPORTED_SCORE_ERR_EMPTY_SCORE;
    }

    ImportedScore score;
    memset(&score, 0, sizeof(score));
    score.source.format = IMPORTED_FORMAT_MUSICXML;
    score.source.meter_notated = true;
    score.source.key_notated = true;
    score.source.spelling_notated = true;
    score.source.warning_count = import.warnings.count;
    bool warned = true;
    for (size_t i = 0; warned && i < import.warnings.message_count; i++) {
        warned = warning_add(&score.source, "Warning: %s: %s", name, import.warnings.messages[i]);
    }
    if (warned && import.warnings.count > import.warnings.message_count) {
        warned = warning_add(&score.source, "Warning: %s: and %zu more", name,
                             import.warnings.count - import.warnings.message_count);
    }
    if (import.tempo_map && !tempo_map_is_defaulted(import.tempo_map) && tempo_map_region_count(import.tempo_map) > 0) {
        unsigned long microseconds = tempo_map_region_microseconds_per_quarter(tempo_map_region(import.tempo_map, 0));
        if (microseconds > 0) {
            score.source.tempo_declared = true;
            score.source.initial_quarters_per_minute = 60000000.0 / (double)microseconds;
        }
    }

    MusicalTime end_time;
    bool ok = warned && notes_last_end((const NoteEvent *const *)import.events, import.count, &end_time) &&
              take_key_signatures(&import, &score.key_signature_events, &score.key_signature) &&
              copy_parts(&import, &score.source) && copy_markings(&import, &score.source);
    if (!ok) {
        midi_key_signature_events_destroy(&score.key_signature_events);
        imported_score_destroy(&score);
        musicxml_import_destroy(&import);
        set_error(error, "Error: out of memory importing %s", name);
        return IMPORTED_SCORE_ERR_ALLOC;
    }

    score.events = import.events;
    score.pitch_values = import.pitch_values;
    score.count = import.count;
    score.streams = malloc((import.count > 0 ? import.count : 1) * sizeof *score.streams);
    if (!score.streams) {
        imported_score_destroy(&score);
        musicxml_import_destroy(&import);
        set_error(error, "Error: out of memory importing %s", name);
        return IMPORTED_SCORE_ERR_ALLOC;
    }
    memcpy(score.streams, import.part_index, import.count * sizeof *score.streams);
    score.source.part_index = import.part_index;
    score.source.staff = import.staff;
    score.meter = (MidiMeterImport){import.meter_map, end_time, NULL, 0};
    import.events = NULL;
    import.pitch_values = NULL;
    import.count = 0;
    import.part_index = NULL;
    import.staff = NULL;
    import.meter_map = NULL;
    musicxml_import_destroy(&import);

    *out = score;
    return IMPORTED_SCORE_OK;
}

/* ---- Entries ---- */

static ImportedScoreResult import_bytes(const unsigned char *bytes, size_t size, ImportedFormat format,
                                        const Origin *origin, ImportedScore *out, ImportedScoreError *error) {
    return format == IMPORTED_FORMAT_MIDI ? import_midi(bytes, size, origin, out, error)
                                          : import_musicxml(bytes, size, origin, out, error);
}

/* The whole file in a new buffer; IMPORTED_SCORE_ERR_IO with the message when it cannot be read. */
static ImportedScoreResult read_file(const char *path, unsigned char **bytes, size_t *size, ImportedScoreError *error) {
    FILE *file = fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0) {
        if (file) {
            fclose(file);
        }
        set_error(error, "Error: cannot read file '%s'", path);
        return IMPORTED_SCORE_ERR_IO;
    }
    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        set_error(error, "Error: cannot read file '%s'", path);
        return IMPORTED_SCORE_ERR_IO;
    }
    rewind(file);
    unsigned char *buffer = malloc((size_t)length > 0 ? (size_t)length : 1);
    if (!buffer) {
        fclose(file);
        set_error(error, "Error: out of memory reading '%s'", path);
        return IMPORTED_SCORE_ERR_IO;
    }
    size_t read_count = fread(buffer, 1, (size_t)length, file);
    fclose(file);
    if (read_count != (size_t)length) {
        free(buffer);
        set_error(error, "Error: cannot read file '%s'", path);
        return IMPORTED_SCORE_ERR_IO;
    }
    *bytes = buffer;
    *size = (size_t)length;
    return IMPORTED_SCORE_OK;
}

ImportedScoreResult imported_score_import_file(const char *path, ImportedFormat format, ImportedScore *out,
                                               ImportedScoreError *error) {
    if (!path || !out) {
        set_error(error, "Error: no input file to import");
        return IMPORTED_SCORE_ERR_ARGUMENT;
    }
    if (format == IMPORTED_FORMAT_AUTO) {
        format = imported_format_detect(path);
        if (format == IMPORTED_FORMAT_AUTO) {
            FILE *file = fopen(path, "rb");
            if (!file) {
                set_error(error, "Error: cannot read file '%s'", path);
                return IMPORTED_SCORE_ERR_IO;
            }
            fclose(file);
            set_error(error, "Error: cannot tell the input format of '%s'; use --input-format midi|musicxml", path);
            return IMPORTED_SCORE_ERR_UNKNOWN_FORMAT;
        }
    }
    unsigned char *bytes = NULL;
    size_t size = 0;
    ImportedScoreResult read = read_file(path, &bytes, &size, error);
    if (read != IMPORTED_SCORE_OK) {
        return read;
    }
    Origin origin = {path};
    ImportedScoreResult result = import_bytes(bytes, size, format, &origin, out, error);
    free(bytes);
    return result;
}

ImportedScoreResult imported_score_import_buffer(const unsigned char *bytes, size_t size, ImportedFormat format,
                                                 ImportedScore *out, ImportedScoreError *error) {
    if (!bytes || size == 0 || !out) {
        set_error(error, "Error: no input bytes to import");
        return IMPORTED_SCORE_ERR_ARGUMENT;
    }
    if (format == IMPORTED_FORMAT_AUTO) {
        format = detect_by_bytes(bytes, size < 64 ? size : 64);
        if (format == IMPORTED_FORMAT_AUTO) {
            set_error(error, "Error: cannot tell the input format of the bytes; pass midi or musicxml");
            return IMPORTED_SCORE_ERR_UNKNOWN_FORMAT;
        }
    }
    Origin origin = {NULL};
    return import_bytes(bytes, size, format, &origin, out, error);
}

/* ---- Readings ---- */

/* One unsigned decimal number from *text, advancing past it; false when there is none or it overflows. */
static bool parse_number(const char **text, unsigned long *out) {
    const char *p = *text;
    if (!isdigit((unsigned char)*p)) {
        return false;
    }
    unsigned long value = 0;
    for (; isdigit((unsigned char)*p); p++) {
        unsigned digit = (unsigned)(*p - '0');
        if (value > (ULONG_MAX - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    *text = p;
    *out = value;
    return true;
}

/* "n" or "n/d", the whole text; the denominator is 1 for "n". */
static bool parse_fraction(const char *text, unsigned long *numerator, unsigned long *denominator) {
    if (!text || !parse_number(&text, numerator)) {
        return false;
    }
    *denominator = 1;
    if (*text == '/') {
        text++;
        if (!parse_number(&text, denominator)) {
            return false;
        }
    }
    return *text == '\0' && *denominator != 0;
}

static bool is_power_of_two(unsigned long value) {
    return value != 0 && (value & (value - 1)) == 0;
}

bool imported_reading_parse_meter(const char *text, ImportedReading *r) {
    unsigned long beats;
    unsigned long beat_type;
    if (!text || !parse_number(&text, &beats) || *text++ != '/' || !parse_number(&text, &beat_type) || *text != '\0') {
        return false;
    }
    if (beats < 1 || beats > 64 || beat_type > 16 || !is_power_of_two(beat_type)) {
        return false;
    }
    r->has_meter = true;
    r->beats = (int)beats;
    r->beat_type = (int)beat_type;
    return true;
}

bool imported_reading_parse_scale(const char *text, ImportedReading *r) {
    unsigned long numerator;
    unsigned long denominator;
    if (!parse_fraction(text, &numerator, &denominator)) {
        return false;
    }
    bool allowed = (denominator == 1 && (numerator == 1 || numerator == 2 || numerator == 4)) ||
                   (numerator == 1 && (denominator == 2 || denominator == 4));
    if (!allowed) {
        return false;
    }
    r->scale = rational_ratio((long long)numerator, (long long)denominator);
    return true;
}

bool imported_reading_parse_pickup(const char *text, ImportedReading *r) {
    unsigned long numerator;
    unsigned long denominator;
    Rational pickup;
    if (!parse_fraction(text, &numerator, &denominator) || numerator == 0 || numerator > LLONG_MAX ||
        denominator > LLONG_MAX || rational_make((long long)numerator, (long long)denominator, &pickup) != RATIONAL_OK) {
        return false;
    }
    r->has_pickup = true;
    r->pickup = pickup;
    return true;
}

ImportedReading imported_reading_none(void) {
    ImportedReading none = {.scale = {1, 1}, .pickup = {0, 1}};
    return none;
}

/* ---- Applying a reading ---- */

/* value times the reading's scale, into *out; false when it does not fit. */
static bool scale_value(Rational value, const ImportedReading *reading, Rational *out) {
    return rational_mul(value, reading->scale, out) == RATIONAL_OK;
}

/* A NoteEvent like note, its start and duration scaled; NULL on allocation failure or overflow. */
static NoteEvent *scaled_event(const NoteEvent *note, const ImportedReading *reading, bool *overflow) {
    const MusicalTime *start = temporal_anchor_position(note_event_temporal_anchor(note));
    const Duration *duration = note_event_duration(note);
    Rational start_value;
    Rational length_value;
    if (!scale_value(start->value, reading, &start_value) || !scale_value(duration->value, reading, &length_value)) {
        *overflow = true;
        return NULL;
    }

    EntityID *id = entity_id_copy(note_event_id(note));
    MusicalTime time;
    Duration length;
    PitchIdentity *pitch = NULL;
    PitchSpelling *spelling = NULL;
    VoiceReference *voice = NULL;
    ProvenanceReference *provenance = NULL;
    NoteEvent *event = NULL;

    bool ok = id && musical_time_make(start_value.num, start_value.den, &time) == MUSICAL_TIME_OK &&
              duration_make(length_value.num, length_value.den, &length) == DURATION_OK;
    if (ok) {
        size_t token_size = 0;
        const void *token = pitch_identity_token(note_event_pitch(note), &token_size);
        ok = pitch_identity_create(token, token_size, &pitch) == PITCH_IDENTITY_OK;
    }
    const PitchSpelling *old_spelling = note_event_optional_pitch_spelling(note);
    if (ok && old_spelling) {
        ok = pitch_spelling_create(pitch_spelling_letter(old_spelling), pitch_spelling_accidental(old_spelling),
                                   pitch_spelling_octave(old_spelling), &spelling) == PITCH_SPELLING_OK;
    }
    const VoiceReference *old_voice = note_event_optional_voice(note);
    if (ok && old_voice) {
        EntityID *voice_id = entity_id_copy(voice_reference_target_id(old_voice));
        ok = voice_id && voice_reference_create(voice_id, &voice) == VOICE_REFERENCE_OK;
        if (!ok) {
            entity_id_destroy(voice_id);
        }
    }
    const ProvenanceReference *old_provenance = note_event_provenance(note);
    if (ok && provenance_reference_state(old_provenance) == PROVENANCE_REFERENCE_PRESENT) {
        EntityID *source_id = entity_id_copy(provenance_reference_target_id(old_provenance));
        ok = source_id && provenance_reference_create_present(source_id, &provenance) == PROVENANCE_REFERENCE_OK;
        if (!ok) {
            entity_id_destroy(source_id);
        }
    } else if (ok) {
        ok = provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK;
    }

    if (ok && note_event_create(id, time, length, pitch, spelling, voice, provenance, &event) == NOTE_EVENT_OK) {
        return event;
    }
    entity_id_destroy(id);
    pitch_identity_destroy(pitch);
    pitch_spelling_destroy(spelling);
    voice_reference_destroy(voice);
    provenance_reference_destroy(provenance);
    return NULL;
}

/* time scaled by the reading, into *out; false (with *overflow set) when it does not fit. */
static bool scaled_value(MusicalTime time, const ImportedReading *reading, bool *overflow, MusicalTime *out) {
    Rational scaled;
    if (!scale_value(time.value, reading, &scaled)) {
        *overflow = true;
        return false;
    }
    return musical_time_make(scaled.num, scaled.den, out) == MUSICAL_TIME_OK;
}

static int exponent_of(int power_of_two) {
    int exponent = 0;
    while (power_of_two > 1) {
        power_of_two /= 2;
        exponent++;
    }
    return exponent;
}

/* The time signature of the reading's meter. */
static TimeSignature *reading_signature(const ImportedReading *reading) {
    NoteValue *unit = NULL;
    TimeSignature *signature = NULL;
    if (note_value_create(exponent_of(reading->beat_type), &unit) != NOTE_VALUE_OK) {
        return NULL;
    }
    if (time_signature_create((unsigned)reading->beats, unit, &signature) != TIME_SIGNATURE_OK) {
        signature = NULL;
    }
    note_value_destroy(unit);
    return signature;
}

/* A pickup shorter than one measure of signature. */
static bool pickup_fits(const ImportedReading *reading, const TimeSignature *signature) {
    Duration measure;
    if (time_signature_measure_duration(signature, &measure) != DURATION_OK) {
        return false;
    }
    return rational_cmp(reading->pickup, measure.value) < 0;
}

typedef struct {
    NoteEvent **events;
    MusicalTime *key_positions;
    MusicalTime end_time;
    MeterMap *meter_map;
    Rational *times; /* tempo marking positions, then the dynamics' starts and ends */
} Prepared;

static void prepared_destroy(Prepared *prepared, const ImportedScore *notes) {
    for (size_t i = 0; prepared->events && i < notes->count; i++) {
        note_event_destroy(prepared->events[i]);
    }
    free(prepared->events);
    free(prepared->key_positions);
    meter_map_destroy(prepared->meter_map);
    free(prepared->times);
}

/* The meter map the reading asks for; NULL on failure (overflow set when a time did not fit). */
static MeterMap *prepared_meter_map(const ImportedScore *notes, const ImportedReading *reading, bool *invalid,
                                    bool *overflow) {
    const MeterMap *old = notes->meter.meter_map;
    TimeSignature *own = NULL;
    const TimeSignature *chosen = NULL;
    size_t first = 0;
    if (reading->has_meter) {
        own = reading_signature(reading);
        chosen = own;
    } else if (reading->has_pickup || meter_map_has_pickup(old)) {
        first = meter_map_has_pickup(old) ? 1 : 0;
        chosen = meter_map_region_time_signature(meter_map_region(old, first));
    }

    MeterMap *map = NULL;
    MusicalTime zero = musical_time_of(rational_ratio(0, 1));
    MusicalTime pickup = zero;
    MeterMapChangePoint points[8];
    size_t count = 0;
    bool ok = true;

    if (reading->has_pickup) {
        if (!chosen || !pickup_fits(reading, chosen)) {
            *invalid = true;
            ok = false;
        } else {
            ok = musical_time_make(reading->pickup.num, reading->pickup.den, &pickup) == MUSICAL_TIME_OK;
            points[0] = (MeterMapChangePoint){chosen, zero};
            points[1] = (MeterMapChangePoint){chosen, pickup};
            count = 2;
        }
    } else if (reading->has_meter) {
        ok = own != NULL;
        points[0] = (MeterMapChangePoint){chosen, zero};
        count = 1;
    }

    if (ok && count == 0) {
        /* No new meter: the old map with its change points scaled. */
        size_t region_count = meter_map_region_count(old);
        size_t from = 0;
        if (meter_map_is_defaulted(old)) {
            from = region_count; /* rebuilt below as the default */
        } else if (region_count > 1 && meter_map_region_is_defaulted(meter_map_region(old, 0))) {
            from = 1; /* a defaulted lead-in is restored by the map itself */
        }
        MeterMapChangePoint *list = malloc((region_count + 1) * sizeof *list);
        ok = list != NULL;
        for (size_t i = from; ok && i < region_count; i++) {
            const MeterMapRegion *region = meter_map_region(old, i);
            MusicalTime region_start = meter_map_region_start(region);
            MusicalTime start = zero;
            ok = scaled_value(region_start, reading, overflow, &start);
            list[count] = (MeterMapChangePoint){meter_map_region_time_signature(region), start};
            count++;
        }
        EntityID *id = ok ? entity_id_copy(meter_map_id(old)) : NULL;
        if (ok && count == 0) {
            ok = id && meter_map_create_defaulted(id, &map) == METER_MAP_OK;
        } else if (ok) {
            ok = id && meter_map_create(id, list, count, &map) == METER_MAP_OK;
        }
        if (!map) {
            entity_id_destroy(id);
        }
        free(list);
        return ok ? map : NULL;
    }

    if (ok) {
        EntityID *id = entity_id_copy(meter_map_id(old));
        ok = id && meter_map_create(id, points, count, &map) == METER_MAP_OK;
        if (!map) {
            entity_id_destroy(id);
        }
    }
    time_signature_destroy(own);
    return ok ? map : NULL;
}

ImportedReadingResult imported_reading_apply(ImportedScore *notes, const ImportedReading *reading) {
    if (!notes || !reading) {
        return IMPORTED_READING_ERR_NULL_ARGUMENT;
    }
    bool scale_ok = (reading->scale.den == 1 && (reading->scale.num == 1 || reading->scale.num == 2 || reading->scale.num == 4)) ||
                    (reading->scale.num == 1 && (reading->scale.den == 2 || reading->scale.den == 4));
    bool meter_ok = !reading->has_meter || (reading->beats >= 1 && reading->beats <= 64 && reading->beat_type >= 1 &&
                                            reading->beat_type <= 16 && (reading->beat_type & (reading->beat_type - 1)) == 0);
    bool pickup_ok = !reading->has_pickup || (reading->pickup.num > 0 && reading->pickup.den > 0);
    if (!scale_ok || !meter_ok || !pickup_ok || !notes->meter.meter_map || notes->events_borrowed) {
        return IMPORTED_READING_ERR_INVALID;
    }

    Prepared prepared = {0};
    bool overflow = false;
    bool invalid = false;
    bool ok = true;

    prepared.events = calloc(notes->count ? notes->count : 1, sizeof *prepared.events);
    ok = prepared.events != NULL;
    for (size_t i = 0; ok && i < notes->count; i++) {
        prepared.events[i] = scaled_event(notes->events[i], reading, &overflow);
        ok = prepared.events[i] != NULL;
    }

    size_t key_count = notes->key_signature_events.count;
    if (ok) {
        prepared.key_positions = calloc(key_count ? key_count : 1, sizeof *prepared.key_positions);
        ok = prepared.key_positions != NULL;
    }
    for (size_t i = 0; ok && i < key_count; i++) {
        ok = scaled_value(notes->key_signature_events.events[i].position, reading, &overflow,
                          &prepared.key_positions[i]);
    }

    if (ok) {
        ok = scaled_value(notes->meter.end_time, reading, &overflow, &prepared.end_time);
    }

    size_t tempo_count = notes->source.tempo_marking_count;
    size_t dynamics_count = notes->source.dynamics_count;
    size_t value_count = tempo_count + 2 * dynamics_count;
    if (ok) {
        prepared.times = calloc(value_count ? value_count : 1, sizeof *prepared.times);
        ok = prepared.times != NULL;
    }
    for (size_t i = 0; ok && i < tempo_count; i++) {
        const ImportedTempoMarking *marking = &notes->source.tempo_markings[i];
        ok = scale_value(marking->position, reading, &prepared.times[i]);
        overflow = overflow || !ok;
    }
    for (size_t i = 0; ok && i < dynamics_count; i++) {
        const ImportedDynamic *dynamic = &notes->source.dynamics[i];
        size_t at = tempo_count + 2 * i;
        ok = scale_value(dynamic->start, reading, &prepared.times[at]) &&
             scale_value(dynamic->end, reading, &prepared.times[at + 1]);
        overflow = overflow || !ok;
    }

    if (ok) {
        prepared.meter_map = prepared_meter_map(notes, reading, &invalid, &overflow);
        ok = prepared.meter_map != NULL;
    }

    if (!ok) {
        prepared_destroy(&prepared, notes);
        return invalid ? IMPORTED_READING_ERR_INVALID : (overflow ? IMPORTED_READING_ERR_OVERFLOW : IMPORTED_READING_ERR_ALLOC);
    }

    /* Nothing below can fail: swap the prepared pieces in. */
    for (size_t i = 0; i < notes->count; i++) {
        note_event_destroy(notes->events[i]);
        notes->events[i] = prepared.events[i];
    }
    free(prepared.events);
    for (size_t i = 0; i < key_count; i++) {
        notes->key_signature_events.events[i].position = prepared.key_positions[i];
    }
    free(prepared.key_positions);
    notes->meter.end_time = prepared.end_time;
    meter_map_destroy(notes->meter.meter_map);
    notes->meter.meter_map = prepared.meter_map;
    for (size_t i = 0; i < tempo_count; i++) {
        notes->source.tempo_markings[i].position = prepared.times[i];
    }
    for (size_t i = 0; i < dynamics_count; i++) {
        size_t at = tempo_count + 2 * i;
        ImportedDynamic *dynamic = &notes->source.dynamics[i];
        dynamic->start = prepared.times[at];
        dynamic->end = prepared.times[at + 1];
    }
    free(prepared.times);
    notes->source.reading_applied = true;
    notes->source.reading = *reading;
    return IMPORTED_READING_OK;
}

/* ---- The view ---- */

/* A copy of the meter map, or a defaulted 4/4 map for NULL or a defaulted one. NULL on allocation failure. */
static MeterMap *copy_meter_map(const MeterMap *map) {
    EntityID *id = NULL;
    MeterMap *copy = NULL;
    if (!map || meter_map_is_defaulted(map)) {
        if (entity_id_create("fragment-meter", &id) != ENTITY_ID_OK) {
            return NULL;
        }
        if (meter_map_create_defaulted(id, &copy) != METER_MAP_OK) {
            entity_id_destroy(id);
            return NULL;
        }
        return copy;
    }
    size_t count = meter_map_region_count(map);
    MeterMapChangePoint *points = calloc(count > 0 ? count : 1, sizeof *points);
    TimeSignature **signatures = calloc(count > 0 ? count : 1, sizeof *signatures);
    if (!points || !signatures) {
        free(points);
        free(signatures);
        return NULL;
    }
    bool ok = true;
    for (size_t i = 0; ok && i < count; i++) {
        const MeterMapRegion *region = meter_map_region(map, i);
        const TimeSignature *source = meter_map_region_time_signature(region);
        ok = time_signature_copy(source, &signatures[i]) == TIME_SIGNATURE_OK;
        points[i].time_signature = signatures[i];
        points[i].start = meter_map_region_start(region);
    }
    ok = ok && entity_id_create("fragment-meter", &id) == ENTITY_ID_OK;
    if (ok && meter_map_create(id, points, count, &copy) != METER_MAP_OK) {
        entity_id_destroy(id);
        copy = NULL;
    }
    for (size_t i = 0; i < count; i++) {
        time_signature_destroy(signatures[i]);
    }
    free(points);
    free(signatures);
    return copy;
}

/* The key signature event at time 0, shaped as the MIDI scan gives them (the mode byte follows the minor flag). */
static bool add_key_signature(ImportedScore *score, int fifths, bool is_minor) {
    MidiKeySignatureEvent *event = calloc(1, sizeof *event);
    if (!event) {
        return false;
    }
    if (midi_key_signature_build(fifths, &event->key_signature) != MIDI_KEY_SIGNATURE_BUILD_OK) {
        free(event);
        return false;
    }
    event->position = musical_time_of(rational_ratio(0, 1));
    event->mode_hint = is_minor ? MIDI_MODE_HINT_MINOR : MIDI_MODE_HINT_MAJOR;
    event->tick = 0;
    event->track_index = 0;
    event->sharps_flats = (int8_t)fifths;
    event->mode_byte = is_minor ? 1 : 0;
    score->key_signature_events.events = event;
    score->key_signature_events.count = 1;
    score->key_signature = (MidiKeySignature){true, (int8_t)fifths, is_minor};
    return true;
}

#define MIDI_NOTE_COUNT 128

static bool view_valid(const ImportedScoreView *view) {
    if (!view || (view->count > 0 && (!view->events || !view->midi)) ||
        (view->has_key && (view->key_fifths < -7 || view->key_fifths > 7))) {
        return false;
    }
    for (size_t i = 0; i < view->count; i++) {
        if (!view->events[i] || view->midi[i] < 0 || view->midi[i] >= MIDI_NOTE_COUNT) {
            return false;
        }
    }
    return true;
}

ImportedScoreResult imported_score_view(const ImportedScoreView *view, ImportedScore *out) {
    if (!out || !view_valid(view)) {
        return IMPORTED_SCORE_ERR_ARGUMENT;
    }
    ImportedScore built;
    memset(&built, 0, sizeof built);
    built.source = (ImportedSource){.format = IMPORTED_FORMAT_MIDI};
    built.events_borrowed = true;
    size_t slots = view->count > 0 ? view->count : 1;
    built.events = calloc(slots, sizeof *built.events);
    built.pitch_values = calloc(slots, sizeof *built.pitch_values);
    built.streams = calloc(slots, sizeof *built.streams);
    EntityID *tuning_id = NULL;
    TuningSystem *tuning = NULL;
    bool ok = built.events && built.pitch_values && built.streams &&
              entity_id_create("midi-note-number", &tuning_id) == ENTITY_ID_OK;
    if (ok && tuning_system_create(tuning_id, "MIDI note number", &tuning) != TUNING_SYSTEM_OK) {
        entity_id_destroy(tuning_id);
        ok = false;
    }
    for (size_t i = 0; ok && i < view->count; i++) {
        ok = pitch_numeric_value_create(tuning, view->midi[i], 1, &built.pitch_values[i]) == PITCH_NUMERIC_VALUE_OK;
        built.events[i] = (NoteEvent *)view->events[i];
        built.streams[i] = view->streams ? view->streams[i] : 0;
        built.count = i + 1;
    }
    tuning_system_destroy(tuning);
    if (ok) {
        built.meter_map_borrowed = view->meter && !meter_map_is_defaulted(view->meter);
        built.meter.meter_map = built.meter_map_borrowed ? (MeterMap *)view->meter : copy_meter_map(NULL);
        ok = notes_last_end(view->events, view->count, &built.meter.end_time) && built.meter.meter_map;
    }
    if (ok && view->has_key) {
        ok = add_key_signature(&built, view->key_fifths, view->key_minor);
    }
    if (!ok) {
        imported_score_destroy(&built);
        return IMPORTED_SCORE_ERR_ALLOC;
    }
    *out = built;
    return IMPORTED_SCORE_OK;
}
