#ifndef MUSIC_THEORY_FRAMEWORK_H
#define MUSIC_THEORY_FRAMEWORK_H

#include <stdbool.h>

#include "identity/entity_id.h"

/*
 * Framework (Category / base type): an interpretive system (theory) --
 * e.g. a species-counterpoint framework or a common-practice-harmony
 * framework, each built as an extension module in a later pass (TDD
 * §244-245). This base type only holds identity and a label; it
 * defines no rules/constraints/preferences of its own.
 *
 * Framework depends only on canonical L1-L6 interfaces. It MUST NOT
 * depend on its own siblings in music-rules (Rule, Constraint,
 * Preference) or on music-theory's own Heuristic/Tendency/
 * StatisticalModel/StyleModel -- Framework is referenced BY those
 * types, never the reverse (Dependency & Layer Spec v2 §2, §3).
 *
 * Framework also does not import music-interpretation: the L6
 * Interpretation<->Framework open issue (Dependency Spec §7.1)
 * resolves from this side by Framework staying the thing an L6+
 * consumer resolves a reference against, never the thing that reaches
 * back into music-interpretation itself.
 */

typedef struct Framework Framework;

typedef enum {
    FRAMEWORK_OK = 0,
    FRAMEWORK_ERR_NULL_ID,
    FRAMEWORK_ERR_NULL_NAME,
    FRAMEWORK_ERR_ALLOC
} FrameworkResult;

/* Takes ownership of id on success (destroyed by framework_destroy). name is copied, required, non-empty. */
FrameworkResult framework_create(EntityID *id, const char *name, Framework **out);
void framework_destroy(Framework *framework);

const EntityID *framework_id(const Framework *framework);
const char *framework_name(const Framework *framework);

bool framework_identity_equals(const Framework *a, const Framework *b);

#endif