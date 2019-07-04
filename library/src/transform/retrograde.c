#include "transform/retrograde.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Retrograde {
    Transformation *transformation;
    unsigned reversed_dimensions;
};

typedef struct {
    RetrogradeDimension flag;
    const char *name;
} DimensionName;

static const DimensionName kDimensionNames[] = {
    {RETROGRADE_DIMENSION_EVENT_ORDER, "event order"},
    {RETROGRADE_DIMENSION_PITCH, "pitch"},
    {RETROGRADE_DIMENSION_RHYTHM, "rhythm"},
    {RETROGRADE_DIMENSION_ARTICULATION, "articulation"},
};
static const size_t kDimensionCount = sizeof(kDimensionNames) / sizeof(kDimensionNames[0]);

/* Writes a comma-separated listing of the flags in mask (or fallback if none) into a heap-allocated buffer. */
static char *describe_dimensions(unsigned mask, const char *fallback_if_empty) {
    char buffer[128];
    buffer[0] = '\0';
    size_t written = 0;
    bool first = true;

    for (size_t i = 0; i < kDimensionCount; i++) {
        if ((mask & kDimensionNames[i].flag) == 0) {
            continue;
        }
        const char *prefix = first ? "" : ", ";
        int n = snprintf(buffer + written, sizeof(buffer) - written, "%s%s", prefix, kDimensionNames[i].name);
        if (n < 0) {
            break;
        }
        written += (size_t)n;
        first = false;
    }

    if (first) {
        size_t len = strlen(fallback_if_empty);
        char *copy = (char *)malloc(len + 1);
        if (copy != NULL) {
            memcpy(copy, fallback_if_empty, len + 1);
        }
        return copy;
    }

    size_t len = strlen(buffer);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, buffer, len + 1);
    }
    return copy;
}

RetrogradeResult retrograde_create(EntityID *id, unsigned reversed_dimensions, ProvenanceReference *provenance,
                                    Retrograde **out) {
    if (id == NULL) {
        return RETROGRADE_ERR_NULL_ID;
    }
    if (reversed_dimensions == 0) {
        return RETROGRADE_ERR_NO_DIMENSIONS;
    }
    if ((reversed_dimensions & ~(unsigned)RETROGRADE_DIMENSION_ALL) != 0) {
        return RETROGRADE_ERR_UNKNOWN_DIMENSION_BITS;
    }
    if (provenance == NULL) {
        return RETROGRADE_ERR_NULL_PROVENANCE;
    }

    Retrograde *retrograde = (Retrograde *)malloc(sizeof(Retrograde));
    if (retrograde == NULL) {
        return RETROGRADE_ERR_ALLOC;
    }

    char *modified_dimensions = describe_dimensions(reversed_dimensions, "");
    if (modified_dimensions == NULL) {
        free(retrograde);
        return RETROGRADE_ERR_ALLOC;
    }

    char *preserved_dimensions = describe_dimensions(~reversed_dimensions & (unsigned)RETROGRADE_DIMENSION_ALL,
                                                       "none (all reversed)");
    if (preserved_dimensions == NULL) {
        free(modified_dimensions);
        free(retrograde);
        return RETROGRADE_ERR_ALLOC;
    }

    Transformation *transformation;
    TransformationResult transformation_result =
        transformation_create(id, "Structure", NULL, "Structure", preserved_dimensions, modified_dimensions,
                               "one-to-one, reversal applied over the sequence's ordered material",
                               "reversed dimension(s) explicitly declared by caller, never defaulted", true,
                               provenance, &transformation);
    free(modified_dimensions);
    free(preserved_dimensions);
    if (transformation_result != TRANSFORMATION_OK) {
        free(retrograde);
        return RETROGRADE_ERR_ALLOC;
    }

    retrograde->transformation = transformation;
    retrograde->reversed_dimensions = reversed_dimensions;

    *out = retrograde;
    return RETROGRADE_OK;
}

void retrograde_destroy(Retrograde *retrograde) {
    if (retrograde == NULL) {
        return;
    }
    transformation_destroy(retrograde->transformation);
    free(retrograde);
}

const EntityID *retrograde_id(const Retrograde *retrograde) {
    return transformation_id(retrograde->transformation);
}

const Transformation *retrograde_transformation(const Retrograde *retrograde) {
    return retrograde->transformation;
}

unsigned retrograde_reversed_dimensions(const Retrograde *retrograde) {
    return retrograde->reversed_dimensions;
}

bool retrograde_identity_equals(const Retrograde *a, const Retrograde *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}