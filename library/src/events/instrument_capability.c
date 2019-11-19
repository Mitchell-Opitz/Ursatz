#include "events/instrument_capability.h"

#include <stdlib.h>

struct InstrumentCapability {
    PitchIdentity *range_lowest;
    PitchIdentity *range_highest;
    int transposition_semitones;
};

static PitchIdentityResult pitch_identity_copy(const PitchIdentity *src, PitchIdentity **out) {
    size_t size = 0;
    const void *token = pitch_identity_token(src, &size);
    return pitch_identity_create(token, size, out);
}

InstrumentCapabilityResult instrument_capability_create_from_instrument(const Instrument *instrument,
                                                                         InstrumentCapability **out) {
    if (instrument == NULL) {
        return INSTRUMENT_CAPABILITY_ERR_NULL_INSTRUMENT;
    }

    PitchIdentity *range_lowest = NULL;
    if (pitch_identity_copy(instrument_range_lowest(instrument), &range_lowest) != PITCH_IDENTITY_OK) {
        return INSTRUMENT_CAPABILITY_ERR_ALLOC;
    }

    PitchIdentity *range_highest = NULL;
    if (pitch_identity_copy(instrument_range_highest(instrument), &range_highest) != PITCH_IDENTITY_OK) {
        pitch_identity_destroy(range_lowest);
        return INSTRUMENT_CAPABILITY_ERR_ALLOC;
    }

    InstrumentCapability *capability = (InstrumentCapability *)malloc(sizeof(InstrumentCapability));
    if (capability == NULL) {
        pitch_identity_destroy(range_lowest);
        pitch_identity_destroy(range_highest);
        return INSTRUMENT_CAPABILITY_ERR_ALLOC;
    }

    capability->range_lowest = range_lowest;
    capability->range_highest = range_highest;
    capability->transposition_semitones = instrument_transposition_semitones(instrument);

    *out = capability;
    return INSTRUMENT_CAPABILITY_OK;
}

void instrument_capability_destroy(InstrumentCapability *capability) {
    if (capability == NULL) {
        return;
    }
    pitch_identity_destroy(capability->range_lowest);
    pitch_identity_destroy(capability->range_highest);
    free(capability);
}

const PitchIdentity *instrument_capability_range_lowest(const InstrumentCapability *capability) {
    return capability->range_lowest;
}

const PitchIdentity *instrument_capability_range_highest(const InstrumentCapability *capability) {
    return capability->range_highest;
}

int instrument_capability_transposition_semitones(const InstrumentCapability *capability) {
    return capability->transposition_semitones;
}

bool instrument_capability_equals(const InstrumentCapability *a, const InstrumentCapability *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    if (a->transposition_semitones != b->transposition_semitones) {
        return false;
    }
    return pitch_identity_equals(a->range_lowest, b->range_lowest) &&
           pitch_identity_equals(a->range_highest, b->range_highest);
}