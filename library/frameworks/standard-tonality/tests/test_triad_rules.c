#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "spelling_test_util.h"
#include "triad_rules.h"

typedef StandardTonalityIntervalQuality IntervalQuality;
typedef StandardTonalityTriadQuality TriadQuality;

#define MA STANDARD_TONALITY_QUALITY_MAJOR
#define MI STANDARD_TONALITY_QUALITY_MINOR
#define P STANDARD_TONALITY_QUALITY_PERFECT
#define AUG STANDARD_TONALITY_QUALITY_AUGMENTED
#define DIM STANDARD_TONALITY_QUALITY_DIMINISHED

static bool stack_quality(PitchLetter root_letter, int root_accidental, int root_octave, PitchLetter third_letter,
                          int third_accidental, int third_octave, PitchLetter fifth_letter, int fifth_accidental,
                          int fifth_octave, TriadQuality *quality) {
    PitchSpelling *root = spell(root_letter, root_accidental, root_octave);
    PitchSpelling *third = spell(third_letter, third_accidental, third_octave);
    PitchSpelling *fifth = spell(fifth_letter, fifth_accidental, fifth_octave);
    bool result = standard_tonality_triad_stack_quality(root, third, fifth, quality);
    pitch_spelling_destroy(root);
    pitch_spelling_destroy(third);
    pitch_spelling_destroy(fifth);
    return result;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_TRIAD_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_triad_rule((StandardTonalityTriadRule)i);
        const Constraint *constraint = standard_tonality_triad_constraint((StandardTonalityTriadRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_triad_rule(STANDARD_TONALITY_TRIAD_RULE_COUNT) == NULL);
}

static void test_names(void) {
    assert(strcmp(standard_tonality_triad_quality_name(STANDARD_TONALITY_TRIAD_MAJOR), "major") == 0);
    assert(strcmp(standard_tonality_triad_quality_name(STANDARD_TONALITY_TRIAD_MINOR), "minor") == 0);
    assert(strcmp(standard_tonality_triad_quality_name(STANDARD_TONALITY_TRIAD_DIMINISHED), "diminished") == 0);
    assert(strcmp(standard_tonality_triad_quality_name(STANDARD_TONALITY_TRIAD_AUGMENTED), "augmented") == 0);
    assert(standard_tonality_triad_quality_name((TriadQuality)99) == NULL);

    assert(strcmp(standard_tonality_triad_member_name(STANDARD_TONALITY_TRIAD_ROOT), "root") == 0);
    assert(strcmp(standard_tonality_triad_member_name(STANDARD_TONALITY_TRIAD_THIRD), "third") == 0);
    assert(strcmp(standard_tonality_triad_member_name(STANDARD_TONALITY_TRIAD_FIFTH), "fifth") == 0);
    assert(standard_tonality_triad_member_name((StandardTonalityTriadMember)99) == NULL);
}

/* ROOT_THIRD_FIFTH: a generic third and fifth above the root. */
static void test_member_sizes(void) {
    assert(standard_tonality_triad_member_size(STANDARD_TONALITY_TRIAD_ROOT) == 1);
    assert(standard_tonality_triad_member_size(STANDARD_TONALITY_TRIAD_THIRD) == 3);
    assert(standard_tonality_triad_member_size(STANDARD_TONALITY_TRIAD_FIFTH) == 5);
    assert(standard_tonality_triad_member_size((StandardTonalityTriadMember)99) == 0);
}

/* QUALITIES: major ma3 P5; minor mi3 P5; diminished mi3 d5; augmented ma3
   A5. */
static void test_qualities_by_intervals(void) {
    IntervalQuality third = P;
    IntervalQuality fifth = P;
    assert(standard_tonality_triad_intervals(STANDARD_TONALITY_TRIAD_MAJOR, &third, &fifth));
    assert(third == MA && fifth == P);
    assert(standard_tonality_triad_intervals(STANDARD_TONALITY_TRIAD_MINOR, &third, &fifth));
    assert(third == MI && fifth == P);
    assert(standard_tonality_triad_intervals(STANDARD_TONALITY_TRIAD_DIMINISHED, &third, &fifth));
    assert(third == MI && fifth == DIM);
    assert(standard_tonality_triad_intervals(STANDARD_TONALITY_TRIAD_AUGMENTED, &third, &fifth));
    assert(third == MA && fifth == AUG);

    assert(!standard_tonality_triad_intervals((TriadQuality)99, &third, &fifth));
    assert(!standard_tonality_triad_intervals(STANDARD_TONALITY_TRIAD_MAJOR, NULL, &fifth));
    assert(!standard_tonality_triad_intervals(STANDARD_TONALITY_TRIAD_MAJOR, &third, NULL));
}

static void test_quality_from_intervals(void) {
    TriadQuality quality = (TriadQuality)42;
    assert(standard_tonality_triad_quality_from_intervals(MA, P, &quality) && quality == STANDARD_TONALITY_TRIAD_MAJOR);
    assert(standard_tonality_triad_quality_from_intervals(MI, P, &quality) && quality == STANDARD_TONALITY_TRIAD_MINOR);
    assert(standard_tonality_triad_quality_from_intervals(MI, DIM, &quality) &&
           quality == STANDARD_TONALITY_TRIAD_DIMINISHED);
    assert(standard_tonality_triad_quality_from_intervals(MA, AUG, &quality) &&
           quality == STANDARD_TONALITY_TRIAD_AUGMENTED);

    /* Not triads: the pairs the four qualities leave out. */
    assert(!standard_tonality_triad_quality_from_intervals(MA, DIM, &quality));
    assert(!standard_tonality_triad_quality_from_intervals(MI, AUG, &quality));
    assert(!standard_tonality_triad_quality_from_intervals(AUG, P, &quality));
    assert(!standard_tonality_triad_quality_from_intervals(MA, P, NULL));
    assert(quality == STANDARD_TONALITY_TRIAD_AUGMENTED);
}

/* Property: intervals and quality round-trip for all four. */
static void test_intervals_round_trip(void) {
    for (int q = 0; q < STANDARD_TONALITY_TRIAD_QUALITY_COUNT; q++) {
        IntervalQuality third;
        IntervalQuality fifth;
        TriadQuality back;
        assert(standard_tonality_triad_intervals((TriadQuality)q, &third, &fifth));
        assert(standard_tonality_triad_quality_from_intervals(third, fifth, &back));
        assert(back == (TriadQuality)q);
    }
}

/* NAMING: major and minor for the third; diminished and augmented for the
   fifth. */
static void test_naming(void) {
    int size = 0;
    IntervalQuality quality = P;
    assert(standard_tonality_triad_naming_interval(STANDARD_TONALITY_TRIAD_MAJOR, &size, &quality));
    assert(size == 3 && quality == MA);
    assert(standard_tonality_triad_naming_interval(STANDARD_TONALITY_TRIAD_MINOR, &size, &quality));
    assert(size == 3 && quality == MI);
    assert(standard_tonality_triad_naming_interval(STANDARD_TONALITY_TRIAD_DIMINISHED, &size, &quality));
    assert(size == 5 && quality == DIM);
    assert(standard_tonality_triad_naming_interval(STANDARD_TONALITY_TRIAD_AUGMENTED, &size, &quality));
    assert(size == 5 && quality == AUG);

    assert(standard_tonality_triad_named_for_third(STANDARD_TONALITY_TRIAD_MAJOR));
    assert(standard_tonality_triad_named_for_third(STANDARD_TONALITY_TRIAD_MINOR));
    assert(!standard_tonality_triad_named_for_third(STANDARD_TONALITY_TRIAD_DIMINISHED));
    assert(standard_tonality_triad_named_for_fifth(STANDARD_TONALITY_TRIAD_DIMINISHED));
    assert(standard_tonality_triad_named_for_fifth(STANDARD_TONALITY_TRIAD_AUGMENTED));
    assert(!standard_tonality_triad_named_for_fifth(STANDARD_TONALITY_TRIAD_MAJOR));

    assert(!standard_tonality_triad_naming_interval((TriadQuality)99, &size, &quality));
    assert(!standard_tonality_triad_naming_interval(STANDARD_TONALITY_TRIAD_MAJOR, NULL, &quality));
    assert(!standard_tonality_triad_naming_interval(STANDARD_TONALITY_TRIAD_MAJOR, &size, NULL));
}

/* Property: a major or minor triad is named for the third and both have a
   perfect fifth; a diminished or augmented triad is named for the fifth. */
static void test_naming_matches_the_intervals(void) {
    for (int q = 0; q < STANDARD_TONALITY_TRIAD_QUALITY_COUNT; q++) {
        IntervalQuality third;
        IntervalQuality fifth;
        int size;
        IntervalQuality named;
        assert(standard_tonality_triad_intervals((TriadQuality)q, &third, &fifth));
        assert(standard_tonality_triad_naming_interval((TriadQuality)q, &size, &named));
        if (standard_tonality_triad_named_for_third((TriadQuality)q)) {
            assert(size == 3 && named == third && fifth == P);
        } else {
            assert(size == 5 && named == fifth);
        }
        assert(standard_tonality_triad_named_for_third((TriadQuality)q) !=
               standard_tonality_triad_named_for_fifth((TriadQuality)q));
    }
}

/* STACKED_IN_THIRDS. */
static void test_stack(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *e4 = spell(PITCH_LETTER_E, 0, 4);
    PitchSpelling *g4 = spell(PITCH_LETTER_G, 0, 4);
    PitchSpelling *c5 = spell(PITCH_LETTER_C, 0, 5);
    PitchSpelling *e_flat4 = spell(PITCH_LETTER_E, -1, 4);
    PitchSpelling *g_sharp4 = spell(PITCH_LETTER_G, 1, 4);

    assert(standard_tonality_is_triad_stack(c4, e4, g4));
    assert(standard_tonality_is_triad_stack(c4, e_flat4, g_sharp4)); /* accidentals do not matter */
    assert(!standard_tonality_is_triad_stack(e4, g4, c5));           /* the fifth is a sixth */
    assert(!standard_tonality_is_triad_stack(c4, g4, e4));           /* out of order */
    assert(!standard_tonality_is_triad_stack(c4, e4, c5));
    assert(!standard_tonality_is_triad_stack(NULL, e4, g4));
    assert(!standard_tonality_is_triad_stack(c4, NULL, g4));
    assert(!standard_tonality_is_triad_stack(c4, e4, NULL));

    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(e4);
    pitch_spelling_destroy(g4);
    pitch_spelling_destroy(c5);
    pitch_spelling_destroy(e_flat4);
    pitch_spelling_destroy(g_sharp4);
}

/* LINES_OR_SPACES: Example 1, a triad on three adjacent lines and one on
   three adjacent spaces. */
static void test_lines_or_spaces(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *e4 = spell(PITCH_LETTER_E, 0, 4);
    PitchSpelling *g4 = spell(PITCH_LETTER_G, 0, 4);
    PitchSpelling *d4 = spell(PITCH_LETTER_D, 0, 4);
    PitchSpelling *f4 = spell(PITCH_LETTER_F, 0, 4);
    PitchSpelling *a4 = spell(PITCH_LETTER_A, 0, 4);

    assert(standard_tonality_triad_on_lines_or_spaces(c4, e4, g4));
    assert(standard_tonality_triad_on_lines_or_spaces(d4, f4, a4));
    assert(!standard_tonality_triad_on_lines_or_spaces(c4, d4, e4));
    assert(!standard_tonality_triad_on_lines_or_spaces(c4, e4, f4));
    assert(!standard_tonality_triad_on_lines_or_spaces(NULL, e4, g4));

    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(e4);
    pitch_spelling_destroy(g4);
    pitch_spelling_destroy(d4);
    pitch_spelling_destroy(f4);
    pitch_spelling_destroy(a4);
}

/* Property: every stack in thirds, from any letter and octave, is all on
   lines or all on spaces. */
static void test_every_stack_is_lines_or_spaces(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    for (int octave = 1; octave <= 6; octave++) {
        for (size_t l = 0; l < 7; l++) {
            PitchSpelling *root = spell(letters[l], 0, octave);
            PitchSpelling *third = spell(letters[(l + 2) % 7], 0, octave + (int)((l + 2) / 7));
            PitchSpelling *fifth = spell(letters[(l + 4) % 7], 0, octave + (int)((l + 4) / 7));
            assert(standard_tonality_is_triad_stack(root, third, fifth));
            assert(standard_tonality_triad_on_lines_or_spaces(root, third, fifth));
            pitch_spelling_destroy(root);
            pitch_spelling_destroy(third);
            pitch_spelling_destroy(fifth);
        }
    }
}

static void test_stack_quality(void) {
    TriadQuality quality = (TriadQuality)42;
    assert(stack_quality(PITCH_LETTER_C, 0, 4, PITCH_LETTER_E, 0, 4, PITCH_LETTER_G, 0, 4, &quality));
    assert(quality == STANDARD_TONALITY_TRIAD_MAJOR);
    assert(stack_quality(PITCH_LETTER_C, 0, 4, PITCH_LETTER_E, -1, 4, PITCH_LETTER_G, 0, 4, &quality));
    assert(quality == STANDARD_TONALITY_TRIAD_MINOR);
    assert(stack_quality(PITCH_LETTER_C, 0, 4, PITCH_LETTER_E, -1, 4, PITCH_LETTER_G, -1, 4, &quality));
    assert(quality == STANDARD_TONALITY_TRIAD_DIMINISHED);
    assert(stack_quality(PITCH_LETTER_C, 0, 4, PITCH_LETTER_E, 0, 4, PITCH_LETTER_G, 1, 4, &quality));
    assert(quality == STANDARD_TONALITY_TRIAD_AUGMENTED);

    /* A major third with a diminished fifth is no triad; neither is a non-stack. */
    quality = (TriadQuality)42;
    assert(!stack_quality(PITCH_LETTER_C, 0, 4, PITCH_LETTER_E, 0, 4, PITCH_LETTER_G, -1, 4, &quality));
    assert(!stack_quality(PITCH_LETTER_C, 0, 4, PITCH_LETTER_D, 0, 4, PITCH_LETTER_E, 0, 4, &quality));
    assert(quality == (TriadQuality)42);
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_member_sizes();
    test_qualities_by_intervals();
    test_quality_from_intervals();
    test_intervals_round_trip();
    test_naming();
    test_naming_matches_the_intervals();
    test_stack();
    test_lines_or_spaces();
    test_every_stack_is_lines_or_spaces();
    test_stack_quality();
    printf("test_triad_rules (standard-tonality): ok\n");
    return 0;
}
