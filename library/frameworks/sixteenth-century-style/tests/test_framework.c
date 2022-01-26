#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "century_framework.h"

/*
 * Construction test: sixteenth_century_framework() returns a single,
 * stable, well-named Framework instance.
 */

int main(void) {
    const Framework *framework = sixteenth_century_framework();
    assert(framework != NULL);
    assert(strcmp(framework_name(framework), "sixteenth-century-style-v1") == 0);

    const Framework *again = sixteenth_century_framework();
    assert(framework_identity_equals(framework, again));

    printf("test_framework (sixteenth-century-style): ok\n");
    return 0;
}
