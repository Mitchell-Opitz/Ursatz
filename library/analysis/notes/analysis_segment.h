#ifndef MUSIC_SEMANTICS_ANALYSIS_SEGMENT_H
#define MUSIC_SEMANTICS_ANALYSIS_SEGMENT_H

#include "theory/vocabulary/beat_strength.h"
#include "model/time/metrical_position.h"
#include "model/time/time_span.h"

/*
 * AnalysisSegment: the neutral sonority segment framework adapters accept (R-242): an id, the span it
 * covers, where it starts in the bar (position, may be NULL) and the beat strength of that start. All
 * pointers are borrowed.
 */
typedef struct {
    const char *id;
    TimeSpan span;
    const MetricalPosition *position;
    BeatStrength strength;
} AnalysisSegment;

#endif
