#include "pitch/pitch_identity.h"

#include <stdlib.h>
#include <string.h>
#include "core/value.h"

struct PitchIdentity {
    Value *value;
};

PitchIdentityResult pitch_identity_create(const void *token, size_t token_size, PitchIdentity **out) {
    if (token == NULL) {
        return PITCH_IDENTITY_ERR_NULL_TOKEN;
    }

    PitchIdentity *identity = (PitchIdentity *)malloc(sizeof(PitchIdentity));
    if (identity == NULL) {
        return PITCH_IDENTITY_ERR_ALLOC;
    }

    if (value_create("PitchIdentity", token, token_size, &identity->value) != VALUE_OK) {
        free(identity);
        return PITCH_IDENTITY_ERR_ALLOC;
    }

    *out = identity;
    return PITCH_IDENTITY_OK;
}

void pitch_identity_destroy(PitchIdentity *identity) {
    if (identity == NULL) {
        return;
    }
    value_destroy(identity->value);
    free(identity);
}

/* Strict identity: same token bytes. Final by design, not a placeholder
   -- see pitch/pitch_identity.h. Theory-relative equivalence (octave
   equivalence, etc.) is PitchClass's job, not this function's. */
bool pitch_identity_equals(const PitchIdentity *a, const PitchIdentity *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return value_equals(a->value, b->value);
}

const void *pitch_identity_token(const PitchIdentity *identity, size_t *out_size) {
    return value_content(identity->value, out_size);
}