#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_TRIAD_IDENTIFICATION_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_TRIAD_IDENTIFICATION_H

/*
 * Not the chord identification owner (common_practice_identify_weighted_sequence and
 * common_practice_identify_weighted). Kept only for the subset coverage reports (known duplicate-id
 * gap) (docs/ARCHITECTURE.md, Analytic job owners).
 */

#include <stdbool.h>
#include <stddef.h>

#include "chord_vocabulary.h"
#include "identity/entity_id.h"
#include "interpretation/chord.h"

/*
 * Deterministic, single-answer triad identification for
 * common-practice-minimal-v1.
 *
 * Input shape: PitchIdentity (pitch/pitch_identity.h) carries no
 * numeric semantics yet, and Sonority (semantics/sonority.h) never
 * resolves its event_ids to actual pitch content -- the same limitation
 * ursatz_chain/subset_chord_analyzer.c already documents ("candidate chord
 * labels and their fit weights are supplied already computed by
 * whoever has the tuning-system and framework knowledge to measure
 * them"). Following that precedent, this module takes caller-supplied
 * diatonic scale-degree integers (1-7, relative to the already-declared
 * key) rather than deriving them from a Sonority itself. The Sonority's
 * own EntityID is still what the resulting Chord's target_id points to.
 *
 * Tie-break rule: matching is by unordered scale-degree-SET membership
 * only, never by which degree is in the bass. A chord supplied in any
 * inversion (e.g. degrees {3, 5, 1} instead of {1, 3, 5}) still
 * resolves to its root-position triad label -- this framework does not
 * identify inversions in this pass.
 *
 * Matching is by SUBSET containment, not exact equality: a triad
 * matches if its 3 degrees are present among the input's distinct
 * degrees, even when extra (non-chord-tone) degrees are also present.
 * This is a deliberate stopgap for arpeggiated/multi-voice textures
 * where a beat's Sonority commonly includes a passing or neighbor tone
 * alongside the underlying triad (see sonority_builder.h and
 * Known_Gaps.md) -- not general non-chord-tone filtering, and it does
 * not tolerate more than one simultaneous non-chord degree in practice
 * (a second extra degree can complete an unrelated triad, see below).
 *
 * Because of subset matching, an input degree set CAN validly satisfy
 * more than one of the seven diatonic triads (e.g. {1, 3, 5, 6} fits
 * both I {1,3,5} and vi {6,1,3}) -- this replaces the exact-match
 * version's guarantee that at most one triad could ever match. When
 * that happens, the first match found in TRIAD_DEGREES scan order
 * (I, ii, iii, IV, V, vi, vii dim) wins, and the ambiguity is
 * reported through common_practice_identify_triad_ex (out_ambiguous).
 * A real confidence-weighted or duration-weighted tie-break is out of
 * scope for this pass, planned for later.
 */

/*
 * Returns true and sets *out to the matching triad when scale_degrees
 * (each 1-7, duplicates ignored, order irrelevant) contains all 3
 * degrees of at least one of the seven diatonic triads (see subset-match
 * note above). Returns false (out untouched) when no match exists --
 * e.g. fewer than 3 distinct degrees, or a combination that does not
 * contain any of this framework's seven triads.
 */
bool common_practice_identify_triad(const int *scale_degrees, size_t scale_degree_count, CommonPracticeTriad *out);

/*
 * As common_practice_identify_triad, and sets *out_ambiguous (may be NULL) to true when more than one
 * triad fits the input; *out is then the first match in scan order. *out_ambiguous is false on no match.
 */
bool common_practice_identify_triad_ex(const int *scale_degrees, size_t scale_degree_count, CommonPracticeTriad *out,
                                       bool *out_ambiguous);

/*
 * Convenience: identifies the triad per common_practice_identify_triad
 * and, on a match, builds the real Chord for it via
 * common_practice_build_triad_chord (see chord_vocabulary.h for
 * ownership rules on sonority_target_id). Returns false with *out_chord
 * untouched when no triad matches; sonority_target_id ownership is
 * unaffected by a no-match return (caller still owns it).
 */
bool common_practice_identify_triad_chord(const int *scale_degrees, size_t scale_degree_count,
                                           EntityID *sonority_target_id, Chord **out_chord);

#endif