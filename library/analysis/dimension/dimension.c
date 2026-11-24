#include "analysis/dimension/dimension.h"

#include <stdlib.h>

#include "analysis/notes/contour.h"
#include "analysis/similarity/similarity_scoring.h"

static const Rational ZERO = {0, 1};
static const Rational ONE = {1, 1};

static DimensionResult from_rational(RationalResult result) {
    return result == RATIONAL_OK ? DIMENSION_OK : DIMENSION_ERR_OVERFLOW;
}

static DimensionResult from_similarity(SimilarityScoreResult result) {
    switch (result) {
    case SIMILARITY_SCORE_OK:
        return DIMENSION_OK;
    case SIMILARITY_SCORE_ERR_NULL_ARGUMENT:
        return DIMENSION_ERR_NULL_ARGUMENT;
    case SIMILARITY_SCORE_ERR_INVALID_INPUT:
        return DIMENSION_ERR_INVALID_INPUT;
    case SIMILARITY_SCORE_ERR_ALLOC:
        return DIMENSION_ERR_ALLOC;
    default:
        return DIMENSION_ERR_OVERFLOW;
    }
}

/* A span is valid when it has a note, a positive length and beat, and positive note durations. */
static DimensionResult check_span(const DimensionSpan *span) {
    if (span == NULL) {
        return DIMENSION_ERR_NULL_ARGUMENT;
    }
    if (span->notes == NULL || span->count == 0 || rational_cmp(span->length, ZERO) <= 0 ||
        rational_cmp(span->beat, ZERO) <= 0) {
        return DIMENSION_ERR_INVALID_INPUT;
    }
    for (size_t i = 0; i < span->count; i++) {
        if (rational_cmp(span->notes[i].duration, ZERO) <= 0) {
            return DIMENSION_ERR_INVALID_INPUT;
        }
    }
    return DIMENSION_OK;
}

/* The mean MIDI pitch: the sum over the count, exact. */
static DimensionResult measure_register(const DimensionSpan *span, Rational *out) {
    long long sum = 0;
    for (size_t i = 0; i < span->count; i++) {
        sum += span->notes[i].pitch;
    }
    return from_rational(rational_make(sum, (long long)span->count, out));
}

/* Notes per beat: the count over the span's length in beats. */
static DimensionResult measure_density(const DimensionSpan *span, Rational *out) {
    Rational beats;
    Rational count;
    DimensionResult result = from_rational(rational_div(span->length, span->beat, &beats));
    if (result == DIMENSION_OK) {
        result = from_rational(rational_from_int((long long)span->count, &count));
    }
    return result == DIMENSION_OK ? from_rational(rational_div(count, beats, out)) : result;
}

/*
 * Brightness (R-730): 1 plus, for each of the degrees 2 to 7, the share of its sounding duration in its raised form (the degree sounds as
 * either of two pitch classes above the center: 2: 1 or 2, 3: 3 or 4, 4: 5 or 6, 5: 6 or 7, 6: 8 or 9, 7: 10 or 11; the larger is the raised
 * one) or, when it does not sound, 1 if the span's material has the raised form, 0 if it has only the lowered, 1/2 with no material. A
 * diatonic mode whose notes all sound measures its rank: locrian 1 to lydian 7 (pitch class 6, which is both the raised 4th and the lowered 5th,
 * is the raised 4th when the natural 5th sounds or the natural 4th does not, else the lowered 5th). A span without a center is refused.
 */
static DimensionResult measure_brightness(const DimensionSpan *span, Rational *out) {
    static const int LOWERED[6] = {1, 3, 5, 6, 8, 10};
    if (!span->has_center || span->center < 0 || span->center > 11) {
        return DIMENSION_ERR_INVALID_INPUT;
    }
    /* The sounding duration of each pitch class above the center. */
    Rational length[12];
    for (int pc = 0; pc < 12; pc++) {
        length[pc] = ZERO;
    }
    for (size_t i = 0; i < span->count; i++) {
        int pc = ((span->notes[i].pitch - span->center) % 12 + 12) % 12;
        DimensionResult added = from_rational(rational_add(length[pc], span->notes[i].duration, &length[pc]));
        if (added != DIMENSION_OK) {
            return added;
        }
    }
    /* Pitch class 6 is both the raised 4th and the lowered 5th. It is the raised 4th when the natural 5th sounds (lydian) or the natural
       4th does not; with the natural 4th and no natural 5th it is the lowered 5th (locrian). Each mode then measures its rank. */
    bool six_is_raised_fourth = rational_cmp(length[7], ZERO) > 0 || rational_cmp(length[5], ZERO) <= 0;
    Rational sum = ONE;
    for (int degree = 0; degree < 6; degree++) {
        int lowered = LOWERED[degree];
        int raised = lowered + 1;
        Rational lowered_length = length[lowered];
        Rational raised_length = length[raised];
        if (lowered == 5 && !six_is_raised_fourth) {
            raised_length = ZERO;
        }
        if (lowered == 6 && six_is_raised_fourth) {
            lowered_length = ZERO;
        }
        Rational sounding;
        DimensionResult added = from_rational(rational_add(lowered_length, raised_length, &sounding));
        if (added != DIMENSION_OK) {
            return added;
        }
        Rational share;
        if (rational_cmp(sounding, ZERO) > 0) {
            DimensionResult divided = from_rational(rational_div(raised_length, sounding, &share));
            if (divided != DIMENSION_OK) {
                return divided;
            }
        } else if (span->material == NULL) {
            share = (Rational){1, 2};
        } else {
            bool has_raised = pitch_material_contains(span->material, raised);
            bool has_lowered = pitch_material_contains(span->material, lowered);
            share = has_raised && !has_lowered ? ONE : has_lowered && !has_raised ? ZERO : (Rational){1, 2};
        }
        DimensionResult total = from_rational(rational_add(sum, share, &sum));
        if (total != DIMENSION_OK) {
            return total;
        }
    }
    *out = sum;
    return DIMENSION_OK;
}

/* The name table of the dimensions (target architecture 5.3): a name and the function that measures it. */
static const struct {
    const char *name;
    DimensionResult (*measure)(const DimensionSpan *span, Rational *out);
} MEASURES[DIMENSION_COUNT] = {{"register", measure_register}, {"density", measure_density}, {"brightness", measure_brightness}};

const char *dimension_name(Dimension dimension) {
    return dimension >= 0 && dimension < DIMENSION_COUNT ? MEASURES[dimension].name : NULL;
}

DimensionResult dimension_measure(Dimension dimension, const DimensionSpan *span, Rational *out) {
    if (span == NULL || out == NULL) {
        return DIMENSION_ERR_NULL_ARGUMENT;
    }
    if (dimension < 0 || dimension >= DIMENSION_COUNT) {
        return DIMENSION_ERR_INVALID_INPUT;
    }
    DimensionResult result = check_span(span);
    return result == DIMENSION_OK ? MEASURES[dimension].measure(span, out) : result;
}

/* |a - b| */
static DimensionResult difference(Rational a, Rational b, Rational *out) {
    Rational delta;
    DimensionResult result = from_rational(rational_sub(a, b, &delta));
    if (result == DIMENSION_OK && rational_is_negative(delta)) {
        result = from_rational(rational_neg(delta, &delta));
    }
    if (result == DIMENSION_OK) {
        *out = delta;
    }
    return result;
}

/* 1 - score */
static DimensionResult distance_of(SimilarityScore score, Rational *out) {
    return from_rational(rational_sub(ONE, score, out));
}

static DimensionResult rhythm_distance(const DimensionSpan *a, const DimensionSpan *b, Rational *out) {
    SimilarityDuration *durations = malloc((a->count + b->count) * sizeof(SimilarityDuration));
    if (durations == NULL) {
        return DIMENSION_ERR_ALLOC;
    }
    for (size_t i = 0; i < a->count; i++) {
        durations[i] = a->notes[i].duration;
    }
    for (size_t i = 0; i < b->count; i++) {
        durations[a->count + i] = b->notes[i].duration;
    }
    SimilarityScore score;
    DimensionResult result = from_similarity(similarity_rhythm_score(durations, a->count, durations + a->count, b->count, &score));
    free(durations);
    return result == DIMENSION_OK ? distance_of(score, out) : result;
}

static ContourDirection direction_between(int from, int to) {
    return to > from ? CONTOUR_DIRECTION_UP : to < from ? CONTOUR_DIRECTION_DOWN : CONTOUR_DIRECTION_SAME;
}

static DimensionResult contour_distance(const DimensionSpan *a, const DimensionSpan *b, Rational *out) {
    if (a->count == 1 || b->count == 1) {
        *out = a->count == b->count ? ZERO : ONE;
        return DIMENSION_OK;
    }
    ContourDirection *directions = malloc((a->count + b->count - 2) * sizeof(ContourDirection));
    if (directions == NULL) {
        return DIMENSION_ERR_ALLOC;
    }
    for (size_t i = 0; i + 1 < a->count; i++) {
        directions[i] = direction_between(a->notes[i].pitch, a->notes[i + 1].pitch);
    }
    ContourDirection *second = directions + (a->count - 1);
    for (size_t i = 0; i + 1 < b->count; i++) {
        second[i] = direction_between(b->notes[i].pitch, b->notes[i + 1].pitch);
    }
    SimilarityScore score;
    DimensionResult result = from_similarity(similarity_contour_score(directions, a->count - 1, second, b->count - 1, &score));
    free(directions);
    return result == DIMENSION_OK ? distance_of(score, out) : result;
}

static DimensionResult measure_difference(Dimension dimension, const DimensionSpan *a, const DimensionSpan *b, Rational *out) {
    Rational first;
    Rational second;
    DimensionResult result = dimension_measure(dimension, a, &first);
    if (result == DIMENSION_OK) {
        result = dimension_measure(dimension, b, &second);
    }
    return result == DIMENSION_OK ? difference(first, second, out) : result;
}

DimensionResult dimension_contrast(const DimensionSpan *a, const DimensionSpan *b, ContrastVector *out) {
    if (a == NULL || b == NULL || out == NULL) {
        return DIMENSION_ERR_NULL_ARGUMENT;
    }
    DimensionResult result = check_span(a);
    if (result == DIMENSION_OK) {
        result = check_span(b);
    }
    ContrastVector vector;
    if (result == DIMENSION_OK) {
        result = rhythm_distance(a, b, &vector.distance[CONTRAST_ASPECT_RHYTHM]);
    }
    if (result == DIMENSION_OK) {
        result = contour_distance(a, b, &vector.distance[CONTRAST_ASPECT_CONTOUR]);
    }
    if (result == DIMENSION_OK) {
        result = measure_difference(DIMENSION_REGISTER, a, b, &vector.distance[CONTRAST_ASPECT_REGISTER]);
    }
    if (result == DIMENSION_OK) {
        result = measure_difference(DIMENSION_DENSITY, a, b, &vector.distance[CONTRAST_ASPECT_DENSITY]);
    }
    if (result == DIMENSION_OK) {
        *out = vector;
    }
    return result;
}
