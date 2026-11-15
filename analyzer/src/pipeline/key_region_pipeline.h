#ifndef URSATZ_ANALYZER_KEY_REGION_PIPELINE_H
#define URSATZ_ANALYZER_KEY_REGION_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "common-practice-minimal/key_regions.h"

/*
 * The local keys of the piece, from the library chain's key regions (analysis_chain_key_regions: Ursatz's
 * key_regions_estimate, with the key signature events as boundaries, the chords of a first pass as functional evidence
 * and the phrase ends as boundaries for a second, and the key changes confirmed by the cadences). The analyzer adds no
 * music theory here: it copies out what the library established -- the regions (key, span, measures, uncertainty), the
 * change between each pair of neighbours (tonicization, weak modulation or modulation, as the library classes it;
 * whether it is a return; whether it has a pivot chord and which segment it is) -- and the home key, the library's key
 * with the longest total duration over its regions.
 *
 * A key signature event in the file is an authoritative boundary for the library, which restricts the region that
 * starts there to that signature's major key and relative minor. The analyzer reports whatever the library finds; it
 * does not compare it with the signature (the analysis document does, and surfaces a disagreement).
 *
 * Fills report's key regions, key changes and home region. The regions' labels and the arrays are owned by the report
 * (analysis_report_destroy). A piece with no regions (found NULL or empty) is left as a piece with no key. Returns
 * false only on allocation failure.
 *
 * The pivot reading of each change is left empty; identify_pivot_chords (chord_pipeline.h) fills it.
 */
bool key_regions_collect(const CommonPracticeKeyRegions *found, AnalysisReport *report);

#endif
