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

/*
 * Regression: two distinct Sonority occurrences of the same triad type
 * must produce two distinct, independently-persisted Interpretation
 * ids (see chord_vocabulary.c history -- the id used to be derived only
 * from the triad type, so every repeated occurrence collided on one id).
 */
static void check_distinct_occurrences_get_distinct_ids(CommonPracticeTriad triad) {
    EntityID *sonority_id_a;
    assert(entity_id_create("sonority-occurrence-a", &sonority_id_a) == ENTITY_ID_OK);
    EntityID *sonority_id_b;
    assert(entity_id_create("sonority-occurrence-b", &sonority_id_b) == ENTITY_ID_OK);

    Chord *chord_a;
    assert(common_practice_build_triad_chord(triad, sonority_id_a, &chord_a) == CHORD_OK);
    Chord *chord_b;
    assert(common_practice_build_triad_chord(triad, sonority_id_b, &chord_b) == CHORD_OK);

    const EntityID *interpretation_id_a = interpretation_id(chord_interpretation(chord_a));
    const EntityID *interpretation_id_b = interpretation_id(chord_interpretation(chord_b));
    assert(!entity_id_equals(interpretation_id_a, interpretation_id_b));

    chord_destroy(chord_a);
    chord_destroy(chord_b);
}

int main(void) {
    check_triad(COMMON_PRACTICE_TRIAD_I, "I", "sonority-test-I");
    check_triad(COMMON_PRACTICE_TRIAD_II, "ii", "sonority-test-ii");
    check_triad(COMMON_PRACTICE_TRIAD_III, "iii", "sonority-test-iii");
    check_triad(COMMON_PRACTICE_TRIAD_IV, "IV", "sonority-test-IV");
    check_triad(COMMON_PRACTICE_TRIAD_V, "V", "sonority-test-V");
    check_triad(COMMON_PRACTICE_TRIAD_VI, "vi", "sonority-test-vi");
    check_triad(COMMON_PRACTICE_TRIAD_VII_DIM, "vii\xC2\xB0", "sonority-test-vii-dim");

    check_distinct_occurrences_get_distinct_ids(COMMON_PRACTICE_TRIAD_I);

    printf("test_chord_vocabulary: ok\n");
    return 0;
}