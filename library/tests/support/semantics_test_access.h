#ifndef URSATZ_TESTS_SUPPORT_SEMANTICS_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_SEMANTICS_TEST_ACCESS_H

#include "analysis/notes/contour.h"
#include "analysis/notes/contour_internal.h"

/* What the tests read of a Contour. No application reads it, so it is not in analysis/notes/. */

static inline size_t contour_pitch_id_count(const Contour *contour) {
    return contour->pitch_id_count;
}

#endif
