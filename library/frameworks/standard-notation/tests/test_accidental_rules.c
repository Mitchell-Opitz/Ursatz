#include <assert.h>
#include <stdio.h>

#include "accidental_rules.h"
#include "framework.h"
#include "semitone.h"
#include "spelling_test_util.h"
#include "step_rules.h"

static Accidental *accidental(AccidentalKind kind) {
    Accidental *a = NULL;
    assert(accidental_create(kind, &a) == ACCIDENTAL_OK);
    return a;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_NOTATION_ACCIDENTAL_RULE_COUNT; i++) {
        const Rule *rule = standard_notation_accidental_rule((StandardNotationAccidentalRule)i);
        const Constraint *constraint =
            standard_notation_accidental_constraint((StandardNotationAccidentalRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_notation_accidental_rule(STANDARD_NOTATION_ACCIDENTAL_RULE_COUNT) == NULL);
}

static void test_half_steps_per_accidental(void) {
    Accidental *dflat = accidental(ACCIDENTAL_DOUBLE_FLAT), *flat = accidental(ACCIDENTAL_FLAT),
               *natural = accidental(ACCIDENTAL_NATURAL), *sharp = accidental(ACCIDENTAL_SHARP),
               *dsharp = accidental(ACCIDENTAL_DOUBLE_SHARP);

    assert(standard_notation_accidental_half_steps(sharp) == 1);
    assert(standard_notation_accidental_half_steps(flat) == -1);
    assert(standard_notation_accidental_half_steps(natural) == 0);
    /* Double accidentals move a whole step: two half steps. */
    assert(standard_notation_accidental_half_steps(dsharp) == standard_notation_half_steps_per_whole_step());
    assert(standard_notation_accidental_half_steps(dflat) == -standard_notation_half_steps_per_whole_step());

    assert(standard_notation_accidental_raises(sharp) && standard_notation_accidental_raises(dsharp));
    assert(!standard_notation_accidental_raises(flat) && !standard_notation_accidental_raises(natural));
    assert(standard_notation_accidental_lowers(flat) && standard_notation_accidental_lowers(dflat));
    assert(!standard_notation_accidental_lowers(sharp) && !standard_notation_accidental_lowers(natural));

    assert(standard_notation_accidental_cancels(natural));
    assert(!standard_notation_accidental_cancels(sharp));
    assert(!standard_notation_accidental_cancels(NULL));
    assert(standard_notation_accidental_half_steps(NULL) == 0);

    accidental_destroy(dflat);
    accidental_destroy(flat);
    accidental_destroy(natural);
    accidental_destroy(sharp);
    accidental_destroy(dsharp);
}

/* A natural cancels a prior accidental; an accidental replaces, never
   accumulates; with nothing written the prior alteration carries. */
static void test_effective_offset(void) {
    Accidental *natural = accidental(ACCIDENTAL_NATURAL), *sharp = accidental(ACCIDENTAL_SHARP),
               *flat = accidental(ACCIDENTAL_FLAT);

    assert(standard_notation_effective_offset(natural, 1) == 0);
    assert(standard_notation_effective_offset(natural, -1) == 0);
    assert(standard_notation_effective_offset(sharp, 1) == 1); /* not 2 */
    assert(standard_notation_effective_offset(flat, 1) == -1);
    assert(standard_notation_effective_offset(NULL, 1) == 1);
    assert(standard_notation_effective_offset(NULL, 0) == 0);

    accidental_destroy(natural);
    accidental_destroy(sharp);
    accidental_destroy(flat);
}

static void test_spelling_accidentals(void) {
    PitchSpelling *c_sharp = spell(PITCH_LETTER_C, 1, 4);
    PitchSpelling *d_flat = spell(PITCH_LETTER_D, -1, 4);
    PitchSpelling *triple = spell(PITCH_LETTER_C, 3, 4);
    Accidental *sharp = accidental(ACCIDENTAL_SHARP);

    assert(standard_notation_spelling_accidental_valid(c_sharp));
    assert(standard_notation_spelling_accidental_valid(d_flat));
    assert(!standard_notation_spelling_accidental_valid(triple));
    assert(!standard_notation_spelling_accidental_valid(NULL));

    assert(standard_notation_spelling_matches_accidental(c_sharp, sharp));
    assert(!standard_notation_spelling_matches_accidental(d_flat, sharp));
    assert(!standard_notation_spelling_matches_accidental(NULL, sharp));

    /* The accidental really moves the pitch: C-sharp is a half step above C. */
    PitchSpelling *c = spell(PITCH_LETTER_C, 0, 4);
    assert(standard_notation_semitone_distance(c, c_sharp) == 1);
    assert(standard_notation_semitone_distance(c, d_flat) == 1);

    pitch_spelling_destroy(c_sharp);
    pitch_spelling_destroy(d_flat);
    pitch_spelling_destroy(triple);
    pitch_spelling_destroy(c);
    accidental_destroy(sharp);
}

int main(void) {
    test_rules_reference_framework();
    test_half_steps_per_accidental();
    test_effective_offset();
    test_spelling_accidentals();
    printf("test_accidental_rules (standard-notation): ok\n");
    return 0;
}
