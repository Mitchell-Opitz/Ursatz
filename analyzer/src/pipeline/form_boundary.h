#ifndef URSATZ_ANALYZER_FORM_BOUNDARY_H
#define URSATZ_ANALYZER_FORM_BOUNDARY_H

#include <stddef.h>

/* Prefixed: standard-harmony has headers of the same names (analysis_input.h, analysis_phrase.h). */
#include "standard-form/analysis/analysis_phrase.h"

/*
 * The bridge from standard-form's injected boundary detector (analysis_phrase.h) to the kernel's Phrase Boundary
 * Detection (analysis/phrase_boundary_detection.h), ported from Ursatz's test glue
 * (tests/integration/form_kernel_bridge.h form_kernel_detect, R-053). A framework may not include kernel analysis, so
 * the Analyzer, which does, injects this function. It adds no segmenter: it maps the points onto the kernel's points
 * (the cadence label is the kernel's cadence_type_label), runs the kernel, and reads the answer from the kernel's
 * PhraseSegmentation and signal Interpretations.
 *
 * Has the StandardFormBoundaryDetector signature; context is unused.
 */
StandardFormAnalysisStatus form_boundary_detect(const StandardFormBoundaryPoint *points, size_t count,
                                                size_t max_length, StandardFormBoundary *out, void *context);

#endif
