#include "chain/chain_inputs_internal.h"

#include <stdlib.h>

static void segment_line_release(OuterVoiceEntry *entries, size_t count) {
    for (size_t i = 0; i < count; i++) {
        entity_id_destroy(entries[i].note_id);
        pitch_numeric_value_destroy(entries[i].pitch);
        time_span_destroy(entries[i].span);
    }
    free(entries);
}

static TimeSpan *span_copy(const TimeSpan *source) {
    TimeSpan *copy = NULL;
    return time_span_create(time_span_start(source), time_span_end(source), time_span_boundary(source), &copy) == TIME_SPAN_OK ? copy : NULL;
}

/* A copy of `source` (the voice segment's entry) as the entry of chord segment `index`, or an absent entry over `window` with no source. */
static bool segment_line_entry(const OuterVoiceEntry *source, size_t index, const TimeSpan *window, OuterVoiceEntry *out) {
    *out = (OuterVoiceEntry){index, false, false, false, NULL, NULL, NULL};
    if (source == NULL) {
        out->span = span_copy(window);
        return out->span != NULL;
    }
    out->present = source->present;
    out->bridged = source->bridged;
    out->guarded = source->guarded;
    out->span = span_copy(source->span);
    if (out->span == NULL) {
        return false;
    }
    if (source->present) {
        if (entity_id_create(entity_id_value(source->note_id), &out->note_id) != ENTITY_ID_OK ||
            pitch_numeric_value_create(pitch_numeric_value_tuning_system(source->pitch), pitch_numeric_value_position_numerator(source->pitch),
                                       pitch_numeric_value_position_denominator(source->pitch), &out->pitch) != PITCH_NUMERIC_VALUE_OK) {
            return false;
        }
    }
    return true;
}

ChainInputStatus chain_build_segment_lines(const ChainSegments *voice_segments, const OuterVoiceLines *lines, const ChainSegments *segments,
                                           size_t count, OuterVoiceLines *out) {
    if (voice_segments == NULL || lines == NULL || segments == NULL || out == NULL || count > segments->count ||
        lines->count != voice_segments->count) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    OuterVoiceEntry *soprano = calloc(count > 0 ? count : 1, sizeof(OuterVoiceEntry));
    OuterVoiceEntry *bass = calloc(count > 0 ? count : 1, sizeof(OuterVoiceEntry));
    if (soprano == NULL || bass == NULL) {
        free(soprano);
        free(bass);
        return CHAIN_INPUT_ERR_ALLOC;
    }
    size_t voice = 0; /* the next voice segment that starts after the chord segment */
    for (size_t i = 0; i < count; i++) {
        while (voice < voice_segments->count && voice_segments->starts[voice] <= segments->starts[i] + 1e-12) {
            voice++;
        }
        bool has = voice > 0;
        const TimeSpan *window = sonority_time_window(segments->sonorities.items[i].sonority);
        if (!segment_line_entry(has ? &lines->soprano[voice - 1] : NULL, i, window, &soprano[i]) ||
            !segment_line_entry(has ? &lines->bass[voice - 1] : NULL, i, window, &bass[i])) {
            segment_line_release(soprano, i + 1);
            segment_line_release(bass, i + 1);
            return CHAIN_INPUT_ERR_ALLOC;
        }
    }
    out->soprano = soprano;
    out->bass = bass;
    out->count = count;
    return CHAIN_INPUT_OK;
}
