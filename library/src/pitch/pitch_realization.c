#include "pitch/pitch_realization.h"

#include <stdlib.h>

#include "core/rational.h"
#include "identity/entity_id.h"

struct PitchRealization {
    TuningSystem *tuning_system;
    long numerator;
    unsigned long denominator;
};

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

PitchRealizationResult pitch_realization_create(const TuningSystem *tuning_system, long ratio_numerator,
                                                 unsigned long ratio_denominator, PitchRealization **out) {
    if (tuning_system == NULL) {
        return PITCH_REALIZATION_ERR_NULL_TUNING_SYSTEM;
    }
    if (ratio_numerator <= 0) {
        return PITCH_REALIZATION_ERR_NON_POSITIVE_NUMERATOR;
    }
    if (ratio_denominator == 0) {
        return PITCH_REALIZATION_ERR_NON_POSITIVE_DENOMINATOR;
    }

    TuningSystem *tuning_system_copy_out = NULL;
    if (tuning_system_copy(tuning_system, &tuning_system_copy_out) != TUNING_SYSTEM_OK) {
        return PITCH_REALIZATION_ERR_ALLOC;
    }

    PitchRealization *pitch_realization = (PitchRealization *)malloc(sizeof(PitchRealization));
    if (pitch_realization == NULL) {
        tuning_system_destroy(tuning_system_copy_out);
        return PITCH_REALIZATION_ERR_ALLOC;
    }

    unsigned long divisor = rational_gcd_unsigned((unsigned long)ratio_numerator, ratio_denominator);
    if (divisor == 0) {
        divisor = 1;
    }

    pitch_realization->tuning_system = tuning_system_copy_out;
    pitch_realization->numerator = ratio_numerator / (long)divisor;
    pitch_realization->denominator = ratio_denominator / divisor;

    *out = pitch_realization;
    return PITCH_REALIZATION_OK;
}

void pitch_realization_destroy(PitchRealization *pitch_realization) {
    if (pitch_realization == NULL) {
        return;
    }
    tuning_system_destroy(pitch_realization->tuning_system);
    free(pitch_realization);
}

const TuningSystem *pitch_realization_tuning_system(const PitchRealization *pitch_realization) {
    return pitch_realization->tuning_system;
}

long pitch_realization_ratio_numerator(const PitchRealization *pitch_realization) {
    return pitch_realization->numerator;
}

unsigned long pitch_realization_ratio_denominator(const PitchRealization *pitch_realization) {
    return pitch_realization->denominator;
}

PitchRealizationFrequencyResult pitch_realization_frequency(const PitchRealization *pitch_realization,
                                                              long reference_numerator,
                                                              unsigned long reference_denominator,
                                                              long *out_numerator, unsigned long *out_denominator) {
    if (reference_denominator == 0) {
        return PITCH_REALIZATION_FREQUENCY_ERR_NON_POSITIVE_REFERENCE_DENOMINATOR;
    }

    long numerator = pitch_realization->numerator * reference_numerator;
    unsigned long denominator = pitch_realization->denominator * reference_denominator;

    unsigned long divisor = rational_gcd_unsigned((unsigned long)(numerator < 0 ? -numerator : numerator), denominator);
    if (divisor == 0) {
        divisor = 1;
    }

    *out_numerator = numerator / (long)divisor;
    *out_denominator = denominator / divisor;
    return PITCH_REALIZATION_FREQUENCY_OK;
}

bool pitch_realization_equals(const PitchRealization *a, const PitchRealization *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    if (!tuning_system_identity_equals(a->tuning_system, b->tuning_system)) {
        return false;
    }
    return a->numerator == b->numerator && a->denominator == b->denominator;
}