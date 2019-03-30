#include "transform/transposition.h"

#include <stdlib.h>
#include <string.h>

struct Transposition {
    Transformation *transformation;
    long semitones;
    char *spelling_behavior;
    bool has_tuning_behavior;
    bool has_range_policy;
    bool has_instrument_context;
};

static char *copy_string(const char *value) {
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, value, len + 1);
    }
    return copy;
}

TranspositionResult transposition_create(EntityID *id, long semitones, const char *spelling_behavior,
                                          bool has_tuning_behavior, bool has_range_policy,
                                          bool has_instrument_context, ProvenanceReference *provenance,
                                          Transposition **out) {
    if (id == NULL) {
        return TRANSPOSITION_ERR_NULL_ID;
    }
    if (spelling_behavior == NULL || spelling_behavior[0] == '\0') {
        return TRANSPOSITION_ERR_NULL_SPELLING_BEHAVIOR;
    }
    if (has_tuning_behavior) {
        return TRANSPOSITION_ERR_TUNING_BEHAVIOR_NOT_SUPPORTED;
    }
    if (has_range_policy) {
        return TRANSPOSITION_ERR_RANGE_POLICY_NOT_SUPPORTED;
    }
    if (has_instrument_context) {
        return TRANSPOSITION_ERR_INSTRUMENT_CONTEXT_NOT_SUPPORTED;
    }
    if (provenance == NULL) {
        return TRANSPOSITION_ERR_NULL_PROVENANCE;
    }

    Transposition *transposition = (Transposition *)malloc(sizeof(Transposition));
    if (transposition == NULL) {
        return TRANSPOSITION_ERR_ALLOC;
    }

    transposition->spelling_behavior = copy_string(spelling_behavior);
    if (transposition->spelling_behavior == NULL) {
        free(transposition);
        return TRANSPOSITION_ERR_ALLOC;
    }

    Value *parameters;
    if (value_create("semitones", &semitones, sizeof(semitones), &parameters) != VALUE_OK) {
        free(transposition->spelling_behavior);
        free(transposition);
        return TRANSPOSITION_ERR_ALLOC;
    }

    Transformation *transformation;
    TransformationResult transformation_result = transformation_create(
        id, "NoteEvent", parameters, "NoteEvent", "rhythm, event order, articulation", "pitch",
        "one-to-one, per-event pitch displacement",
        "spelling behavior applied per configured policy; tuning/range/instrument checks not yet implemented "
        "(Deliverable D)",
        true, provenance, &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        value_destroy(parameters);
        free(transposition->spelling_behavior);
        free(transposition);
        return TRANSPOSITION_ERR_ALLOC;
    }

    transposition->transformation = transformation;
    transposition->semitones = semitones;
    transposition->has_tuning_behavior = has_tuning_behavior;
    transposition->has_range_policy = has_range_policy;
    transposition->has_instrument_context = has_instrument_context;

    *out = transposition;
    return TRANSPOSITION_OK;
}

void transposition_destroy(Transposition *transposition) {
    if (transposition == NULL) {
        return;
    }
    transformation_destroy(transposition->transformation);
    free(transposition->spelling_behavior);
    free(transposition);
}

const EntityID *transposition_id(const Transposition *transposition) {
    return transformation_id(transposition->transformation);
}

const Transformation *transposition_transformation(const Transposition *transposition) {
    return transposition->transformation;
}

long transposition_semitones(const Transposition *transposition) {
    return transposition->semitones;
}

const char *transposition_spelling_behavior(const Transposition *transposition) {
    return transposition->spelling_behavior;
}

bool transposition_has_tuning_behavior(const Transposition *transposition) {
    return transposition->has_tuning_behavior;
}

bool transposition_has_range_policy(const Transposition *transposition) {
    return transposition->has_range_policy;
}

bool transposition_has_instrument_context(const Transposition *transposition) {
    return transposition->has_instrument_context;
}

bool transposition_identity_equals(const Transposition *a, const Transposition *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}