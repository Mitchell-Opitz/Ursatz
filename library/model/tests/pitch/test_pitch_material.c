#include "model/pitch/pitch_material.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

/*
 * The pitch material value (K1): the degree and containment queries, the 12-bit mask on a center, the third, and the diatonic family
 * test. Materials are built by hand here; the snapshot test builds them from the knowledge entries.
 */

static PitchMaterial make(const char *family, size_t count, const int *steps) {
    PitchMaterial material;
    memset(&material, 0, sizeof(material));
    material.id = "test.material.x";
    material.name = "x";
    material.key_word = "x";
    material.family = family;
    material.step_count = count;
    memcpy(material.steps, steps, count * sizeof(int));
    return material;
}

static const int MAJOR_STEPS[7] = {0, 2, 4, 5, 7, 9, 11};
static const int MINOR_STEPS[7] = {0, 2, 3, 5, 7, 8, 10};
static const int PENTATONIC_STEPS[5] = {0, 2, 4, 7, 9};
static const int PAIR_STEPS[2] = {0, 7};

static void test_step(void) {
    PitchMaterial major = make("diatonic", 7, MAJOR_STEPS);
    assert(pitch_material_step(&major, 1) == 0 && pitch_material_step(&major, 4) == 5 && pitch_material_step(&major, 7) == 11);
    assert(pitch_material_step(&major, 0) == -1 && pitch_material_step(&major, 8) == -1 && pitch_material_step(&major, -3) == -1);
    assert(pitch_material_step(NULL, 1) == -1);
}

static void test_contains(void) {
    PitchMaterial major = make("diatonic", 7, MAJOR_STEPS);
    assert(pitch_material_contains(&major, 0) && pitch_material_contains(&major, 11) && pitch_material_contains(&major, 5));
    assert(!pitch_material_contains(&major, 1) && !pitch_material_contains(&major, 6) && !pitch_material_contains(&major, 10));
    /* any integer reduces to 0 to 11 */
    assert(pitch_material_contains(&major, 12) && pitch_material_contains(&major, 24 + 7) && pitch_material_contains(&major, -1));
    assert(!pitch_material_contains(&major, 13) && !pitch_material_contains(&major, -2));
    assert(!pitch_material_contains(NULL, 0));
}

static void test_mask(void) {
    PitchMaterial major = make("diatonic", 7, MAJOR_STEPS);
    PitchMaterial minor = make("diatonic", 7, MINOR_STEPS);
    PitchMaterial pair = make(NULL, 2, PAIR_STEPS);
    /* C major: C D E F G A B */
    assert(pitch_material_mask(&major, 0) == ((1u << 0) | (1u << 2) | (1u << 4) | (1u << 5) | (1u << 7) | (1u << 9) | (1u << 11)));
    /* G major: G A B C D E F# */
    assert(pitch_material_mask(&major, 7) == ((1u << 7) | (1u << 9) | (1u << 11) | (1u << 0) | (1u << 2) | (1u << 4) | (1u << 6)));
    /* A natural minor holds the pitch classes of C major */
    assert(pitch_material_mask(&minor, 9) == pitch_material_mask(&major, 0));
    /* a center is reduced to 0 to 11 */
    assert(pitch_material_mask(&major, 12) == pitch_material_mask(&major, 0));
    assert(pitch_material_mask(&major, -5) == pitch_material_mask(&major, 7));
    assert(pitch_material_mask(&major, 19) == pitch_material_mask(&major, 7));
    assert(pitch_material_mask(&pair, 5) == ((1u << 5) | (1u << 0)));
    assert((pitch_material_mask(&major, 3) & ~0xFFFu) == 0);
    assert(pitch_material_mask(NULL, 0) == 0);
}

static void test_third(void) {
    PitchMaterial major = make("diatonic", 7, MAJOR_STEPS);
    PitchMaterial minor = make("diatonic", 7, MINOR_STEPS);
    PitchMaterial pair = make(NULL, 2, PAIR_STEPS);
    assert(pitch_material_third(&major) == 4);
    assert(pitch_material_third(&minor) == 3);
    assert(pitch_material_third(&pair) == -1);
    assert(pitch_material_third(NULL) == -1);
}

static void test_is_diatonic(void) {
    PitchMaterial major = make("diatonic", 7, MAJOR_STEPS);
    PitchMaterial harmonic = make("minor", 7, MINOR_STEPS);
    PitchMaterial no_family = make(NULL, 7, MAJOR_STEPS);
    PitchMaterial five = make("diatonic", 5, PENTATONIC_STEPS);
    assert(pitch_material_is_diatonic(&major));
    assert(!pitch_material_is_diatonic(&harmonic));
    assert(!pitch_material_is_diatonic(&no_family));
    assert(!pitch_material_is_diatonic(&five));
    assert(!pitch_material_is_diatonic(NULL));
}

int main(void) {
    test_step();
    test_contains();
    test_mask();
    test_third();
    test_is_diatonic();
    return 0;
}
