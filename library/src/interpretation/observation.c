#include "interpretation/observation.h"

#include <stdlib.h>
#include <string.h>

#include "core/string_copy.h"

struct Observation {
    EntityID *id;
    EntityID *subject_id;
    char *property;
    Value *value_or_candidates;
    char *method;
    char *source;
    Uncertainty *uncertainty;
    Value *timestamp;
    ProvenanceReference *provenance;
};

ObservationResult observation_create(EntityID *id, EntityID *subject_id, const char *property,
                                      Value *value_or_candidates, const char *method, const char *source,
                                      Uncertainty *uncertainty, Value *timestamp, ProvenanceReference *provenance,
                                      Observation **out) {
    if (id == NULL) {
        return OBSERVATION_ERR_NULL_ID;
    }
    if (subject_id == NULL) {
        return OBSERVATION_ERR_NULL_SUBJECT_ID;
    }
    if (property == NULL || property[0] == '\0') {
        return OBSERVATION_ERR_NULL_PROPERTY;
    }
    if (value_or_candidates == NULL) {
        return OBSERVATION_ERR_NULL_VALUE_OR_CANDIDATES;
    }
    if (method == NULL || method[0] == '\0') {
        return OBSERVATION_ERR_NULL_METHOD;
    }
    if (source == NULL || source[0] == '\0') {
        return OBSERVATION_ERR_NULL_SOURCE;
    }
    if (uncertainty == NULL) {
        return OBSERVATION_ERR_NULL_UNCERTAINTY;
    }
    if (provenance == NULL) {
        return OBSERVATION_ERR_NULL_PROVENANCE;
    }

    Observation *observation = (Observation *)malloc(sizeof(Observation));
    if (observation == NULL) {
        return OBSERVATION_ERR_ALLOC;
    }

    observation->property = ursatz_string_copy(property);
    if (observation->property == NULL) {
        free(observation);
        return OBSERVATION_ERR_ALLOC;
    }

    observation->method = ursatz_string_copy(method);
    if (observation->method == NULL) {
        free(observation->property);
        free(observation);
        return OBSERVATION_ERR_ALLOC;
    }

    observation->source = ursatz_string_copy(source);
    if (observation->source == NULL) {
        free(observation->method);
        free(observation->property);
        free(observation);
        return OBSERVATION_ERR_ALLOC;
    }

    observation->id = id;
    observation->subject_id = subject_id;
    observation->value_or_candidates = value_or_candidates;
    observation->uncertainty = uncertainty;
    observation->timestamp = timestamp;
    observation->provenance = provenance;

    *out = observation;
    return OBSERVATION_OK;
}

void observation_destroy(Observation *observation) {
    if (observation == NULL) {
        return;
    }
    entity_id_destroy(observation->id);
    entity_id_destroy(observation->subject_id);
    free(observation->property);
    value_destroy(observation->value_or_candidates);
    free(observation->method);
    free(observation->source);
    uncertainty_destroy(observation->uncertainty);
    value_destroy(observation->timestamp);
    provenance_reference_destroy(observation->provenance);
    free(observation);
}

const EntityID *observation_id(const Observation *observation) {
    return observation->id;
}

const EntityID *observation_subject_id(const Observation *observation) {
    return observation->subject_id;
}

const char *observation_property(const Observation *observation) {
    return observation->property;
}

const Value *observation_value_or_candidates(const Observation *observation) {
    return observation->value_or_candidates;
}

const char *observation_method(const Observation *observation) {
    return observation->method;
}

const char *observation_source(const Observation *observation) {
    return observation->source;
}

const Uncertainty *observation_uncertainty(const Observation *observation) {
    return observation->uncertainty;
}

const Value *observation_timestamp(const Observation *observation) {
    return observation->timestamp;
}

const ProvenanceReference *observation_provenance(const Observation *observation) {
    return observation->provenance;
}