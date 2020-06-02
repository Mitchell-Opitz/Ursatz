#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "clef_letter_rules.h"
#include "framework.h"

static StaffPosition *position(int steps) {
    StaffPosition *p = NULL;
    assert(staff_position_create(steps, &p) == STAFF_POSITION_OK);
    return p;
}

static char letter_char(PitchLetter letter) {
    switch (letter) {
        case PITCH_LETTER_A: return 'A';
        case PITCH_LETTER_B: return 'B';
        case PITCH_LETTER_C: return 'C';
        case PITCH_LETTER_D: return 'D';
        case PITCH_LETTER_E: return 'E';
        case PITCH_LETTER_F: return 'F';
        default: return 'G';
    }
}

/* Letters of the five lines (bottom to top) and four spaces (lowest to
   highest) as one string each. */
static void names(const Clef *clef, char lines[6], char spaces[5]) {
    for (int i = 1; i <= STANDARD_NOTATION_STAFF_LINES; i++) {
        PitchLetter letter;
        assert(standard_notation_clef_line_letter(clef, i, &letter));
        lines[i - 1] = letter_char(letter);
    }
    lines[5] = '\0';
    for (int i = 1; i <= STANDARD_NOTATION_STAFF_SPACES; i++) {
        PitchLetter letter;
        assert(standard_notation_clef_space_letter(clef, i, &letter));
        spaces[i - 1] = letter_char(letter);
    }
    spaces[4] = '\0';
}

static void expect_names(const Clef *clef, const char *lines, const char *spaces) {
    char got_lines[6], got_spaces[5];
    names(clef, got_lines, got_spaces);
    assert(strcmp(got_lines, lines) == 0);
    assert(strcmp(got_spaces, spaces) == 0);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_NOTATION_CLEF_LETTER_RULE_COUNT; i++) {
        const Rule *rule = standard_notation_clef_letter_rule((StandardNotationClefLetterRule)i);
        const Constraint *constraint =
            standard_notation_clef_letter_constraint((StandardNotationClefLetterRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_notation_clef_letter_rule(STANDARD_NOTATION_CLEF_LETTER_RULE_COUNT) == NULL);
}

/* Letter names from the Reading Clefs chapter. */
static void test_line_and_space_letters(void) {
    Clef *treble = NULL, *bass = NULL, *alto = NULL, *tenor = NULL;
    assert(clef_create_treble(&treble) == CLEF_OK);
    assert(clef_create_bass(&bass) == CLEF_OK);
    assert(clef_create_alto(&alto) == CLEF_OK);
    assert(clef_create_tenor(&tenor) == CLEF_OK);

    expect_names(treble, "EGBDF", "FACE");
    expect_names(bass, "GBDFA", "ACEG");
    expect_names(alto, "FACEG", "GBDF");
    expect_names(tenor, "DFACE", "EGBD");

    clef_destroy(treble);
    clef_destroy(bass);
    clef_destroy(alto);
    clef_destroy(tenor);
}

/* The other C clefs put C on their anchor line and follow the same loop. */
static void test_other_c_clef_letters(void) {
    Clef *soprano = NULL, *mezzo = NULL, *baritone = NULL;
    assert(clef_create_soprano(&soprano) == CLEF_OK);
    assert(clef_create_mezzo_soprano(&mezzo) == CLEF_OK);
    assert(clef_create_baritone(&baritone) == CLEF_OK);

    expect_names(soprano, "CEGBD", "DFAC");
    expect_names(mezzo, "EGBDF", "FACE");
    expect_names(baritone, "BDFAC", "CEGB");

    clef_destroy(soprano);
    clef_destroy(mezzo);
    clef_destroy(baritone);
}

static void test_out_of_range_and_unpitched(void) {
    Clef *treble = NULL, *neutral = NULL;
    assert(clef_create_treble(&treble) == CLEF_OK);
    assert(clef_create_neutral(&neutral) == CLEF_OK);

    PitchLetter letter = PITCH_LETTER_A;
    assert(!standard_notation_clef_line_letter(treble, 0, &letter));
    assert(!standard_notation_clef_line_letter(treble, 6, &letter));
    assert(!standard_notation_clef_space_letter(treble, 0, &letter));
    assert(!standard_notation_clef_space_letter(treble, 5, &letter));
    assert(!standard_notation_clef_line_letter(neutral, 1, &letter));
    assert(!standard_notation_clef_space_letter(NULL, 1, &letter));
    assert(letter == PITCH_LETTER_A);

    clef_destroy(treble);
    clef_destroy(neutral);
}

/* Ledger lines and the spaces beyond them keep the letter loop going,
   above and below, for every pitched clef. */
static void test_ledger_continuation(void) {
    Clef *clefs[7];
    assert(clef_create_treble(&clefs[0]) == CLEF_OK);
    assert(clef_create_bass(&clefs[1]) == CLEF_OK);
    assert(clef_create_alto(&clefs[2]) == CLEF_OK);
    assert(clef_create_tenor(&clefs[3]) == CLEF_OK);
    assert(clef_create_soprano(&clefs[4]) == CLEF_OK);
    assert(clef_create_mezzo_soprano(&clefs[5]) == CLEF_OK);
    assert(clef_create_baritone(&clefs[6]) == CLEF_OK);

    for (int c = 0; c < 7; c++) {
        for (int steps = -10; steps <= 18; steps++) {
            StaffPosition *p = position(steps);
            assert(standard_notation_ledger_continues_letters(clefs[c], p));
            staff_position_destroy(p);
        }
        clef_destroy(clefs[c]);
    }
}

static void test_ledger_continuation_unpitched(void) {
    Clef *neutral = NULL;
    assert(clef_create_neutral(&neutral) == CLEF_OK);
    StaffPosition *p = position(10);
    assert(!standard_notation_ledger_continues_letters(neutral, p));
    assert(!standard_notation_ledger_continues_letters(NULL, p));
    assert(!standard_notation_ledger_continues_letters(neutral, NULL));
    staff_position_destroy(p);
    clef_destroy(neutral);
}

int main(void) {
    test_rules_reference_framework();
    test_line_and_space_letters();
    test_other_c_clef_letters();
    test_out_of_range_and_unpitched();
    test_ledger_continuation();
    test_ledger_continuation_unpitched();
    printf("test_clef_letter_rules (standard-notation): ok\n");
    return 0;
}
