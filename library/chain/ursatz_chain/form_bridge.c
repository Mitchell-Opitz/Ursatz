#include "ursatz_chain/form_bridge.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/motive/motif_window_matching.h"
#include "analysis/form/phrase_boundary_detection.h"

StandardFormAnalysisStatus chain_form_boundary_detect(const StandardFormBoundaryPoint *points, size_t count, size_t max_length,
                                                      StandardFormBoundary *out, void *context) {
    (void)context;
    if (points == NULL || out == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    memset(out, 0, count * sizeof(*out));
    if (count == 0) {
        return STANDARD_FORM_ANALYSIS_OK;
    }
    PhraseBoundaryDetectionPoint *kernel_points = (PhraseBoundaryDetectionPoint *)calloc(count, sizeof(*kernel_points));
    bool *is_boundary = (bool *)calloc(count, sizeof(bool));
    PhraseBoundarySignal *signal = (PhraseBoundarySignal *)calloc(count, sizeof(PhraseBoundarySignal));
    if (kernel_points == NULL || is_boundary == NULL || signal == NULL) {
        free(kernel_points);
        free(is_boundary);
        free(signal);
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        kernel_points[i].has_cadence = points[i].has_cadence;
        kernel_points[i].has_discontinuity = points[i].has_discontinuity;
    }
    phrase_boundary_detect(kernel_points, count, max_length, is_boundary, signal);
    for (size_t i = 0; i < count; i++) {
        out[i].is_boundary = is_boundary[i];
        out[i].by_cadence = is_boundary[i] && signal[i] == PHRASE_BOUNDARY_SIGNAL_CADENCE;
    }
    free(kernel_points);
    free(is_boundary);
    free(signal);
    return STANDARD_FORM_ANALYSIS_OK;
}

/*
 * The same for the motive analysis (analysis_motive.h): standard-form's injected window matcher calls the kernel's Motif Window Matching,
 * which finds the windows whose Contour direction sequence and RhythmicPattern both equal another window's. The kernel returns one
 * MotifWindowOccurrence per window that matched, borrowing the window's event ids, which is how a window is found again here.
 */
StandardFormAnalysisStatus chain_form_match_windows(const StandardFormMotifWindow *windows, size_t count, bool *matched,
                                                    void *context) {
    (void)context;
    if (count > 0 && (windows == NULL || matched == NULL)) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    memset(matched, 0, count * sizeof(bool));
    if (count == 0) {
        return STANDARD_FORM_ANALYSIS_OK;
    }
    MotifWindowOccurrence *found = NULL;
    size_t found_count = 0;
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_ERR_DETECTOR;
    if (motif_window_matching_find(windows, count, &found, &found_count) == MOTIF_WINDOW_MATCHING_OK) {
        status = STANDARD_FORM_ANALYSIS_OK;
        for (size_t j = 0; j < found_count; j++) {
            for (size_t i = 0; i < count; i++) {
                if (found[j].matched_event_ids == windows[i].event_ids) {
                    matched[i] = true;
                }
            }
        }
    }
    motif_window_matching_destroy_occurrences(found);
    return status;
}
