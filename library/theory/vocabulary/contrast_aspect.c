#include "theory/vocabulary/contrast_aspect.h"

#include <stddef.h>
#include <string.h>

static const char *const NAMES[CONTRAST_ASPECT_COUNT] = {"rhythm", "contour", "register", "density"};

const char *contrast_aspect_name(ContrastAspect aspect) {
    return aspect >= 0 && aspect < CONTRAST_ASPECT_COUNT ? NAMES[aspect] : NULL;
}

bool contrast_aspect_from_name(const char *name, ContrastAspect *out) {
    if (name == NULL || out == NULL) {
        return false;
    }
    for (int i = 0; i < CONTRAST_ASPECT_COUNT; i++) {
        if (strcmp(name, NAMES[i]) == 0) {
            *out = (ContrastAspect)i;
            return true;
        }
    }
    return false;
}
