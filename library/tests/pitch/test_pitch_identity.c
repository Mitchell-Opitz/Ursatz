#include "pitch/pitch_identity.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static void test_create_rejects_null_token(void) {
    PitchIdentity *identity = NULL;
    PitchIdentityResult r = pitch_identity_create(NULL, 0, &identity);
    assert(r == PITCH_IDENTITY_ERR_NULL_TOKEN);
    assert(identity == NULL);
}

/* Construction takes only a token, no tuning-system parameters
   (no 12-TET, no A440, no MIDI note number). */
static void test_create_accepts_valid_token(void) {
    const char token[] = "internal-token-1";
    PitchIdentity *identity = NULL;
    PitchIdentityResult r = pitch_identity_create(token, sizeof(token), &identity);
    assert(r == PITCH_IDENTITY_OK);
    assert(identity != NULL);
    pitch_identity_destroy(identity);
}

/* PitchIdentity's constructor has no EntityID-accepting overload: its
   token parameter is a raw byte buffer, not an EntityID*, so an
   EntityID-typed token cannot be constructed here at all -- rejection
   is structural (a compile error), not a runtime branch to exercise. */
static void test_token_is_opaque_not_entity_id(void) {
    const char token[] = "not-an-entity-id";
    PitchIdentity *identity = NULL;
    pitch_identity_create(token, sizeof(token), &identity);

    size_t stored_size = 0;
    const void *stored = pitch_identity_token(identity, &stored_size);
    assert(stored_size == sizeof(token));
    assert(memcmp(stored, token, sizeof(token)) == 0);

    pitch_identity_destroy(identity);
}

static void test_equality_independent_instances_same_token(void) {
    const char token_a[] = "same-token";
    const char token_b[] = "same-token";

    PitchIdentity *a = NULL;
    PitchIdentity *b = NULL;
    pitch_identity_create(token_a, sizeof(token_a), &a);
    pitch_identity_create(token_b, sizeof(token_b), &b);

    assert(a != b);
    assert(pitch_identity_equals(a, b));

    pitch_identity_destroy(a);
    pitch_identity_destroy(b);
}

static void test_inequality_different_token(void) {
    const char token_a[] = "token-a";
    const char token_b[] = "token-b";

    PitchIdentity *a = NULL;
    PitchIdentity *b = NULL;
    pitch_identity_create(token_a, sizeof(token_a), &a);
    pitch_identity_create(token_b, sizeof(token_b), &b);

    assert(!pitch_identity_equals(a, b));

    pitch_identity_destroy(a);
    pitch_identity_destroy(b);
}

int main(void) {
    test_create_rejects_null_token();
    test_create_accepts_valid_token();
    test_token_is_opaque_not_entity_id();
    test_equality_independent_instances_same_token();
    test_inequality_different_token();
    return 0;
}