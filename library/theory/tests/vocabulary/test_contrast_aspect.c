#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/vocabulary/contrast_aspect.h"

static void test_names(void) {
    assert(strcmp(contrast_aspect_name(CONTRAST_ASPECT_RHYTHM), "rhythm") == 0);
    assert(strcmp(contrast_aspect_name(CONTRAST_ASPECT_CONTOUR), "contour") == 0);
    assert(strcmp(contrast_aspect_name(CONTRAST_ASPECT_REGISTER), "register") == 0);
    assert(strcmp(contrast_aspect_name(CONTRAST_ASPECT_DENSITY), "density") == 0);
    assert(contrast_aspect_name(CONTRAST_ASPECT_COUNT) == NULL);
    assert(contrast_aspect_name((ContrastAspect)-1) == NULL);
}

static void test_round_trip(void) {
    for (int i = 0; i < CONTRAST_ASPECT_COUNT; i++) {
        ContrastAspect back = CONTRAST_ASPECT_COUNT;
        assert(contrast_aspect_from_name(contrast_aspect_name((ContrastAspect)i), &back));
        assert(back == (ContrastAspect)i);
    }
}

static void test_refusals(void) {
    ContrastAspect out = CONTRAST_ASPECT_DENSITY;
    assert(!contrast_aspect_from_name("brightness", &out) && out == CONTRAST_ASPECT_DENSITY);
    assert(!contrast_aspect_from_name("Rhythm", &out) && out == CONTRAST_ASPECT_DENSITY);
    assert(!contrast_aspect_from_name(NULL, &out) && out == CONTRAST_ASPECT_DENSITY);
    assert(!contrast_aspect_from_name("rhythm", NULL));
}

int main(void) {
    test_names();
    test_round_trip();
    test_refusals();
    printf("test_contrast_aspect: ok\n");
    return 0;
}
