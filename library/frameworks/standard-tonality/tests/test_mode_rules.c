#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "major_scale_rules.h"
#include "minor_scale_rules.h"
#include "mode_rules.h"
#include "spelling_test_util.h"

#define NOTES STANDARD_TONALITY_MODE_NOTE_COUNT

static void destroy_scale(PitchSpelling *scale[], size_t count) {
    for (size_t i = 0; i < count; i++) {
        pitch_spelling_destroy(scale[i]);
    }
}

static PitchLetter next_letter(PitchLetter letter) {
    switch (letter) {
        case PITCH_LETTER_C:
            return PITCH_LETTER_D;
        case PITCH_LETTER_D:
            return PITCH_LETTER_E;
        case PITCH_LETTER_E:
            return PITCH_LETTER_F;
        case PITCH_LETTER_F:
            return PITCH_LETTER_G;
        case PITCH_LETTER_G:
            return PITCH_LETTER_A;
        case PITCH_LETTER_A:
            return PITCH_LETTER_B;
        default:
            return PITCH_LETTER_C;
    }
}

static void check_mode(StandardTonalityMode mode, PitchLetter tonic_letter, int tonic_accidental,
                       const int accidentals[NOTES]) {
    PitchSpelling *tonic = spell(tonic_letter, tonic_accidental, 4);
    PitchSpelling *scale[NOTES];
    assert(standard_tonality_build_mode_scale(mode, tonic, scale));

    /* Each letter once in turn from the tonic's. */
    PitchLetter letter = tonic_letter;
    for (size_t i = 0; i < NOTES; i++) {
        assert(pitch_spelling_letter(scale[i]) == letter);
        assert(pitch_spelling_accidental(scale[i]) == accidentals[i]);
        letter = next_letter(letter);
    }
    assert(standard_tonality_is_mode_scale(mode, (const PitchSpelling *const *)scale, NOTES));
    destroy_scale(scale, NOTES);
    pitch_spelling_destroy(tonic);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_MODE_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_mode_rule((StandardTonalityModeRule)i);
        const Constraint *constraint = standard_tonality_mode_constraint((StandardTonalityModeRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_mode_rule(STANDARD_TONALITY_MODE_RULE_COUNT) == NULL);
}

static void test_names(void) {
    const char *expected[] = {"lydian", "ionian", "mixolydian", "dorian", "aeolian", "phrygian", "locrian"};
    for (int mode = 0; mode < STANDARD_TONALITY_MODE_COUNT; mode++) {
        assert(strcmp(standard_tonality_mode_name((StandardTonalityMode)mode), expected[mode]) == 0);
    }
    assert(standard_tonality_mode_name((StandardTonalityMode)99) == NULL);
    assert(standard_tonality_mode_name((StandardTonalityMode)-1) == NULL);
}

/* W = 2, H = 1. */
static void test_step_patterns(void) {
    int expected[STANDARD_TONALITY_MODE_COUNT][7] = {
        {2, 2, 2, 1, 2, 2, 1}, /* lydian     W-W-W-H-W-W-H */
        {2, 2, 1, 2, 2, 2, 1}, /* ionian     W-W-H-W-W-W-H */
        {2, 2, 1, 2, 2, 1, 2}, /* mixolydian W-W-H-W-W-H-W */
        {2, 1, 2, 2, 2, 1, 2}, /* dorian     W-H-W-W-W-H-W */
        {2, 1, 2, 2, 1, 2, 2}, /* aeolian    W-H-W-W-H-W-W */
        {1, 2, 2, 2, 1, 2, 2}, /* phrygian   H-W-W-W-H-W-W */
        {1, 2, 2, 1, 2, 2, 2}, /* locrian    H-W-W-H-W-W-W */
    };
    for (int mode = 0; mode < STANDARD_TONALITY_MODE_COUNT; mode++) {
        int total = 0;
        for (size_t i = 0; i < 7; i++) {
            assert(standard_tonality_mode_step((StandardTonalityMode)mode, i) == expected[mode][i]);
            total += expected[mode][i];
        }
        assert(total == 12);
    }
}

/* Ionian is the major scale and aeolian the natural minor scale. */
static void test_ionian_is_major_and_aeolian_is_natural_minor(void) {
    for (size_t i = 0; i < 7; i++) {
        assert(standard_tonality_mode_step(STANDARD_TONALITY_MODE_IONIAN, i) == standard_tonality_major_scale_step(i));
        assert(standard_tonality_mode_step(STANDARD_TONALITY_MODE_AEOLIAN, i) ==
               standard_tonality_minor_scale_step(STANDARD_TONALITY_MINOR_NATURAL, i));
    }

    PitchSpelling *tonic = spell(PITCH_LETTER_E, -1, 4);
    PitchSpelling *mode[NOTES];
    PitchSpelling *major[8];
    PitchSpelling *minor[8];
    assert(standard_tonality_build_mode_scale(STANDARD_TONALITY_MODE_IONIAN, tonic, mode));
    assert(standard_tonality_build_major_scale(tonic, major));
    for (size_t i = 0; i < NOTES; i++) {
        assert(pitch_spelling_equals(mode[i], major[i]));
    }
    destroy_scale(mode, NOTES);
    destroy_scale(major, 8);

    assert(standard_tonality_build_mode_scale(STANDARD_TONALITY_MODE_AEOLIAN, tonic, mode));
    assert(standard_tonality_build_minor_scale(STANDARD_TONALITY_MINOR_NATURAL, tonic, minor));
    for (size_t i = 0; i < NOTES; i++) {
        assert(pitch_spelling_equals(mode[i], minor[i]));
    }
    destroy_scale(mode, NOTES);
    destroy_scale(minor, 8);
    pitch_spelling_destroy(tonic);
}

/* Examples 2 to 8 and 9: every mode on C. */
static void test_modes_on_c(void) {
    int lydian[NOTES] = {0, 0, 0, 1, 0, 0, 0, 0};      /* F-sharp */
    int ionian[NOTES] = {0, 0, 0, 0, 0, 0, 0, 0};
    int mixolydian[NOTES] = {0, 0, 0, 0, 0, 0, -1, 0}; /* B-flat */
    int dorian[NOTES] = {0, 0, -1, 0, 0, 0, -1, 0};    /* E-flat B-flat */
    int aeolian[NOTES] = {0, 0, -1, 0, 0, -1, -1, 0};  /* E-flat A-flat B-flat */
    int phrygian[NOTES] = {0, -1, -1, 0, 0, -1, -1, 0};
    int locrian[NOTES] = {0, -1, -1, 0, -1, -1, -1, 0}; /* G-flat too */

    check_mode(STANDARD_TONALITY_MODE_LYDIAN, PITCH_LETTER_C, 0, lydian);
    check_mode(STANDARD_TONALITY_MODE_IONIAN, PITCH_LETTER_C, 0, ionian);
    check_mode(STANDARD_TONALITY_MODE_MIXOLYDIAN, PITCH_LETTER_C, 0, mixolydian);
    check_mode(STANDARD_TONALITY_MODE_DORIAN, PITCH_LETTER_C, 0, dorian);
    check_mode(STANDARD_TONALITY_MODE_AEOLIAN, PITCH_LETTER_C, 0, aeolian);
    check_mode(STANDARD_TONALITY_MODE_PHRYGIAN, PITCH_LETTER_C, 0, phrygian);
    check_mode(STANDARD_TONALITY_MODE_LOCRIAN, PITCH_LETTER_C, 0, locrian);
}

/* Each mode starts on any note: mixolydian on D-flat, aeolian on G-flat,
   lydian on F-sharp. */
static void test_modes_on_any_note(void) {
    int mixolydian_d_flat[NOTES] = {-1, -1, 0, -1, -1, -1, -1, -1};
    int aeolian_g_flat[NOTES] = {-1, -1, -2, -1, -1, -2, -1, -1}; /* B and E double-flat */
    int lydian_f_sharp[NOTES] = {1, 1, 1, 1, 1, 1, 1, 1};         /* E-sharp, B-sharp too */

    check_mode(STANDARD_TONALITY_MODE_MIXOLYDIAN, PITCH_LETTER_D, -1, mixolydian_d_flat);
    check_mode(STANDARD_TONALITY_MODE_AEOLIAN, PITCH_LETTER_G, -1, aeolian_g_flat);
    check_mode(STANDARD_TONALITY_MODE_LYDIAN, PITCH_LETTER_F, 1, lydian_f_sharp);
}

/* A scale of one mode is a scale of no other mode. */
static void test_modes_are_distinct(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_D, 0, 4);
    for (int built = 0; built < STANDARD_TONALITY_MODE_COUNT; built++) {
        PitchSpelling *scale[NOTES];
        assert(standard_tonality_build_mode_scale((StandardTonalityMode)built, tonic, scale));
        for (int checked = 0; checked < STANDARD_TONALITY_MODE_COUNT; checked++) {
            assert(standard_tonality_is_mode_scale((StandardTonalityMode)checked,
                                                   (const PitchSpelling *const *)scale, NOTES) == (built == checked));
        }
        destroy_scale(scale, NOTES);
    }
    pitch_spelling_destroy(tonic);
}

static void test_brightness(void) {
    for (int mode = 0; mode < STANDARD_TONALITY_MODE_COUNT; mode++) {
        assert(standard_tonality_mode_brightness_rank((StandardTonalityMode)mode) == mode + 1);
    }
    assert(standard_tonality_mode_brightness_rank((StandardTonalityMode)99) == 0);

    assert(standard_tonality_mode_is_brighter(STANDARD_TONALITY_MODE_LYDIAN, STANDARD_TONALITY_MODE_IONIAN));
    assert(standard_tonality_mode_is_brighter(STANDARD_TONALITY_MODE_IONIAN, STANDARD_TONALITY_MODE_MIXOLYDIAN));
    assert(standard_tonality_mode_is_brighter(STANDARD_TONALITY_MODE_LYDIAN, STANDARD_TONALITY_MODE_LOCRIAN));
    assert(!standard_tonality_mode_is_brighter(STANDARD_TONALITY_MODE_LOCRIAN, STANDARD_TONALITY_MODE_LYDIAN));
    assert(!standard_tonality_mode_is_brighter(STANDARD_TONALITY_MODE_DORIAN, STANDARD_TONALITY_MODE_DORIAN));
    assert(!standard_tonality_mode_is_brighter((StandardTonalityMode)99, STANDARD_TONALITY_MODE_DORIAN));
}

/* Property: the darker the mode, the lower the sum of its degrees against
   ionian -- brightness is a continuum, not just a label. */
static void test_brightness_tracks_lowered_degrees(void) {
    int previous_total = 100;
    for (int mode = 0; mode < STANDARD_TONALITY_MODE_COUNT; mode++) {
        int degrees[6];
        int offsets[6];
        size_t count = standard_tonality_mode_alterations((StandardTonalityMode)mode,
                                                          STANDARD_TONALITY_MODE_REFERENCE_IONIAN, degrees, offsets);
        int total = 0;
        for (size_t i = 0; i < count; i++) {
            total += offsets[i];
        }
        assert(total < previous_total);
        previous_total = total;
    }
}

/* THIRD: lydian, ionian, mixolydian have mi (major third); the other four
   have me (minor third). */
static void test_bright_and_dark_modes(void) {
    StandardTonalityMode bright[] = {STANDARD_TONALITY_MODE_LYDIAN, STANDARD_TONALITY_MODE_IONIAN,
                                     STANDARD_TONALITY_MODE_MIXOLYDIAN};
    StandardTonalityMode dark[] = {STANDARD_TONALITY_MODE_DORIAN, STANDARD_TONALITY_MODE_AEOLIAN,
                                   STANDARD_TONALITY_MODE_PHRYGIAN, STANDARD_TONALITY_MODE_LOCRIAN};
    for (size_t i = 0; i < 3; i++) {
        assert(standard_tonality_mode_is_bright(bright[i]));
        assert(!standard_tonality_mode_is_dark(bright[i]));
        assert(standard_tonality_mode_third_semitones(bright[i]) == 4);
    }
    for (size_t i = 0; i < 4; i++) {
        assert(standard_tonality_mode_is_dark(dark[i]));
        assert(!standard_tonality_mode_is_bright(dark[i]));
        assert(standard_tonality_mode_third_semitones(dark[i]) == 3);
    }
    assert(!standard_tonality_mode_is_bright((StandardTonalityMode)99));
    assert(!standard_tonality_mode_is_dark((StandardTonalityMode)99));
    assert(standard_tonality_mode_third_semitones((StandardTonalityMode)99) == 0);

    /* Bright modes are brighter than every dark mode. */
    for (size_t b = 0; b < 3; b++) {
        for (size_t d = 0; d < 4; d++) {
            assert(standard_tonality_mode_is_brighter(bright[b], dark[d]));
        }
    }
}

static void check_alterations(StandardTonalityMode mode, StandardTonalityModeReference reference, size_t count,
                              const int want_degrees[], const int want_offsets[]) {
    int degrees[6] = {0};
    int offsets[6] = {0};
    assert(standard_tonality_mode_alterations(mode, reference, degrees, offsets) == count);
    for (size_t i = 0; i < count; i++) {
        assert(degrees[i] == want_degrees[i]);
        assert(offsets[i] == want_offsets[i]);
    }
}

/* Example 1: alterations against ionian. */
static void test_alterations_against_ionian(void) {
    const StandardTonalityModeReference ionian = STANDARD_TONALITY_MODE_REFERENCE_IONIAN;
    check_alterations(STANDARD_TONALITY_MODE_LYDIAN, ionian, 1, (int[]){4}, (int[]){1});
    check_alterations(STANDARD_TONALITY_MODE_IONIAN, ionian, 0, NULL, NULL);
    check_alterations(STANDARD_TONALITY_MODE_MIXOLYDIAN, ionian, 1, (int[]){7}, (int[]){-1});
    check_alterations(STANDARD_TONALITY_MODE_DORIAN, ionian, 2, (int[]){3, 7}, (int[]){-1, -1});
    check_alterations(STANDARD_TONALITY_MODE_AEOLIAN, ionian, 3, (int[]){3, 6, 7}, (int[]){-1, -1, -1});
    check_alterations(STANDARD_TONALITY_MODE_PHRYGIAN, ionian, 4, (int[]){2, 3, 6, 7}, (int[]){-1, -1, -1, -1});
    check_alterations(STANDARD_TONALITY_MODE_LOCRIAN, ionian, 5, (int[]){2, 3, 5, 6, 7},
                      (int[]){-1, -1, -1, -1, -1});
}

/* The dark modes against natural minor: dorian raises 6, phrygian lowers
   2, locrian lowers 2 and 5. */
static void test_alterations_against_aeolian(void) {
    const StandardTonalityModeReference aeolian = STANDARD_TONALITY_MODE_REFERENCE_AEOLIAN;
    check_alterations(STANDARD_TONALITY_MODE_DORIAN, aeolian, 1, (int[]){6}, (int[]){1});
    check_alterations(STANDARD_TONALITY_MODE_AEOLIAN, aeolian, 0, NULL, NULL);
    check_alterations(STANDARD_TONALITY_MODE_PHRYGIAN, aeolian, 1, (int[]){2}, (int[]){-1});
    check_alterations(STANDARD_TONALITY_MODE_LOCRIAN, aeolian, 2, (int[]){2, 5}, (int[]){-1, -1});
    check_alterations(STANDARD_TONALITY_MODE_IONIAN, aeolian, 3, (int[]){3, 6, 7}, (int[]){1, 1, 1});
    check_alterations(STANDARD_TONALITY_MODE_LYDIAN, aeolian, 4, (int[]){3, 4, 6, 7}, (int[]){1, 1, 1, 1});
    check_alterations(STANDARD_TONALITY_MODE_MIXOLYDIAN, aeolian, 2, (int[]){3, 6}, (int[]){1, 1});
}

static void test_alterations_reject_invalid_arguments(void) {
    int degrees[6];
    int offsets[6];
    assert(standard_tonality_mode_alterations((StandardTonalityMode)99, STANDARD_TONALITY_MODE_REFERENCE_IONIAN,
                                              degrees, offsets) == 0);
    assert(standard_tonality_mode_alterations(STANDARD_TONALITY_MODE_DORIAN,
                                              (StandardTonalityModeReference)99, degrees, offsets) == 0);
    assert(standard_tonality_mode_alterations(STANDARD_TONALITY_MODE_DORIAN, STANDARD_TONALITY_MODE_REFERENCE_IONIAN,
                                              NULL, offsets) == 0);
    assert(standard_tonality_mode_alterations(STANDARD_TONALITY_MODE_DORIAN, STANDARD_TONALITY_MODE_REFERENCE_IONIAN,
                                              degrees, NULL) == 0);
}

/* PARALLEL: modes on one tonic are parallel; the octave does not matter. */
static void test_parallel_modes(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *c5 = spell(PITCH_LETTER_C, 0, 5);
    PitchSpelling *c_sharp = spell(PITCH_LETTER_C, 1, 4);
    PitchSpelling *d = spell(PITCH_LETTER_D, 0, 4);
    assert(standard_tonality_modes_parallel(c4, c5));
    assert(!standard_tonality_modes_parallel(c4, c_sharp));
    assert(!standard_tonality_modes_parallel(c4, d));
    assert(!standard_tonality_modes_parallel(c4, NULL));

    /* Every mode built on one tonic starts on that tonic. */
    for (int mode = 0; mode < STANDARD_TONALITY_MODE_COUNT; mode++) {
        PitchSpelling *scale[NOTES];
        assert(standard_tonality_build_mode_scale((StandardTonalityMode)mode, c4, scale));
        assert(standard_tonality_modes_parallel(scale[0], c4));
        assert(standard_tonality_modes_parallel(scale[7], c4));
        destroy_scale(scale, NOTES);
    }
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(c5);
    pitch_spelling_destroy(c_sharp);
    pitch_spelling_destroy(d);
}

static void test_invalid_arguments(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *scale[NOTES] = {NULL};
    assert(!standard_tonality_build_mode_scale((StandardTonalityMode)99, tonic, scale));
    assert(!standard_tonality_build_mode_scale(STANDARD_TONALITY_MODE_DORIAN, NULL, scale));
    assert(scale[0] == NULL);
    assert(!standard_tonality_is_mode_scale((StandardTonalityMode)99, NULL, NOTES));
    assert(!standard_tonality_is_mode_scale(STANDARD_TONALITY_MODE_DORIAN, NULL, NOTES));
    const PitchSpelling *one[] = {tonic};
    assert(!standard_tonality_is_mode_scale(STANDARD_TONALITY_MODE_DORIAN, one, 1));
    pitch_spelling_destroy(tonic);
}

/* Beyond a double accidental: locrian on C double-flat needs D triple-flat. */
static void test_build_rejects_modes_beyond_double_accidentals(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_C, -2, 4);
    PitchSpelling *scale[NOTES] = {NULL};
    assert(!standard_tonality_build_mode_scale(STANDARD_TONALITY_MODE_LOCRIAN, tonic, scale));
    for (size_t i = 0; i < NOTES; i++) {
        assert(scale[i] == NULL);
    }
    pitch_spelling_destroy(tonic);
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_step_patterns();
    test_ionian_is_major_and_aeolian_is_natural_minor();
    test_modes_on_c();
    test_modes_on_any_note();
    test_modes_are_distinct();
    test_brightness();
    test_brightness_tracks_lowered_degrees();
    test_bright_and_dark_modes();
    test_alterations_against_ionian();
    test_alterations_against_aeolian();
    test_alterations_reject_invalid_arguments();
    test_parallel_modes();
    test_invalid_arguments();
    test_build_rejects_modes_beyond_double_accidentals();
    printf("test_mode_rules (standard-tonality): ok\n");
    return 0;
}
