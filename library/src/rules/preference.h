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
 * A Preference is structurally distinct from Constraint's evaluate: its
 * scoring callback has no PASS/FAIL vocabulary and no way to signal
 * rejection -- it only ever returns a score for a candidate, never that
 * a candidate is invalid.
 *
 * The scoring callback (PreferenceScoreFn) is registered on a Preference
 * by the Preference's framework. Like Constraint's evaluation callback, a
 * Preference holds only that pointer: the framework owns the callback and
 * whatever data is behind it, and nothing owned is embedded in Preference.
 * A score is an exact-rational (numerator/denominator, never
 * float/double) in [0, 1]. The struct is in rules/preference_internal.h;
 * tests read it through tests/support/preference_test_access.h.
 */

typedef struct Preference Preference;

typedef enum {
    PREFERENCE_OK = 0,
    PREFERENCE_ERR_NULL_ID,
    PREFERENCE_ERR_NULL_FRAMEWORK,
    PREFERENCE_ERR_ALLOC
} PreferenceResult;

/*
 * An exact-rational score (numerator/denominator, never float/double -- the
 * same representation Constraint's weight uses). A score for a
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

/* Takes ownership of id on success. framework is required and referenced only, not owned; it must outlive this Preference. The frameworks keep their Preferences for the life of the process. */
PreferenceResult preference_create(EntityID *id, const Framework *framework, Preference **out);

/*
 * Registers (or, with NULL, clears) the scoring callback. Preference keeps
 * the pointer only and never owns what is behind it. A Preference starts
 * with no callback. No-op on a NULL preference.
 */
void preference_set_score_fn(Preference *preference, PreferenceScoreFn score);

#endif