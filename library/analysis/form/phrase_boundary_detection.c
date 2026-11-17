#include "analysis/form/phrase_boundary_detection.h"

void phrase_boundary_detect(const PhraseBoundaryDetectionPoint *points, size_t point_count, size_t max_length_without_cadence,
                            bool *is_boundary, PhraseBoundarySignal *signal) {
    size_t max_length = max_length_without_cadence == 0 ? PHRASE_BOUNDARY_DETECTION_DEFAULT_MAX_LENGTH : max_length_without_cadence;
    size_t since_boundary = 0;

    for (size_t i = 0; i < point_count; i++) {
        const PhraseBoundaryDetectionPoint *point = &points[i];
        bool boundary = false;
        PhraseBoundarySignal kind = PHRASE_BOUNDARY_SIGNAL_CADENCE;

        if (point->has_cadence) {
            boundary = true;
            kind = PHRASE_BOUNDARY_SIGNAL_CADENCE;
        } else if (since_boundary + 1 >= max_length && point->has_discontinuity) {
            boundary = true;
            kind = PHRASE_BOUNDARY_SIGNAL_CONTOUR_RHYTHM;
        }

        is_boundary[i] = boundary;
        if (signal != NULL) {
            signal[i] = kind;
        }

        since_boundary = boundary ? 0 : since_boundary + 1;
    }
}
