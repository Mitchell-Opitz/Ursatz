#include "time/metronome_marking.h"

#include <stdlib.h>
#include <string.h>
#include "core/rational.h"
#include "core/value.h"

struct MetronomeMarking {
    Value *value;
};

typedef struct {
    unsigned long beat_unit_numerator;
    unsigned long beat_unit_denominator;
    unsigned long bpm_numerator;
    unsigned long bpm_denominator;
} MetronomeMarkingContent;

static void reduce(unsigned long *numerator, unsigned long *denominator) {
    unsigned long divisor = (unsigned long)rational_gcd_unsigned(*numerator, *denominator);
    if (divisor > 1) {
        *numerator /= divisor;
        *denominator /= divisor;
    }
}

MetronomeMarkingResult metronome_marking_create(const Duration *beat_unit, unsigned long bpm_numerator,
                                                unsigned long bpm_denominator, MetronomeMarking **out) {
    if (beat_unit == NULL) {
        return METRONOME_MARKING_ERR_NULL_BEAT_UNIT;
    }
    if (duration_numerator(beat_unit) == 0) {
        return METRONOME_MARKING_ERR_ZERO_BEAT_UNIT;
    }
    if (bpm_numerator == 0 || bpm_denominator == 0) {
        return METRONOME_MARKING_ERR_NON_POSITIVE_BPM;
    }

    /* memset first: value_equals compares raw bytes, so struct padding
       must be zeroed. */
    MetronomeMarkingContent content;
    memset(&content, 0, sizeof(content));
    content.beat_unit_numerator = (unsigned long)duration_numerator(beat_unit);
    content.beat_unit_denominator = duration_denominator(beat_unit);
    content.bpm_numerator = bpm_numerator;
    content.bpm_denominator = bpm_denominator;
    reduce(&content.beat_unit_numerator, &content.beat_unit_denominator);
    reduce(&content.bpm_numerator, &content.bpm_denominator);

    MetronomeMarking *marking = (MetronomeMarking *)malloc(sizeof(MetronomeMarking));
    if (marking == NULL) {
        return METRONOME_MARKING_ERR_ALLOC;
    }

    if (value_create("MetronomeMarking", &content, sizeof(content), &marking->value) != VALUE_OK) {
        free(marking);
        return METRONOME_MARKING_ERR_ALLOC;
    }

    *out = marking;
    return METRONOME_MARKING_OK;
}

void metronome_marking_destroy(MetronomeMarking *marking) {
    if (marking == NULL) {
        return;
    }
    value_destroy(marking->value);
    free(marking);
}

static const MetronomeMarkingContent *content_of(const MetronomeMarking *marking) {
    return (const MetronomeMarkingContent *)value_content(marking->value, NULL);
}

unsigned long metronome_marking_beat_unit_numerator(const MetronomeMarking *marking) {
    return content_of(marking)->beat_unit_numerator;
}

unsigned long metronome_marking_beat_unit_denominator(const MetronomeMarking *marking) {
    return content_of(marking)->beat_unit_denominator;
}

unsigned long metronome_marking_bpm_numerator(const MetronomeMarking *marking) {
    return content_of(marking)->bpm_numerator;
}

unsigned long metronome_marking_bpm_denominator(const MetronomeMarking *marking) {
    return content_of(marking)->bpm_denominator;
}

void metronome_marking_seconds_per_beat_unit(const MetronomeMarking *marking, unsigned long *out_numerator,
                                             unsigned long *out_denominator) {
    const MetronomeMarkingContent *content = content_of(marking);
    unsigned long numerator = 60UL * content->bpm_denominator;
    unsigned long denominator = content->bpm_numerator;
    reduce(&numerator, &denominator);
    *out_numerator = numerator;
    *out_denominator = denominator;
}

void metronome_marking_seconds_for_duration(const MetronomeMarking *marking, const Duration *duration,
                                            unsigned long *out_numerator, unsigned long *out_denominator) {
    const MetronomeMarkingContent *content = content_of(marking);

    /* (duration / beat_unit) * (60 / bpm) */
    unsigned long numerator =
        (unsigned long)duration_numerator(duration) * content->beat_unit_denominator * 60UL * content->bpm_denominator;
    unsigned long denominator =
        duration_denominator(duration) * content->beat_unit_numerator * content->bpm_numerator;
    if (numerator == 0) {
        *out_numerator = 0;
        *out_denominator = 1;
        return;
    }
    reduce(&numerator, &denominator);
    *out_numerator = numerator;
    *out_denominator = denominator;
}

bool metronome_marking_equals(const MetronomeMarking *a, const MetronomeMarking *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return value_equals(a->value, b->value);
}
