#ifndef URSATZ_ANALYZER_MOTIVE_SKETCH_MAP_H
#define URSATZ_ANALYZER_MOTIVE_SKETCH_MAP_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>

#include "analysis/motive_sketch.h"

/*
 * The features of a motive family (AnalysisMotiveFeatures) as a library MotiveSketch: the prototype's notes, the
 * key region's scale, the meter and the pickup. No chords, features or role. The document reader (motive links) reads the document's features into an AnalysisMotiveFeatures and calls this too, so a family
 * has one sketch whichever side it is read from.
 *
 * A sketch's notes may not overlap, so a note held past the next onset is cut at it; the features keep the real
 * durations. The features' pickup is the first note's position in its bar (0 on a downbeat); the sketch's pickup is the
 * length before the first downbeat, so it is the bar minus that position (0 on a downbeat).
 *
 * False, sketch cleared, when the features hold no notes, more than MOTIVE_SKETCH_MAX_NOTES, a time that is not a
 * fraction, or a sketch that fails motive_sketch_validate.
 */
bool motive_features_to_sketch(const AnalysisMotiveFeatures *features, MotiveSketch *sketch);
bool motive_features_to_sketch(const AnalysisMotiveFeatures *features, MotiveSketch *sketch);

#endif
