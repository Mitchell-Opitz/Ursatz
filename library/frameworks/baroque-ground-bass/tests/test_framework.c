#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ground_framework.h"

/*
 * Construction test: baroque_ground_bass_framework() returns a single,
 * stable, well-named Framework instance.
 */

int main(void) {
    const Framework *framework = baroque_ground_bass_framework();
    assert(framework != NULL);
    assert(strcmp(framework_name(framework), "baroque-ground-bass-v1") == 0);

    const Framework *again = baroque_ground_bass_framework();
    assert(framework_identity_equals(framework, again));

    printf("test_framework (baroque-ground-bass): ok\n");
    return 0;
}
