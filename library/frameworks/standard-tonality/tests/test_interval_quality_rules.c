#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "interval_quality_rules.h"
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

/* Checks the interval between two spellings by the semitone engine and,
   where the lower note has a major scale, by the major-scale method. */
static void check(PitchLetter lower_letter, int lower_accidental, int lower_octave, PitchLetter upper_letter,
                  int upper_accidental, int upper_octave, int expected_size, Quality expected_quality) {
    PitchSpelling *lower = spell(lower_letter, lower_accidental, lower_octave);
    PitchSpelling *upper = spell(upper_letter, upper_accidental, upper_octave);

    int size = 0;
    Quality quality = P;
    assert(standard_tonality_interval_quality(lower, upper, &size, &quality));
    assert(size == expected_size);
    assert(quality == expected_quality);

    int scale_size = 0;
    Quality scale_quality = P;
    if (standard_tonality_interval_quality_by_major_scale(lower, upper, &scale_size, &scale_quality)) {
        assert(scale_size == expected_size);
        assert(scale_quality == expected_quality);
    }

    pitch_spelling_destroy(lower);
    pitch_spelling_destroy(upper);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_INTERVAL_QUALITY_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_interval_quality_rule((StandardTonalityIntervalQualityRule)i);
        const Constraint *constraint =
            standard_tonality_interval_quality_constraint((StandardTonalityIntervalQualityRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_interval_quality_rule(STANDARD_TONALITY_INTERVAL_QUALITY_RULE_COUNT) == NULL);
}

static void test_names_and_abbreviations(void) {
    const char *names[] = {"triply diminished", "doubly diminished", "diminished", "minor",           "perfect",
                           "major",             "augmented",         "doubly augmented", "triply augmented"};
    const char *abbreviations[] = {"ddd", "dd", "d", "mi", "P", "ma", "A", "AA", "AAA"};
    for (int q = 0; q < STANDARD_TONALITY_QUALITY_COUNT; q++) {
        assert(strcmp(standard_tonality_interval_quality_name((Quality)q), names[q]) == 0);
        assert(strcmp(standard_tonality_interval_quality_abbreviation((Quality)q), abbreviations[q]) == 0);
    }
    assert(standard_tonality_interval_quality_name((Quality)99) == NULL);
    assert(standard_tonality_interval_quality_abbreviation((Quality)-1) == NULL);
}

static void test_augmented_and_diminished_families(void) {
    for (int q = 0; q < STANDARD_TONALITY_QUALITY_COUNT; q++) {
        bool aug = q == AUG || q == AA || q == AAA;
        bool dim = q == DIM || q == DD || q == DDD;
        assert(standard_tonality_interval_quality_is_augmented((Quality)q) == aug);
        assert(standard_tonality_interval_quality_is_diminished((Quality)q) == dim);
    }
}

/* CLASSES: unisons, 4ths, 5ths, octaves are perfect; 2nds, 3rds, 6ths,
   7ths major and minor. */
static void test_classes(void) {
    for (int size = 1; size <= 15; size++) {
        int simple = size <= 8 ? size : size - 7;
        bool perfect = simple == 1 || simple == 4 || simple == 5 || simple == 8;
        assert(standard_tonality_interval_size_is_perfect_class(size) == perfect);
        assert(standard_tonality_interval_quality_fits_size(size, P) == perfect);
        assert(standard_tonality_interval_quality_fits_size(size, MA) == !perfect);
        assert(standard_tonality_interval_quality_fits_size(size, MI) == !perfect);
        /* Augmented and diminished fit every size. */
        assert(standard_tonality_interval_quality_fits_size(size, AUG));
        assert(standard_tonality_interval_quality_fits_size(size, DIM));
        assert(standard_tonality_interval_quality_fits_size(size, DD));
        assert(standard_tonality_interval_quality_fits_size(size, AAA));
    }
    assert(!standard_tonality_interval_size_is_perfect_class(0));
    assert(!standard_tonality_interval_quality_fits_size(0, AUG));
    assert(!standard_tonality_interval_quality_fits_size(3, (Quality)99));
}

static void test_major_interval_semitones(void) {
    int expected[] = {0, 2, 4, 5, 7, 9, 11, 12};
    for (int size = 1; size <= 8; size++) {
        assert(standard_tonality_major_interval_semitones(size) == expected[size - 1]);
    }
    assert(standard_tonality_major_interval_semitones(0) == 0);
    assert(standard_tonality_major_interval_semitones(9) == 0);
}

/* MAJOR_SCALE_METHOD, Example 5. */
static void test_example_5(void) {
    check(PITCH_LETTER_F, 0, 4, PITCH_LETTER_C, 0, 5, 5, P);   /* C is in F major */
    check(PITCH_LETTER_E, -1, 4, PITCH_LETTER_C, -1, 5, 6, MI); /* C-flat is not in E-flat major */
    check(PITCH_LETTER_E, -1, 4, PITCH_LETTER_C, 0, 5, 6, MA);  /* C natural is */
}

static void test_top_in_major_scale(void) {
    PitchSpelling *f4 = spell(PITCH_LETTER_F, 0, 4);
    PitchSpelling *c5 = spell(PITCH_LETTER_C, 0, 5);
    PitchSpelling *c_sharp5 = spell(PITCH_LETTER_C, 1, 5);
    PitchSpelling *b4 = spell(PITCH_LETTER_B, -1, 4); /* B-flat is in F major */
    PitchSpelling *b4_natural = spell(PITCH_LETTER_B, 0, 4);
    assert(standard_tonality_interval_top_in_major_scale(f4, c5));
    assert(!standard_tonality_interval_top_in_major_scale(f4, c_sharp5));
    assert(standard_tonality_interval_top_in_major_scale(f4, b4));
    assert(!standard_tonality_interval_top_in_major_scale(f4, b4_natural));
    assert(!standard_tonality_interval_top_in_major_scale(NULL, c5));
    pitch_spelling_destroy(f4);
    pitch_spelling_destroy(c5);
    pitch_spelling_destroy(c_sharp5);
    pitch_spelling_destroy(b4);
    pitch_spelling_destroy(b4_natural);
}

/* Example 6: augmented intervals by raising the top note (a) or lowering
   the bottom (b). */
static void test_example_6_augmented(void) {
    check(PITCH_LETTER_F, 0, 4, PITCH_LETTER_C, 1, 5, 5, AUG);  /* F-C-sharp */
    check(PITCH_LETTER_G, 0, 4, PITCH_LETTER_E, 0, 5, 6, MA);   /* G-E */
    check(PITCH_LETTER_G, 0, 4, PITCH_LETTER_E, 1, 5, 6, AUG);  /* G-E-sharp */
    check(PITCH_LETTER_F, -1, 4, PITCH_LETTER_C, 0, 5, 5, AUG); /* F-flat-C */
    check(PITCH_LETTER_G, -1, 4, PITCH_LETTER_E, 0, 5, 6, AUG); /* G-flat-E */
}

/* Example 7: diminished intervals. */
static void test_example_7_diminished(void) {
    check(PITCH_LETTER_F, 0, 4, PITCH_LETTER_C, -1, 5, 5, DIM);   /* F-C-flat: the tritone */
    check(PITCH_LETTER_G, 0, 4, PITCH_LETTER_E, -1, 5, 6, MI);    /* G-E-flat */
    check(PITCH_LETTER_G, 0, 4, PITCH_LETTER_E, -2, 5, 6, DIM);   /* G-E-double-flat */
    check(PITCH_LETTER_F, 1, 4, PITCH_LETTER_C, 0, 5, 5, DIM);    /* F-sharp-C */
    check(PITCH_LETTER_G, 1, 4, PITCH_LETTER_E, 0, 5, 6, MI);     /* G-sharp-E */
    check(PITCH_LETTER_G, 2, 4, PITCH_LETTER_E, 0, 5, 6, DIM);    /* G-double-sharp-E */
}

/* Example 8: each step is a half step -- diminished, minor, major,
   augmented for a sixth; diminished, perfect, augmented for a fifth. */
static void test_example_8_ladders(void) {
    check(PITCH_LETTER_G, 0, 4, PITCH_LETTER_E, -2, 5, 6, DIM);
    check(PITCH_LETTER_G, 0, 4, PITCH_LETTER_E, -1, 5, 6, MI);
    check(PITCH_LETTER_G, 0, 4, PITCH_LETTER_E, 0, 5, 6, MA);
    check(PITCH_LETTER_G, 0, 4, PITCH_LETTER_E, 1, 5, 6, AUG);

    check(PITCH_LETTER_F, 0, 4, PITCH_LETTER_C, -1, 5, 5, DIM);
    check(PITCH_LETTER_F, 0, 4, PITCH_LETTER_C, 0, 5, 5, P);
    check(PITCH_LETTER_F, 0, 4, PITCH_LETTER_C, 1, 5, 5, AUG);
}

/* Beyond augmented and diminished. */
static void test_doubly_and_triply_altered(void) {
    check(PITCH_LETTER_F, 0, 4, PITCH_LETTER_C, 2, 5, 5, AA);  /* F-C-double-sharp */
    check(PITCH_LETTER_F, -1, 4, PITCH_LETTER_C, 1, 5, 5, AA);
    check(PITCH_LETTER_F, -2, 4, PITCH_LETTER_C, 1, 5, 5, AAA);
    check(PITCH_LETTER_F, 0, 4, PITCH_LETTER_C, -2, 5, 5, DD);
    check(PITCH_LETTER_F, 1, 4, PITCH_LETTER_C, -2, 5, 5, DDD);
    check(PITCH_LETTER_G, 0, 4, PITCH_LETTER_E, 2, 5, 6, AA);
    check(PITCH_LETTER_G, 1, 4, PITCH_LETTER_E, -2, 5, 6, DD);   /* G-sharp-E-double-flat */
    check(PITCH_LETTER_G, 2, 4, PITCH_LETTER_E, -2, 5, 6, DDD);  /* G-double-sharp-E-double-flat */
}

/* A fourth tritone and its inversion: 6 half steps either way. */
static void test_tritone(void) {
    check(PITCH_LETTER_F, 0, 4, PITCH_LETTER_B, 0, 4, 4, AUG); /* F-B */
    check(PITCH_LETTER_B, 0, 3, PITCH_LETTER_F, 0, 4, 5, DIM); /* B-F */
}

/* Example 9: compound intervals keep their quality. */
static void test_compound_quality(void) {
    check(PITCH_LETTER_A, 0, 2, PITCH_LETTER_C, 0, 3, 3, MI);
    check(PITCH_LETTER_A, 0, 2, PITCH_LETTER_C, 0, 4, 10, MI);
    check(PITCH_LETTER_C, 0, 4, PITCH_LETTER_C, 0, 5, 8, P);
    check(PITCH_LETTER_C, 0, 4, PITCH_LETTER_C, 0, 6, 15, P);
    check(PITCH_LETTER_C, 0, 4, PITCH_LETTER_D, 0, 5, 9, MA);
    check(PITCH_LETTER_C, 0, 4, PITCH_LETTER_F, 0, 5, 11, P);
    check(PITCH_LETTER_C, 0, 4, PITCH_LETTER_G, 0, 5, 12, P);
    check(PITCH_LETTER_C, 0, 4, PITCH_LETTER_A, 0, 5, 13, MA);
}

/* Property: raising the top note an octave never changes the quality, over
   every spelled pair within an octave. */
static void test_compound_keeps_quality_everywhere(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int checked = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int lower_accidental = -1; lower_accidental <= 1; lower_accidental++) {
            PitchSpelling *lower = spell(letters[l], lower_accidental, 3);
            for (size_t u = 0; u < 8; u++) {
                for (int upper_accidental = -1; upper_accidental <= 1; upper_accidental++) {
                    PitchLetter upper_letter = letters[(l + u) % 7];
                    int upper_octave = 3 + (int)((l + u) / 7);
                    PitchSpelling *upper = spell(upper_letter, upper_accidental, upper_octave);
                    PitchSpelling *upper_high = spell(upper_letter, upper_accidental, upper_octave + 1);

                    int size = 0;
                    int compound_size = 0;
                    Quality quality = P;
                    Quality compound_quality = P;
                    bool simple_ok = standard_tonality_interval_quality(lower, upper, &size, &quality);
                    bool compound_ok = standard_tonality_interval_quality(lower, upper_high, &compound_size,
                                                                           &compound_quality);
                    /* A reversed unison (C-sharp up to C-flat) is not an interval
                       but its octave is, so only the simple side gates. */
                    if (simple_ok) {
                        assert(compound_ok);
                        assert(compound_size == size + 7);
                        assert(compound_quality == quality);
                        checked++;
                    }
                    pitch_spelling_destroy(upper);
                    pitch_spelling_destroy(upper_high);
                }
            }
            pitch_spelling_destroy(lower);
        }
    }
    assert(checked > 100);
}

/* Property: the major-scale method and the semitone engine agree for every
   lower note whose major scale exists. */
static void test_method_and_engine_agree(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int compared = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int lower_accidental = -2; lower_accidental <= 2; lower_accidental++) {
            PitchSpelling *lower = spell(letters[l], lower_accidental, 3);
            for (size_t u = 0; u < 15; u++) {
                for (int upper_accidental = -2; upper_accidental <= 2; upper_accidental++) {
                    PitchLetter upper_letter = letters[(l + u) % 7];
                    int upper_octave = 3 + (int)((l + u) / 7);
                    PitchSpelling *upper = spell(upper_letter, upper_accidental, upper_octave);

                    int size = 0;
                    int scale_size = 0;
                    Quality quality = P;
                    Quality scale_quality = P;
                    bool engine = standard_tonality_interval_quality(lower, upper, &size, &quality);
                    bool method = standard_tonality_interval_quality_by_major_scale(lower, upper, &scale_size,
                                                                                    &scale_quality);
                    if (engine && method) {
                        assert(size == scale_size && quality == scale_quality);
                        /* In the scale exactly when perfect or major. */
                        assert(standard_tonality_interval_top_in_major_scale(lower, upper) == (quality == P || quality == MA));
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

static void test_reversed_or_invalid_intervals_are_rejected(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *b3 = spell(PITCH_LETTER_B, 0, 3);
    PitchSpelling *c_sharp4 = spell(PITCH_LETTER_C, 1, 4);
    PitchSpelling *c_flat4 = spell(PITCH_LETTER_C, -1, 4);
    int size = 42;
    Quality quality = (Quality)42;
    assert(!standard_tonality_interval_quality(c4, b3, &size, &quality));
    /* C-sharp up to C-flat: the same letter but a lower sound -- not an interval. */
    assert(!standard_tonality_interval_quality(c_sharp4, c_flat4, &size, &quality));
    assert(!standard_tonality_interval_quality(NULL, c4, &size, &quality));
    assert(!standard_tonality_interval_quality(c4, NULL, &size, &quality));
    assert(!standard_tonality_interval_quality(c4, c4, NULL, &quality));
    assert(!standard_tonality_interval_quality(c4, c4, &size, NULL));
    assert(size == 42 && quality == (Quality)42);

    /* An augmented unison is fine. */
    assert(standard_tonality_interval_quality(c4, c_sharp4, &size, &quality));
    assert(size == 1 && quality == AUG);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(b3);
    pitch_spelling_destroy(c_sharp4);
    pitch_spelling_destroy(c_flat4);
}

/* AUGMENTED / DIMINISHED: one half step larger or smaller. */
static void test_quality_shift(void) {
    Quality out = P;
    /* A perfect or major interval a half step larger is augmented. */
    assert(standard_tonality_interval_quality_shift(5, P, 1, &out) && out == AUG);
    assert(standard_tonality_interval_quality_shift(6, MA, 1, &out) && out == AUG);
    /* A major interval a half step smaller is minor. */
    assert(standard_tonality_interval_quality_shift(6, MA, -1, &out) && out == MI);
    /* A perfect or minor interval a half step smaller is diminished. */
    assert(standard_tonality_interval_quality_shift(5, P, -1, &out) && out == DIM);
    assert(standard_tonality_interval_quality_shift(6, MI, -1, &out) && out == DIM);
    /* A minor interval a half step larger is major. */
    assert(standard_tonality_interval_quality_shift(3, MI, 1, &out) && out == MA);
    /* Two or three half steps. */
    assert(standard_tonality_interval_quality_shift(5, P, 2, &out) && out == AA);
    assert(standard_tonality_interval_quality_shift(5, P, 3, &out) && out == AAA);
    assert(standard_tonality_interval_quality_shift(5, P, -2, &out) && out == DD);
    assert(standard_tonality_interval_quality_shift(5, P, -3, &out) && out == DDD);
    assert(standard_tonality_interval_quality_shift(6, MA, -3, &out) && out == DD);
    assert(standard_tonality_interval_quality_shift(6, MA, -4, &out) && out == DDD);
    /* No change. */
    assert(standard_tonality_interval_quality_shift(4, P, 0, &out) && out == P);
    /* Beyond triply altered, a quality that does not fit, a NULL output. */
    assert(!standard_tonality_interval_quality_shift(5, P, 4, &out));
    assert(!standard_tonality_interval_quality_shift(5, P, -4, &out));
    assert(!standard_tonality_interval_quality_shift(6, MA, -5, &out));
    assert(!standard_tonality_interval_quality_shift(5, MA, 1, &out)); /* no major fifth */
    assert(!standard_tonality_interval_quality_shift(3, P, 1, &out));  /* no perfect third */
    assert(!standard_tonality_interval_quality_shift(5, P, 1, NULL));
}

/* Property: one half step per step along the ladder, for every size. */
static void test_quality_ladder_is_a_half_step_per_step(void) {
    Quality perfect_ladder[] = {DDD, DD, DIM, P, AUG, AA, AAA};
    Quality major_ladder[] = {DDD, DD, DIM, MI, MA, AUG, AA, AAA};
    for (int size = 1; size <= 15; size++) {
        bool perfect = standard_tonality_interval_size_is_perfect_class(size);
        const Quality *ladder = perfect ? perfect_ladder : major_ladder;
        size_t count = perfect ? 7 : 8;
        int previous = 0;
        for (size_t i = 0; i < count; i++) {
            int semitones = 0;
            assert(standard_tonality_interval_semitones(size, ladder[i], &semitones));
            if (i > 0) {
                assert(semitones == previous + 1);
            }
            previous = semitones;
        }
    }
}

static void test_semitones(void) {
    int semitones = 0;
    assert(standard_tonality_interval_semitones(5, P, &semitones) && semitones == 7);
    assert(standard_tonality_interval_semitones(5, DIM, &semitones) && semitones == 6);
    assert(standard_tonality_interval_semitones(4, AUG, &semitones) && semitones == 6);
    assert(standard_tonality_interval_semitones(7, MA, &semitones) && semitones == 11);
    assert(standard_tonality_interval_semitones(8, P, &semitones) && semitones == 12);
    assert(standard_tonality_interval_semitones(3, MI, &semitones) && semitones == 3);
    assert(standard_tonality_interval_semitones(10, MI, &semitones) && semitones == 15); /* mi3 + an octave */
    assert(standard_tonality_interval_semitones(1, AUG, &semitones) && semitones == 1);
    assert(standard_tonality_interval_semitones(2, DIM, &semitones) && semitones == 0);
    assert(standard_tonality_interval_semitones(15, P, &semitones) && semitones == 24);
    assert(!standard_tonality_interval_semitones(5, MA, &semitones));
    assert(!standard_tonality_interval_semitones(0, P, &semitones));
    assert(!standard_tonality_interval_semitones(5, P, NULL));
}

int main(void) {
    test_rules_reference_framework();
    test_names_and_abbreviations();
    test_augmented_and_diminished_families();
    test_classes();
    test_major_interval_semitones();
    test_example_5();
    test_top_in_major_scale();
    test_example_6_augmented();
    test_example_7_diminished();
    test_example_8_ladders();
    test_doubly_and_triply_altered();
    test_tritone();
    test_compound_quality();
    test_compound_keeps_quality_everywhere();
    test_method_and_engine_agree();
    test_reversed_or_invalid_intervals_are_rejected();
    test_quality_shift();
    test_quality_ladder_is_a_half_step_per_step();
    test_semitones();
    printf("test_interval_quality_rules (standard-tonality): ok\n");
    return 0;
}
