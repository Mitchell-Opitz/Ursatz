#ifndef URSATZ_TESTS_INTEGRATION_FORM_KERNEL_BRIDGE_H
#define URSATZ_TESTS_INTEGRATION_FORM_KERNEL_BRIDGE_H

#include "analysis_motive.h"
#include "analysis_phrase.h"

#include "ursatz_chain/form_bridge.h"

/*
 * The glue between standard-form's injected detectors and the kernel now lives in the chain (chain/ursatz_chain/form_bridge.h). These
 * two names stay as calls to it until the tests call the chain directly.
 */
static inline StandardFormAnalysisStatus form_kernel_detect(const StandardFormBoundaryPoint *points, size_t count,
                                                            size_t max_length, StandardFormBoundary *out, void *context) {
    return chain_form_boundary_detect(points, count, max_length, out, context);
}

static inline StandardFormAnalysisStatus form_kernel_match_windows(const StandardFormMotifWindow *windows, size_t count,
                                                                   bool *matched, void *context) {
    return chain_form_match_windows(windows, count, matched, context);
}

#endif
