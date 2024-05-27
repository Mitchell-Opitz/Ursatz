#ifndef URSATZ_TESTS_INTEGRATION_FORM_KERNEL_BRIDGE_H
#define URSATZ_TESTS_INTEGRATION_FORM_KERNEL_BRIDGE_H

#include <stdlib.h>
#include <string.h>

#include "analysis/motif_window_matching.h"
#include "analysis/phrase_boundary_detection.h"
#include "analysis_motive.h"
#include "analysis_phrase.h"
#include "core/value.h"
#include "interpretation/interpretation.h"
#include "semantics/cadence_type.h"

/*
 * The bridge from standard-form's injected boundary detector (analysis_phrase.h) to the kernel's Phrase Boundary Detection
 * (src/analysis/phrase_boundary_detection.h). A framework may not include kernel analysis, so this glue, which does, lives in the
 * tests (the unit tests of standard-form's adapter and the integration tests share it). It adds no segmenter: it maps the points
 * onto PhraseBoundaryDetectionPoints (the cadence label is the kernel's cadence_type_label), runs the kernel, and reads the answer
 * from the PhraseSegmentation Interpretations (is_boundary per point) and the signal Interpretations (cadence or contour/rhythm,
 * one per boundary point, in order).
 */

static inline StandardFormAnalysisStatus form_kernel_detect(const StandardFormBoundaryPoint *points, size_t count,
                                                            size_t max_length, StandardFormBoundary *out, void *context) {
    (void)context;
    if (points == NULL || out == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    memset(out, 0, count * sizeof(*out));
    if (count == 0) {
        return STANDARD_FORM_ANALYSIS_OK;
    }
    PhraseBoundaryDetectionPoint *kernel_points = (PhraseBoundaryDetectionPoint *)calloc(count, sizeof(*kernel_points));
    if (kernel_points == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        kernel_points[i].event = points[i].event;
        kernel_points[i].has_cadence = points[i].has_cadence;
        kernel_points[i].cadence_type = points[i].has_cadence ? cadence_type_label(points[i].cadence_type) : NULL;
        kernel_points[i].has_discontinuity = points[i].has_discontinuity;
    }
    PhraseBoundaryDetectionInput input = {kernel_points, count, max_length};
    AnalysisResult *boundary_result = NULL;
    AnalysisResult *signal_result = NULL;
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_ERR_DETECTOR;
    if (phrase_boundary_detection_analyze(&input, NULL, NULL, &boundary_result) == ANALYZER_OK &&
        phrase_boundary_detection_signal_analyze(&input, NULL, NULL, &signal_result) == ANALYZER_OK &&
        analysis_result_interpretation_count(boundary_result) == count) {
        size_t signal_index = 0;
        status = STANDARD_FORM_ANALYSIS_OK;
        for (size_t i = 0; i < count; i++) {
            const Value *claim = interpretation_claim(analysis_result_interpretation_at(boundary_result, i));
            size_t size = 0;
            const PhraseSegmentationClaim *boundary = (const PhraseSegmentationClaim *)value_content(claim, &size);
            if (boundary == NULL || size != sizeof(*boundary)) {
                status = STANDARD_FORM_ANALYSIS_ERR_DETECTOR;
                break;
            }
            out[i].is_boundary = boundary->is_boundary;
            if (boundary->is_boundary) {
                const Value *signal_claim =
                    signal_index < analysis_result_interpretation_count(signal_result)
                        ? interpretation_claim(analysis_result_interpretation_at(signal_result, signal_index))
                        : NULL;
                const PhraseBoundaryDetectionSignalClaim *signal =
                    signal_claim != NULL ? (const PhraseBoundaryDetectionSignalClaim *)value_content(signal_claim, &size) : NULL;
                if (signal == NULL || size != sizeof(*signal)) {
                    status = STANDARD_FORM_ANALYSIS_ERR_DETECTOR;
                    break;
                }
                out[i].by_cadence = signal->signal == PHRASE_BOUNDARY_SIGNAL_CADENCE;
                signal_index++;
            }
        }
    }
    analysis_result_destroy(boundary_result);
    analysis_result_destroy(signal_result);
    free(kernel_points);
    return status;
}

/*
 * The same for the motive analysis (analysis_motive.h): standard-form's injected window matcher calls the kernel's Motif Window Matching,
 * which finds the windows whose Contour direction sequence and RhythmicPattern both equal another window's. The kernel returns one
 * MotifDetectionOccurrence per window that matched, borrowing the window's event ids, which is how a window is found again here.
 */
static inline StandardFormAnalysisStatus form_kernel_match_windows(const StandardFormMotifWindow *windows, size_t count,
                                                                   bool *matched, void *context) {
    (void)context;
    if (count > 0 && (windows == NULL || matched == NULL)) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    memset(matched, 0, count * sizeof(bool));
    if (count == 0) {
        return STANDARD_FORM_ANALYSIS_OK;
    }
    MotifWindow *kernel = (MotifWindow *)calloc(count, sizeof(MotifWindow));
    if (kernel == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        kernel[i].contour = windows[i].contour;
        kernel[i].rhythm = windows[i].rhythm;
        kernel[i].event_ids = windows[i].event_ids;
        kernel[i].event_id_count = windows[i].event_id_count;
    }
    MotifDetectionOccurrence *found = NULL;
    size_t found_count = 0;
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_ERR_DETECTOR;
    if (motif_window_matching_find(kernel, count, &found, &found_count) == MOTIF_WINDOW_MATCHING_OK) {
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
    free(kernel);
    return status;
}

#endif
