#ifndef URSATZ_TESTS_SUPPORT_SEMANTICS_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_SEMANTICS_TEST_ACCESS_H

#include "analysis/notes/contour.h"
#include "analysis/notes/contour_internal.h"
#include "analysis/notes/interval.h"
#include "analysis/notes/interval_internal.h"

/* What the tests read of a Contour and an Interval. No application reads these, so they are not in semantics/. */

static inline size_t contour_pitch_id_count(const Contour *contour) {
    return contour->pitch_id_count;
}

static inline bool interval_has_directed_semitones(const Interval *interval) {
    return interval->has_directed_semitones;
}

static inline long interval_directed_semitones(const Interval *interval) {
    return interval->directed_semitones;
}

#endif
