#include <assert.h>
#include <stdio.h>

#include "rule_check.h"
#include "tie_rules.h"

static PitchSpelling *spell(PitchLetter letter, int accidental, int octave) {
    PitchSpelling *spelling = NULL;
    assert(pitch_spelling_create(letter, accidental, octave, &spelling) == PITCH_SPELLING_OK);
    return spelling;
}

static NoteValue *make(int exponent) {
    NoteValue *value = NULL;
    assert(note_value_create(exponent, &value) == NOTE_VALUE_OK);
    return value;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_rhythm_tie_rule, standard_rhythm_tie_constraint,
                                    STANDARD_RHYTHM_TIE_RULE_COUNT);
}

/* Same pitch only; different pitches would be a slur. */
static void test_same_pitch(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *also_c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *d4 = spell(PITCH_LETTER_D, 0, 4);
    PitchSpelling *c5 = spell(PITCH_LETTER_C, 0, 5);
    PitchSpelling *c_sharp4 = spell(PITCH_LETTER_C, 1, 4);
    PitchSpelling *d_flat4 = spell(PITCH_LETTER_D, -1, 4);

    assert(standard_rhythm_tie_same_pitch(c4, also_c4));
    assert(!standard_rhythm_tie_same_pitch(c4, d4));
    assert(!standard_rhythm_tie_same_pitch(c4, c5));
    /* compared as written: enharmonic respellings are different pitches here */
    assert(!standard_rhythm_tie_same_pitch(c_sharp4, d_flat4));
    assert(!standard_rhythm_tie_same_pitch(c4, NULL));

    assert(standard_rhythm_tie_valid(c4, also_c4));
    assert(!standard_rhythm_tie_valid(c4, d4));

    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(also_c4);
    pitch_spelling_destroy(d4);
    pitch_spelling_destroy(c5);
    pitch_spelling_destroy(c_sharp4);
    pitch_spelling_destroy(d_flat4);
}

/* Ties are never used with rests (a NULL pitch is a rest). */
static void test_not_on_rests(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);

    assert(standard_rhythm_tie_allowed_between(c4, c4));
    assert(!standard_rhythm_tie_allowed_between(c4, NULL));
    assert(!standard_rhythm_tie_allowed_between(NULL, c4));
    assert(!standard_rhythm_tie_allowed_between(NULL, NULL));
    assert(!standard_rhythm_tie_valid(NULL, NULL));
    assert(!standard_rhythm_tie_valid(c4, NULL));

    pitch_spelling_destroy(c4);
}

/* Only the first note of a tied chain is articulated. */
static void test_no_rearticulation(void) {
    assert(standard_rhythm_tied_note_articulated(0));
    assert(!standard_rhythm_tied_note_articulated(1));
    assert(!standard_rhythm_tied_note_articulated(2));
}

/* A half note tied to a quarter note is held three quarters -- a dotted half. */
static void test_duration_sum(void) {
    NoteValue *half = make(1);
    NoteValue *quarter = make(2);
    const NoteValue *chain[] = {half, quarter};
    Duration *total = NULL;
    assert(standard_rhythm_tied_duration(chain, 2, &total));
    assert(duration_numerator(total) == 3 && duration_denominator(total) == 4);

    NoteValue *dotted_half = NULL;
    assert(note_value_create_dotted(1, 1, &dotted_half) == NOTE_VALUE_OK);
    Duration *dotted = NULL;
    assert(note_value_duration(dotted_half, &dotted) == DURATION_OK);
    assert(duration_equals(total, dotted));

    duration_destroy(total);
    duration_destroy(dotted);
    note_value_destroy(dotted_half);

    /* a chain of any length, dots included */
    NoteValue *dotted_quarter = NULL;
    assert(note_value_create_dotted(2, 1, &dotted_quarter) == NOTE_VALUE_OK);
    const NoteValue *longer[] = {half, quarter, dotted_quarter};
    assert(standard_rhythm_tied_duration(longer, 3, &total));
    assert(duration_numerator(total) == 9 && duration_denominator(total) == 8);
    duration_destroy(total);

    /* a single note is its own duration */
    const NoteValue *single[] = {quarter};
    assert(standard_rhythm_tied_duration(single, 1, &total));
    assert(duration_numerator(total) == 1 && duration_denominator(total) == 4);
    duration_destroy(total);

    note_value_destroy(dotted_quarter);
    note_value_destroy(half);
    note_value_destroy(quarter);
}

static void test_duration_sum_declines(void) {
    NoteValue *quarter = make(2);
    Duration *total = NULL;
    const NoteValue *with_null[] = {quarter, NULL};

    assert(!standard_rhythm_tied_duration(NULL, 1, &total));
    assert(!standard_rhythm_tied_duration(with_null, 0, &total));
    assert(!standard_rhythm_tied_duration(with_null, 2, &total));
    assert(total == NULL);

    note_value_destroy(quarter);
}

int main(void) {
    test_rules_reference_framework();
    test_same_pitch();
    test_not_on_rests();
    test_no_rearticulation();
    test_duration_sum();
    test_duration_sum_declines();
    printf("test_tie_rules (standard-rhythm): ok\n");
    return 0;
}
