#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_LIMITS_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_LIMITS_H

#include <stddef.h>

/*
 * The numbers behind the rules that ask a writer to "limit" something
 * without saying how much. Callers pass their own values; the defaults
 * are this framework's.
 *
 *   max_consecutive_repeats   the longest run of one pitch sounding again
 *                             and again (1: no note is immediately
 *                             repeated)
 *   max_pitch_occurrences     how often one pitch may appear in the whole
 *                             line, repeated or not
 *   max_consecutive_leaps     the longest run of leaps (intervals of a
 *                             third or more) with no step between them
 *   leap_window               a span of this many consecutive intervals
 *   max_leaps_in_window       the most leaps allowed within one such span
 *   max_same_generic_run      the longest run of intervals of the same
 *                             generic size (every one a second, say),
 *                             direction ignored
 */

typedef struct {
    size_t max_consecutive_repeats;
    size_t max_pitch_occurrences;
    size_t max_consecutive_leaps;
    size_t leap_window;
    size_t max_leaps_in_window;
    size_t max_same_generic_run;
} StandardCounterpointLimits;

static inline StandardCounterpointLimits standard_counterpoint_default_limits(void) {
    StandardCounterpointLimits limits = {1, 3, 2, 5, 3, 3};
    return limits;
}

#endif
