#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_TRIAD_IDENTIFICATION_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_TRIAD_IDENTIFICATION_H

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
 * analysis/chord_identification.c already documents ("candidate chord
 * labels and their fit weights are supplied already computed by
 * whoever has the tuning-system and framework knowledge to measure
 * them"). Following that precedent, this module takes caller-supplied
 * diatonic scale-degree integers (1-7, relative to the already-declared
 * key) rather than deriving them from a Sonority itself. The Sonority's
 * own EntityID is still what the resulting Chord's target_id points to.
 *
 * Tie-break rule (the one required by this pass's deliberately
 * single-answer scope): matching is by unordered scale-degree-SET
 * membership only, never by which degree is in the bass. A chord
 * supplied in any inversion (e.g. degrees {3, 5, 1} instead of {1, 3,
 * 5}) still resolves to its root-position triad label -- this
 * framework does not identify inversions in this pass. Root-position
 * preference is therefore not actually needed to break a tie in
 * practice: each of the seven diatonic triads' three-degree sets is
 * distinct from every other's within one diatonic collection, so no
 * input scale-degree set can validly match more than one of the seven
 * -- true multi-candidate/confidence-weighted analysis (e.g. resolving
 * a set that also happens to work under a different framework, or
 * partial/four-note sonorities) is deliberately out of scope for this
 * pass, planned for later.
 */

/*
 * Returns true and sets *out to the matching triad when scale_degrees
 * (each 1-7, duplicates ignored, order irrelevant) matches exactly one
 * of the seven diatonic triads' degree sets. Returns false (out
 * untouched) when no match exists -- e.g. fewer than 3 distinct degrees,
 * a degree outside 1-7, or a combination this framework does not
 * define as one of its seven triads.
 */
bool common_practice_identify_triad(const int *scale_degrees, size_t scale_degree_count, CommonPracticeTriad *out);

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