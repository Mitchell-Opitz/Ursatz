#ifndef MUSIC_ANALYSIS_PHRASE_BOUNDARY_DETECTION_H
#define MUSIC_ANALYSIS_PHRASE_BOUNDARY_DETECTION_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The phrase boundary loop: derives is_boundary values for a sequence of points from cadences (primary signal) and contour/rhythm
 * discontinuity (secondary signal). The chain's form bridge (ursatz_chain/form_bridge.h) calls it for standard-form's injected boundary
 * detector.
 *
 * Signal 1 (primary), cadence: a cadence at a point is always a boundary, whatever its type, including an unclassified one.
 *
 * Signal 2 (secondary), contour/rhythm discontinuity: a rest, a clear directional or rhythmic break, supplied already observed by the
 * caller as has_discontinuity per point. It makes a boundary only once the phrase has reached max_length_without_cadence points (it never
 * cuts a phrase short), and only where no cadence is.
 */

/* The default for the phrase length a discontinuity may end; 0 selects it. */
#define PHRASE_BOUNDARY_DETECTION_DEFAULT_MAX_LENGTH 8u

typedef struct {
    bool has_cadence;
    bool has_discontinuity;
} PhraseBoundaryDetectionPoint;

typedef enum { PHRASE_BOUNDARY_SIGNAL_CADENCE, PHRASE_BOUNDARY_SIGNAL_CONTOUR_RHYTHM } PhraseBoundarySignal;

/*
 * Writes is_boundary[i] for each of point_count points, and signal[i] when signal is non-NULL (the signal that made the boundary;
 * PHRASE_BOUNDARY_SIGNAL_CADENCE for a point that is no boundary). is_boundary has point_count entries; max_length_without_cadence 0
 * selects PHRASE_BOUNDARY_DETECTION_DEFAULT_MAX_LENGTH.
 */
void phrase_boundary_detect(const PhraseBoundaryDetectionPoint *points, size_t point_count, size_t max_length_without_cadence,
                            bool *is_boundary, PhraseBoundarySignal *signal);

#endif
