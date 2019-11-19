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
 * Equality is strict identity: value equality on the token bytes. This
 * is intentional and final, not a placeholder -- it answers "is this
 * the same token," which is a different question from "are these
 * equivalent under some theory" (octave equivalence, enharmonic
 * equivalence under some tuning, etc.). That second question is
 * answered by PitchClass (pitch/pitch_class.h), which wraps a
 * PitchIdentity together with a caller-supplied equivalence relation.
 * PitchIdentity itself never gains theory-relative comparison; asking
 * for it here would be asking the wrong type the wrong question.
 */

typedef struct PitchIdentity PitchIdentity;

typedef enum {
    PITCH_IDENTITY_OK = 0,
    PITCH_IDENTITY_ERR_NULL_TOKEN,
    PITCH_IDENTITY_ERR_ALLOC
} PitchIdentityResult;

PitchIdentityResult pitch_identity_create(const void *token, size_t token_size, PitchIdentity **out);
void pitch_identity_destroy(PitchIdentity *identity);

/* Strict identity equality: same token bytes. Final by design -- see
   PitchClass (pitch/pitch_class.h) for theory-relative equivalence. */
bool pitch_identity_equals(const PitchIdentity *a, const PitchIdentity *b);

const void *pitch_identity_token(const PitchIdentity *identity, size_t *out_size);

#endif