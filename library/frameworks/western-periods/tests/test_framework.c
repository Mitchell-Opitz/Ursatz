#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"

/*
 * Construction test: western_periods_framework() returns a single,
 * stable, well-named Framework instance.
 */

int main(void) {
    const Framework *framework = western_periods_framework();
    assert(framework != NULL);
    assert(strcmp(framework_name(framework), "western-periods-v1") == 0);

    const Framework *again = western_periods_framework();
    assert(framework_identity_equals(framework, again));

    printf("test_framework (western-periods): ok\n");
    return 0;
}
