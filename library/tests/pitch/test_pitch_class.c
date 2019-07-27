#include "pitch/pitch_class.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

/*
 * mod12_equivalent is a TEST-LOCAL stand-in for octave equivalence
 * (interpreting the single-byte token as a MIDI-like note number). It
 * exists only to property-test PitchClass's plug-in mechanism; it is
 * deliberately not part of pitch_class.c itself -- production code
 * bakes in no particular equivalence relation.
 */
static bool mod12_equivalent(const PitchIdentity *a, const PitchIdentity *b) {
    size_t size_a = 0;
    size_t size_b = 0;
    const unsigned char *token_a = (const unsigned char *)pitch_identity_token(a, &size_a);
    const unsigned char *token_b = (const unsigned char *)pitch_identity_token(b, &size_b);
    if (size_a != 1 || size_b != 1) {
        return false;
    }
    return (token_a[0] % 12) == (token_b[0] % 12);
}

/* A second, distinct relation used only to prove pitch_class_equals
   distinguishes classes built under different relations. */
static bool always_equivalent(const PitchIdentity *a, const PitchIdentity *b) {
    (void)a;
    (void)b;
    return true;
}

static PitchIdentity *make_note(unsigned char note_number) {
    PitchIdentity *identity = NULL;
    pitch_identity_create(&note_number, 1, &identity);
    return identity;
}

static void test_create_rejects_null_representative(void) {
    PitchClass *pitch_class = NULL;
    PitchClassResult r = pitch_class_create(NULL, mod12_equivalent, &pitch_class);
    assert(r == PITCH_CLASS_ERR_NULL_REPRESENTATIVE);
    assert(pitch_class == NULL);
}

static void test_create_rejects_null_equivalence(void) {
    PitchIdentity *representative = make_note(60);

    PitchClass *pitch_class = NULL;
    PitchClassResult r = pitch_class_create(representative, NULL, &pitch_class);
    assert(r == PITCH_CLASS_ERR_NULL_EQUIVALENCE);
    assert(pitch_class == NULL);

    pitch_identity_destroy(representative);
}

static void test_create_accepts_valid(void) {
    PitchClass *pitch_class = NULL;
    PitchClassResult r = pitch_class_create(make_note(60), mod12_equivalent, &pitch_class);
    assert(r == PITCH_CLASS_OK);
    assert(pitch_class != NULL);
    assert(pitch_class_equivalence(pitch_class) == mod12_equivalent);

    pitch_class_destroy(pitch_class);
}

static void test_equals_same_class_under_shared_relation(void) {
    PitchClass *a = NULL;
    PitchClass *b = NULL;
    pitch_class_create(make_note(60), mod12_equivalent, &a);  /* note 60 -> class 0 */
    pitch_class_create(make_note(72), mod12_equivalent, &b);  /* note 72 -> class 0 */

    assert(pitch_class_equals(a, b));

    pitch_class_destroy(a);
    pitch_class_destroy(b);
}

static void test_inequals_different_class_under_shared_relation(void) {
    PitchClass *a = NULL;
    PitchClass *b = NULL;
    pitch_class_create(make_note(60), mod12_equivalent, &a);  /* class 0 */
    pitch_class_create(make_note(61), mod12_equivalent, &b);  /* class 1 */

    assert(!pitch_class_equals(a, b));

    pitch_class_destroy(a);
    pitch_class_destroy(b);
}

/* Two PitchClasses built under different relations are not comparable,
   even when the underlying relation would call them equivalent. */
static void test_inequals_when_relations_differ(void) {
    PitchClass *a = NULL;
    PitchClass *b = NULL;
    pitch_class_create(make_note(60), mod12_equivalent, &a);
    pitch_class_create(make_note(61), always_equivalent, &b);

    assert(!pitch_class_equals(a, b));

    pitch_class_destroy(a);
    pitch_class_destroy(b);
}

/* Property: mod12_equivalent is reflexive over note numbers 0..23. */
static void test_relation_is_reflexive(void) {
    for (unsigned char note = 0; note < 24; note++) {
        PitchIdentity *id = make_note(note);
        assert(mod12_equivalent(id, id));
        pitch_identity_destroy(id);
    }
}

/* Property: mod12_equivalent is symmetric over all pairs in 0..23. */
static void test_relation_is_symmetric(void) {
    for (unsigned char a = 0; a < 24; a++) {
        for (unsigned char b = 0; b < 24; b++) {
            PitchIdentity *id_a = make_note(a);
            PitchIdentity *id_b = make_note(b);
            assert(mod12_equivalent(id_a, id_b) == mod12_equivalent(id_b, id_a));
            pitch_identity_destroy(id_a);
            pitch_identity_destroy(id_b);
        }
    }
}

/* Property: mod12_equivalent is transitive over all triples in 0..23. */
static void test_relation_is_transitive(void) {
    for (unsigned char a = 0; a < 24; a++) {
        for (unsigned char b = 0; b < 24; b++) {
            for (unsigned char c = 0; c < 24; c++) {
                PitchIdentity *id_a = make_note(a);
                PitchIdentity *id_b = make_note(b);
                PitchIdentity *id_c = make_note(c);

                if (mod12_equivalent(id_a, id_b) && mod12_equivalent(id_b, id_c)) {
                    assert(mod12_equivalent(id_a, id_c));
                }

                pitch_identity_destroy(id_a);
                pitch_identity_destroy(id_b);
                pitch_identity_destroy(id_c);
            }
        }
    }
}

int main(void) {
    test_create_rejects_null_representative();
    test_create_rejects_null_equivalence();
    test_create_accepts_valid();
    test_equals_same_class_under_shared_relation();
    test_inequals_different_class_under_shared_relation();
    test_inequals_when_relations_differ();
    test_relation_is_reflexive();
    test_relation_is_symmetric();
    test_relation_is_transitive();
    printf("test_pitch_class: ok\n");
    return 0;
}