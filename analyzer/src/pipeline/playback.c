#include <ursatz_analyzer/playback.h>

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "events/note_event.h"
#include "time/temporal_anchor.h"
#include "time/time_rational.h"
#include "util/json_util.h"

#define DEFAULT_QUARTERS_PER_MINUTE 120.0
#define MICROSECONDS_PER_MINUTE 60000000.0
#define MIDI_MAX 127

static double round_to_milli(double value) {
    return floor(value * 1000.0 + 0.5) / 1000.0;
}

static bool rational_at(long long num, unsigned long long den, Rational *out) {
    if (den == 0 || den > (unsigned long long)LLONG_MAX) {
        return false;
    }
    return rational_make(num, (long long)den, out) == RATIONAL_OK;
}

/* ---- tempo ---- */

/* The MIDI file's set-tempo regions, read from the import's tempo map. */
static AnalyzerStatus midi_tempo(const TempoMap *map, AnalyzerTempoRegion **out, size_t *count, bool *defaulted) {
    size_t region_count = tempo_map_region_count(map);
    AnalyzerTempoRegion *regions = calloc(region_count > 0 ? region_count : 1, sizeof(AnalyzerTempoRegion));
    if (!regions) {
        return ANALYZER_ERR_ALLOC;
    }
    for (size_t i = 0; i < region_count; i++) {
        const TempoMapRegion *region = tempo_map_region(map, i);
        const MusicalTime *start = region ? tempo_map_region_start(region) : NULL;
        unsigned long microseconds = region ? tempo_map_region_microseconds_per_quarter(region) : 0;
        if (!start || microseconds == 0 || rational_of_musical_time(start, &regions[i].at) != RATIONAL_OK) {
            free(regions);
            return ANALYZER_ERR_LIBRARY;
        }
        regions[i].quarters_per_minute = round_to_milli(MICROSECONDS_PER_MINUTE / (double)microseconds);
    }
    *defaulted = tempo_map_is_defaulted(map);
    *out = regions;
    *count = region_count;
    return ANALYZER_OK;
}

/* A MusicXML score's tempo: its declared tempo at 0, then one region per metronome mark. */
static AnalyzerStatus score_tempo(const ImportedSource *source, AnalyzerTempoRegion **out, size_t *count,
                                  bool *defaulted) {
    AnalyzerTempoRegion *regions = calloc(source->tempo_marking_count + 1, sizeof(AnalyzerTempoRegion));
    if (!regions) {
        return ANALYZER_ERR_ALLOC;
    }
    regions[0].at = rational_ratio(0, 1);
    if (!source->tempo_declared || !(source->initial_quarters_per_minute > 0.0)) {
        regions[0].quarters_per_minute = DEFAULT_QUARTERS_PER_MINUTE;
        *out = regions;
        *count = 1;
        *defaulted = true;
        return ANALYZER_OK;
    }
    regions[0].quarters_per_minute = round_to_milli(source->initial_quarters_per_minute);
    size_t used = 1;
    for (size_t i = 0; i < source->tempo_marking_count; i++) {
        const ImportedTempoMarking *marking = &source->tempo_markings[i];
        if (!marking->has_metronome || marking->bpm_denominator == 0 || marking->beat_unit_denominator == 0 ||
            marking->position_numerator < 0) {
            continue;
        }
        double qpm = ((double)marking->bpm_numerator / (double)marking->bpm_denominator) *
                     ((double)marking->beat_unit_numerator / (double)marking->beat_unit_denominator) * 4.0;
        if (!(qpm > 0.0)) {
            continue;
        }
        Rational at;
        if (!rational_at(marking->position_numerator, marking->position_denominator, &at)) {
            free(regions);
            return ANALYZER_ERR_LIBRARY;
        }
        size_t slot = 0;
        while (slot < used && rational_cmp(regions[slot].at, at) < 0) {
            slot++;
        }
        if (slot < used && rational_cmp(regions[slot].at, at) == 0) {
            regions[slot].quarters_per_minute = round_to_milli(qpm);
            continue;
        }
        memmove(&regions[slot + 1], &regions[slot], (used - slot) * sizeof(AnalyzerTempoRegion));
        regions[slot].at = at;
        regions[slot].quarters_per_minute = round_to_milli(qpm);
        used++;
    }
    *out = regions;
    *count = used;
    *defaulted = false;
    return ANALYZER_OK;
}

/* Multiplies every region's tempo by num/den, and its position too when positions is set. */
static AnalyzerStatus scale_tempo(AnalyzerTempoRegion *regions, size_t count, long num, unsigned long den,
                                  bool positions) {
    if (num <= 0 || den == 0 || (num == 1 && den == 1)) {
        return ANALYZER_OK;
    }
    Rational scale;
    if (!rational_at(num, den, &scale)) {
        return ANALYZER_ERR_LIBRARY;
    }
    for (size_t i = 0; i < count; i++) {
        regions[i].quarters_per_minute = round_to_milli(regions[i].quarters_per_minute * (double)num / (double)den);
        if (positions && rational_mul(regions[i].at, scale, &regions[i].at) != RATIONAL_OK) {
            return ANALYZER_ERR_LIBRARY;
        }
    }
    return ANALYZER_OK;
}

/* ---- notes ---- */

static AnalyzerStatus build_notes(const ImportedNotes *imported, AnalyzerPlayback *out) {
    out->end = rational_ratio(0, 1);
    if (imported->count == 0) {
        return ANALYZER_OK;
    }
    out->notes = calloc(imported->count, sizeof(AnalyzerPlaybackNote));
    if (!out->notes) {
        return ANALYZER_ERR_ALLOC;
    }
    out->note_count = imported->count;
    for (size_t i = 0; i < imported->count; i++) {
        const NoteEvent *event = imported->events[i];
        const MusicalTime *position = event ? temporal_anchor_position(note_event_temporal_anchor(event)) : NULL;
        const Duration *duration = event ? note_event_duration(event) : NULL;
        const PitchNumericValue *value = imported->pitch_values[i];
        AnalyzerPlaybackNote *note = &out->notes[i];
        Rational note_end;
        if (!position || !duration || !value || rational_of_musical_time(position, &note->onset) != RATIONAL_OK ||
            rational_of_duration(duration, &note->length) != RATIONAL_OK ||
            rational_add(note->onset, note->length, &note_end) != RATIONAL_OK) {
            return ANALYZER_ERR_LIBRARY;
        }
        if (rational_cmp(note_end, out->end) > 0) {
            out->end = note_end;
        }
        /* The MIDI note number, as the Ursatz MIDI renderer reads it. */
        long long den = (long long)pitch_numeric_value_position_denominator(value);
        long long pitch = den > 0 ? pitch_numeric_value_position_numerator(value) / den : 0;
        note->midi = (int)(pitch < 0 ? 0 : pitch > MIDI_MAX ? MIDI_MAX : pitch);
        note->index = i;
        note->part = imported->streams ? imported->streams[i] : 0;
    }
    return ANALYZER_OK;
}

AnalyzerStatus analyzer_playback_build(const ImportedNotes *notes, AnalyzerPlayback *out) {
    if (!out) {
        return ANALYZER_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    if (!notes || (notes->count > 0 && (!notes->events || !notes->pitch_values))) {
        return ANALYZER_ERR_ARGUMENT;
    }
    bool is_midi = notes->source.format == INPUT_FORMAT_MIDI;
    if (is_midi && !notes->tempo_map) {
        return ANALYZER_ERR_ARGUMENT;
    }
    bool defaulted = false;
    AnalyzerStatus status = is_midi ? midi_tempo(notes->tempo_map, &out->tempo, &out->tempo_count, &defaulted)
                                    : score_tempo(&notes->source, &out->tempo, &out->tempo_count, &defaulted);
    if (status == ANALYZER_OK && notes->source.reading_applied) {
        status = scale_tempo(out->tempo, out->tempo_count, notes->source.reading.scale_numerator,
                             notes->source.reading.scale_denominator, is_midi);
    }
    if (status == ANALYZER_OK) {
        status = build_notes(notes, out);
    }
    if (status != ANALYZER_OK) {
        analyzer_playback_destroy(out);
        return status;
    }
    out->tempo_source = defaulted ? "default" : (is_midi ? "file" : "score");
    return ANALYZER_OK;
}

void analyzer_playback_destroy(AnalyzerPlayback *playback) {
    if (!playback) {
        return;
    }
    free(playback->tempo);
    free(playback->notes);
    memset(playback, 0, sizeof(*playback));
}

/* ---- JSON ---- */

static cJSON *rational_json(Rational value) {
    cJSON *array = cJSON_CreateArray();
    if (!array || !json_append(array, cJSON_CreateNumber((double)value.num)) ||
        !json_append(array, cJSON_CreateNumber((double)value.den))) {
        cJSON_Delete(array);
        return NULL;
    }
    return array;
}

static int compare_size_t(const void *left, const void *right) {
    size_t a = *(const size_t *)left;
    size_t b = *(const size_t *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

static bool add_tempo(cJSON *root, const AnalyzerPlayback *playback) {
    cJSON *array = cJSON_CreateArray();
    if (!array) {
        return false;
    }
    for (size_t i = 0; i < playback->tempo_count; i++) {
        cJSON *entry = cJSON_CreateObject();
        if (!entry || !json_append(array, entry) || !json_add(entry, "at", rational_json(playback->tempo[i].at)) ||
            !json_add(entry, "qpm", cJSON_CreateNumber(playback->tempo[i].quarters_per_minute))) {
            cJSON_Delete(array);
            return false;
        }
    }
    return json_add(root, "tempo", array);
}

static bool add_parts(cJSON *root, const AnalyzerPlayback *playback, const ImportedNotes *imported) {
    cJSON *array = cJSON_CreateArray();
    size_t *ids = playback->note_count > 0 ? malloc(playback->note_count * sizeof(size_t)) : NULL;
    if (!array || (playback->note_count > 0 && !ids)) {
        cJSON_Delete(array);
        free(ids);
        return false;
    }
    for (size_t i = 0; i < playback->note_count; i++) {
        ids[i] = playback->notes[i].part;
    }
    if (playback->note_count > 0) {
        qsort(ids, playback->note_count, sizeof(size_t), compare_size_t);
    }
    bool is_midi = imported->source.format == INPUT_FORMAT_MIDI;
    for (size_t i = 0; i < playback->note_count; i++) {
        if (i > 0 && ids[i] == ids[i - 1]) {
            continue;
        }
        size_t index = ids[i];
        const char *name = "";
        if (!is_midi && index < imported->source.part_count && imported->source.parts[index].name) {
            name = imported->source.parts[index].name;
        }
        cJSON *entry = cJSON_CreateObject();
        if (!entry || !json_append(array, entry) || !json_add(entry, "index", cJSON_CreateNumber((double)index)) ||
            !json_add(entry, "name", cJSON_CreateString(name)) || !json_add(entry, "program", cJSON_CreateNumber(0))) {
            cJSON_Delete(array);
            free(ids);
            return false;
        }
    }
    free(ids);
    return json_add(root, "parts", array);
}

static bool add_notes(cJSON *root, const AnalyzerPlayback *playback) {
    cJSON *array = cJSON_CreateArray();
    if (!array) {
        return false;
    }
    for (size_t i = 0; i < playback->note_count; i++) {
        const AnalyzerPlaybackNote *note = &playback->notes[i];
        cJSON *entry = cJSON_CreateObject();
        if (!entry || !json_append(array, entry) || !json_add(entry, "i", cJSON_CreateNumber((double)note->index)) ||
            !json_add(entry, "on", rational_json(note->onset)) || !json_add(entry, "len", rational_json(note->length)) ||
            !json_add(entry, "pitch", cJSON_CreateNumber(note->midi)) ||
            !json_add(entry, "part", cJSON_CreateNumber((double)note->part))) {
            cJSON_Delete(array);
            return false;
        }
    }
    return json_add(root, "notes", array);
}

AnalyzerStatus analyzer_playback_to_json(const AnalyzerPlayback *playback, const ImportedNotes *notes, char **out) {
    if (!out) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    if (!playback || !notes || !playback->tempo_source || playback->note_count != notes->count) {
        return ANALYZER_ERR_ARGUMENT;
    }
    cJSON *root = cJSON_CreateObject();
    bool ok = root && json_add(root, "ok", cJSON_CreateTrue()) &&
              json_add(root, "tempo_source", cJSON_CreateString(playback->tempo_source)) && add_tempo(root, playback) &&
              add_parts(root, playback, notes);
    if (ok) {
        ok = json_add(root, "end", rational_json(playback->end)) && add_notes(root, playback);
    }
    char *text = ok ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    if (!text) {
        return ANALYZER_ERR_ALLOC;
    }
    *out = text;
    return ANALYZER_OK;
}
