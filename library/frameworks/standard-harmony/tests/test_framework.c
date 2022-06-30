#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_framework.h"

/*
 * Construction test: standard_harmony_framework() returns a single, stable,
 * well-named Framework instance.
 */

int main(void) {
    const Framework *framework = standard_harmony_framework();
    assert(framework != NULL);
    assert(strcmp(framework_name(framework), "standard-harmony-v1") == 0);

    const Framework *again = standard_harmony_framework();
    assert(framework_identity_equals(framework, again));

    printf("test_framework (standard-harmony): ok\n");
    return 0;
}
