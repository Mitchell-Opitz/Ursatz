#include "pitch/generic_interval.h"

#include <assert.h>
#include <stddef.h>

static PitchSpelling *spell(PitchLetter letter, int accidental, int octave) {
    PitchSpelling *spelling = NULL;
    assert(pitch_spelling_create(letter, accidental, octave, &spelling) == PITCH_SPELLING_OK);
    return spelling;
}

static int size_between(PitchSpelling *lower, PitchSpelling *upper) {
    GenericInterval *interval = NULL;
    assert(generic_interval_between(lower, upper, &interval) == GENERIC_INTERVAL_OK);
    int size = generic_interval_size(interval);
    generic_interval_destroy(interval);
    return size;
}

static void test_create_rejects_sizes_below_one(void) {
    GenericInterval *interval = NULL;
    assert(generic_interval_create(0, &interval) == GENERIC_INTERVAL_ERR_INVALID_SIZE);
    assert(generic_interval_create(-3, &interval) == GENERIC_INTERVAL_ERR_INVALID_SIZE);
    assert(interval == NULL);
}

static void test_create_and_size(void) {
    for (int size = 1; size <= 20; size++) {
        GenericInterval *interval = NULL;
        assert(generic_interval_create(size, &interval) == GENERIC_INTERVAL_OK);
        assert(generic_interval_size(interval) == size);
        generic_interval_destroy(interval);
    }
}

/* Example 2: the eight sizes within a C major scale, counting from one. */
static void test_sizes_in_c_major(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_C};
    int octaves[] = {4, 4, 4, 4, 4, 4, 4, 5};
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    for (int i = 0; i < 8; i++) {
        PitchSpelling *upper = spell(letters[i], 0, octaves[i]);
        assert(size_between(c4, upper) == i + 1);
        pitch_spelling_destroy(upper);
    }
    pitch_spelling_destroy(c4);
}

/* Example 3: accidentals do not change a generic size. */
static void test_accidentals_do_not_change_size(void) {
    PitchSpelling *c = spell(PITCH_LETTER_C, 0, 4);
    for (int lower_accidental = -2; lower_accidental <= 2; lower_accidental++) {
        for (int upper_accidental = -2; upper_accidental <= 2; upper_accidental++) {
            PitchSpelling *lower = spell(PITCH_LETTER_C, lower_accidental, 4);
            PitchSpelling *upper = spell(PITCH_LETTER_E, upper_accidental, 4);
            assert(size_between(lower, upper) == 3);
            pitch_spelling_destroy(lower);
            pitch_spelling_destroy(upper);
        }
    }
    pitch_spelling_destroy(c);
}

static void test_octave_number_belongs_to_the_letter(void) {
    /* B3 to C4: a second, since B is the last letter of octave 3. */
    PitchSpelling *b3 = spell(PITCH_LETTER_B, 0, 3);
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    assert(size_between(b3, c4) == 2);

    /* Example 9: A2 to C3 is a third, A2 to C4 a tenth. */
    PitchSpelling *a2 = spell(PITCH_LETTER_A, 0, 2);
    PitchSpelling *c3 = spell(PITCH_LETTER_C, 0, 3);
    PitchSpelling *c4_high = spell(PITCH_LETTER_C, 0, 4);
    assert(size_between(a2, c3) == 3);
    assert(size_between(a2, c4_high) == 10);

    pitch_spelling_destroy(b3);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(a2);
    pitch_spelling_destroy(c3);
    pitch_spelling_destroy(c4_high);
}

static void test_unison_and_octave(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *c4_sharp = spell(PITCH_LETTER_C, 1, 4);
    PitchSpelling *c5 = spell(PITCH_LETTER_C, 0, 5);
    assert(size_between(c4, c4) == 1);
    assert(size_between(c4, c4_sharp) == 1); /* an augmented unison is still a unison by size */
    assert(size_between(c4, c5) == 8);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(c4_sharp);
    pitch_spelling_destroy(c5);
}

static void test_upper_below_lower_is_rejected(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *b3 = spell(PITCH_LETTER_B, 0, 3);
    GenericInterval *interval = NULL;
    assert(generic_interval_between(c4, b3, &interval) == GENERIC_INTERVAL_ERR_UPPER_BELOW_LOWER);
    assert(interval == NULL);
    assert(generic_interval_between(NULL, b3, &interval) == GENERIC_INTERVAL_ERR_NULL_SPELLING);
    assert(generic_interval_between(c4, NULL, &interval) == GENERIC_INTERVAL_ERR_NULL_SPELLING);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(b3);
}

static void test_simple_and_compound(void) {
    for (int size = 1; size <= 8; size++) {
        GenericInterval *interval = NULL;
        assert(generic_interval_create(size, &interval) == GENERIC_INTERVAL_OK);
        assert(generic_interval_is_simple(interval));
        assert(!generic_interval_is_compound(interval));
        assert(generic_interval_simple_size(interval) == size);
        generic_interval_destroy(interval);
    }
    for (int size = 9; size <= 22; size++) {
        GenericInterval *interval = NULL;
        assert(generic_interval_create(size, &interval) == GENERIC_INTERVAL_OK);
        assert(generic_interval_is_compound(interval));
        assert(!generic_interval_is_simple(interval));
        int simple = generic_interval_simple_size(interval);
        assert(simple >= 2 && simple <= 8);
        assert((size - simple) % 7 == 0);
        generic_interval_destroy(interval);
    }
}

/* Adding an octave adds 7: a third becomes a tenth, a unison an octave. */
static void test_add_octave(void) {
    GenericInterval *third = NULL;
    GenericInterval *tenth = NULL;
    assert(generic_interval_create(3, &third) == GENERIC_INTERVAL_OK);
    assert(generic_interval_add_octave(third, &tenth) == GENERIC_INTERVAL_OK);
    assert(generic_interval_size(tenth) == 10);
    assert(generic_interval_simple_size(tenth) == 3);

    GenericInterval *unison = NULL;
    GenericInterval *octave = NULL;
    assert(generic_interval_create(1, &unison) == GENERIC_INTERVAL_OK);
    assert(generic_interval_add_octave(unison, &octave) == GENERIC_INTERVAL_OK);
    assert(generic_interval_size(octave) == 8);

    generic_interval_destroy(third);
    generic_interval_destroy(tenth);
    generic_interval_destroy(unison);
    generic_interval_destroy(octave);
}

static void test_equality(void) {
    GenericInterval *a = NULL;
    GenericInterval *b = NULL;
    GenericInterval *c = NULL;
    assert(generic_interval_create(5, &a) == GENERIC_INTERVAL_OK);
    assert(generic_interval_create(5, &b) == GENERIC_INTERVAL_OK);
    assert(generic_interval_create(6, &c) == GENERIC_INTERVAL_OK);
    assert(generic_interval_equals(a, b));
    assert(!generic_interval_equals(a, c));
    assert(generic_interval_equals(NULL, NULL));
    assert(!generic_interval_equals(a, NULL));
    generic_interval_destroy(a);
    generic_interval_destroy(b);
    generic_interval_destroy(c);
}

static void test_destroy_null_is_safe(void) {
    generic_interval_destroy(NULL);
}

int main(void) {
    test_create_rejects_sizes_below_one();
    test_create_and_size();
    test_sizes_in_c_major();
    test_accidentals_do_not_change_size();
    test_octave_number_belongs_to_the_letter();
    test_unison_and_octave();
    test_upper_below_lower_is_rejected();
    test_simple_and_compound();
    test_add_octave();
    test_equality();
    test_destroy_null_is_safe();
    return 0;
}
