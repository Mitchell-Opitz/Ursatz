#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_SEVENTH_IDENTIFICATION_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_SEVENTH_IDENTIFICATION_H

/*
 * Not the chord identification owner (common_practice_identify_weighted_sequence and
 * common_practice_identify_weighted). Kept only for the subset coverage reports (known duplicate-id
 * gap) (docs/ARCHITECTURE.md, Analytic job owners).
 */

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
 * Matching is by unordered interval-SET SUBSET containment (see
 * seventh_identification.c): a match requires that one of the four
 * diatonic seventh-chord quality patterns this framework knows
 * (dominant7, major7, minor7, half-diminished7 --
 * chord_vocabulary_seventh.h) has all 4 of its intervals present among
 * the input's distinct intervals, even when extra (non-chord-tone)
 * intervals are also present. Fully-diminished7 is not one of the four
 * (it belongs to the weighted minor vocabulary, weighted_identification.h). This mirrors triad_identification.c's subset-match
 * stopgap for arpeggiated/multi-voice textures (see Known_Gaps.md); it
 * does not tolerate more than one simultaneous non-chord interval in
 * practice, since a second extra interval can complete an unrelated
 * quality (see below).
 *
 * Because of subset matching, an input interval set CAN validly satisfy
 * more than one of the four qualities -- this replaces the exact-match
 * version's guarantee that at most one quality could ever match. When
 * that happens, the first match found in SEVENTH_INTERVALS scan order
 * (dominant7, major7, minor7, half-diminished7) wins, and the ambiguity
 * is only logged to stderr (see seventh_identification.c), not surfaced
 * through this function's return value -- known limitation, not fixed
 * here. A real confidence-weighted or duration-weighted tie-break is
 * out of scope for this pass, planned for later.
 *
 * Tie-break rule: matching is by interval-set only, never by which
 * tone is in the bass -- a seventh chord supplied in any inversion
 * still resolves to its root-position quality label. This framework
 * does not identify seventh-chord inversions in this pass.
 */

/*
 * Returns true and sets *out to the matching quality when
 * root_relative_intervals (each 0-11, duplicates ignored, order
 * irrelevant) contains all 4 intervals of at least one of the four
 * diatonic seventh-chord quality interval sets (see subset-match note
 * above). Returns false (out untouched) when no match exists -- e.g.
 * fewer than 4 distinct intervals, or a combination that does not
 * contain any of this framework's four qualities.
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
