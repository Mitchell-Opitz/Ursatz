#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_CADENCE_CLAIMS_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_CADENCE_CLAIMS_H

/*
 * Half cadences a phrase may end on, and the cadences claimed at phrase points (R-390). Nothing in the analysis chain
 * calls these yet.
 */

#include <stddef.h>

#include "analysis_cadence.h"
#include "analysis_input.h"

/*
 * HALF READINGS (harmony_analysis_half_readings). harmony_analysis_find_cadences reads a V as a half cadence only
 * when a tonic or a predominant comes before it and it falls on a cadence point, and drops a V that a later cadence
 * resolves. A phrase on a grid may still end on such a V, so these are the HALF results the grid may end on, beside
 * the found ones. Each is built as harmony_analysis_add_boundary_cadences builds its half reading: the Cadence shell,
 * the Interpretation, the uncertainty of the chords it spans, share 1000 and no alternative. The results are in
 * order of arrival. A measure is the one in a segment's MetricalPosition; a downbeat is offset 0.
 *
 *   END ON V      for each found result whose primary type is PERFECT_AUTHENTIC, IMPERFECT_AUTHENTIC,
 *                 AUTHENTIC_UNSPECIFIED or SUBVERTED, whose arrival's first segment starts a measure and whose
 *                 dominant's first segment (the cadential 6/4's when it has one) is in the measure before: a HALF
 *                 arriving on the dominant, from the dominant's first segment to the segment before the found arrival.
 *                 The cadence the V resolves into is read as the V's own end, as when a phrase stops on V and the
 *                 next one starts on I.
 *   DOMINANT AGAIN  a valid segment that is a root-position V (harmony_cadence_is_dominant_root) and starts a measure,
 *                 where walking back over segments that are root-position V or triads in second inversion reaches a
 *                 valid segment of degree 1, 2, 3, 4 or 6, and the segment starting the next measure is not a
 *                 root-position tonic (or none does): a HALF arriving on that segment alone. A V that comes back after a
 *                 neighbor or cadential 6/4 is the same dominant, and the measure it starts is where the phrase may
 *                 stop.
 *
 * A reading whose arrival segment is the arrival of a found HALF is left out. `found` NULL or empty gives only the
 * second kind. On OK, *out is owned by the caller (harmony_cadences_destroy). config may be NULL for the defaults;
 * BAD_CONFIG for a bad config.
 */
HarmonyCadenceAnalysisStatus harmony_analysis_half_readings(const HarmonySegment *segments, size_t count,
                                                            const HarmonyCadenceConfig *config,
                                                            const HarmonyCadences *found, HarmonyCadences *out);

#endif
