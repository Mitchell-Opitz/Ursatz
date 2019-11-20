#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"

/*
 * Construction test: common_practice_minimal_framework() returns a
 * single, stable, well-named Framework instance.
 */

int main(void) {
    const Framework *framework = common_practice_minimal_framework();
    assert(framework != NULL);
    assert(strcmp(framework_name(framework), "common-practice-minimal-v1") == 0);

    const Framework *again = common_practice_minimal_framework();
    assert(framework_identity_equals(framework, again));

    printf("test_framework (common-practice-minimal): ok\n");
    return 0;
}