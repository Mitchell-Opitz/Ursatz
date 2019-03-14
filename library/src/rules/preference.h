#ifndef MUSIC_RULES_PREFERENCE_H
#define MUSIC_RULES_PREFERENCE_H

#include <stdbool.h>

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

/* Takes ownership of id on success (destroyed by preference_destroy). framework is required and referenced only, not owned; it must outlive this Preference. */
PreferenceResult preference_create(EntityID *id, const Framework *framework, Preference **out);
void preference_destroy(Preference *preference);

const EntityID *preference_id(const Preference *preference);
const Framework *preference_framework(const Preference *preference);

bool preference_identity_equals(const Preference *a, const Preference *b);

/*
 * Stub -- see file header. Never rejects/invalidates a candidate: on
 * PREFERENCE_RANK_COMPUTED, out_numerator/out_denominator are set;
 * this pass never reaches PREFERENCE_RANK_COMPUTED since no ranking
 * logic is wired up yet (always returns PREFERENCE_RANK_NOT_IMPLEMENTED
 * for a non-NULL candidate).
 */
PreferenceRankResult preference_rank(const Preference *preference, const Value *candidate,
                                      unsigned long *out_numerator, unsigned long *out_denominator);

#endif