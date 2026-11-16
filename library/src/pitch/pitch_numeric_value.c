#include "pitch/pitch_numeric_value.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/rational.h"
#include "identity/entity_id.h"

struct PitchNumericValue {
    TuningSystem *tuning_system;
    long numerator;
    unsigned long denominator;
};

static unsigned long abs_long(long v) {
    return v < 0 ? (unsigned long)(-v) : (unsigned long)v;
}

static TuningSystemResult tuning_system_copy(const TuningSystem *src, TuningSystem **out) {
    EntityID *id_copy = NULL;
    EntityIDResult id_result = entity_id_create(entity_id_value(tuning_system_id(src)), &id_copy);
    if (id_result != ENTITY_ID_OK) {
        return TUNING_SYSTEM_ERR_ALLOC;
    }

    TuningSystemResult result = tuning_system_create(id_copy, tuning_system_name(src), out);
    if (result != TUNING_SYSTEM_OK) {
        entity_id_destroy(id_copy);
    }
    return result;
}

/* Reduces numerator/denominator to lowest terms in place. denominator must already be positive. */
static void reduce(long *numerator, unsigned long *denominator) {
    unsigned long divisor = rational_gcd_unsigned(abs_long(*numerator), *denominator);
    if (divisor == 0) {
        divisor = 1;
    }
    *numerator = *numerator / (long)divisor;
    *denominator = *denominator / divisor;
}

PitchNumericValueResult pitch_numeric_value_create(const TuningSystem *tuning_system, long position_numerator,
                                                    unsigned long position_denominator, PitchNumericValue **out) {
    if (tuning_system == NULL) {
        return PITCH_NUMERIC_VALUE_ERR_NULL_TUNING_SYSTEM;
    }
    if (position_denominator == 0) {
        return PITCH_NUMERIC_VALUE_ERR_NON_POSITIVE_DENOMINATOR;
    }

    TuningSystem *tuning_system_copy_out = NULL;
    if (tuning_system_copy(tuning_system, &tuning_system_copy_out) != TUNING_SYSTEM_OK) {
        return PITCH_NUMERIC_VALUE_ERR_ALLOC;
    }

    PitchNumericValue *value = (PitchNumericValue *)malloc(sizeof(PitchNumericValue));
    if (value == NULL) {
        tuning_system_destroy(tuning_system_copy_out);
        return PITCH_NUMERIC_VALUE_ERR_ALLOC;
    }

    value->tuning_system = tuning_system_copy_out;
    value->numerator = position_numerator;
    value->denominator = position_denominator;
    reduce(&value->numerator, &value->denominator);

    *out = value;
    return PITCH_NUMERIC_VALUE_OK;
}

void pitch_numeric_value_destroy(PitchNumericValue *value) {
    if (value == NULL) {
        return;
    }
    tuning_system_destroy(value->tuning_system);
    free(value);
}

const TuningSystem *pitch_numeric_value_tuning_system(const PitchNumericValue *value) {
    return value->tuning_system;
}

long pitch_numeric_value_position_numerator(const PitchNumericValue *value) {
    return value->numerator;
}

unsigned long pitch_numeric_value_position_denominator(const PitchNumericValue *value) {
    return value->denominator;
}

PitchNumericValueToIdentityResult pitch_numeric_value_to_identity(const PitchNumericValue *value,
                                                                    PitchIdentity **out) {
    if (value == NULL) {
        return PITCH_NUMERIC_VALUE_TO_IDENTITY_ERR_NULL_VALUE;
    }

    char buffer[256];
    int written = snprintf(buffer, sizeof(buffer), "PitchNumericValue:v1:%s:%ld:%lu",
                            entity_id_value(tuning_system_id(value->tuning_system)), value->numerator,
                            value->denominator);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return PITCH_NUMERIC_VALUE_TO_IDENTITY_ERR_ALLOC;
    }

    if (pitch_identity_create(buffer, (size_t)written + 1, out) != PITCH_IDENTITY_OK) {
        return PITCH_NUMERIC_VALUE_TO_IDENTITY_ERR_ALLOC;
    }
    return PITCH_NUMERIC_VALUE_TO_IDENTITY_OK;
}