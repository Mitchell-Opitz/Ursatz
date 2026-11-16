#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_GROUPING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_GROUPING_RULES_H

#include <stdbool.h>

/*
 * Beat and beat-grouping rules for standard-meter-v1. A beat is a pulse
 * in music that regularly recurs; beats group into measures:
 *
 *   BEAT_DIVISION  in a simple meter the beat divides into two, and each
 *                  half further subdivides, giving four per beat
 *   GROUPING       two beats per group is duple, three triple, four
 *                  quadruple
 *   BEAT_ROLES     beat 1 of a group is the downbeat and carries more
 *                  "weight"; the last beat is the upbeat and is
 *                  anticipatory
 *
 * That a simple quadruple meter can feel like two groups of two (and so
 * be hard to tell from duple by ear) is a perceptual ambiguity, not a
 * notation rule, and is not asserted here.
 */

typedef enum {
    METER_GROUPING_NONE,
    METER_GROUPING_DUPLE,
    METER_GROUPING_TRIPLE,
    METER_GROUPING_QUADRUPLE
} MeterGrouping;

/* Deepest division level BEAT_DIVISION answers for (2^6 parts). */
#define STANDARD_METER_MAX_DIVISION_DEPTH 6

#endif
