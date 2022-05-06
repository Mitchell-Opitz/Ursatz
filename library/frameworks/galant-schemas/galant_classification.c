#include "galant_classification.h"

#include "galant_matching_rules.h"

bool galant_resembles(const GalantExemplar *exemplar, GalantSchemaId id) {
    GalantMatch match = {galant_prototype(id), exemplar};
    unsigned hard, soft;
    return galant_matching_check(&match, &hard, &soft) && hard == 0;
}

bool galant_classify(const GalantExemplar *exemplar, unsigned *schemas) {
    if (schemas == NULL || !galant_exemplar_valid(exemplar)) {
        return false;
    }
    unsigned found = 0;
    for (int id = 0; id < GALANT_SCHEMA_COUNT; id++) {
        if (galant_resembles(exemplar, (GalantSchemaId)id)) {
            found |= 1u << id;
        }
    }
    *schemas = found;
    return true;
}
