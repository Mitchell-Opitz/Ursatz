#include "transform/retrograde_inversion.h"

#include <stdlib.h>
#include <string.h>

struct RetrogradeInversion {
    Transformation *transformation;
    Retrograde *retrograde;
    Inversion *inversion;
};

static char *concat(const char *base, const char *suffix) {
    size_t base_len = strlen(base);
    size_t suffix_len = strlen(suffix);
    char *result = (char *)malloc(base_len + suffix_len + 1);
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, base, base_len);
    memcpy(result + base_len, suffix, suffix_len + 1);
    return result;
}

RetrogradeInversionResult retrograde_inversion_create(EntityID *id, unsigned reversed_dimensions, PitchIdentity *axis,
                                                        const char *pitch_space, const char *spelling_policy,
                                                        ProvenanceReference *provenance, RetrogradeInversion **out) {
    if (id == NULL) {
        return RETROGRADE_INVERSION_ERR_NULL_ID;
    }
    if (reversed_dimensions == 0) {
        return RETROGRADE_INVERSION_ERR_NO_DIMENSIONS;
    }
    if ((reversed_dimensions & ~(unsigned)RETROGRADE_DIMENSION_ALL) != 0) {
        return RETROGRADE_INVERSION_ERR_UNKNOWN_DIMENSION_BITS;
    }
    if (axis == NULL) {
        return RETROGRADE_INVERSION_ERR_NULL_AXIS;
    }
    if (pitch_space == NULL || pitch_space[0] == '\0') {
        return RETROGRADE_INVERSION_ERR_NULL_PITCH_SPACE;
    }
    if (spelling_policy == NULL || spelling_policy[0] == '\0') {
        return RETROGRADE_INVERSION_ERR_NULL_SPELLING_POLICY;
    }
    if (provenance == NULL) {
        return RETROGRADE_INVERSION_ERR_NULL_PROVENANCE;
    }

    const char *id_value = entity_id_value(id);

    char *retrograde_id_value = concat(id_value, "-retrograde");
    if (retrograde_id_value == NULL) {
        return RETROGRADE_INVERSION_ERR_ALLOC;
    }
    EntityID *retrograde_id;
    EntityIDResult retrograde_id_result = entity_id_create(retrograde_id_value, &retrograde_id);
    free(retrograde_id_value);
    if (retrograde_id_result != ENTITY_ID_OK) {
        return RETROGRADE_INVERSION_ERR_ALLOC;
    }

    ProvenanceReference *retrograde_provenance;
    if (provenance_reference_create_absent(&retrograde_provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(retrograde_id);
        return RETROGRADE_INVERSION_ERR_ALLOC;
    }

    Retrograde *retrograde;
    if (retrograde_create(retrograde_id, reversed_dimensions, retrograde_provenance, &retrograde) != RETROGRADE_OK) {
        entity_id_destroy(retrograde_id);
        provenance_reference_destroy(retrograde_provenance);
        return RETROGRADE_INVERSION_ERR_ALLOC;
    }

    char *inversion_id_value = concat(id_value, "-inversion");
    if (inversion_id_value == NULL) {
        retrograde_destroy(retrograde);
        return RETROGRADE_INVERSION_ERR_ALLOC;
    }
    EntityID *inversion_id;
    EntityIDResult inversion_id_result = entity_id_create(inversion_id_value, &inversion_id);
    free(inversion_id_value);
    if (inversion_id_result != ENTITY_ID_OK) {
        retrograde_destroy(retrograde);
        return RETROGRADE_INVERSION_ERR_ALLOC;
    }

    ProvenanceReference *inversion_provenance;
    if (provenance_reference_create_absent(&inversion_provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(inversion_id);
        retrograde_destroy(retrograde);
        return RETROGRADE_INVERSION_ERR_ALLOC;
    }

    Inversion *inversion;
    InversionResult inversion_result =
        inversion_create(inversion_id, axis, pitch_space, spelling_policy, false, inversion_provenance, &inversion);
    if (inversion_result != INVERSION_OK) {
        entity_id_destroy(inversion_id);
        provenance_reference_destroy(inversion_provenance);
        retrograde_destroy(retrograde);
        return RETROGRADE_INVERSION_ERR_ALLOC;
    }

    RetrogradeInversion *retrograde_inversion = (RetrogradeInversion *)malloc(sizeof(RetrogradeInversion));
    if (retrograde_inversion == NULL) {
        inversion_destroy(inversion);
        retrograde_destroy(retrograde);
        return RETROGRADE_INVERSION_ERR_ALLOC;
    }

    Transformation *transformation;
    TransformationResult transformation_result = transformation_create(
        id, "Structure", NULL, "Structure",
        "whichever dimensions the composed Retrograde preserves, other than pitch (see "
        "retrograde_inversion_retrograde)",
        "pitch (via the composed Inversion), plus whichever dimensions the composed Retrograde reverses (see "
        "retrograde_inversion_retrograde)",
        "composed: the sequence is retrograded, then each event's pitch is inverted about the composed "
        "Inversion's axis",
        "delegated to the composed Retrograde and Inversion; see each component's own validation", true, provenance,
        &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(retrograde_inversion);
        inversion_destroy(inversion);
        retrograde_destroy(retrograde);
        return RETROGRADE_INVERSION_ERR_ALLOC;
    }

    retrograde_inversion->transformation = transformation;
    retrograde_inversion->retrograde = retrograde;
    retrograde_inversion->inversion = inversion;

    *out = retrograde_inversion;
    return RETROGRADE_INVERSION_OK;
}

void retrograde_inversion_destroy(RetrogradeInversion *retrograde_inversion) {
    if (retrograde_inversion == NULL) {
        return;
    }
    transformation_destroy(retrograde_inversion->transformation);
    retrograde_destroy(retrograde_inversion->retrograde);
    inversion_destroy(retrograde_inversion->inversion);
    free(retrograde_inversion);
}

const EntityID *retrograde_inversion_id(const RetrogradeInversion *retrograde_inversion) {
    return transformation_id(retrograde_inversion->transformation);
}

const Transformation *retrograde_inversion_transformation(const RetrogradeInversion *retrograde_inversion) {
    return retrograde_inversion->transformation;
}

const Retrograde *retrograde_inversion_retrograde(const RetrogradeInversion *retrograde_inversion) {
    return retrograde_inversion->retrograde;
}

const Inversion *retrograde_inversion_inversion(const RetrogradeInversion *retrograde_inversion) {
    return retrograde_inversion->inversion;
}

bool retrograde_inversion_identity_equals(const RetrogradeInversion *a, const RetrogradeInversion *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}