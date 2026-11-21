#include "analysis/harmony/analysis_input.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/harmony/analysis_six_four.h"
#include "theory/tonality/seventh_rules.h"
#include "theory/tonality/triad_rules.h"

typedef struct {
    StandardTonalityTriadQuality triad;            /* the quality, when the chord has three tones */
    StandardTonalitySeventhQuality seventh;        /* the quality, when it has four */
    size_t count;
    HarmonyAppliedQuality applied_quality; /* when the set is a quality an applied chord can have */
    bool applied_possible;
} Shape;

/* The intervals of each shape are the owner's (triad_rules.h, seventh_rules.h); only the map to an applied quality is here. */
static const Shape SHAPES[] = {
    {STANDARD_TONALITY_TRIAD_MAJOR, STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR, 3, HARMONY_APPLIED_MAJOR_TRIAD, true},
    {STANDARD_TONALITY_TRIAD_MINOR, STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR, 3, HARMONY_APPLIED_MAJOR_TRIAD, false},
    {STANDARD_TONALITY_TRIAD_DIMINISHED, STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR, 3, HARMONY_APPLIED_DIMINISHED_TRIAD, true},
    {STANDARD_TONALITY_TRIAD_AUGMENTED, STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR, 3, HARMONY_APPLIED_MAJOR_TRIAD, false},
    {STANDARD_TONALITY_TRIAD_MAJOR, STANDARD_TONALITY_SEVENTH_MAJOR_MINOR, 4, HARMONY_APPLIED_DOMINANT_SEVENTH, true},
    {STANDARD_TONALITY_TRIAD_MAJOR, STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR, 4, HARMONY_APPLIED_DOMINANT_SEVENTH, false},
    {STANDARD_TONALITY_TRIAD_MAJOR, STANDARD_TONALITY_SEVENTH_MINOR_MINOR, 4, HARMONY_APPLIED_DOMINANT_SEVENTH, false},
    {STANDARD_TONALITY_TRIAD_MAJOR, STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED, 4, HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH,
     true},
    {STANDARD_TONALITY_TRIAD_MAJOR, STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED, 4,
     HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, true}};

/* The shape's intervals above the root. */
static bool shape_intervals(const Shape *shape, int out[HARMONY_SEGMENT_MAX_INTERVALS]) {
    return shape->count == 3 ? standard_tonality_triad_quality_semitones(shape->triad, out)
                             : standard_tonality_seventh_quality_semitones(shape->seventh, out);
}

static const Shape *shape_of(const HarmonySegmentInput *input) {
    if (input->interval_count != 3 && input->interval_count != 4) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(SHAPES) / sizeof(SHAPES[0]); i++) {
        int intervals[HARMONY_SEGMENT_MAX_INTERVALS];
        if (SHAPES[i].count == input->interval_count && shape_intervals(&SHAPES[i], intervals) &&
            memcmp(intervals, input->intervals, SHAPES[i].count * sizeof(int)) == 0) {
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
        if (input->segment.id == NULL || input->segment.id[0] == '\0' || input->key_ref == NULL ||
            input->key_ref[0] == '\0' || input->segment.position == NULL ||
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
        if (shape == NULL || !harmony_mode_valid(input->mode) || !beat_strength_valid(input->segment.strength) ||
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
    harmony_analysis_mark_cadential(segments, count);
    *out = segments;
    return HARMONY_ANALYSIS_OK;
}

void harmony_analysis_destroy(HarmonySegment *segments) {
    free(segments);
}
