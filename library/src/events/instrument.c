#include "events/instrument.h"

#include <stdlib.h>
#include <string.h>

struct Instrument {
    EntityID *id;
    char *name;
    PitchIdentity *range_lowest;
    PitchIdentity *range_highest;
    int transposition_semitones;
};

InstrumentResult instrument_create(EntityID *id, const char *name, PitchIdentity *range_lowest,
                                    PitchIdentity *range_highest, int transposition_semitones, Instrument **out) {
    if (id == NULL) {
        return INSTRUMENT_ERR_NULL_ID;
    }
    if (name == NULL || name[0] == '\0') {
        return INSTRUMENT_ERR_NULL_NAME;
    }
    if (range_lowest == NULL) {
        return INSTRUMENT_ERR_NULL_RANGE_LOWEST;
    }
    if (range_highest == NULL) {
        return INSTRUMENT_ERR_NULL_RANGE_HIGHEST;
    }

    Instrument *instrument = (Instrument *)malloc(sizeof(Instrument));
    if (instrument == NULL) {
        return INSTRUMENT_ERR_ALLOC;
    }

    size_t len = strlen(name);
    instrument->name = (char *)malloc(len + 1);
    if (instrument->name == NULL) {
        free(instrument);
        return INSTRUMENT_ERR_ALLOC;
    }
    memcpy(instrument->name, name, len + 1);

    instrument->id = id;
    instrument->range_lowest = range_lowest;
    instrument->range_highest = range_highest;
    instrument->transposition_semitones = transposition_semitones;

    *out = instrument;
    return INSTRUMENT_OK;
}

void instrument_destroy(Instrument *instrument) {
    if (instrument == NULL) {
        return;
    }
    entity_id_destroy(instrument->id);
    free(instrument->name);
    pitch_identity_destroy(instrument->range_lowest);
    pitch_identity_destroy(instrument->range_highest);
    free(instrument);
}

const EntityID *instrument_id(const Instrument *instrument) {
    return instrument->id;
}

const char *instrument_name(const Instrument *instrument) {
    return instrument->name;
}

const PitchIdentity *instrument_range_lowest(const Instrument *instrument) {
    return instrument->range_lowest;
}

const PitchIdentity *instrument_range_highest(const Instrument *instrument) {
    return instrument->range_highest;
}

int instrument_transposition_semitones(const Instrument *instrument) {
    return instrument->transposition_semitones;
}

bool instrument_identity_equals(const Instrument *a, const Instrument *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}