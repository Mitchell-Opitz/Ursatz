#include "transform/displacement.h"

#include <stdlib.h>

struct Displacement {
    Transformation *transformation;
    DisplacementDirection direction;
    Duration *magnitude;
};

static const char *direction_label(DisplacementDirection direction) {
    switch (direction) {
        case DISPLACEMENT_DIRECTION_FORWARD:
            return "forward";
        case DISPLACEMENT_DIRECTION_BACKWARD:
            return "backward";
        default:
            return NULL;
    }
}

DisplacementResult displacement_create(EntityID *id, DisplacementDirection direction, Duration *magnitude,
                                        ProvenanceReference *provenance, Displacement **out) {
    if (id == NULL) {
        return DISPLACEMENT_ERR_NULL_ID;
    }
    if (direction_label(direction) == NULL) {
        return DISPLACEMENT_ERR_UNKNOWN_DIRECTION;
    }
    if (magnitude == NULL) {
        return DISPLACEMENT_ERR_NULL_MAGNITUDE;
    }
    if (provenance == NULL) {
        return DISPLACEMENT_ERR_NULL_PROVENANCE;
    }

    Displacement *displacement = (Displacement *)malloc(sizeof(Displacement));
    if (displacement == NULL) {
        return DISPLACEMENT_ERR_ALLOC;
    }

    Transformation *transformation = NULL;
    TransformationResult transformation_result = transformation_create(
        id, "NoteEvent", NULL, "NoteEvent", "pitch, duration, articulation", "temporal position (TemporalAnchor)",
        "one-to-one, shifts only the event's TemporalAnchor by the configured direction and magnitude; every "
        "other field unchanged",
        "magnitude is non-negative (enforced by Duration itself); direction explicitly declared", true, provenance,
        &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(displacement);
        return DISPLACEMENT_ERR_ALLOC;
    }

    displacement->transformation = transformation;
    displacement->direction = direction;
    displacement->magnitude = magnitude;

    *out = displacement;
    return DISPLACEMENT_OK;
}

void displacement_destroy(Displacement *displacement) {
    if (displacement == NULL) {
        return;
    }
    transformation_destroy(displacement->transformation);
    duration_destroy(displacement->magnitude);
    free(displacement);
}

const EntityID *displacement_id(const Displacement *displacement) {
    return transformation_id(displacement->transformation);
}

const Transformation *displacement_transformation(const Displacement *displacement) {
    return displacement->transformation;
}

DisplacementDirection displacement_direction(const Displacement *displacement) {
    return displacement->direction;
}

const Duration *displacement_magnitude(const Displacement *displacement) {
    return displacement->magnitude;
}

bool displacement_identity_equals(const Displacement *a, const Displacement *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}