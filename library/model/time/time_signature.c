#include "model/time/time_signature.h"

#include <stdlib.h>

/* Room for the default groups of the largest top number (one unit per group). */
#define GROUP_CAPACITY TIME_SIGNATURE_MAX_TOP_NUMBER

struct TimeSignature {
    unsigned top_number;
    int bottom_unit_exponent;
    size_t group_count;
    unsigned char groups[GROUP_CAPACITY];
};

static TimeSignatureResult check_bottom(const NoteValue *bottom_unit, int *exponent) {
    if (bottom_unit == NULL) {
        return TIME_SIGNATURE_ERR_NULL_BOTTOM_UNIT;
    }
    if (note_value_dots(bottom_unit) != 0) {
        return TIME_SIGNATURE_ERR_BOTTOM_UNIT_DOTTED;
    }
    *exponent = note_value_exponent(bottom_unit);
    if (*exponent < TIME_SIGNATURE_MIN_BOTTOM_UNIT_EXPONENT || *exponent > TIME_SIGNATURE_MAX_BOTTOM_UNIT_EXPONENT) {
        return TIME_SIGNATURE_ERR_BOTTOM_UNIT_OUT_OF_RANGE;
    }
    return TIME_SIGNATURE_OK;
}

TimeSignatureResult time_signature_create(unsigned top_number, const NoteValue *bottom_unit, TimeSignature **out) {
    int exponent = 0;
    TimeSignatureResult checked = check_bottom(bottom_unit, &exponent);
    if (checked != TIME_SIGNATURE_OK) {
        return checked;
    }
    if (top_number < 1 || top_number > TIME_SIGNATURE_MAX_TOP_NUMBER) {
        return TIME_SIGNATURE_ERR_TOP_NUMBER_OUT_OF_RANGE;
    }

    TimeSignature *time_signature = (TimeSignature *)calloc(1, sizeof(TimeSignature));
    if (time_signature == NULL) {
        return TIME_SIGNATURE_ERR_ALLOC;
    }
    time_signature->top_number = top_number;
    time_signature->bottom_unit_exponent = exponent;
    /* the default groups: threes above 3 when 3 divides the top, else one unit each */
    unsigned size = top_number > 3 && top_number % 3 == 0 ? 3 : 1;
    time_signature->group_count = top_number / size;
    for (size_t i = 0; i < time_signature->group_count; i++) {
        time_signature->groups[i] = (unsigned char)size;
    }

    *out = time_signature;
    return TIME_SIGNATURE_OK;
}

TimeSignatureResult time_signature_create_grouped(const unsigned *groups, size_t group_count, const NoteValue *bottom_unit,
                                                  TimeSignature **out) {
    int exponent = 0;
    TimeSignatureResult checked = check_bottom(bottom_unit, &exponent);
    if (checked != TIME_SIGNATURE_OK) {
        return checked;
    }
    if (groups == NULL || group_count < 1 || group_count > TIME_SIGNATURE_MAX_GROUPS) {
        return TIME_SIGNATURE_ERR_GROUPS_OUT_OF_RANGE;
    }
    unsigned top_number = 0;
    for (size_t i = 0; i < group_count; i++) {
        if (groups[i] < 1 || groups[i] > TIME_SIGNATURE_MAX_GROUP_UNITS) {
            return TIME_SIGNATURE_ERR_GROUPS_OUT_OF_RANGE;
        }
        top_number += groups[i];
    }
    if (top_number > TIME_SIGNATURE_MAX_TOP_NUMBER) {
        return TIME_SIGNATURE_ERR_TOP_NUMBER_OUT_OF_RANGE;
    }

    TimeSignature *time_signature = (TimeSignature *)calloc(1, sizeof(TimeSignature));
    if (time_signature == NULL) {
        return TIME_SIGNATURE_ERR_ALLOC;
    }
    time_signature->top_number = top_number;
    time_signature->bottom_unit_exponent = exponent;
    time_signature->group_count = group_count;
    for (size_t i = 0; i < group_count; i++) {
        time_signature->groups[i] = (unsigned char)groups[i];
    }

    *out = time_signature;
    return TIME_SIGNATURE_OK;
}

TimeSignatureResult time_signature_copy(const TimeSignature *source, TimeSignature **out) {
    if (source == NULL) {
        return TIME_SIGNATURE_ERR_NULL_SOURCE;
    }
    TimeSignature *copy = (TimeSignature *)malloc(sizeof(TimeSignature));
    if (copy == NULL) {
        return TIME_SIGNATURE_ERR_ALLOC;
    }
    *copy = *source;
    *out = copy;
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

DurationResult time_signature_unit_duration(const TimeSignature *time_signature, Duration *out) {
    return duration_make(1, 1LL << time_signature->bottom_unit_exponent, out);
}

DurationResult time_signature_measure_duration(const TimeSignature *time_signature, Duration *out) {
    return duration_make((long long)time_signature->top_number, 1LL << time_signature->bottom_unit_exponent, out);
}

size_t time_signature_group_count(const TimeSignature *time_signature) {
    return time_signature->group_count;
}

unsigned time_signature_group_at(const TimeSignature *time_signature, size_t index) {
    return index < time_signature->group_count ? time_signature->groups[index] : 0;
}

/* The units in every group when all groups are the same size, else 0. */
static unsigned equal_group_units(const TimeSignature *time_signature) {
    unsigned first = time_signature->groups[0];
    for (size_t i = 1; i < time_signature->group_count; i++) {
        if (time_signature->groups[i] != first) {
            return 0;
        }
    }
    return first;
}

bool time_signature_is_compound(const TimeSignature *time_signature) {
    return time_signature->group_count >= 2 && equal_group_units(time_signature) == 3;
}

DurationResult time_signature_beat_length(const TimeSignature *time_signature, Duration *out) {
    unsigned units = equal_group_units(time_signature);
    return duration_make(units > 0 ? (long long)units : 1, 1LL << time_signature->bottom_unit_exponent, out);
}

bool time_signature_equals(const TimeSignature *a, const TimeSignature *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    if (a->top_number != b->top_number || a->bottom_unit_exponent != b->bottom_unit_exponent ||
        a->group_count != b->group_count) {
        return false;
    }
    for (size_t i = 0; i < a->group_count; i++) {
        if (a->groups[i] != b->groups[i]) {
            return false;
        }
    }
    return true;
}
