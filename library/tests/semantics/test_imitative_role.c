#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/imitative_role.h"

static void test_validity(void) {
    for (int i = 0; i < IMITATIVE_ROLE_COUNT; i++) {
        assert(imitative_role_valid((ImitativeRole)i));
    }
    assert(!imitative_role_valid(IMITATIVE_ROLE_COUNT));
    assert(!imitative_role_valid((ImitativeRole)-1));
}

static void test_names(void) {
    assert(strcmp(imitative_role_name(IMITATIVE_ROLE_DUX), "dux") == 0);
    assert(strcmp(imitative_role_name(IMITATIVE_ROLE_COMES), "comes") == 0);
    assert(imitative_role_name(IMITATIVE_ROLE_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_imitative_role: ok\n");
    return 0;
}
