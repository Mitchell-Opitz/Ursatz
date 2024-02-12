#ifndef URSATZ_COMPOSER_FORM_INTERNAL_H
#define URSATZ_COMPOSER_FORM_INTERNAL_H

#include <stddef.h>

#include <standard-form/analysis/analysis_phrase.h>

/*
 * The StandardFormBoundaryDetector the form adapter is given: the kernel's Phrase Boundary Detection
 * (analysis/phrase_boundary_detection.h). standard-form may not include kernel analysis, so this bridge,
 * which does, is the Composer's. It adds no segmenter: it maps the points onto the kernel's (the cadence
 * label is cadence_type_label), runs it, and reads each point's answer from the PhraseSegmentation
 * Interpretations and each boundary's signal from the signal Interpretations, one per boundary, in order.
 */
StandardFormAnalysisStatus form_boundary_detect(const StandardFormBoundaryPoint *points, size_t count,
                                                size_t max_length, StandardFormBoundary *out, void *context);

#endif
