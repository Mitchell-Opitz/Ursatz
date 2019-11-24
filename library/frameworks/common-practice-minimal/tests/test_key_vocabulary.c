#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"
#include "interpretation/key.h"
#include "key_vocabulary.h"

/*
 * Construction tests: one major key and one minor key, confirming each
 * resolves to a real Key labeled with its actual key name.
 */

static void check_key(CommonPracticeKeyMode mode, const char *tonic_name, const char *expected_label,
                       const char *context_id_value) {
    EntityID *context_id;
    assert(entity_id_create(context_id_value, &context_id) == ENTITY_ID_OK);

    Key *key;
    assert(common_practice_build_key(mode, tonic_name, context_id, &key) == KEY_OK);
    assert(strcmp(key_label(key), expected_label) == 0);
    assert(interpretation_framework_reference(key_interpretation(key)) != NULL);

    key_destroy(key);
}

int main(void) {
    check_key(COMMON_PRACTICE_KEY_MAJOR, "C", "C major", "context-test-c-major");
    check_key(COMMON_PRACTICE_KEY_MINOR, "A", "A minor", "context-test-a-minor");

    printf("test_key_vocabulary: ok\n");
    return 0;
}