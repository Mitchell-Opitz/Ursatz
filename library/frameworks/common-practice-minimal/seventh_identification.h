#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_SEVENTH_IDENTIFICATION_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_SEVENTH_IDENTIFICATION_H

#include <stdbool.h>
#include <stddef.h>

#include "chord_vocabulary_seventh.h"
#include "identity/entity_id.h"
#include "interpretation/chord.h"

/*
 * Deterministic, single-answer seventh-chord identification for
 * common-practice-minimal-v1.
 *
 * Unlike triad_identification.h, quality here cannot be resolved from
 * scale-degree position alone: e.g. scale degree 7 in a major key carries
 * a half-diminished seventh, but which quality any given seventh chord
 * is depends on its actual interval content, not which degree it happens
 * to sit on (this is exactly why Cadence Detection needs a real quality
 * label rather than a degree-only one). So, following the same
 * caller-supplies-the-dimension pattern key_identification.h already
 * uses for mode (root-relative semitone pitch classes rather than scale
 * degrees), this module takes each chord tone's semitone interval
 * (0-11) relative to the chord's own candidate root, already measured
 * by whoever has the tuning-system knowledge to do so. The Sonority's
 * own EntityID is still what the resulting Chord's target_id points to.
 *
 * Matching is by unordered interval-SET membership, exact cardinality
 * required (see seventh_identification.c): a match requires exactly 4
 * distinct root-relative intervals equal to one of the four diatonic
 * seventh-chord quality patterns this framework knows (dominant7,
 * major7, minor7, half-diminished7 -- chord_vocabulary_seventh.h).
 * Fully-diminished7 is not one of them (see that header). This is the
 * same exact-cardinality mechanism triad_identification.c uses, so it
 * inherits the same known rigidity: a sonority with any extra tone
 * beyond the required 4 fails to match rather than matching with the
 * extras tolerated. That rigidity is a disclosed, not-yet-fixed gap in
 * the chord-matching code path generally (see README.md); it is not
 * fixed or worsened here.
 *
 * Tie-break rule: matching is by interval-set only, never by which
 * tone is in the bass -- a seventh chord supplied in any inversion
 * still resolves to its root-position quality label. This framework
 * does not identify seventh-chord inversions in this pass.
 */

/*
 * Returns true and sets *out to the matching quality when
 * root_relative_intervals (each 0-11, duplicates ignored, order
 * irrelevant) matches exactly one of the four diatonic seventh-chord
 * quality interval sets. Returns false (out untouched) when no match
 * exists -- e.g. fewer than 4 distinct intervals, a value outside 0-11,
 * or a combination this framework does not define as one of its four
 * qualities.
 */
bool common_practice_identify_seventh(const int *root_relative_intervals, size_t interval_count,
                                       CommonPracticeSeventh *out);

/*
 * Convenience: identifies the quality per common_practice_identify_seventh
 * and, on a match, builds the real Chord for it via
 * common_practice_build_seventh_chord (see chord_vocabulary_seventh.h
 * for ownership rules on sonority_target_id). Returns false with
 * *out_chord untouched when no quality matches; sonority_target_id
 * ownership is unaffected by a no-match return (caller still owns it).
 */
bool common_practice_identify_seventh_chord(const int *root_relative_intervals, size_t interval_count,
                                             EntityID *sonority_target_id, Chord **out_chord);

#endif
