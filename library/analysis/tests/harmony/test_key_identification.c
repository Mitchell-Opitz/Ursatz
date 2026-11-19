#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "model/identity/entity_id.h"
#include "theory/tonality/key_vocabulary.h"
#include "analysis/harmony/key_identification.h"
#include "interpretation_test_access.h"
#include "test_knowledge.h"

/*
 * Identification tests: major and natural-minor pitch-class collections
 * (semitones 0-11 relative to a candidate tonic, see
 * key_identification.h for why intervals rather than a raw Context),
 * confirming correct mode identification and the key built from it.
 */

static void check_identification(const int *pitch_classes, size_t pitch_class_count, CommonPracticeKeyMode expected_mode,
                                  const char *tonic_name, int expected_tonic) {
    CommonPracticeKeyMode mode;
    assert(common_practice_identify_key_mode(test_materials(), pitch_classes, pitch_class_count, &mode));
    assert(mode == expected_mode);

    KeyStructure key;
    assert(common_practice_build_key(common_practice_material(test_materials(), mode), tonic_name, &key));
    assert(key.tonic_pitch_class == expected_tonic && key.material == common_practice_material(test_materials(), expected_mode));
}

int main(void) {
    /* C major: W-W-H-W-W-W-H from tonic. */
    int c_major[] = {0, 2, 4, 5, 7, 9, 11};
    check_identification(c_major, 7, COMMON_PRACTICE_KEY_MAJOR, "C", 0);

    /* A natural minor: W-H-W-W-H-W-W from tonic. */
    int a_minor[] = {0, 2, 3, 5, 7, 8, 10};
    check_identification(a_minor, 7, COMMON_PRACTICE_KEY_MINOR, "A", 9);

    /* Order and duplicates do not change the result. */
    int a_minor_reordered_with_duplicates[] = {10, 0, 8, 2, 0, 7, 3, 5, 10};
    CommonPracticeKeyMode reordered_mode;
    assert(common_practice_identify_key_mode(test_materials(), a_minor_reordered_with_duplicates, 9, &reordered_mode));
    assert(reordered_mode == COMMON_PRACTICE_KEY_MINOR);

    /* No match: fewer than 7 distinct pitch classes. */
    int incomplete[] = {0, 2, 4, 5, 7};
    CommonPracticeKeyMode unused_mode;
    assert(!common_practice_identify_key_mode(test_materials(), incomplete, 5, &unused_mode));

    /* No match: a 7-pitch-class collection that is neither major nor
       natural minor. */
    int chromatic_cluster[] = {0, 1, 2, 3, 4, 5, 6};
    assert(!common_practice_identify_key_mode(test_materials(), chromatic_cluster, 7, &unused_mode));

    /* Real-world case: all 7 diatonic pitch classes plus one extra
       chromatic tone (e.g. a passing/neighbor tone) still identifies --
       extras beyond the required 7 do not block the match. */
    int c_major_with_passing_tone[] = {0, 1, 2, 4, 5, 7, 9, 11};
    check_identification(c_major_with_passing_tone, 8, COMMON_PRACTICE_KEY_MAJOR, "C", 0);

    printf("test_key_identification: ok\n");
    return 0;
}
