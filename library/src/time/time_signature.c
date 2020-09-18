#include "time/time_signature.h"

#include <stdlib.h>

struct TimeSignature {
    unsigned top_number;
    int bottom_unit_exponent;
};

TimeSignatureResult time_signature_create(unsigned top_number, const NoteValue *bottom_unit, TimeSignature **out) {
    if (bottom_unit == NULL) {
        return TIME_SIGNATURE_ERR_NULL_BOTTOM_UNIT;
    }
    if (top_number < 1 || top_number > TIME_SIGNATURE_MAX_TOP_NUMBER) {
        return TIME_SIGNATURE_ERR_TOP_NUMBER_OUT_OF_RANGE;
    }
    if (note_value_dots(bottom_unit) != 0) {
        return TIME_SIGNATURE_ERR_BOTTOM_UNIT_DOTTED;
    }
    int exponent = note_value_exponent(bottom_unit);
    if (exponent < TIME_SIGNATURE_MIN_BOTTOM_UNIT_EXPONENT || exponent > TIME_SIGNATURE_MAX_BOTTOM_UNIT_EXPONENT) {
        return TIME_SIGNATURE_ERR_BOTTOM_UNIT_OUT_OF_RANGE;
    }

    TimeSignature *time_signature = (TimeSignature *)malloc(sizeof(TimeSignature));
    if (time_signature == NULL) {
        return TIME_SIGNATURE_ERR_ALLOC;
    }
    time_signature->top_number = top_number;
    time_signature->bottom_unit_exponent = exponent;

    *out = time_signature;
    return TIME_SIGNATURE_OK;
}

void time_signature_destroy(TimeSignature *time_signature) {
    free(time_signature);
}

unsigned time_signature_top_number(const TimeSignature *time_signature) {
    return time_signature->top_number;
}

int time_signature_bottom_unit_exponent(const TimeSignature *time_signature) {
    return time_signature->bottom_unit_exponent;
}

unsigned time_signature_bottom_number(const TimeSignature *time_signature) {
    return 1u << time_signature->bottom_unit_exponent;
}

DurationResult time_signature_unit_duration(const TimeSignature *time_signature, Duration **out) {
    return duration_create(1, 1UL << time_signature->bottom_unit_exponent, out);
}

DurationResult time_signature_measure_duration(const TimeSignature *time_signature, Duration **out) {
    return duration_create((long)time_signature->top_number, 1UL << time_signature->bottom_unit_exponent, out);
}

bool time_signature_equals(const TimeSignature *a, const TimeSignature *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->top_number == b->top_number && a->bottom_unit_exponent == b->bottom_unit_exponent;
}
