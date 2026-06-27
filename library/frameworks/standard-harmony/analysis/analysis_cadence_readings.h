#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_CADENCE_READINGS_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_CADENCE_READINGS_H

#include <stddef.h>

#include "analysis_cadence.h"
#include "analysis_input.h"
#include "analysis_runs.h"

/*
 * The evidence for two competing cadence readings (R-090, R-091, R-093, R-094). Two places in real music have two
 * defensible readings, and the adapter reports both with a share each, in thousandths, that sum to 1000:
 *
 *   C7  a root-position V(7) followed by I6 at a cadence point: an evaded (subverted) cadence, or a prolongation of
 *       the dominant that goes on to a later cadence.
 *   C9  a root-position V directly before a section start: a half cadence ending the earlier section, or the
 *       authentic cadence into the tonic that starts the next.
 *
 * The functions below are pure: they read the prepared segments and the runs (analysis_runs.h) and return the share
 * of the FIRST reading (the evaded cadence for C7, the half cadence for C9), clamped to [50, 950] so neither reading
 * is ever certain. The weights are in the HarmonyCadenceConfig (thousandths, added to or taken from a base of 500).
 *
 * EVADED SHARE = 500
 *   +/- reading_strong_arrival       the I6 starts on a STRONG beat (plus) or not (minus)
 *   +/- reading_cadential_approach   a predominant precedes the V, or the V has a cadential 6/4 (plus), or neither (minus)
 *   +   reading_followup_cadence     an authentic cadence (PAC, IAC or unspecified) arrives within
 *                                    reading_followup_beats beats after the I6 starts: the V was evaded and the
 *                                    phrase closes soon after (R-093)
 *   -   reading_tonic_continues      the run after the I6 is I or I6: the tonic is settled, the V was a passing step
 *
 * HALF SHARE = 500
 *   + reading_long_dominant          the V lasts at least reading_long_dominant_beats beats
 *   + reading_break_after_dominant   the soprano rests at the end of the V or at the section start
 *   - reading_pac_shape              the tonic starts on a STRONG beat with the soprano on scale degree 1
 *   - reading_short_dominant         the V lasts under 2 beats
 *
 * A beat is the segment's own `beat_length`, or config.beat_length when it has none.
 */

/* The evaded share, in thousandths, of the V(7) run `dominant_run` followed by the I6 run `dominant_run + 1`. 0 when
   the runs are not that shape or an argument is NULL. config may be NULL for the defaults. */
int harmony_cadence_evaded_share(const HarmonySegment *segments, size_t count, const HarmonyRun *runs,
                                 size_t run_count, size_t dominant_run, const HarmonyCadenceConfig *config);

/* The half-cadence share, in thousandths, of the V run `dominant_run` directly before the section start (the first
   segment of run `dominant_run + 1`). 0 when the runs are not that shape or an argument is NULL. */
int harmony_cadence_boundary_half_share(const HarmonySegment *segments, size_t count, const HarmonyRun *runs,
                                        size_t run_count, size_t dominant_run, const HarmonyCadenceConfig *config);

#endif
