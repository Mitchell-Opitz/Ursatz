#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/phrase_role.h"

static void test_validity(void) {
    for (int i = 0; i < PHRASE_ROLE_COUNT; i++) {
        assert(phrase_role_valid((PhraseRole)i));
    }
    assert(!phrase_role_valid(PHRASE_ROLE_COUNT));
    assert(!phrase_role_valid((PhraseRole)-1));
}

static void test_names(void) {
    assert(strcmp(phrase_role_name(PHRASE_ROLE_ANTECEDENT), "antecedent") == 0);
    assert(strcmp(phrase_role_name(PHRASE_ROLE_CONSEQUENT), "consequent") == 0);
    assert(phrase_role_name(PHRASE_ROLE_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_phrase_role: ok\n");
    return 0;
}
