#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"

/*
 * Construction test: standard_counterpoint_framework() returns a single,
 * stable, well-named Framework instance.
 */

int main(void) {
    const Framework *framework = standard_counterpoint_framework();
    assert(framework != NULL);
    assert(strcmp(framework_name(framework), "standard-counterpoint-v1") == 0);

    const Framework *again = standard_counterpoint_framework();
    assert(framework_identity_equals(framework, again));

    printf("test_framework (standard-counterpoint): ok\n");
    return 0;
}
