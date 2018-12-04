#include "core/equality.h"

#include <string.h>

bool structural_equals(const Entity *a, const Entity *b) {
    if (a == NULL || b == NULL) {
        return false;
    }

    if (strcmp(entity_type(a), entity_type(b)) != 0) {
        return false;
    }

    const char *metadata_a = entity_metadata(a);
    const char *metadata_b = entity_metadata(b);

    if (metadata_a == NULL || metadata_b == NULL) {
        return metadata_a == metadata_b;
    }

    return strcmp(metadata_a, metadata_b) == 0;
}

bool canonical_equals(const Value *a, const Value *b, unsigned long policy_version, CanonicalizeFn canonicalize) {
    if (a == NULL || b == NULL || canonicalize == NULL) {
        return false;
    }

    Value *canonical_a = canonicalize(a, policy_version);
    Value *canonical_b = canonicalize(b, policy_version);

    bool equal = value_equals(canonical_a, canonical_b);

    value_destroy(canonical_a);
    value_destroy(canonical_b);

    return equal;
}