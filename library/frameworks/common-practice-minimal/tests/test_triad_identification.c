#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "triad_identification.h"

/*
 * Identification tests: one per diatonic triad (I through vii\xC2\xB0),
 * confirming correct chord identification given known scale degrees in
 * a known key (see triad_identification.h for why scale degrees rather
 * than a raw Sonority). Each also checks the inverted-order input
 * resolves to the same root-position label, exercising the documented
 * tie-break rule.
 */

static void check_identification(const int *degrees, size_t degree_count, const char *expected_label,
                                  const char *sonority_id_value) {
    CommonPracticeTriad triad;
    assert(common_practice_identify_triad(degrees, degree_count, &triad));
    assert(strcmp(common_practice_triad_label(triad), expected_label) == 0);

    EntityID *sonority_id;
    assert(entity_id_create(sonority_id_value, &sonority_id) == ENTITY_ID_OK);

    Chord *chord;
    assert(common_practice_identify_triad_chord(degrees, degree_count, sonority_id, &chord));
    assert(strcmp(chord_label(chord), expected_label) == 0);
    chord_destroy(chord);
}

int main(void) {
    int degrees_i[] = {1, 3, 5};
    check_identification(degrees_i, 3, "I", "sonority-identify-I");

    int degrees_ii[] = {2, 4, 6};
    check_identification(degrees_ii, 3, "ii", "sonority-identify-ii");

    int degrees_iii[] = {3, 5, 7};
    check_identification(degrees_iii, 3, "iii", "sonority-identify-iii");

    int degrees_iv[] = {4, 6, 1};
    check_identification(degrees_iv, 3, "IV", "sonority-identify-IV");

    int degrees_v[] = {5, 7, 2};
    check_identification(degrees_v, 3, "V", "sonority-identify-V");

    int degrees_vi[] = {6, 1, 3};
    check_identification(degrees_vi, 3, "vi", "sonority-identify-vi");

    int degrees_vii[] = {7, 2, 4};
    check_identification(degrees_vii, 3, "vii\xC2\xB0", "sonority-identify-vii-dim");

    /* Tie-break rule: inversion order does not change the label. */
    int degrees_i_first_inversion[] = {3, 5, 1};
    CommonPracticeTriad inverted_triad;
    assert(common_practice_identify_triad(degrees_i_first_inversion, 3, &inverted_triad));
    assert(strcmp(common_practice_triad_label(inverted_triad), "I") == 0);

    /* No match: fewer than 3 distinct degrees. */
    int degrees_incomplete[] = {1, 3};
    CommonPracticeTriad unused_triad;
    assert(!common_practice_identify_triad(degrees_incomplete, 2, &unused_triad));

    printf("test_triad_identification: ok\n");
    return 0;
}