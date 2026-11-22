#include "analysis/dimension/dimension.h"

static const Rational ZERO = {0, 1};

static DimensionResult from_rational(RationalResult result) {
    return result == RATIONAL_OK ? DIMENSION_OK : DIMENSION_ERR_OVERFLOW;
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

/* The name table of the dimensions (target architecture 5.3): a name and the function that measures it. */
static const struct {
    const char *name;
    DimensionResult (*measure)(const DimensionSpan *span, Rational *out);
} MEASURES[DIMENSION_COUNT] = {{"register", measure_register}, {"density", measure_density}};

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
