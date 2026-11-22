#ifndef URSATZ_THEORY_VOCABULARY_CONTRAST_ASPECT_H
#define URSATZ_THEORY_VOCABULARY_CONTRAST_ASPECT_H

#include <stdbool.h>

/*
 * The aspects two spans of music can contrast in (R-709, target architecture 5.3): rhythm, contour, register and density. A
 * contrast target (a formal function's `contrast`, a plan's `contrast` constraint) names an aspect by its lowercase name; the
 * dimension measures of `analysis/dimension/dimension.h` give the distance in each. More aspects (brightness, harmonic tension,
 * dynamics, intensity) join with the phases that measure them.
 */
typedef enum {
    CONTRAST_ASPECT_RHYTHM = 0,
    CONTRAST_ASPECT_CONTOUR,
    CONTRAST_ASPECT_REGISTER,
    CONTRAST_ASPECT_DENSITY,
    CONTRAST_ASPECT_COUNT
} ContrastAspect;

/* The name: "rhythm", "contour", "register", "density"; NULL for an invalid value. */
const char *contrast_aspect_name(ContrastAspect aspect);

/* Reads a name back (exact, lowercase). False (output untouched) for NULL, an unknown name or a NULL output. */
bool contrast_aspect_from_name(const char *name, ContrastAspect *out);

#endif
