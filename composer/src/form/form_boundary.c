#include "form/form_internal.h"

#include <stdlib.h>
#include <string.h>

#include <analysis/phrase_boundary_detection.h>
#include <core/value.h>
#include <interpretation/interpretation.h>
#include <semantics/cadence_type.h>

/* Point i's answer from the kernel's results; the signal index moves on past each boundary. False on a malformed claim. */
static bool read_point(const AnalysisResult *boundaries, const AnalysisResult *signals, size_t i, size_t *signal_index,
                       StandardFormBoundary *out) {
    size_t size = 0;
    const Value *claim = interpretation_claim(analysis_result_interpretation_at(boundaries, i));
    const PhraseSegmentationClaim *boundary = (const PhraseSegmentationClaim *)value_content(claim, &size);
    if (!boundary || size != sizeof(*boundary)) {
        return false;
    }
    out->is_boundary = boundary->is_boundary;
    if (!boundary->is_boundary) {
        return true;
    }
    if (*signal_index >= analysis_result_interpretation_count(signals)) {
        return false;
    }
    claim = interpretation_claim(analysis_result_interpretation_at(signals, *signal_index));
    const PhraseBoundaryDetectionSignalClaim *signal =
        (const PhraseBoundaryDetectionSignalClaim *)value_content(claim, &size);
    if (!signal || size != sizeof(*signal)) {
        return false;
    }
    out->by_cadence = signal->signal == PHRASE_BOUNDARY_SIGNAL_CADENCE;
    (*signal_index)++;
    return true;
}

StandardFormAnalysisStatus form_boundary_detect(const StandardFormBoundaryPoint *points, size_t count,
                                                size_t max_length, StandardFormBoundary *out, void *context) {
    (void)context;
    if (!points || !out) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    memset(out, 0, count * sizeof(*out));
    if (count == 0) {
        return STANDARD_FORM_ANALYSIS_OK;
    }
    PhraseBoundaryDetectionPoint *kernel = calloc(count, sizeof(*kernel));
    if (!kernel) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        kernel[i].event = points[i].event;
        kernel[i].has_cadence = points[i].has_cadence;
        kernel[i].cadence_type = points[i].has_cadence ? cadence_type_label(points[i].cadence_type) : NULL;
        kernel[i].has_discontinuity = points[i].has_discontinuity;
    }
    PhraseBoundaryDetectionInput input = {kernel, count, max_length};
    AnalysisResult *boundaries = NULL;
    AnalysisResult *signals = NULL;
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_ERR_DETECTOR;
    if (phrase_boundary_detection_analyze(&input, NULL, NULL, &boundaries) == ANALYZER_OK &&
        phrase_boundary_detection_signal_analyze(&input, NULL, NULL, &signals) == ANALYZER_OK &&
        analysis_result_interpretation_count(boundaries) == count) {
        status = STANDARD_FORM_ANALYSIS_OK;
        size_t signal_index = 0;
        for (size_t i = 0; i < count && status == STANDARD_FORM_ANALYSIS_OK; i++) {
            if (!read_point(boundaries, signals, i, &signal_index, &out[i])) {
                status = STANDARD_FORM_ANALYSIS_ERR_DETECTOR;
            }
        }
    }
    analysis_result_destroy(boundaries);
    analysis_result_destroy(signals);
    free(kernel);
    return status;
}
