#ifndef MUSIC_RULES_PREFERENCE_H
#define MUSIC_RULES_PREFERENCE_H

#include <stdbool.h>

#include "context/context.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "theory/framework.h"

/*
 * Preference (Category / base type): ranks candidates WITHOUT
 * invalidating them (TDD §97) -- distinct from Constraint
 * (rules/constraint.h), which can reject a candidate outright.
 *
 * Fields: id, framework.
 *
 * framework is a reference only, not owned -- same pattern as Rule and
 * Constraint.
 *
 * preference_rank is structurally distinct from Constraint's
 * evaluate: it has no PASS/FAIL vocabulary and no way to signal
 * rejection -- it only ever reports a rank magnitude (or that ranking
 * logic is not yet wired up) for a candidate, never that a candidate
 * is invalid. There is no rejection/invalidation return path anywhere
 * in this header. Like constraint_evaluate, it is a stub: actually
 * ranking candidates against generated material is a
 * generation-pipeline concern (music-generation, L8) and out of scope
 * for this pass.
 */

typedef struct Preference Preference;

typedef enum {
    PREFERENCE_OK = 0,
    PREFERENCE_ERR_NULL_ID,
    PREFERENCE_ERR_NULL_FRAMEWORK,
    PREFERENCE_ERR_ALLOC
} PreferenceResult;

typedef enum {
    PREFERENCE_RANK_COMPUTED = 0,
    PREFERENCE_RANK_ERR_NULL_CANDIDATE,
    PREFERENCE_RANK_NOT_IMPLEMENTED
} PreferenceRankResult;

/*
 * An exact-rational score (numerator/denominator, never float/double -- the
 * same representation Constraint's weight and Evidence use). A score for a
 * candidate lies in [0, 1] (numerator <= denominator): 1/1 is the best,
 * 0/1 the worst. A denominator of 0 means the callback could not score the
 * candidate at all (e.g. a candidate of a kind it does not know).
 */
typedef struct {
    unsigned long numerator;
    unsigned long denominator;
} PreferenceScore;

/*
 * Scoring callback a framework registers on a Preference (see
 * preference_set_score_fn). A Preference holds only this pointer: the
 * framework owns the callback and any data behind it, and nothing owned is
 * embedded in Preference. candidate is non-NULL; context may be NULL. Both
 * are referenced only for the duration of the call. The callback has no way
 * to reject a candidate: it only ever returns a score.
 */
typedef PreferenceScore (*PreferenceScoreFn)(const Preference *preference, const Value *candidate,
                                             const Context *context);

/* Takes ownership of id on success (destroyed by preference_destroy). framework is required and referenced only, not owned; it must outlive this Preference. */
PreferenceResult preference_create(EntityID *id, const Framework *framework, Preference **out);
void preference_destroy(Preference *preference);

const EntityID *preference_id(const Preference *preference);
const Framework *preference_framework(const Preference *preference);

bool preference_identity_equals(const Preference *a, const Preference *b);

/*
 * Registers (or, with NULL, clears) the scoring callback. Preference keeps
 * the pointer only and never owns what is behind it. A Preference starts
 * with no callback. No-op on a NULL preference.
 */
void preference_set_score_fn(Preference *preference, PreferenceScoreFn score);

/* The registered callback, or NULL if none (or preference is NULL). */
PreferenceScoreFn preference_score_fn(const Preference *preference);

/*
 * Ranks candidate by calling the scoring callback the Preference's framework
 * registered. Never rejects/invalidates a candidate: on
 * PREFERENCE_RANK_COMPUTED, out_numerator/out_denominator hold the callback's
 * score (each is written only if non-NULL). Returns
 * PREFERENCE_RANK_ERR_NULL_CANDIDATE for a NULL preference or candidate, and
 * PREFERENCE_RANK_NOT_IMPLEMENTED when no callback is registered or the
 * callback could not score the candidate (a score with denominator 0); the
 * outputs are left untouched in both cases. context is optional -- pass NULL
 * if not supplied.
 */
PreferenceRankResult preference_rank(const Preference *preference, const Value *candidate, const Context *context,
                                     unsigned long *out_numerator, unsigned long *out_denominator);

#endif