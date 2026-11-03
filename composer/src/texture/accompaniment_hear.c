#include <stdlib.h>
#include <string.h>

#include <standard-texture/analysis/texture_analysis_classify.h>
#include <standard-texture/texture_rules.h>

#include <identity/entity_id.h>
#include <time/meter_map.h>
#include <time/musical_time.h>
#include <time/note_value.h>
#include <time/time_signature.h>
#include <time/time_rational.h>

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/composer_api.h>
#include <ursatz_chain/analysis_chain.h>

#include "render/render_notes.h"
#include "texture/texture_internal.h"
#include "util/position.h"

/*
 * The notes of the melody, the bass and the candidate are heard by the Analyzer's texture stage (analysis_chain_texture),
 * as three streams: the melody the first, the bass the second, the accompaniment the third, whose voices the library
 * separates itself. Only the texture stage runs: it reads the notes alone.
 */

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

/* The texture the target asks for, or its category, is the texture a window was heard as. */
static bool window_is(int heard, int asked) {
    StandardTexture category;
    return heard >= 0 && (heard == asked || (standard_texture_category((StandardTexture)heard, &category) &&
                                              (int)category == asked));
}

/* Adds a note of a lane, from onset to end of the target, which starts phase into a bar. */
static bool add_note(RenderNote *notes, size_t *count, const AccompContext *context, PlanLaneKind lane, int voice,
                     Rational onset, Rational end, int pitch) {
    RenderNote *note = &notes[(*count)++];
    note->onset = position_add(context->phase, onset);
    note->duration = position_sub(end, onset);
    note->midi = pitch;
    note->lane = lane;
    note->voice = voice;
    return rational_cmp(note->duration, (Rational){0, 1}) > 0;
}

bool accomp_hear(const AccompContext *context, const AccompCandidate *candidate, bool *heard) {
    bool ok = context->texture >= 0 && context->texture < (int)STANDARD_TEXTURE_COUNT;
    if (!ok) {
        return false;
    }
    size_t capacity = context->melody_count + context->bass_count;
    for (size_t voice = 0; voice < ACCOMP_VOICES; voice++) {
        capacity += candidate->note_count[voice];
    }
    RenderNote *notes = calloc(capacity ? capacity : 1, sizeof(*notes));
    MeterMap *map = NULL;
    RenderedPlan rendered;
    AnalyzerResult *result = NULL;
    int *midi = NULL;
    size_t count = 0;
    memset(&rendered, 0, sizeof(rendered));
    ok = notes && meter_map_of(&context->meter, &map);
    for (size_t i = 0; ok && i < context->melody_count; i++) {
        ok = add_note(notes, &count, context, PLAN_LANE_MELODY, 0, context->melody[i].onset, context->melody[i].end,
                      context->melody[i].pitch);
    }
    for (size_t i = 0; ok && i < context->bass_count; i++) {
        ok = add_note(notes, &count, context, PLAN_LANE_BASS, 0, context->bass[i].onset, context->bass[i].end,
                      context->bass[i].pitch);
    }
    for (size_t voice = 0; ok && voice < ACCOMP_VOICES; voice++) {
        for (size_t n = 0; ok && n < candidate->note_count[voice]; n++) {
            const AccompNote *note = &candidate->notes[voice][n];
            ok = add_note(notes, &count, context, PLAN_LANE_INNER_VOICE, 2, note->onset,
                          position_add(note->onset, note->duration), note->pitch);
        }
    }
    ok = ok && count > 0 && render_notes(notes, count, &context->key, &rendered) == PLAN_RENDER_OK;
    midi = ok ? calloc(rendered.count, sizeof(*midi)) : NULL;
    ok = ok && midi;
    for (size_t i = 0; ok && i < rendered.count; i++) {
        midi[i] = (int)pitch_numeric_value_position_numerator(rendered.pitch_values[i]);
    }
    if (ok) {
        AnalyzerHearing input;
        memset(&input, 0, sizeof(input));
        input.events = (const NoteEvent *const *)rendered.events;
        input.count = rendered.count;
        input.midi = midi;
        input.meter = map;
        input.streams = rendered.part_index;
        AnalyzerHearingOptions options = analyzer_hearing_options_default();
        options.analysis.stages = ANALYSIS_CHAIN_STAGE_TEXTURE;
        ok = analyzer_hear(&input, &options, &result) == ANALYZER_OK;
    }
    const StandardTextureAnalysis *analysis = ok ? analysis_chain_texture(analyzer_result_chain(result)) : NULL;
    ok = ok && analysis;
    if (ok) {
        *heard = analysis->windows.count > 0;
        for (size_t w = 0; w < analysis->windows.count; w++) {
            *heard = *heard && window_is(analysis->window_texture[w], context->texture);
        }
    }
    analyzer_result_destroy(result);
    free(midi);
    rendered_plan_destroy(&rendered);
    meter_map_destroy(map);
    free(notes);
    return ok;
}
