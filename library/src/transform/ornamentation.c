#include "transform/ornamentation.h"

#include <stdlib.h>

struct Ornamentation {
    Transformation *transformation;
    EntityID *source_event_id;
    EntityID **derived_event_ids;
    ProvenanceReference **derived_event_provenances;
    size_t derived_event_count;
};

OrnamentationResult ornamentation_create(EntityID *id, EntityID *source_event_id, EntityID **derived_event_ids,
                                          ProvenanceReference **derived_event_provenances, size_t derived_event_count,
                                          ProvenanceReference *provenance, Ornamentation **out) {
    if (id == NULL) {
        return ORNAMENTATION_ERR_NULL_ID;
    }
    if (source_event_id == NULL) {
        return ORNAMENTATION_ERR_NULL_SOURCE_EVENT_ID;
    }
    if (derived_event_ids == NULL || derived_event_count == 0) {
        return ORNAMENTATION_ERR_NULL_DERIVED_EVENT_IDS;
    }
    if (derived_event_provenances == NULL) {
        return ORNAMENTATION_ERR_NULL_DERIVED_EVENT_PROVENANCES;
    }
    for (size_t i = 0; i < derived_event_count; i++) {
        if (derived_event_ids[i] == NULL) {
            return ORNAMENTATION_ERR_NULL_DERIVED_EVENT_ID;
        }
    }
    for (size_t i = 0; i < derived_event_count; i++) {
        ProvenanceReference *entry = derived_event_provenances[i];
        if (entry == NULL) {
            return ORNAMENTATION_ERR_NULL_DERIVED_EVENT_PROVENANCE;
        }
        if (provenance_reference_state(entry) != PROVENANCE_REFERENCE_PRESENT) {
            return ORNAMENTATION_ERR_DERIVED_EVENT_PROVENANCE_ABSENT;
        }
        if (!entity_id_equals(provenance_reference_target_id(entry), source_event_id)) {
            return ORNAMENTATION_ERR_DERIVED_EVENT_PROVENANCE_TARGET_MISMATCH;
        }
    }
    if (provenance == NULL) {
        return ORNAMENTATION_ERR_NULL_PROVENANCE;
    }

    Ornamentation *ornamentation = (Ornamentation *)malloc(sizeof(Ornamentation));
    if (ornamentation == NULL) {
        return ORNAMENTATION_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * derived_event_count);
    if (owned_ids == NULL) {
        free(ornamentation);
        return ORNAMENTATION_ERR_ALLOC;
    }
    for (size_t i = 0; i < derived_event_count; i++) {
        owned_ids[i] = derived_event_ids[i];
    }

    ProvenanceReference **owned_provenances =
        (ProvenanceReference **)malloc(sizeof(ProvenanceReference *) * derived_event_count);
    if (owned_provenances == NULL) {
        free(owned_ids);
        free(ornamentation);
        return ORNAMENTATION_ERR_ALLOC;
    }
    for (size_t i = 0; i < derived_event_count; i++) {
        owned_provenances[i] = derived_event_provenances[i];
    }

    Transformation *transformation = NULL;
    TransformationResult transformation_result = transformation_create(
        id, "NoteEvent", NULL, "NoteEvent", "overall harmonic/structural identity of the source event",
        "event count (one-to-many fan-out), ornamental pitch/rhythm detail added",
        "one-to-many: the source event derives multiple decorative events, each carrying an explicit "
        "ProvenanceReference back to the source",
        "every derived event MUST carry a PRESENT ProvenanceReference targeting source_event_id", true, provenance,
        &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(owned_provenances);
        free(owned_ids);
        free(ornamentation);
        return ORNAMENTATION_ERR_ALLOC;
    }

    ornamentation->transformation = transformation;
    ornamentation->source_event_id = source_event_id;
    ornamentation->derived_event_ids = owned_ids;
    ornamentation->derived_event_provenances = owned_provenances;
    ornamentation->derived_event_count = derived_event_count;

    *out = ornamentation;
    return ORNAMENTATION_OK;
}

void ornamentation_destroy(Ornamentation *ornamentation) {
    if (ornamentation == NULL) {
        return;
    }
    transformation_destroy(ornamentation->transformation);
    entity_id_destroy(ornamentation->source_event_id);
    for (size_t i = 0; i < ornamentation->derived_event_count; i++) {
        entity_id_destroy(ornamentation->derived_event_ids[i]);
        provenance_reference_destroy(ornamentation->derived_event_provenances[i]);
    }
    free(ornamentation->derived_event_ids);
    free(ornamentation->derived_event_provenances);
    free(ornamentation);
}

const EntityID *ornamentation_id(const Ornamentation *ornamentation) {
    return transformation_id(ornamentation->transformation);
}

const Transformation *ornamentation_transformation(const Ornamentation *ornamentation) {
    return ornamentation->transformation;
}

const EntityID *ornamentation_source_event_id(const Ornamentation *ornamentation) {
    return ornamentation->source_event_id;
}

size_t ornamentation_derived_event_count(const Ornamentation *ornamentation) {
    return ornamentation->derived_event_count;
}

const EntityID *ornamentation_derived_event_id_at(const Ornamentation *ornamentation, size_t index) {
    return ornamentation->derived_event_ids[index];
}

const ProvenanceReference *ornamentation_derived_event_provenance_at(const Ornamentation *ornamentation,
                                                                      size_t index) {
    return ornamentation->derived_event_provenances[index];
}

bool ornamentation_identity_equals(const Ornamentation *a, const Ornamentation *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}