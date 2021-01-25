#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "interval_inversion_rules.h"
#include "interval_size_rules.h"
#include "spelling_test_util.h"

typedef StandardTonalityIntervalQuality Quality;

#define P STANDARD_TONALITY_QUALITY_PERFECT
#define MA STANDARD_TONALITY_QUALITY_MAJOR
#define MI STANDARD_TONALITY_QUALITY_MINOR
#define AUG STANDARD_TONALITY_QUALITY_AUGMENTED
#define DIM STANDARD_TONALITY_QUALITY_DIMINISHED
#define AA STANDARD_TONALITY_QUALITY_DOUBLY_AUGMENTED
#define AAA STANDARD_TONALITY_QUALITY_TRIPLY_AUGMENTED
#define DD STANDARD_TONALITY_QUALITY_DOUBLY_DIMINISHED
#define DDD STANDARD_TONALITY_QUALITY_TRIPLY_DIMINISHED

static void check_inversion(int size, Quality quality, int expected_size, Quality expected_quality) {
    int inverted_size = 0;
    Quality inverted_quality = P;
    assert(standard_tonality_interval_invert(size, quality, &inverted_size, &inverted_quality));
    assert(inverted_size == expected_size);
    assert(inverted_quality == expected_quality);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_INTERVAL_INVERSION_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_interval_inversion_rule((StandardTonalityIntervalInversionRule)i);
        const Constraint *constraint =
            standard_tonality_interval_inversion_constraint((StandardTonalityIntervalInversionRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_interval_inversion_rule(STANDARD_TONALITY_INTERVAL_INVERSION_RULE_COUNT) == NULL);
}

/* SIZES: they add up to 9. */
static void test_sizes_add_up_to_nine(void) {
    int expected[] = {8, 7, 6, 5, 4, 3, 2, 1};
    for (int size = 1; size <= 8; size++) {
        assert(standard_tonality_interval_inverted_size(size) == expected[size - 1]);
        assert(size + standard_tonality_interval_inverted_size(size) == 9);
    }
    /* A compound interval inverts through its simple counterpart. */
    assert(standard_tonality_interval_inverted_size(10) == 6);
    assert(standard_tonality_interval_inverted_size(15) == 1);
    assert(standard_tonality_interval_inverted_size(0) == 0);
}

/* QUALITIES: perfect to perfect, major to minor, augmented to diminished. */
static void test_qualities(void) {
    assert(standard_tonality_interval_inverted_quality(P) == P);
    assert(standard_tonality_interval_inverted_quality(MA) == MI);
    assert(standard_tonality_interval_inverted_quality(MI) == MA);
    assert(standard_tonality_interval_inverted_quality(AUG) == DIM);
    assert(standard_tonality_interval_inverted_quality(DIM) == AUG);
    assert(standard_tonality_interval_inverted_quality(AA) == DD);
    assert(standard_tonality_interval_inverted_quality(DD) == AA);
    assert(standard_tonality_interval_inverted_quality(AAA) == DDD);
    assert(standard_tonality_interval_inverted_quality(DDD) == AAA);
    assert(standard_tonality_interval_inverted_quality((Quality)99) == (Quality)99);

    /* Inverting twice returns the quality. */
    for (int q = 0; q < STANDARD_TONALITY_QUALITY_COUNT; q++) {
        assert(standard_tonality_interval_inverted_quality(standard_tonality_interval_inverted_quality((Quality)q)) ==
               (Quality)q);
    }
}

/* The source's examples: ma7 to mi2, A6 to d3, P4 to P5. */
static void test_source_examples(void) {
    check_inversion(7, MA, 2, MI);
    check_inversion(6, AUG, 3, DIM);
    check_inversion(4, P, 5, P);
    check_inversion(3, MA, 6, MI); /* Example 10: ma3 inverts to mi6 */
    check_inversion(1, P, 8, P);
    check_inversion(8, P, 1, P);
}

static void test_invert_rejects_invalid_arguments(void) {
    int size = 42;
    Quality quality = (Quality)42;
    assert(!standard_tonality_interval_invert(5, MA, &size, &quality)); /* no major fifth */
    assert(!standard_tonality_interval_invert(3, P, &size, &quality));
    assert(!standard_tonality_interval_invert(0, P, &size, &quality));
    assert(!standard_tonality_interval_invert(5, P, NULL, &quality));
    assert(!standard_tonality_interval_invert(5, P, &size, NULL));
    assert(size == 42 && quality == (Quality)42);
}

/* Property: for every simple size and fitting quality, inverting twice is
   the identity, the sizes add to 9, and the half steps add to 12. */
static void test_inversion_properties(void) {
    for (int size = 1; size <= 8; size++) {
        for (int q = 0; q < STANDARD_TONALITY_QUALITY_COUNT; q++) {
            if (!standard_tonality_interval_quality_fits_size(size, (Quality)q)) {
                continue;
            }
            int inverted_size;
            Quality inverted_quality;
            assert(standard_tonality_interval_invert(size, (Quality)q, &inverted_size, &inverted_quality));
            assert(size + inverted_size == 9);

            int back_size;
            Quality back_quality;
            assert(standard_tonality_interval_invert(inverted_size, inverted_quality, &back_size, &back_quality));
            assert(back_size == size && back_quality == (Quality)q);

            int semitones;
            int inverted_semitones;
            assert(standard_tonality_interval_semitones(size, (Quality)q, &semitones));
            assert(standard_tonality_interval_semitones(inverted_size, inverted_quality, &inverted_semitones));
            assert(semitones + inverted_semitones == STANDARD_TONALITY_INTERVAL_OCTAVE + 4);
        }
    }
}

/* IDENTIFICATION, Example 11: E-double-flat up to A-flat is an augmented
   fourth, found through the inversion A-flat up to E-double-flat, a
   diminished fifth. */
static void test_example_11(void) {
    PitchSpelling *e_double_flat3 = spell(PITCH_LETTER_E, -2, 3);
    PitchSpelling *a_flat3 = spell(PITCH_LETTER_A, -1, 3);
    int size = 0;
    Quality quality = P;
    assert(standard_tonality_interval_quality_via_inversion(e_double_flat3, a_flat3, &size, &quality));
    assert(size == 4 && quality == AUG);

    /* The inversion itself: A-flat up to E-double-flat an octave higher. */
    PitchSpelling *e_double_flat4 = spell(PITCH_LETTER_E, -2, 4);
    assert(standard_tonality_interval_quality_by_major_scale(a_flat3, e_double_flat4, &size, &quality));
    assert(size == 5 && quality == DIM);

    /* And directly. */
    assert(standard_tonality_interval_quality(e_double_flat3, a_flat3, &size, &quality));
    assert(size == 4 && quality == AUG);

    pitch_spelling_destroy(e_double_flat3);
    pitch_spelling_destroy(a_flat3);
    pitch_spelling_destroy(e_double_flat4);
}

/* Property: identifying by inversion gives the same answer as direct
   identification for every simple interval both can reach. */
static void test_inversion_agrees_with_direct_identification(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int compared = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int lower_accidental = -2; lower_accidental <= 2; lower_accidental++) {
            PitchSpelling *lower = spell(letters[l], lower_accidental, 3);
            for (size_t u = 0; u < 7; u++) { /* sizes 1..7; the augmented octave is left out */
                for (int upper_accidental = -2; upper_accidental <= 2; upper_accidental++) {
                    PitchSpelling *upper =
                        spell(letters[(l + u) % 7], upper_accidental, 3 + (int)((l + u) / 7));

                    int direct_size = 0;
                    int inverted_size = 0;
                    Quality direct_quality = P;
                    Quality inverted_quality = P;
                    bool direct = standard_tonality_interval_quality(lower, upper, &direct_size, &direct_quality);
                    bool inverted =
                        standard_tonality_interval_quality_via_inversion(lower, upper, &inverted_size, &inverted_quality);
                    if (direct && inverted) {
                        assert(direct_size == inverted_size);
                        assert(direct_quality == inverted_quality);
                        compared++;
                    }
                    pitch_spelling_destroy(upper);
                }
            }
            pitch_spelling_destroy(lower);
        }
    }
    assert(compared > 500);
}

static void test_inversion_identification_rejects_invalid_arguments(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *c6 = spell(PITCH_LETTER_C, 0, 6);
    PitchSpelling *b3 = spell(PITCH_LETTER_B, 0, 3);
    int size = 42;
    Quality quality = (Quality)42;
    assert(!standard_tonality_interval_quality_via_inversion(c4, c6, &size, &quality)); /* compound */
    assert(!standard_tonality_interval_quality_via_inversion(c4, b3, &size, &quality)); /* reversed */
    assert(!standard_tonality_interval_quality_via_inversion(NULL, c4, &size, &quality));
    assert(!standard_tonality_interval_quality_via_inversion(c4, NULL, &size, &quality));
    assert(!standard_tonality_interval_quality_via_inversion(c4, c4, NULL, &quality));
    assert(!standard_tonality_interval_quality_via_inversion(c4, c4, &size, NULL));
    assert(size == 42 && quality == (Quality)42);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(c6);
    pitch_spelling_destroy(b3);
}

int main(void) {
    test_rules_reference_framework();
    test_sizes_add_up_to_nine();
    test_qualities();
    test_source_examples();
    test_invert_rejects_invalid_arguments();
    test_inversion_properties();
    test_example_11();
    test_inversion_agrees_with_direct_identification();
    test_inversion_identification_rejects_invalid_arguments();
    printf("test_interval_inversion_rules (standard-tonality): ok\n");
    return 0;
}
