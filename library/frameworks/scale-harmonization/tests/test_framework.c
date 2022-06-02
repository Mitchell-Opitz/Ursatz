#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "scale_framework.h"

/*
 * Construction test: scale_harmonization_framework() returns a single, stable,
 * well-named Framework instance.
 */

int main(void) {
    const Framework *framework = scale_harmonization_framework();
    assert(framework != NULL);
    assert(strcmp(framework_name(framework), "scale-harmonization-v1") == 0);

    const Framework *again = scale_harmonization_framework();
    assert(framework_identity_equals(framework, again));

    printf("test_framework (scale-harmonization): ok\n");
    return 0;
}
