#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "seventh_rules.h"
#include "spelling_test_util.h"

typedef StandardTonalitySeventhQuality SeventhQuality;

#define MAMA STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR
#define MAMI STANDARD_TONALITY_SEVENTH_MAJOR_MINOR
#define MIMI STANDARD_TONALITY_SEVENTH_MINOR_MINOR
#define HDIM STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED
#define FDIM STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED

typedef struct {
    PitchLetter letter;
    int accidental;
    int octave;
} Note;

static bool stack_quality(const Note *n, SeventhQuality *quality) {
    PitchSpelling *root = spell(n[0].letter, n[0].accidental, n[0].octave);
    PitchSpelling *third = spell(n[1].letter, n[1].accidental, n[1].octave);
    PitchSpelling *fifth = spell(n[2].letter, n[2].accidental, n[2].octave);
    PitchSpelling *seventh = spell(n[3].letter, n[3].accidental, n[3].octave);
    bool result = standard_tonality_seventh_stack_quality(root, third, fifth, seventh, quality);
    pitch_spelling_destroy(root);
    pitch_spelling_destroy(third);
    pitch_spelling_destroy(fifth);
    pitch_spelling_destroy(seventh);
    return result;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_SEVENTH_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_seventh_rule((StandardTonalitySeventhRule)i);
        const Constraint *constraint = standard_tonality_seventh_constraint((StandardTonalitySeventhRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_seventh_rule(STANDARD_TONALITY_SEVENTH_RULE_COUNT) == NULL);
}

/* NAMING: triad quality then seventh quality; ALTERNATE_NAMES. */
static void test_names(void) {
    assert(strcmp(standard_tonality_seventh_quality_name(MAMA), "major-major seventh") == 0);
    assert(strcmp(standard_tonality_seventh_quality_name(MAMI), "major-minor seventh") == 0);
    assert(strcmp(standard_tonality_seventh_quality_name(MIMI), "minor-minor seventh") == 0);
    assert(strcmp(standard_tonality_seventh_quality_name(HDIM), "half-diminished seventh") == 0);
    assert(strcmp(standard_tonality_seventh_quality_name(FDIM), "fully diminished seventh") == 0);
    assert(standard_tonality_seventh_quality_name((SeventhQuality)99) == NULL);

    assert(strcmp(standard_tonality_seventh_quality_alternate_name(MAMA), "major seventh") == 0);
    assert(strcmp(standard_tonality_seventh_quality_alternate_name(MAMI), "dominant seventh") == 0);
    assert(strcmp(standard_tonality_seventh_quality_alternate_name(MIMI), "minor seventh") == 0);
    assert(standard_tonality_seventh_quality_alternate_name(HDIM) == NULL); /* none */
    assert(strcmp(standard_tonality_seventh_quality_alternate_name(FDIM), "diminished seventh") == 0);
    assert(standard_tonality_seventh_quality_alternate_name((SeventhQuality)99) == NULL);

    assert(strcmp(standard_tonality_seventh_member_name(STANDARD_TONALITY_SEVENTH_ROOT), "root") == 0);
    assert(strcmp(standard_tonality_seventh_member_name(STANDARD_TONALITY_SEVENTH_THIRD), "third") == 0);
    assert(strcmp(standard_tonality_seventh_member_name(STANDARD_TONALITY_SEVENTH_FIFTH), "fifth") == 0);
    assert(strcmp(standard_tonality_seventh_member_name(STANDARD_TONALITY_SEVENTH_SEVENTH), "seventh") == 0);
    assert(standard_tonality_seventh_member_name((StandardTonalitySeventhMember)99) == NULL);
}

static void test_member_sizes(void) {
    assert(standard_tonality_seventh_member_size(STANDARD_TONALITY_SEVENTH_ROOT) == 1);
    assert(standard_tonality_seventh_member_size(STANDARD_TONALITY_SEVENTH_THIRD) == 3);
    assert(standard_tonality_seventh_member_size(STANDARD_TONALITY_SEVENTH_FIFTH) == 5);
    assert(standard_tonality_seventh_member_size(STANDARD_TONALITY_SEVENTH_SEVENTH) == 7);
    assert(standard_tonality_seventh_member_size((StandardTonalitySeventhMember)99) == 0);
}

/* QUALITIES: the triad plus the seventh. */
static void test_parts(void) {
    StandardTonalityTriadQuality triad;
    StandardTonalityIntervalQuality seventh;

    assert(standard_tonality_seventh_parts(MAMA, &triad, &seventh));
    assert(triad == STANDARD_TONALITY_TRIAD_MAJOR && seventh == STANDARD_TONALITY_QUALITY_MAJOR);
    assert(standard_tonality_seventh_parts(MAMI, &triad, &seventh));
    assert(triad == STANDARD_TONALITY_TRIAD_MAJOR && seventh == STANDARD_TONALITY_QUALITY_MINOR);
    assert(standard_tonality_seventh_parts(MIMI, &triad, &seventh));
    assert(triad == STANDARD_TONALITY_TRIAD_MINOR && seventh == STANDARD_TONALITY_QUALITY_MINOR);
    assert(standard_tonality_seventh_parts(HDIM, &triad, &seventh));
    assert(triad == STANDARD_TONALITY_TRIAD_DIMINISHED && seventh == STANDARD_TONALITY_QUALITY_MINOR);
    assert(standard_tonality_seventh_parts(FDIM, &triad, &seventh));
    assert(triad == STANDARD_TONALITY_TRIAD_DIMINISHED && seventh == STANDARD_TONALITY_QUALITY_DIMINISHED);

    assert(!standard_tonality_seventh_parts((SeventhQuality)99, &triad, &seventh));
    assert(!standard_tonality_seventh_parts(MAMA, NULL, &seventh));
    assert(!standard_tonality_seventh_parts(MAMA, &triad, NULL));
}

static void test_quality_from_parts(void) {
    SeventhQuality quality;
    for (int q = 0; q < STANDARD_TONALITY_SEVENTH_QUALITY_COUNT; q++) {
        StandardTonalityTriadQuality triad;
        StandardTonalityIntervalQuality seventh;
        assert(standard_tonality_seventh_parts((SeventhQuality)q, &triad, &seventh));
        assert(standard_tonality_seventh_quality_from_parts(triad, seventh, &quality));
        assert(quality == (SeventhQuality)q);
    }

    /* Outside the five: a minor triad with a major seventh, an augmented
       triad, a major triad with a diminished seventh. */
    quality = (SeventhQuality)42;
    assert(!standard_tonality_seventh_quality_from_parts(STANDARD_TONALITY_TRIAD_MINOR,
                                                         STANDARD_TONALITY_QUALITY_MAJOR, &quality));
    assert(!standard_tonality_seventh_quality_from_parts(STANDARD_TONALITY_TRIAD_AUGMENTED,
                                                         STANDARD_TONALITY_QUALITY_MAJOR, &quality));
    assert(!standard_tonality_seventh_quality_from_parts(STANDARD_TONALITY_TRIAD_MAJOR,
                                                         STANDARD_TONALITY_QUALITY_DIMINISHED, &quality));
    assert(quality == (SeventhQuality)42);
    assert(!standard_tonality_seventh_quality_from_parts(STANDARD_TONALITY_TRIAD_MAJOR,
                                                         STANDARD_TONALITY_QUALITY_MAJOR, NULL));
}

/* STACKED_IN_THIRDS: the root with a third, fifth, and seventh above. */
static void test_stack(void) {
    PitchSpelling *d4 = spell(PITCH_LETTER_D, 0, 4);
    PitchSpelling *f4 = spell(PITCH_LETTER_F, 0, 4);
    PitchSpelling *a4 = spell(PITCH_LETTER_A, 0, 4);
    PitchSpelling *c5 = spell(PITCH_LETTER_C, 0, 5);
    PitchSpelling *b4 = spell(PITCH_LETTER_B, 0, 4);
    PitchSpelling *c_flat5 = spell(PITCH_LETTER_C, -1, 5);

    assert(standard_tonality_is_seventh_stack(d4, f4, a4, c5));
    assert(standard_tonality_is_seventh_stack(d4, f4, a4, c_flat5)); /* accidentals aside */
    assert(!standard_tonality_is_seventh_stack(d4, f4, a4, b4));     /* a sixth */
    assert(!standard_tonality_is_seventh_stack(d4, f4, c5, a4));     /* not in order */
    assert(!standard_tonality_is_seventh_stack(NULL, f4, a4, c5));
    assert(!standard_tonality_is_seventh_stack(d4, f4, a4, NULL));

    pitch_spelling_destroy(d4);
    pitch_spelling_destroy(f4);
    pitch_spelling_destroy(a4);
    pitch_spelling_destroy(c5);
    pitch_spelling_destroy(b4);
    pitch_spelling_destroy(c_flat5);
}

/* LINES_OR_SPACES: all four on lines (E G B D) or all on spaces (F A C E),
   never mixed. */
static void test_lines_or_spaces(void) {
    PitchSpelling *e4 = spell(PITCH_LETTER_E, 0, 4);
    PitchSpelling *g4 = spell(PITCH_LETTER_G, 0, 4);
    PitchSpelling *b4 = spell(PITCH_LETTER_B, 0, 4);
    PitchSpelling *d5 = spell(PITCH_LETTER_D, 0, 5);
    PitchSpelling *f4 = spell(PITCH_LETTER_F, 0, 4);
    PitchSpelling *a4 = spell(PITCH_LETTER_A, 0, 4);
    PitchSpelling *c5 = spell(PITCH_LETTER_C, 0, 5);
    PitchSpelling *e5 = spell(PITCH_LETTER_E, 0, 5);

    assert(standard_tonality_seventh_on_lines_or_spaces(e4, g4, b4, d5));
    assert(standard_tonality_seventh_on_lines_or_spaces(f4, a4, c5, e5));
    assert(!standard_tonality_seventh_on_lines_or_spaces(e4, g4, b4, c5));
    assert(!standard_tonality_seventh_on_lines_or_spaces(f4, g4, b4, d5));
    assert(!standard_tonality_seventh_on_lines_or_spaces(NULL, g4, b4, d5));
    assert(!standard_tonality_seventh_on_lines_or_spaces(e4, g4, b4, NULL));

    pitch_spelling_destroy(e4);
    pitch_spelling_destroy(g4);
    pitch_spelling_destroy(b4);
    pitch_spelling_destroy(d5);
    pitch_spelling_destroy(f4);
    pitch_spelling_destroy(a4);
    pitch_spelling_destroy(c5);
    pitch_spelling_destroy(e5);
}

/* QUALITIES: every quality on F, read from the stacked notes. */
static void test_every_quality_on_f(void) {
    Note major_major[] = {{PITCH_LETTER_F, 0, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}, {PITCH_LETTER_E, 0, 5}};
    Note major_minor[] = {{PITCH_LETTER_F, 0, 4}, {PITCH_LETTER_A, 0, 4}, {PITCH_LETTER_C, 0, 5}, {PITCH_LETTER_E, -1, 5}};
    Note minor_minor[] = {{PITCH_LETTER_F, 0, 4}, {PITCH_LETTER_A, -1, 4}, {PITCH_LETTER_C, 0, 5}, {PITCH_LETTER_E, -1, 5}};
    Note half_dim[] = {{PITCH_LETTER_F, 0, 4}, {PITCH_LETTER_A, -1, 4}, {PITCH_LETTER_C, -1, 5}, {PITCH_LETTER_E, -1, 5}};
    Note fully_dim[] = {{PITCH_LETTER_F, 0, 4}, {PITCH_LETTER_A, -1, 4}, {PITCH_LETTER_C, -1, 5}, {PITCH_LETTER_E, -2, 5}};

    SeventhQuality quality;
    assert(stack_quality(major_major, &quality) && quality == MAMA);
    assert(stack_quality(major_minor, &quality) && quality == MAMI);
    assert(stack_quality(minor_minor, &quality) && quality == MIMI);
    assert(stack_quality(half_dim, &quality) && quality == HDIM);
    assert(stack_quality(fully_dim, &quality) && quality == FDIM);
}

/* The qualities outside the five are not given one. */
static void test_unlisted_stacks(void) {
    /* Minor triad, major seventh. */
    Note minor_major[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, -1, 4}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_B, 0, 4}};
    /* Augmented triad, major seventh. */
    Note augmented_major[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 1, 4}, {PITCH_LETTER_B, 0, 4}};
    /* Not a stack. */
    Note not_thirds[] = {{PITCH_LETTER_C, 0, 4}, {PITCH_LETTER_E, 0, 4}, {PITCH_LETTER_G, 0, 4}, {PITCH_LETTER_A, 0, 4}};

    SeventhQuality quality = (SeventhQuality)42;
    assert(!stack_quality(minor_major, &quality));
    assert(!stack_quality(augmented_major, &quality));
    assert(!stack_quality(not_thirds, &quality));
    assert(quality == (SeventhQuality)42);
    assert(!standard_tonality_seventh_stack_quality(NULL, NULL, NULL, NULL, &quality));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_member_sizes();
    test_parts();
    test_quality_from_parts();
    test_stack();
    test_lines_or_spaces();
    test_every_quality_on_f();
    test_unlisted_stacks();
    printf("test_seventh_rules (standard-tonality): ok\n");
    return 0;
}
