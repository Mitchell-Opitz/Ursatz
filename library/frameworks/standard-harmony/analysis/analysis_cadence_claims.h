#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_CADENCE_CLAIMS_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_CADENCE_CLAIMS_H

/*
 * Half cadences a phrase may end on, and the cadences claimed at phrase points (R-390). Nothing in the analysis chain
 * calls these yet.
 */

#include <stdbool.h>
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

/*
 * CADENCES CLAIMED AT PHRASE POINTS (harmony_analysis_claim_cadences). A phrase point is a measure where a phrase may
 * end: a unit's last measure (`unit_end`), or the last measure of the first half of a unit of 8 or 16 (a half point).
 * `unit` numbers the unit, shared by the half point and the end of the same unit. `section_follows` marks a unit end
 * after which the next unit starts on the next measure (not elided). The points are data; where they come from is
 * not decided here. Measures are those of the segments' MetricalPositions.
 *
 * The claims are the cadences that end phrases: the found ones (harmony_analysis_find_cadences) and the readings
 * (harmony_analysis_half_readings), taken at the points and no others. The steps:
 *
 *   a. The POINT of a result is the measure of its arrival's first segment; when that is no point and the window of
 *      that segment ends in a later measure that is a point (a chord held over the barline), that measure.
 *   b. A HALF (found or reading) whose measure is no point moves its arrival to the segment that starts the next
 *      measure when that measure is a point and the segment is a root-position V with only root-position V and
 *      second-inversion triads between.
 *   c. BOUNDARIES first: harmony_analysis_add_boundary_cadences on a copy of `found`, with the section starts at the
 *      first measure after each unit end with `section_follows` and after each half point (C9, R-091: a V ending the
 *      measure before gets its half reading, and the authentic one when a root-position tonic starts the next measure).
 *      At a half point the result keeps only its HALF reading, since the authentic arrival opens the consequent. A found
 *      SUBVERTED that arrives on the downbeat after a point and whose dominant is in the point is replaced by the
 *      HALF reading of the dominant (one reading: the next tonic is inverted).
 *   d. At each point, the results whose point it is and whose primary type is a goal: PERFECT_AUTHENTIC,
 *      IMPERFECT_AUTHENTIC, AUTHENTIC_UNSPECIFIED or HALF. Unit ends first, then half points: a half point keeps no
 *      HALF when the end of its unit keeps a HALF (one phrase ending twice on the dominant).
 *   e. One result per point: the strongest (PERFECT_AUTHENTIC, then IMPERFECT_AUTHENTIC and AUTHENTIC_UNSPECIFIED,
 *      then HALF), a tie to the later arrival. Repeated closing V to I bars so give one closing cadence.
 *   f. A post-cadential PLAGAL result directly after a kept PERFECT_AUTHENTIC is kept. Nothing else is.
 *
 * `out` is in order of arrival and owns its results, which are copies: `found` and `readings` stay the caller's, and
 * either may be NULL or empty. No points give no cadences. On OK, *out is owned by the caller (harmony_cadences_destroy);
 * otherwise untouched. config may be NULL for the defaults. NULL_ARGUMENT for a NULL `segments` (with a count), `points`
 * (with a count) or `out`; BAD_CONFIG for a bad config.
 */
typedef struct {
    int measure;
    bool unit_end;
    size_t unit;
    bool section_follows;
} HarmonyPhrasePoint;

HarmonyCadenceAnalysisStatus harmony_analysis_claim_cadences(const HarmonySegment *segments, size_t count,
                                                             const HarmonyCadenceConfig *config,
                                                             const HarmonyCadences *found,
                                                             const HarmonyCadences *readings,
                                                             const HarmonyPhrasePoint *points, size_t point_count,
                                                             HarmonyCadences *out);

#endif
