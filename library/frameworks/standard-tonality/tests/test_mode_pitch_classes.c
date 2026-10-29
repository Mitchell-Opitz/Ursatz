#include <assert.h>
#include <stdio.h>

#include "tonal_key.h"

/*
 * standard_tonality_mode_pitch_classes: the four modes on all twelve tonics, and the two unions the
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
        assert(standard_tonality_mode_pitch_classes(tonic, STANDARD_TONALITY_MODE_MAJOR) == rotate(c_major, tonic));
        assert(standard_tonality_mode_pitch_classes(tonic, STANDARD_TONALITY_MODE_NATURAL_MINOR) ==
               rotate(c_natural, tonic));
        assert(standard_tonality_mode_pitch_classes(tonic, STANDARD_TONALITY_MODE_HARMONIC_MINOR) ==
               rotate(c_harmonic, tonic));
        assert(standard_tonality_mode_pitch_classes(tonic, STANDARD_TONALITY_MODE_MELODIC_MINOR) ==
               rotate(c_melodic, tonic));
    }
}

static void test_composer_unions(void) {
    /* melody_scale_mask: the minor's six fixed notes plus the raised or the natural seventh. */
    for (int tonic = 0; tonic < 12; tonic++) {
        unsigned natural = standard_tonality_mode_pitch_classes(tonic, STANDARD_TONALITY_MODE_NATURAL_MINOR);
        unsigned harmonic = standard_tonality_mode_pitch_classes(tonic, STANDARD_TONALITY_MODE_HARMONIC_MINOR);
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
        unsigned natural = standard_tonality_mode_pitch_classes(tonic, STANDARD_TONALITY_MODE_NATURAL_MINOR);
        unsigned melodic = standard_tonality_mode_pitch_classes(tonic, STANDARD_TONALITY_MODE_MELODIC_MINOR);
        unsigned expected = 0;
        static const int WRITTEN[9] = {0, 2, 3, 5, 7, 8, 9, 10, 11};
        for (int i = 0; i < 9; i++) {
            expected |= 1u << ((tonic + WRITTEN[i]) % 12);
        }
        assert((natural | melodic) == expected);
    }
}

static void test_invalid(void) {
    assert(standard_tonality_mode_pitch_classes(-1, STANDARD_TONALITY_MODE_MAJOR) == 0);
    assert(standard_tonality_mode_pitch_classes(12, STANDARD_TONALITY_MODE_MAJOR) == 0);
    assert(standard_tonality_mode_pitch_classes(0, (StandardTonalityMode)99) == 0);
    assert(standard_tonality_mode_pitch_classes(0, (StandardTonalityMode)-1) == 0);
}

int main(void) {
    test_modes_on_every_tonic();
    test_composer_unions();
    test_counterpoint_union();
    test_invalid();
    printf("standard-tonality mode pitch classes tests passed\n");
    return 0;
}
