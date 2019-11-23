#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "chord_vocabulary.h"
#include "identity/entity_id.h"
#include "interpretation/chord.h"

/*
 * Construction tests: one per diatonic triad (I through vii\xC2\xB0),
 * confirming each resolves to a real Chord labeled with its actual
 * roman numeral, and that the shared Rule/Constraint for each triad are
 * real, stable, framework-linked objects.
 */

static void check_triad(CommonPracticeTriad triad, const char *expected_label, const char *sonority_id_value) {
    assert(strcmp(common_practice_triad_label(triad), expected_label) == 0);

    const Rule *rule = common_practice_triad_rule(triad);
    assert(rule != NULL);
    const Constraint *constraint = common_practice_triad_constraint(triad);
    assert(constraint != NULL);
    assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    assert(rule_framework(rule) == constraint_framework(constraint));

    EntityID *sonority_id;
    assert(entity_id_create(sonority_id_value, &sonority_id) == ENTITY_ID_OK);

    Chord *chord;
    assert(common_practice_build_triad_chord(triad, sonority_id, &chord) == CHORD_OK);
    assert(strcmp(chord_label(chord), expected_label) == 0);
    assert(interpretation_framework_reference(chord_interpretation(chord)) != NULL);

    chord_destroy(chord);
}

int main(void) {
    check_triad(COMMON_PRACTICE_TRIAD_I, "I", "sonority-test-I");
    check_triad(COMMON_PRACTICE_TRIAD_II, "ii", "sonority-test-ii");
    check_triad(COMMON_PRACTICE_TRIAD_III, "iii", "sonority-test-iii");
    check_triad(COMMON_PRACTICE_TRIAD_IV, "IV", "sonority-test-IV");
    check_triad(COMMON_PRACTICE_TRIAD_V, "V", "sonority-test-V");
    check_triad(COMMON_PRACTICE_TRIAD_VI, "vi", "sonority-test-vi");
    check_triad(COMMON_PRACTICE_TRIAD_VII_DIM, "vii\xC2\xB0", "sonority-test-vii-dim");

    printf("test_chord_vocabulary: ok\n");
    return 0;
}