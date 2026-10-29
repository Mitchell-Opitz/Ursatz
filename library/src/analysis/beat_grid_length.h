#ifndef URSATZ_ANALYSIS_BEAT_GRID_LENGTH_H
#define URSATZ_ANALYSIS_BEAT_GRID_LENGTH_H

#include <stdbool.h>

#include "core/rational.h"
#include "semantics/sonority_segmentation.h"
#include "time/meter_map.h"

/*
 * Internal to src/analysis/. The beat length from an injected beat grid: the length of the beat that holds a time. The grid is a
 * BeatGridProvider (the beats the caller's meter framework reads for a measure), not the signature's unit, so this is not
 * meter_map_beat_length; only the measure lookup is the kernel's (time/meter_arithmetic.h).
 */

typedef struct {
    const MeterMap *map;
    BeatGridProvider provider;
    void *provider_context;
} BeatGridContext;

/* Matches the beat length callback of outer_voice_guard_register and vs_reference_kept; context is a BeatGridContext. */
bool beat_grid_beat_length(void *context, Rational at, Rational *out);

#endif
