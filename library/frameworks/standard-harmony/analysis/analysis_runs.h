#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_RUNS_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_RUNS_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_input.h"

/*
 * Runs: consecutive prepared segments that make the same chord count as one chord sounded over several segments.
 * Two valid diatonic segments are the same chord when their degree, inversion, seventh, cadential mark, leading
 * tone and half-diminished mark agree; two applied segments, when their kind, quality, target, degree and inversion
 * agree; consecutive invalid segments are one invalid run. `uncertainty` is the largest input uncertainty in the run.
 */

typedef struct {
    size_t first;
    size_t last;
    bool valid;
    bool applied;
    double uncertainty;
} HarmonyRun;

/* Builds the runs of `count` segments into `runs` (room for `count`); returns how many. */
size_t harmony_analysis_build_runs(const HarmonySegment *segments, size_t count, HarmonyRun *runs);

#endif
