#ifndef MUSIC_THEORY_HEURISTIC_H
#define MUSIC_THEORY_HEURISTIC_H

#include <stdbool.h>

#include "identity/entity_id.h"

/*
 * Heuristic (Category / base type): a strategy/estimate, explicitly
 * non-authoritative (TDD §98).
 *
 * A Heuristic MUST NOT be represented or documented as a universal
 * truth/rule -- unlike Rule/Constraint (rules/rule.h,
 * rules/constraint.h), this type carries no evaluate/pass-fail API and
 * no framework-scoped enforcement semantics at all. It is nothing more
 * than an identified, labeled strategy description; anything that
 * treats a Heuristic's label as authoritative is misusing this type.
 */

typedef struct Heuristic Heuristic;

typedef enum {
    HEURISTIC_OK = 0,
    HEURISTIC_ERR_NULL_ID,
    HEURISTIC_ERR_NULL_LABEL,
    HEURISTIC_ERR_ALLOC
} HeuristicResult;

/* Takes ownership of id on success (destroyed by heuristic_destroy). label is copied, required, non-empty. */
HeuristicResult heuristic_create(EntityID *id, const char *label, Heuristic **out);
void heuristic_destroy(Heuristic *heuristic);

const EntityID *heuristic_id(const Heuristic *heuristic);
const char *heuristic_label(const Heuristic *heuristic);

bool heuristic_identity_equals(const Heuristic *a, const Heuristic *b);

#endif