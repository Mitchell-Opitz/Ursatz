#include "theory/tendency.h"

#include <stdlib.h>
#include <string.h>

struct Tendency {
    EntityID *id;
    char *label;
    unsigned long frequency_numerator;
    unsigned long frequency_denominator;
    Value *observed_data;
};

TendencyResult tendency_create(EntityID *id, const char *label, unsigned long frequency_numerator,
                                unsigned long frequency_denominator, Value *observed_data, Tendency **out) {
    if (id == NULL) {
        return TENDENCY_ERR_NULL_ID;
    }
    if (label == NULL || label[0] == '\0') {
        return TENDENCY_ERR_NULL_LABEL;
    }
    if (frequency_denominator == 0) {
        return TENDENCY_ERR_NON_POSITIVE_DENOMINATOR;
    }
    if (frequency_numerator > frequency_denominator) {
        return TENDENCY_ERR_FREQUENCY_OUT_OF_RANGE;
    }

    Tendency *tendency = (Tendency *)malloc(sizeof(Tendency));
    if (tendency == NULL) {
        return TENDENCY_ERR_ALLOC;
    }

    size_t len = strlen(label);
    tendency->label = (char *)malloc(len + 1);
    if (tendency->label == NULL) {
        free(tendency);
        return TENDENCY_ERR_ALLOC;
    }
    memcpy(tendency->label, label, len + 1);

    tendency->id = id;
    tendency->frequency_numerator = frequency_numerator;
    tendency->frequency_denominator = frequency_denominator;
    tendency->observed_data = observed_data;

    *out = tendency;
    return TENDENCY_OK;
}

void tendency_destroy(Tendency *tendency) {
    if (tendency == NULL) {
        return;
    }
    entity_id_destroy(tendency->id);
    free(tendency->label);
    value_destroy(tendency->observed_data);
    free(tendency);
}

const EntityID *tendency_id(const Tendency *tendency) {
    return tendency->id;
}

const char *tendency_label(const Tendency *tendency) {
    return tendency->label;
}

unsigned long tendency_frequency_numerator(const Tendency *tendency) {
    return tendency->frequency_numerator;
}

unsigned long tendency_frequency_denominator(const Tendency *tendency) {
    return tendency->frequency_denominator;
}

const Value *tendency_observed_data(const Tendency *tendency) {
    return tendency->observed_data;
}

bool tendency_identity_equals(const Tendency *a, const Tendency *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}