#include "pipeline/form_boundary.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/phrase_boundary_detection.h"
#include "core/value.h"
#include "interpretation/interpretation.h"
#include "semantics/cadence_type.h"

StandardFormAnalysisStatus form_boundary_detect(const StandardFormBoundaryPoint *points, size_t count,
                                                size_t max_length, StandardFormBoundary *out, void *context) {
    (void)context;
    if (points == NULL || out == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    memset(out, 0, count * sizeof(*out));
    if (count == 0) {
        return STANDARD_FORM_ANALYSIS_OK;
    }
    PhraseBoundaryDetectionPoint *kernel_points = calloc(count, sizeof(*kernel_points));
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
            if (!boundary->is_boundary) {
                continue;
            }
            /* The signal Interpretations are one per boundary point, in order. */
            const Value *signal_claim =
                signal_index < analysis_result_interpretation_count(signal_result)
                    ? interpretation_claim(analysis_result_interpretation_at(signal_result, signal_index))
                    : NULL;
            const PhraseBoundaryDetectionSignalClaim *signal =
                signal_claim != NULL ? (const PhraseBoundaryDetectionSignalClaim *)value_content(signal_claim, &size)
                                     : NULL;
            if (signal == NULL || size != sizeof(*signal)) {
                status = STANDARD_FORM_ANALYSIS_ERR_DETECTOR;
                break;
            }
            out[i].by_cadence = signal->signal == PHRASE_BOUNDARY_SIGNAL_CADENCE;
            signal_index++;
        }
    }
    analysis_result_destroy(boundary_result);
    analysis_result_destroy(signal_result);
    free(kernel_points);
    return status;
}
