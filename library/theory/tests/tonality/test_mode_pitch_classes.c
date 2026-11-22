#include <assert.h>
#include <stdio.h>

#include "theory/tonality/tonal_key.h"

#include "ursatz_test_knowledge.h"

/* The four tonal materials by index: major, natural minor, harmonic minor, melodic minor; NULL for another index. */
enum { TEST_MODE_MAJOR, TEST_MODE_NATURAL_MINOR, TEST_MODE_HARMONIC_MINOR, TEST_MODE_MELODIC_MINOR };

static const PitchMaterial *material_of(int mode) {
    const CommonPracticeMaterials *m = test_materials();
    switch (mode) {
        case TEST_MODE_MAJOR:
            return m->major;
        case TEST_MODE_NATURAL_MINOR:
            return m->natural_minor;
        case TEST_MODE_HARMONIC_MINOR:
            return m->harmonic_minor;
        case TEST_MODE_MELODIC_MINOR:
            return m->melodic_minor;
        default:
            return NULL;
    }
}

/*
 * standard_tonality_material_pitch_classes: the four tonal materials on all twelve tonics, and the two unions the
 * Composer builds from them (melody_nct.c melody_scale_mask).
 */

static unsigned rotate(unsigned mask, int by) {
    unsigned result = 0;
    for (int pc = 0; pc < 12; pc++) {
        if ((mask >> pc) & 1u) {
            result |= 1u << ((pc + by) % 12);
        }
    }
    return result;
}

static void test_modes_on_every_tonic(void) {
    /* The scale on C as a mask, then the same shape on every tonic. */
    const unsigned c_major = (1u << 0) | (1u << 2) | (1u << 4) | (1u << 5) | (1u << 7) | (1u << 9) | (1u << 11);
    const unsigned c_natural = (1u << 0) | (1u << 2) | (1u << 3) | (1u << 5) | (1u << 7) | (1u << 8) | (1u << 10);
    const unsigned c_harmonic = (1u << 0) | (1u << 2) | (1u << 3) | (1u << 5) | (1u << 7) | (1u << 8) | (1u << 11);
    const unsigned c_melodic = (1u << 0) | (1u << 2) | (1u << 3) | (1u << 5) | (1u << 7) | (1u << 9) | (1u << 11);
    for (int tonic = 0; tonic < 12; tonic++) {
        assert(standard_tonality_material_pitch_classes(tonic, material_of(TEST_MODE_MAJOR)) == rotate(c_major, tonic));
        assert(standard_tonality_material_pitch_classes(tonic, material_of(TEST_MODE_NATURAL_MINOR)) ==
               rotate(c_natural, tonic));
        assert(standard_tonality_material_pitch_classes(tonic, material_of(TEST_MODE_HARMONIC_MINOR)) ==
               rotate(c_harmonic, tonic));
        assert(standard_tonality_material_pitch_classes(tonic, material_of(TEST_MODE_MELODIC_MINOR)) ==
               rotate(c_melodic, tonic));
    }
}

static void test_composer_unions(void) {
    /* melody_scale_mask: the minor's six fixed notes plus the raised or the natural seventh. */
    for (int tonic = 0; tonic < 12; tonic++) {
        unsigned natural = standard_tonality_material_pitch_classes(tonic, material_of(TEST_MODE_NATURAL_MINOR));
        unsigned harmonic = standard_tonality_material_pitch_classes(tonic, material_of(TEST_MODE_HARMONIC_MINOR));
        unsigned fixed = 0;
        static const int MINOR[6] = {0, 2, 3, 5, 7, 8};
        for (int i = 0; i < 6; i++) {
            fixed |= 1u << ((tonic + MINOR[i]) % 12);
        }
        unsigned raised = 1u << ((tonic + 11) % 12);
        unsigned lowered = 1u << ((tonic + 10) % 12);
        assert((natural & fixed) == fixed && (harmonic & fixed) == fixed);
        assert((fixed | lowered) == natural);
        assert((fixed | raised) == harmonic);
        assert((natural | raised) == (fixed | lowered | raised));
    }
}

static void test_counterpoint_union(void) {
    /* counterpoint_write.c's minor: natural with the raised sixth and seventh, the melodic form. */
    for (int tonic = 0; tonic < 12; tonic++) {
        unsigned natural = standard_tonality_material_pitch_classes(tonic, material_of(TEST_MODE_NATURAL_MINOR));
        unsigned melodic = standard_tonality_material_pitch_classes(tonic, material_of(TEST_MODE_MELODIC_MINOR));
        unsigned expected = 0;
        static const int WRITTEN[9] = {0, 2, 3, 5, 7, 8, 9, 10, 11};
        for (int i = 0; i < 9; i++) {
            expected |= 1u << ((tonic + WRITTEN[i]) % 12);
        }
        assert((natural | melodic) == expected);
    }
}

static void test_invalid(void) {
    /* A tonic outside 0 to 11 is reduced to a pitch class; a material that is not there has no pitch classes. */
    assert(standard_tonality_material_pitch_classes(-1, material_of(TEST_MODE_MAJOR)) ==
           standard_tonality_material_pitch_classes(11, material_of(TEST_MODE_MAJOR)));
    assert(standard_tonality_material_pitch_classes(12, material_of(TEST_MODE_MAJOR)) ==
           standard_tonality_material_pitch_classes(0, material_of(TEST_MODE_MAJOR)));
    assert(standard_tonality_material_pitch_classes(0, material_of(99)) == 0);
    assert(standard_tonality_material_pitch_classes(0, NULL) == 0);
}

static void test_with_leading_tone(void) {
    for (int mode = TEST_MODE_MAJOR; mode <= TEST_MODE_MELODIC_MINOR; mode++) {
        for (int tonic = 0; tonic < 12; tonic++) {
            unsigned plain = standard_tonality_material_pitch_classes(tonic, material_of(mode));
            unsigned with = standard_tonality_material_pitch_classes_with_leading_tone(tonic, material_of(mode));
            assert(with == (plain | (1u << ((tonic + 11) % 12))));
            assert((with & plain) == plain);
        }
    }
    /* major already holds its leading tone, natural minor gains one */
    assert(standard_tonality_material_pitch_classes_with_leading_tone(0, material_of(TEST_MODE_MAJOR)) == 0xAB5u);
    assert(standard_tonality_material_pitch_classes_with_leading_tone(0, material_of(TEST_MODE_NATURAL_MINOR)) ==
           (standard_tonality_material_pitch_classes(0, material_of(TEST_MODE_NATURAL_MINOR)) | (1u << 11)));
    assert(standard_tonality_material_pitch_classes_with_leading_tone(-1, material_of(TEST_MODE_MAJOR)) ==
           standard_tonality_material_pitch_classes_with_leading_tone(11, material_of(TEST_MODE_MAJOR)));
    assert(standard_tonality_material_pitch_classes_with_leading_tone(12, material_of(TEST_MODE_MAJOR)) ==
           standard_tonality_material_pitch_classes_with_leading_tone(0, material_of(TEST_MODE_MAJOR)));
    assert(standard_tonality_material_pitch_classes_with_leading_tone(0, material_of(99)) == 0);
}

int main(void) {
    test_modes_on_every_tonic();
    test_composer_unions();
    test_counterpoint_union();
    test_invalid();
    test_with_leading_tone();
    printf("standard-tonality mode pitch classes tests passed\n");
    return 0;
}
