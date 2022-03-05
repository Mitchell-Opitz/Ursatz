#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fugue_framework.h"

/*
 * Construction test: high_baroque_fugue_framework() returns a single, stable,
 * well-named Framework instance.
 */

int main(void) {
    const Framework *framework = high_baroque_fugue_framework();
    assert(framework != NULL);
    assert(strcmp(framework_name(framework), "high-baroque-fugue-v1") == 0);

    const Framework *again = high_baroque_fugue_framework();
    assert(framework_identity_equals(framework, again));

    printf("test_framework (high-baroque-fugue): ok\n");
    return 0;
}
