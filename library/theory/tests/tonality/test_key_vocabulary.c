#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/tonality/key_vocabulary.h"
#include "ursatz_test_knowledge.h"

/*
 * Construction tests: one major key and one minor key, confirming each gives the tonic's pitch class and the material of its mode, and
 * that a bad material or tonic name is refused.
 */

static void check_key(CommonPracticeKeyMode mode, const char *tonic_name, int expected_tonic) {
    const PitchMaterial *material = common_practice_material(test_materials(), mode);
    KeyStructure key;
    assert(common_practice_build_key(material, tonic_name, &key));
    assert(key.tonic_pitch_class == expected_tonic && key.material == material);
}

int main(void) {
    check_key(COMMON_PRACTICE_KEY_MAJOR, "C", 0);
    check_key(COMMON_PRACTICE_KEY_MINOR, "A", 9);
    check_key(COMMON_PRACTICE_KEY_MAJOR, "F#", 6);
    check_key(COMMON_PRACTICE_KEY_MAJOR, "Bb", 10);

    KeyStructure key = {-5, NULL};
    assert(!common_practice_build_key(NULL, "C", &key));
    assert(!common_practice_build_key(common_practice_material(test_materials(), COMMON_PRACTICE_KEY_MAJOR), "", &key));
    assert(!common_practice_build_key(common_practice_material(test_materials(), COMMON_PRACTICE_KEY_MAJOR), "H", &key));
    assert(!common_practice_build_key(common_practice_material(test_materials(), COMMON_PRACTICE_KEY_MAJOR), "C", NULL));
    assert(key.tonic_pitch_class == -5 && key.material == NULL);

    printf("test_key_vocabulary: ok\n");
    return 0;
}
