#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"

/*
 * Construction test: standard_form_framework() returns a single,
 * stable, well-named Framework instance.
 */

int main(void) {
    const Framework *framework = standard_form_framework();
    assert(framework != NULL);
    assert(strcmp(framework_name(framework), "standard-form-v1") == 0);

    const Framework *again = standard_form_framework();
    assert(framework_identity_equals(framework, again));

    printf("test_framework (standard-form): ok\n");
    return 0;
}
