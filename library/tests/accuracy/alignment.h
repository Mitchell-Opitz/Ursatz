#ifndef URSATZ_TESTS_ACCURACY_ALIGNMENT_H
#define URSATZ_TESTS_ACCURACY_ALIGNMENT_H

#include "io/musicxml_importer.h"
#include "romantext.h"

/*
 * Whether an analysis and a score are of the same music in the same key (test infrastructure). Two sources of
 * one chorale can differ in transposition, in the notes of the cadence or in how the measures are cut; any of
 * these makes every number measured on the pair meaningless. This share is the first check: of the supported
 * annotated chords, the part whose root pitch class is sounding in some note of the score at the moment the chord
 * starts. A matching pair is near 1; a transposed one is far below.
 */
double alignment_root_share(const RomanTextAnalysis *analysis, const MusicXmlImport *score);

#endif
