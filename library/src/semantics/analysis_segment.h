#ifndef MUSIC_SEMANTICS_ANALYSIS_SEGMENT_H
#define MUSIC_SEMANTICS_ANALYSIS_SEGMENT_H

#include <stdbool.h>

#include "semantics/beat_strength.h"
#include "time/metrical_position.h"
#include "time/time_span.h"

/*
 * AnalysisSegment: the neutral sonority segment framework adapters accept (R-242): an id, the span it
 * covers, where it starts in the bar (position, may be NULL) and the beat strength of that start. All
 * pointers are borrowed.
 */
typedef struct {
    const char *id;
    const TimeSpan *span;
    const MetricalPosition *position;
    BeatStrength strength;
} AnalysisSegment;

/* Non-NULL id and span, a span that ends after it starts, a valid strength. */
bool analysis_segment_valid(const AnalysisSegment *segment);

#endif
