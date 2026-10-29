#include "semantics/analysis_segment.h"

#include "core/rational.h"
#include "time/time_rational.h"

bool analysis_segment_valid(const AnalysisSegment *segment) {
    if (segment == NULL || segment->id == NULL || segment->span == NULL || !beat_strength_valid(segment->strength)) {
        return false;
    }
    Rational start;
    Rational end;
    if (rational_of_musical_time(time_span_start(segment->span), &start) != RATIONAL_OK ||
        rational_of_musical_time(time_span_end(segment->span), &end) != RATIONAL_OK) {
        return false;
    }
    return rational_cmp(end, start) > 0;
}
