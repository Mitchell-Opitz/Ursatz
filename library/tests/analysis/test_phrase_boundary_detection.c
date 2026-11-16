#include <assert.h>
#include <stdio.h>

#include "analysis/phrase_boundary_detection.h"

/*
 * Analysis-regression test: a fixed five-point sequence exercises both boundary signals (a cadence; contour/rhythm discontinuity once
 * max_length_without_cadence elapses) under the primary/secondary signal rule, with and without the signal output, and the default
 * length for max_length_without_cadence 0.
 */

int main(void) {
    /* Behavioral contract: no points writes nothing and does not read the points. */
    phrase_boundary_detect(NULL, 0, 0, NULL, NULL);

    /*
     * Analysis-regression sequence, max_length_without_cadence overridden to 3:
     *   p0: no cadence, no discontinuity            -- non-boundary
     *   p1: no cadence, no discontinuity            -- non-boundary
     *   p2: no cadence, discontinuity, 3rd since last -- boundary (contour/rhythm)
     *   p3: cadence                                  -- boundary (cadence)
     *   p4: no cadence, no discontinuity            -- trailing, no boundary signal
     */
    PhraseBoundaryDetectionPoint points[5] = {
        {false, false},
        {false, false},
        {false, true},
        {true, false},
        {false, false},
    };
    bool is_boundary[5];
    PhraseBoundarySignal signal[5];
    phrase_boundary_detect(points, 5, 3, is_boundary, signal);
    static const bool expected_is_boundary[5] = {false, false, true, true, false};
    for (size_t i = 0; i < 5; i++) {
        assert(is_boundary[i] == expected_is_boundary[i]);
    }
    assert(signal[2] == PHRASE_BOUNDARY_SIGNAL_CONTOUR_RHYTHM);
    assert(signal[3] == PHRASE_BOUNDARY_SIGNAL_CADENCE);

    /* The signal output is optional. */
    bool again[5];
    phrase_boundary_detect(points, 5, 3, again, NULL);
    for (size_t i = 0; i < 5; i++) {
        assert(again[i] == expected_is_boundary[i]);
    }

    /* A discontinuity before max_length points have passed is no boundary; a cadence always is. */
    PhraseBoundaryDetectionPoint early[3] = {{false, true}, {true, true}, {false, true}};
    bool early_boundary[3];
    phrase_boundary_detect(early, 3, 3, early_boundary, NULL);
    assert(!early_boundary[0] && early_boundary[1] && !early_boundary[2]);

    /* max_length_without_cadence 0 selects the default: a discontinuity ends a phrase only at its 8th point. */
    PhraseBoundaryDetectionPoint run[9];
    bool run_boundary[9];
    for (size_t i = 0; i < 9; i++) {
        run[i].has_cadence = false;
        run[i].has_discontinuity = true;
    }
    phrase_boundary_detect(run, 9, 0, run_boundary, NULL);
    for (size_t i = 0; i < 9; i++) {
        assert(run_boundary[i] == (i + 1 == PHRASE_BOUNDARY_DETECTION_DEFAULT_MAX_LENGTH));
    }

    printf("test_phrase_boundary_detection: ok\n");
    return 0;
}
