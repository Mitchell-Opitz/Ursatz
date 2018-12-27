#ifndef MUSIC_PITCH_PITCH_IDENTITY_H
#define MUSIC_PITCH_PITCH_IDENTITY_H

#include <stdbool.h>
#include <stddef.h>

/*
 * PitchIdentity: an opaque token identifying a pitch, carrying no
 * lifecycle/versioning semantics.
 *
 * token is NOT an EntityID: this header takes no dependency on
 * identity/entity_id.h and exposes no constructor accepting an EntityID.
 * A caller cannot pass an EntityID* here — the parameter type is a raw
 * byte buffer, so an EntityID pointer would not even compile as an
 * argument. That structural gap is the enforcement mechanism; there is
 * no runtime EntityID check to perform.
 *
 * Construction MUST NOT require tuning-system parameters (no 12-TET, no
 * A440, no MIDI note number assumptions).
 *
 * Equality relation is a PLACEHOLDER: value equality on the token bytes.
 * Exact equivalence is deferred to Deliverable D (not yet available);
 * expect this to be revised later.
 */

typedef struct PitchIdentity PitchIdentity;

typedef enum {
    PITCH_IDENTITY_OK = 0,
    PITCH_IDENTITY_ERR_NULL_TOKEN,
    PITCH_IDENTITY_ERR_ALLOC
} PitchIdentityResult;

PitchIdentityResult pitch_identity_create(const void *token, size_t token_size, PitchIdentity **out);
void pitch_identity_destroy(PitchIdentity *identity);

/* Placeholder equality: same token bytes. Not the true equivalence
   relation (Deliverable D). */
bool pitch_identity_equals(const PitchIdentity *a, const PitchIdentity *b);

const void *pitch_identity_token(const PitchIdentity *identity, size_t *out_size);

#endif