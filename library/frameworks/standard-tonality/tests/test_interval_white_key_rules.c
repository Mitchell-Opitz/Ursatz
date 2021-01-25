#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "interval_quality_rules.h"
#include "interval_white_key_rules.h"
#include "spelling_test_util.h"

typedef StandardTonalityIntervalQuality Quality;

#define P STANDARD_TONALITY_QUALITY_PERFECT
#define MA STANDARD_TONALITY_QUALITY_MAJOR
#define MI STANDARD_TONALITY_QUALITY_MINOR
#define AUG STANDARD_TONALITY_QUALITY_AUGMENTED
#define DIM STANDARD_TONALITY_QUALITY_DIMINISHED

static const PitchLetter LETTERS[7] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                                       PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};

static Quality white_key(PitchLetter lower, int size) {
    Quality quality = (Quality)42;
    assert(standard_tonality_white_key_quality(lower, size, &quality));
    return quality;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_WHITE_KEY_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_white_key_rule((StandardTonalityWhiteKeyRule)i);
        const Constraint *constraint = standard_tonality_white_key_constraint((StandardTonalityWhiteKeyRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_white_key_rule(STANDARD_TONALITY_WHITE_KEY_RULE_COUNT) == NULL);
}

/* SECONDS: major except E-F and B-C. */
static void test_seconds(void) {
    for (size_t i = 0; i < 7; i++) {
        bool exception = LETTERS[i] == PITCH_LETTER_E || LETTERS[i] == PITCH_LETTER_B;
        assert(white_key(LETTERS[i], 2) == (exception ? MI : MA));
    }
}

/* THIRDS: minor except C-E, F-A, G-B. */
static void test_thirds(void) {
    for (size_t i = 0; i < 7; i++) {
        bool exception = LETTERS[i] == PITCH_LETTER_C || LETTERS[i] == PITCH_LETTER_F || LETTERS[i] == PITCH_LETTER_G;
        assert(white_key(LETTERS[i], 3) == (exception ? MA : MI));
    }
}

/* FOURTHS: perfect except F-B. */
static void test_fourths(void) {
    for (size_t i = 0; i < 7; i++) {
        assert(white_key(LETTERS[i], 4) == (LETTERS[i] == PITCH_LETTER_F ? AUG : P));
    }
}

/* BY_INVERSION: fifths perfect except B-F; sixths invert the thirds;
   sevenths minor except F-E and C-B. */
static void test_by_inversion(void) {
    for (size_t i = 0; i < 7; i++) {
        PitchLetter l = LETTERS[i];
        assert(white_key(l, 5) == (l == PITCH_LETTER_B ? DIM : P));
        bool minor_sixth = l == PITCH_LETTER_E || l == PITCH_LETTER_A || l == PITCH_LETTER_B;
        assert(white_key(l, 6) == (minor_sixth ? MI : MA));
        bool major_seventh = l == PITCH_LETTER_C || l == PITCH_LETTER_F;
        assert(white_key(l, 7) == (major_seventh ? MA : MI));
    }
}

static void test_unisons_and_octaves(void) {
    for (size_t i = 0; i < 7; i++) {
        assert(white_key(LETTERS[i], 1) == P);
        assert(white_key(LETTERS[i], 8) == P);
    }
}

/* Example 15: the seven white-key sevenths. */
static void test_example_15(void) {
    Quality expected[] = {MA, MI, MI, MA, MI, MI, MI};
    for (size_t i = 0; i < 7; i++) {
        assert(white_key(LETTERS[i], 7) == expected[i]);
    }
}

/* The memorized table agrees with the interval computed from the notes. */
static void test_agrees_with_computed_quality(void) {
    for (size_t l = 0; l < 7; l++) {
        PitchSpelling *lower = spell(LETTERS[l], 0, 4);
        for (int size = 1; size <= 8; size++) {
            PitchSpelling *upper = spell(LETTERS[(l + (size_t)size - 1) % 7], 0, 4 + (int)((l + (size_t)size - 1) / 7));
            int computed_size = 0;
            Quality computed = P;
            assert(standard_tonality_interval_quality(lower, upper, &computed_size, &computed));
            assert(computed_size == size);
            assert(white_key(LETTERS[l], size) == computed);
            pitch_spelling_destroy(upper);
        }
        pitch_spelling_destroy(lower);
    }
}

static void test_default_quality_and_exceptions(void) {
    Quality default_quality = (Quality)42;
    Quality expected[] = {P, MA, MI, P, P, MA, MI, P};
    int exception_counts[] = {0, 2, 3, 1, 1, 3, 2, 0};
    for (int size = 1; size <= 8; size++) {
        assert(standard_tonality_white_key_default_quality(size, &default_quality));
        assert(default_quality == expected[size - 1]);

        int exceptions = 0;
        for (size_t i = 0; i < 7; i++) {
            if (standard_tonality_white_key_is_exception(LETTERS[i], size)) {
                exceptions++;
            }
        }
        assert(exceptions == exception_counts[size - 1]);
    }
    assert(standard_tonality_white_key_is_exception(PITCH_LETTER_E, 2));  /* E-F */
    assert(standard_tonality_white_key_is_exception(PITCH_LETTER_B, 2));  /* B-C */
    assert(!standard_tonality_white_key_is_exception(PITCH_LETTER_C, 2));
    assert(standard_tonality_white_key_is_exception(PITCH_LETTER_F, 4));  /* F-B */
    assert(!standard_tonality_white_key_is_exception(PITCH_LETTER_C, 0));
    assert(!standard_tonality_white_key_default_quality(0, &default_quality));
    assert(!standard_tonality_white_key_default_quality(9, &default_quality));
    assert(!standard_tonality_white_key_default_quality(3, NULL));
}

static void test_invalid_sizes(void) {
    Quality quality = (Quality)42;
    assert(!standard_tonality_white_key_quality(PITCH_LETTER_C, 0, &quality));
    assert(!standard_tonality_white_key_quality(PITCH_LETTER_C, 9, &quality));
    assert(!standard_tonality_white_key_quality(PITCH_LETTER_C, 3, NULL));
    assert(quality == (Quality)42);
}

/* ALTERATION: D to F is a minor third; D to F-sharp, a half step larger, is
   major. */
static void test_alteration(void) {
    Quality quality = (Quality)42;
    assert(standard_tonality_white_key_altered_quality(PITCH_LETTER_D, 0, 3, 0, &quality) && quality == MI);
    assert(standard_tonality_white_key_altered_quality(PITCH_LETTER_D, 0, 3, 1, &quality) && quality == MA);

    /* Example 6 and 7, from the white-key fifth F-C and sixth G-E. */
    assert(standard_tonality_white_key_altered_quality(PITCH_LETTER_F, 0, 5, 1, &quality) && quality == AUG);
    assert(standard_tonality_white_key_altered_quality(PITCH_LETTER_F, -1, 5, 0, &quality) && quality == AUG);
    assert(standard_tonality_white_key_altered_quality(PITCH_LETTER_F, 0, 5, -1, &quality) && quality == DIM);
    assert(standard_tonality_white_key_altered_quality(PITCH_LETTER_F, 1, 5, 0, &quality) && quality == DIM);
    assert(standard_tonality_white_key_altered_quality(PITCH_LETTER_G, 0, 6, -1, &quality) && quality == MI);
    assert(standard_tonality_white_key_altered_quality(PITCH_LETTER_G, 1, 6, 0, &quality) && quality == MI);

    /* Raising both notes together leaves the quality alone. */
    for (int accidental = -2; accidental <= 2; accidental++) {
        assert(standard_tonality_white_key_altered_quality(PITCH_LETTER_D, accidental, 3, accidental, &quality));
        assert(quality == MI);
    }
}

/* Property: the white-key interval, altered by the accidentals, is the
   interval computed from the spelled notes. */
static void test_alteration_agrees_with_computed_quality(void) {
    int compared = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int lower_accidental = -2; lower_accidental <= 2; lower_accidental++) {
            PitchSpelling *lower = spell(LETTERS[l], lower_accidental, 4);
            for (int size = 1; size <= 8; size++) {
                for (int upper_accidental = -2; upper_accidental <= 2; upper_accidental++) {
                    size_t upper_index = l + (size_t)size - 1;
                    PitchSpelling *upper = spell(LETTERS[upper_index % 7], upper_accidental, 4 + (int)(upper_index / 7));

                    int computed_size = 0;
                    Quality computed = P;
                    Quality altered = P;
                    bool engine = standard_tonality_interval_quality(lower, upper, &computed_size, &computed);
                    bool white = standard_tonality_white_key_altered_quality(LETTERS[l], lower_accidental, size,
                                                                              upper_accidental, &altered);
                    assert(engine == white);
                    if (engine) {
                        assert(computed_size == size);
                        assert(computed == altered);
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

static void test_alteration_rejects_invalid_arguments(void) {
    Quality quality = (Quality)42;
    assert(!standard_tonality_white_key_altered_quality(PITCH_LETTER_D, 0, 0, 0, &quality));
    assert(!standard_tonality_white_key_altered_quality(PITCH_LETTER_D, 0, 9, 0, &quality));
    assert(!standard_tonality_white_key_altered_quality(PITCH_LETTER_D, 0, 3, 0, NULL));
    /* Beyond triply augmented. */
    assert(!standard_tonality_white_key_altered_quality(PITCH_LETTER_F, -2, 5, 2, &quality));
    /* A unison made smaller than a unison is not an interval. */
    assert(!standard_tonality_white_key_altered_quality(PITCH_LETTER_C, 1, 1, -1, &quality));
    assert(quality == (Quality)42);
}

int main(void) {
    test_rules_reference_framework();
    test_seconds();
    test_thirds();
    test_fourths();
    test_by_inversion();
    test_unisons_and_octaves();
    test_example_15();
    test_agrees_with_computed_quality();
    test_default_quality_and_exceptions();
    test_invalid_sizes();
    test_alteration();
    test_alteration_agrees_with_computed_quality();
    test_alteration_rejects_invalid_arguments();
    printf("test_interval_white_key_rules (standard-tonality): ok\n");
    return 0;
}
