#include <assert.h>
#include <stdio.h>

#include "enharmonic_rules.h"
#include "framework.h"
#include "spelling_test_util.h"

typedef struct {
    PitchLetter a_letter;
    int a_accidental;
    int a_octave;
    PitchLetter b_letter;
    int b_accidental;
    int b_octave;
} Pair;

static void test_rule_references_framework(void) {
    const Rule *rule = standard_notation_enharmonic_rule(STANDARD_NOTATION_ENHARMONIC_RULE_ENHARMONIC_EQUIVALENCE);
    const Constraint *constraint =
        standard_notation_enharmonic_constraint(STANDARD_NOTATION_ENHARMONIC_RULE_ENHARMONIC_EQUIVALENCE);
    assert(rule != NULL && constraint != NULL);
    assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
    assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    assert(standard_notation_enharmonic_rule(STANDARD_NOTATION_ENHARMONIC_RULE_COUNT) == NULL);
}

/* Equivalences named in the Half Steps chapter, including both octave
   crossings (B-sharp = C, C-flat = B). */
static void test_equivalent_pairs(void) {
    Pair pairs[] = {
        {PITCH_LETTER_C, 1, 4, PITCH_LETTER_D, -1, 4},  /* C# = Db */
        {PITCH_LETTER_E, 1, 4, PITCH_LETTER_F, 0, 4},   /* E# = F */
        {PITCH_LETTER_F, -1, 4, PITCH_LETTER_E, 0, 4},  /* Fb = E */
        {PITCH_LETTER_B, 1, 3, PITCH_LETTER_C, 0, 4},   /* B# = C */
        {PITCH_LETTER_C, -1, 4, PITCH_LETTER_B, 0, 3},  /* Cb = B */
        {PITCH_LETTER_D, 2, 4, PITCH_LETTER_E, 0, 4},   /* D## = E */
        {PITCH_LETTER_A, -2, 4, PITCH_LETTER_G, 0, 4},  /* Abb = G */
        {PITCH_LETTER_E, 2, 4, PITCH_LETTER_F, 1, 4},   /* E## = F# */
        {PITCH_LETTER_E, 2, 4, PITCH_LETTER_G, -1, 4},  /* E## = Gb */
        {PITCH_LETTER_C, -2, 4, PITCH_LETTER_B, -1, 3}, /* Cbb = Bb */
        {PITCH_LETTER_C, -2, 4, PITCH_LETTER_A, 1, 3},  /* Cbb = A# */
        {PITCH_LETTER_C, 2, 4, PITCH_LETTER_D, 0, 4},   /* C## = D */
        {PITCH_LETTER_E, -2, 4, PITCH_LETTER_D, 0, 4},  /* Ebb = D */
        {PITCH_LETTER_C, 2, 4, PITCH_LETTER_E, -2, 4},  /* C## = Ebb: D three ways */
    };
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
        PitchSpelling *a = spell(pairs[i].a_letter, pairs[i].a_accidental, pairs[i].a_octave);
        PitchSpelling *b = spell(pairs[i].b_letter, pairs[i].b_accidental, pairs[i].b_octave);
        assert(standard_notation_sounds_the_same(a, b));
        assert(standard_notation_enharmonic_equivalent(a, b));
        assert(standard_notation_enharmonic_equivalent(b, a));
        /* Strict spelling equality keeps them apart. */
        assert(!pitch_spelling_equals(a, b));
        pitch_spelling_destroy(a);
        pitch_spelling_destroy(b);
    }
}

static void test_not_equivalent(void) {
    PitchSpelling *c = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *c_again = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *d = spell(PITCH_LETTER_D, 0, 4);
    PitchSpelling *c5 = spell(PITCH_LETTER_C, 0, 5);
    PitchSpelling *b_sharp4 = spell(PITCH_LETTER_B, 1, 4); /* sounds as C5, not C4 */

    /* A spelling sounds like itself but is not "enharmonic" to itself. */
    assert(standard_notation_sounds_the_same(c, c_again));
    assert(!standard_notation_enharmonic_equivalent(c, c_again));

    assert(!standard_notation_enharmonic_equivalent(c, d));
    assert(!standard_notation_enharmonic_equivalent(c, c5)); /* octave apart is not the same pitch */
    assert(!standard_notation_enharmonic_equivalent(c, b_sharp4));
    assert(standard_notation_enharmonic_equivalent(c5, b_sharp4));

    assert(!standard_notation_enharmonic_equivalent(c, NULL));
    assert(!standard_notation_sounds_the_same(NULL, c));

    pitch_spelling_destroy(c);
    pitch_spelling_destroy(c_again);
    pitch_spelling_destroy(d);
    pitch_spelling_destroy(c5);
    pitch_spelling_destroy(b_sharp4);
}

int main(void) {
    test_rule_references_framework();
    test_equivalent_pairs();
    test_not_equivalent();
    printf("test_enharmonic_rules (standard-notation): ok\n");
    return 0;
}
