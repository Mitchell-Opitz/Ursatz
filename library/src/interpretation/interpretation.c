#include "interpretation/interpretation.h"

#include <stdlib.h>
#include <string.h>

#include "core/string_copy.h"

struct Interpretation {
    EntityID *id;
    EntityID *target_id;
    Value *claim;
    EntityID *context_id;
    EntityID *evidence_id;
    FrameworkReference *framework_reference;
    Uncertainty *uncertainty;
    char *author;
    char *method;
    InterpretationStatus status;
    ProvenanceReference *provenance;
};

InterpretationResult interpretation_create(EntityID *id, EntityID *target_id, Value *claim, EntityID *context_id,
                                            EntityID *evidence_id, FrameworkReference *framework_reference,
                                            Uncertainty *uncertainty, const char *author, const char *method,
                                            InterpretationStatus status, ProvenanceReference *provenance,
                                            Interpretation **out) {
    if (id == NULL) {
        return INTERPRETATION_ERR_NULL_ID;
    }
    if (target_id == NULL) {
        return INTERPRETATION_ERR_NULL_TARGET_ID;
    }
    if (claim == NULL) {
        return INTERPRETATION_ERR_NULL_CLAIM;
    }
    if (uncertainty == NULL) {
        return INTERPRETATION_ERR_NULL_UNCERTAINTY;
    }
    if (author == NULL || author[0] == '\0') {
        return INTERPRETATION_ERR_NULL_AUTHOR;
    }
    if (method == NULL || method[0] == '\0') {
        return INTERPRETATION_ERR_NULL_METHOD;
    }
    if (provenance == NULL) {
        return INTERPRETATION_ERR_NULL_PROVENANCE;
    }

    Interpretation *interpretation = (Interpretation *)malloc(sizeof(Interpretation));
    if (interpretation == NULL) {
        return INTERPRETATION_ERR_ALLOC;
    }

    interpretation->author = ursatz_string_copy(author);
    if (interpretation->author == NULL) {
        free(interpretation);
        return INTERPRETATION_ERR_ALLOC;
    }

    interpretation->method = ursatz_string_copy(method);
    if (interpretation->method == NULL) {
        free(interpretation->author);
        free(interpretation);
        return INTERPRETATION_ERR_ALLOC;
    }

    interpretation->id = id;
    interpretation->target_id = target_id;
    interpretation->claim = claim;
    interpretation->context_id = context_id;
    interpretation->evidence_id = evidence_id;
    interpretation->framework_reference = framework_reference;
    interpretation->uncertainty = uncertainty;
    interpretation->status = status;
    interpretation->provenance = provenance;

    *out = interpretation;
    return INTERPRETATION_OK;
}

void interpretation_destroy(Interpretation *interpretation) {
    if (interpretation == NULL) {
        return;
    }
    entity_id_destroy(interpretation->id);
    entity_id_destroy(interpretation->target_id);
    value_destroy(interpretation->claim);
    entity_id_destroy(interpretation->context_id);
    entity_id_destroy(interpretation->evidence_id);
    framework_reference_destroy(interpretation->framework_reference);
    uncertainty_destroy(interpretation->uncertainty);
    free(interpretation->author);
    free(interpretation->method);
    provenance_reference_destroy(interpretation->provenance);
    free(interpretation);
}

const EntityID *interpretation_id(const Interpretation *interpretation) {
    return interpretation->id;
}

const EntityID *interpretation_target_id(const Interpretation *interpretation) {
    return interpretation->target_id;
}

const Value *interpretation_claim(const Interpretation *interpretation) {
    return interpretation->claim;
}

const EntityID *interpretation_context_id(const Interpretation *interpretation) {
    return interpretation->context_id;
}

const EntityID *interpretation_evidence_id(const Interpretation *interpretation) {
    return interpretation->evidence_id;
}

const FrameworkReference *interpretation_framework_reference(const Interpretation *interpretation) {
    return interpretation->framework_reference;
}

const Uncertainty *interpretation_uncertainty(const Interpretation *interpretation) {
    return interpretation->uncertainty;
}

const char *interpretation_author(const Interpretation *interpretation) {
    return interpretation->author;
}

const char *interpretation_method(const Interpretation *interpretation) {
    return interpretation->method;
}

InterpretationStatus interpretation_status(const Interpretation *interpretation) {
    return interpretation->status;
}

const ProvenanceReference *interpretation_provenance(const Interpretation *interpretation) {
    return interpretation->provenance;
}