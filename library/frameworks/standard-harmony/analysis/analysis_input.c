#include "analysis_input.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int intervals[HARMONY_SEGMENT_MAX_INTERVALS];
    size_t count;
    HarmonyAppliedQuality applied_quality; /* when the set is a quality an applied chord can have */
    bool applied_possible;
} Shape;

static const Shape SHAPES[] = {
    {{0, 4, 7, 0}, 3, HARMONY_APPLIED_MAJOR_TRIAD, true},
    {{0, 3, 7, 0}, 3, HARMONY_APPLIED_MAJOR_TRIAD, false},
    {{0, 3, 6, 0}, 3, HARMONY_APPLIED_DIMINISHED_TRIAD, true},
    {{0, 4, 8, 0}, 3, HARMONY_APPLIED_MAJOR_TRIAD, false},
    {{0, 4, 7, 10}, 4, HARMONY_APPLIED_DOMINANT_SEVENTH, true},
    {{0, 4, 7, 11}, 4, HARMONY_APPLIED_DOMINANT_SEVENTH, false},
    {{0, 3, 7, 10}, 4, HARMONY_APPLIED_DOMINANT_SEVENTH, false},
    {{0, 3, 6, 10}, 4, HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH, true},
    {{0, 3, 6, 9}, 4, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, true}};

static const Shape *shape_of(const HarmonySegmentInput *input) {
    if (input->interval_count != 3 && input->interval_count != 4) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(SHAPES) / sizeof(SHAPES[0]); i++) {
        if (SHAPES[i].count == input->interval_count &&
            memcmp(SHAPES[i].intervals, input->intervals, SHAPES[i].count * sizeof(int)) == 0) {
            return &SHAPES[i];
        }
    }
    return NULL;
}

static bool is_diminished(const HarmonySegmentInput *input) {
    return input->intervals[1] == 3 && input->intervals[2] == 6;
}

static void read_diatonic(HarmonySegment *segment, const Shape *shape) {
    const HarmonySegmentInput *input = segment->input;
    HarmonyChord chord;
    memset(&chord, 0, sizeof(chord));
    chord.degree = input->degree;
    chord.inversion = input->inversion;
    chord.seventh = shape->count == 4;
    /* In a minor key the vii is the diminished chord on the raised seventh; a major chord on 7 is the subtonic VII. */
    chord.leading_tone = input->mode == HARMONY_MODE_MINOR && input->degree == 7 && is_diminished(input);
    chord.half_diminished = chord.seventh && input->degree == 7 && input->intervals[1] == 3 &&
                            input->intervals[2] == 6 && input->intervals[3] == 10;
    if (harmony_chord_valid(&chord)) {
        segment->chord = chord;
        segment->valid = true;
    }
}

static void read_applied(HarmonySegment *segment, const Shape *shape) {
    const HarmonySegmentInput *input = segment->input;
    if (!shape->applied_possible) {
        return;
    }
    HarmonyAppliedChord applied;
    memset(&applied, 0, sizeof(applied));
    applied.quality = shape->applied_quality;
    applied.target = input->applied_to;
    /* V and V7 are on the fifth of the temporary key, the leading-tone chords on its seventh. */
    bool leading_tone = shape->applied_quality == HARMONY_APPLIED_DIMINISHED_TRIAD ||
                        shape->applied_quality == HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH ||
                        shape->applied_quality == HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH;
    if ((leading_tone && input->degree != 7) || (!leading_tone && input->degree != 5)) {
        return;
    }
    if (!harmony_applied_quality_kind(applied.quality, &applied.kind) ||
        !harmony_applied_valid(input->mode, &applied)) {
        return;
    }
    segment->applied = true;
    segment->applied_chord = applied;
    segment->valid = true;
}

HarmonyAnalysisStatus harmony_analysis_prepare(const HarmonySegmentInput *inputs, size_t count,
                                               HarmonySegment **out) {
    if (out == NULL || (inputs == NULL && count > 0)) {
        return HARMONY_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    for (size_t i = 0; i < count; i++) {
        const HarmonySegmentInput *input = &inputs[i];
        if (input->segment_id == NULL || input->segment_id[0] == '\0' || input->key_ref == NULL ||
            input->key_ref[0] == '\0' || input->span == NULL || input->position == NULL ||
            !(input->uncertainty >= 0.0 && input->uncertainty <= 1.0)) {
            return HARMONY_ANALYSIS_ERR_BAD_INPUT;
        }
    }
    HarmonySegment *segments = (HarmonySegment *)calloc(count > 0 ? count : 1, sizeof(HarmonySegment));
    if (segments == NULL) {
        return HARMONY_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        const HarmonySegmentInput *input = &inputs[i];
        HarmonySegment *segment = &segments[i];
        segment->input = input;
        const Shape *shape = shape_of(input);
        if (shape == NULL || !harmony_mode_valid(input->mode) || !beat_strength_valid(input->beat_strength) ||
            input->degree < 1 || input->degree > 7 || input->inversion < 0 ||
            (size_t)input->inversion >= input->interval_count || input->applied_to < 0 || input->applied_to > 7) {
            continue;
        }
        if (input->applied_to == 0) {
            read_diatonic(segment, shape);
        } else {
            read_applied(segment, shape);
        }
    }
    *out = segments;
    return HARMONY_ANALYSIS_OK;
}

void harmony_analysis_destroy(HarmonySegment *segments) {
    free(segments);
}
