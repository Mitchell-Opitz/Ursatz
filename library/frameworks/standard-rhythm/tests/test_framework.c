#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"

/*
 * Construction test: standard_rhythm_framework() returns a single,
 * stable, well-named Framework instance.
 */

int main(void) {
    const Framework *framework = standard_rhythm_framework();
    assert(framework != NULL);
    assert(strcmp(framework_name(framework), "standard-rhythm-v1") == 0);

    const Framework *again = standard_rhythm_framework();
    assert(framework_identity_equals(framework, again));

    printf("test_framework (standard-rhythm): ok\n");
    return 0;
}
