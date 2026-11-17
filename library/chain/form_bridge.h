#ifndef URSATZ_CHAIN_FORM_BRIDGE_H
#define URSATZ_CHAIN_FORM_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/form/analysis_motive.h"
#include "analysis/form/analysis_phrase.h"

/*
 * The bridges from standard-form's injected detectors to the kernel. A framework may not include kernel analysis, so the glue that
 * does lives in the chain.
 *
 * chain_form_boundary_detect is a StandardFormBoundaryDetector (analysis_phrase.h) over the kernel's phrase boundary loop
 * (src/analysis/phrase_boundary_detection.h). It adds no segmenter: it maps the points onto PhraseBoundaryDetectionPoints, runs
 * phrase_boundary_detect, and answers is_boundary per point and, for a boundary point, whether a cadence made it. `context` is unused.
 */
StandardFormAnalysisStatus chain_form_boundary_detect(const StandardFormBoundaryPoint *points, size_t count, size_t max_length,
                                                      StandardFormBoundary *out, void *context);

/*
 * The same for the motive analysis (analysis_motive.h): standard-form's injected window matcher calls the kernel's Motif Window
 * Matching, which finds the windows whose Contour direction sequence and RhythmicPattern both equal another window's. The kernel
 * returns one MotifWindowOccurrence per window that matched, borrowing the window's event ids, which is how a window is found
 * again here. `context` is unused.
 */
StandardFormAnalysisStatus chain_form_match_windows(const StandardFormMotifWindow *windows, size_t count, bool *matched,
                                                    void *context);

#endif
