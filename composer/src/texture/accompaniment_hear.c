#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-texture/analysis/texture_analysis_classify.h>
#include <standard-texture/analysis/texture_analysis_input.h>
#include <standard-texture/texture_rules.h>

#include <identity/entity_id.h>
#include <time/meter_map.h>
#include <time/musical_time.h>
#include <time/note_value.h>
#include <time/time_signature.h>
#include <time/time_span.h>

#include "texture/texture_internal.h"

/* The tracks of the input: one for each of the parts, as they would be separated from a score. */
#define TRACK_MELODY 0
#define TRACK_BASS 1
#define TRACK_ACCOMPANIMENT 2

typedef struct {
    MeterMap *map;
    StandardTextureInput input;
    TimeSpan **spans;
    size_t span_count;
    size_t span_capacity;
    unsigned next_id;
} Hearing;

static void hearing_destroy(Hearing *hearing) {
    standard_texture_input_destroy(&hearing->input);
    for (size_t i = 0; i < hearing->span_count; i++) {
        time_span_destroy(hearing->spans[i]);
    }
    free(hearing->spans);
    meter_map_destroy(hearing->map);
}

/* The meter map of one signature from time 0. */
static bool meter_map_of(const PlanMeter *meter, MeterMap **out) {
    NoteValue *unit = NULL;
    TimeSignature *signature = NULL;
    MusicalTime *zero = NULL;
    EntityID *id = NULL;
    int exponent = 0;
    bool ok = false;
    for (int beat = meter->beat_unit; beat > 1; beat /= 2) {
        exponent++;
    }
    if (meter->beat_unit != (1 << exponent) || meter->beats_per_bar <= 0) {
        return false;
    }
    if (note_value_create(exponent, &unit) == NOTE_VALUE_OK &&
        time_signature_create((unsigned)meter->beats_per_bar, unit, &signature) == TIME_SIGNATURE_OK &&
        musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK && entity_id_create("composer-accompaniment-meter", &id) == ENTITY_ID_OK) {
        MeterMapChangePoint point = {signature, zero};
        ok = meter_map_create(id, &point, 1, out) == METER_MAP_OK;
        if (!ok) {
            entity_id_destroy(id);
        }
    }
    note_value_destroy(unit);
    time_signature_destroy(signature);
    musical_time_destroy(zero);
    return ok;
}

/* A note of a track and voice from onset to end ticks of the target, which starts phase ticks into a bar. */
static bool add_note(Hearing *hearing, const AccompContext *context, size_t track, size_t voice, long onset, long end,
                     int pitch) {
    MusicalTime *start = NULL;
    MusicalTime *stop = NULL;
    TimeSpan *span = NULL;
    char id[STANDARD_TEXTURE_ID_MAX];
    bool ok = false;
    if (hearing->span_count == hearing->span_capacity) {
        size_t capacity = hearing->span_capacity ? hearing->span_capacity * 2 : 64;
        TimeSpan **spans = realloc(hearing->spans, capacity * sizeof(*spans));
        if (!spans) {
            return false;
        }
        hearing->spans = spans;
        hearing->span_capacity = capacity;
    }
    if (musical_time_create(context->phase + onset, MELODY_TICKS, &start) == MUSICAL_TIME_OK &&
        musical_time_create(context->phase + end, MELODY_TICKS, &stop) == MUSICAL_TIME_OK &&
        time_span_create(start, stop, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK) {
        snprintf(id, sizeof(id), "n%u", hearing->next_id++);
        hearing->spans[hearing->span_count++] = span;
        ok = standard_texture_input_add_note(&hearing->input, id, span, pitch, track, voice, 1.0) ==
             STANDARD_TEXTURE_ANALYSIS_OK;
    }
    musical_time_destroy(start);
    musical_time_destroy(stop);
    return ok;
}

/* The texture the target asks for, or its category, is the texture a window was heard as. */
static bool window_is(int heard, int asked) {
    StandardTexture category;
    return heard >= 0 && (heard == asked || (standard_texture_category((StandardTexture)heard, &category) &&
                                              (int)category == asked));
}

bool accomp_hear(const AccompContext *context, const AccompCandidate *candidate, bool *heard) {
    Hearing hearing;
    StandardTextureAnalysis analysis;
    bool ok = context->texture >= 0 && context->texture < (int)STANDARD_TEXTURE_COUNT;
    memset(&hearing, 0, sizeof(hearing));
    memset(&analysis, 0, sizeof(analysis));
    if (!ok || !meter_map_of(&context->meter, &hearing.map)) {
        return false;
    }
    ok = standard_texture_input_init(&hearing.input, hearing.map) == STANDARD_TEXTURE_ANALYSIS_OK;
    for (size_t i = 0; ok && i < context->melody_count; i++) {
        ok = add_note(&hearing, context, TRACK_MELODY, 0, context->melody[i].onset, context->melody[i].end,
                      context->melody[i].pitch);
    }
    for (size_t i = 0; ok && i < context->bass_count; i++) {
        ok = add_note(&hearing, context, TRACK_BASS, 0, context->bass[i].onset, context->bass[i].end, context->bass[i].pitch);
    }
    for (size_t voice = 0; ok && voice < ACCOMP_VOICES; voice++) {
        for (size_t n = 0; ok && n < candidate->note_count[voice]; n++) {
            const AccompNote *note = &candidate->notes[voice][n];
            ok = add_note(&hearing, context, TRACK_ACCOMPANIMENT, voice, note->onset, note->onset + note->duration,
                          note->pitch);
        }
    }
    ok = ok && standard_texture_analyze(&hearing.input, NULL, &analysis) == STANDARD_TEXTURE_ANALYSIS_OK;
    if (ok) {
        *heard = analysis.windows.count > 0;
        for (size_t w = 0; w < analysis.windows.count; w++) {
            *heard = *heard && window_is(analysis.window_texture[w], context->texture);
        }
    }
    standard_texture_analysis_destroy(&analysis);
    hearing_destroy(&hearing);
    return ok;
}
