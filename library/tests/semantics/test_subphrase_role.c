#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/subphrase_role.h"

static void test_validity(void) {
    for (int i = 0; i < SUBPHRASE_ROLE_COUNT; i++) {
        assert(subphrase_role_valid((SubphraseRole)i));
    }
    assert(!subphrase_role_valid(SUBPHRASE_ROLE_COUNT));
    assert(!subphrase_role_valid((SubphraseRole)-1));
}

static void test_names(void) {
    assert(strcmp(subphrase_role_name(SUBPHRASE_ROLE_PRESENTATION), "presentation") == 0);
    assert(strcmp(subphrase_role_name(SUBPHRASE_ROLE_CONTINUATION), "continuation") == 0);
    assert(subphrase_role_name(SUBPHRASE_ROLE_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_subphrase_role: ok\n");
    return 0;
}
