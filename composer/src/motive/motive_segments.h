#ifndef URSATZ_COMPOSER_MOTIVE_SEGMENTS_H
#define URSATZ_COMPOSER_MOTIVE_SEGMENTS_H

#include <stddef.h>

#include <ursatz_composer/motive_analysis.h>

/*
 * A segment proposal for a motive, from its bars and rhythmic groups (docs/motive-format.md, "Segments"). Writes at most
 * max segments (labelled a to h, a repeat of an earlier rhythm as a', a'') into out and returns how many: none for a
 * motive with no notes, one segment over every note for a motive with one group or without a rhythm. Deterministic.
 */
size_t motive_segments_propose(const MotiveAnalysis *analysis, MotiveSketchFeature *out, size_t max);

#endif
