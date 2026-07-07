#ifndef URSATZ_ANALYZER_MOTIVE_FEATURES_H
#define URSATZ_ANALYZER_MOTIVE_FEATURES_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>

#include "analysis/motive_sketch.h"

/*
 * The features of a motive family (AnalysisMotiveFeatures) as a library MotiveSketch: the prototype's notes, the
 * key region's scale, the meter and the pickup. No chords, features or role. The document reader (motive links) builds
 * the same sketch from the document's features, so a family has one sketch whichever side it is read from.
 *
 * A sketch's notes may not overlap, so a note held past the next onset is cut at it; the features keep the real
 * durations. The features' pickup is the first note's position in its bar (0 on a downbeat); the sketch's pickup is the
 * length before the first downbeat, so it is the bar minus that position (0 on a downbeat).
 *
 * False, sketch cleared, when the features hold no notes, more than MOTIVE_SKETCH_MAX_NOTES, a time that is not a
 * fraction, or a sketch that fails motive_sketch_validate.
 */
bool motive_features_sketch(const AnalysisMotiveFeatures *features, MotiveSketch *sketch);

/* The degree of a pitch class against the major scale of the tonic: "1 b2 2 b3 3 4 #4 5 b6 6 b7 7". Static. */
const char *motive_features_degree_label(int pitch_class, int tonic_pc);

/* Frees the features' arrays and clears them. */
void motive_features_destroy(AnalysisMotiveFeatures *features);

#endif
