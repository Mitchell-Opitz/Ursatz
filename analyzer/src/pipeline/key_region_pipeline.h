#ifndef URSATZ_ANALYZER_KEY_REGION_PIPELINE_H
#define URSATZ_ANALYZER_KEY_REGION_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "common-practice-minimal/key_regions.h"
#include "pipeline/segmentation.h"
#include "time/musical_time.h"

/*
 * The local keys of the piece, from Ursatz's key-region detection
 * (key_regions_estimate in frameworks/common-practice-minimal/key_regions.h). The analyzer adds no
 * music theory here: it weights each segment as the chord identification does, hands the library the
 * segments and the file's key signature events, and copies out what it establishes -- the regions
 * (key, span, measures, uncertainty), the change between each pair of neighbours (tonicization,
 * weak modulation or modulation, as the library classes it; whether it is a return; whether it has a
 * pivot chord and which segment it is).
 *
 * A key signature event in the file is an authoritative boundary for the library, which restricts the
 * region that starts there to that signature's major key and relative minor. The analyzer passes the
 * events and reports whatever the library finds; it does not compare them with the result (the
 * analysis document does, and surfaces a disagreement).
 *
 * On success *regions and *changes are owned by the caller (release each region's label, each change's
 * pivot with analysis_chord_reading_destroy, and both arrays; analysis_report_destroy does). The
 * library failing to find regions (no segments, a segment it cannot weight) is not an error: it leaves
 * *region_count at 0, as a piece with no key. Returns false only on allocation failure.
 *
 * chords is each segment's identified chord as functional evidence (common_practice_key_chord_of, from a pass
 * before this one), or NULL for none: the library then reads the pitch classes alone.
 *
 * boundaries are phrase boundary times (NULL with boundary_count 0 for none), which the library snaps the
 * region edges to (CommonPracticeKeyRegionsConfig.phrase_boundaries).
 *
 * *found holds the library's own regions and changes, parallel to *regions and *changes, for the stages that read
 * its kernel Keys; owned by the caller (common_practice_key_regions_destroy), zeroed when there are none.
 *
 * The pivot reading of each change is left empty; identify_pivot_chords (chord_pipeline.h) fills it.
 */
bool estimate_key_regions(const ImportedNotes *notes, const Segmentation *segmentation,
                          const CommonPracticeKeyChord *chords, const MusicalTime *const *boundaries,
                          size_t boundary_count, CommonPracticeKeyRegions *found,
                          AnalysisKeyRegion **regions, size_t *region_count, AnalysisKeyChange **changes,
                          size_t *change_count);

/*
 * Copies the extent and cadence confirmation of each of found's changes into the parallel changes, after the caller
 * has confirmed them with the cadences (harmony_stage_confirm_key_changes).
 */
void sync_key_change_extents(const CommonPracticeKeyRegions *found, AnalysisKeyChange *changes);

/*
 * The home key: the library offers no determination of its own, so it is the key (tonic and mode) with
 * the greatest total duration over all its regions, a tie going to the key that appears first. Returns the
 * first region in that key and sets *duration to the key's total time in whole notes, as a rational.
 * Total time, not the longest single region, so a key the detection splits into several regions is not
 * outweighed by one long stretch of another. region_count must be at least 1.
 */
size_t home_key_region(const AnalysisKeyRegion *regions, size_t region_count, long *duration_numerator,
                       unsigned long *duration_denominator);

#endif
