#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/form_level.h"

static void test_validity(void) {
    for (int i = 0; i < FORM_LEVEL_COUNT; i++) {
        assert(form_level_valid((FormLevel)i));
    }
    assert(!form_level_valid(FORM_LEVEL_COUNT));
    assert(!form_level_valid((FormLevel)-1));
}

static void test_names(void) {
    assert(strcmp(form_level_name(FORM_LEVEL_PIECE), "piece") == 0);
    assert(strcmp(form_level_name(FORM_LEVEL_MOVEMENT), "movement") == 0);
    assert(strcmp(form_level_name(FORM_LEVEL_SECTION), "section") == 0);
    assert(strcmp(form_level_name(FORM_LEVEL_THEME), "theme") == 0);
    assert(strcmp(form_level_name(FORM_LEVEL_PHRASE), "phrase") == 0);
    assert(strcmp(form_level_name(FORM_LEVEL_SUBPHRASE), "subphrase") == 0);
    assert(strcmp(form_level_name(FORM_LEVEL_IDEA), "idea") == 0);
    assert(strcmp(form_level_name(FORM_LEVEL_MOTIVE), "motive") == 0);
    assert(form_level_name(FORM_LEVEL_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_form_level: ok\n");
    return 0;
}
