#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/vocabulary/framework_names.h"

/* The ten strings the analysis document's claims carry. The stored documents hold them, so a change here is a new version. */
int main(void) {
    assert(strcmp(FRAMEWORK_COMMON_PRACTICE_MINIMAL, "common-practice-minimal-v1") == 0);
    assert(strcmp(FRAMEWORK_STANDARD_FORM, "standard-form-v1") == 0);
    assert(strcmp(FRAMEWORK_STANDARD_COUNTERPOINT, "standard-counterpoint-v1") == 0);
    assert(strcmp(FRAMEWORK_STANDARD_HARMONY, "standard-harmony-v1") == 0);
    assert(strcmp(FRAMEWORK_STANDARD_METER, "standard-meter-v1") == 0);
    assert(strcmp(FRAMEWORK_STANDARD_TEXTURE, "standard-texture-v1") == 0);
    assert(strcmp(FRAMEWORK_SCALE_HARMONIZATION, "scale-harmonization-v1") == 0);
    assert(strcmp(FRAMEWORK_STANDARD_VOICING, "standard-voicing-v1") == 0);
    assert(strcmp(FRAMEWORK_GALANT_SCHEMAS, "galant-schemas-v1") == 0);
    assert(strcmp(FRAMEWORK_STANDARD_MARKINGS, "standard-markings-v1") == 0);
    printf("framework_names: ok\n");
    return 0;
}
