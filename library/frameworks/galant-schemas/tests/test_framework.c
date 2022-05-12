#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "galant_framework.h"

/*
 * Construction test: galant_schemas_framework() returns a single, stable,
 * well-named Framework instance.
 */

int main(void) {
    const Framework *framework = galant_schemas_framework();
    assert(framework != NULL);
    assert(strcmp(framework_name(framework), "galant-schemas-v1") == 0);

    const Framework *again = galant_schemas_framework();
    assert(framework_identity_equals(framework, again));

    printf("test_framework (galant-schemas): ok\n");
    return 0;
}
