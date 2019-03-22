#ifndef MUSIC_RULES_RULE_H
#define MUSIC_RULES_RULE_H

#include <stdbool.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "theory/framework.h"

/*
 * Rule (Category / base type): a formal evaluative statement (TDD
 * §95), associated with the Framework (music-theory) it is evaluated
 * under.
 *
 * Fields: id, framework, scope.
 *
 * framework is a reference only, not owned -- the same reference-only
 * pattern Context uses for its parent (context/context.h): rule_destroy
 * never destroys a Rule's framework, since the same Framework is
 * expected to be referenced by many Rules/Constraints/Preferences.
 * Caller is responsible for the Framework's lifetime. music-rules
 * depends directly on music-theory (Dependency & Layer Spec v2 §2, §3
 * -- both L7, this is not a sideways L6-style dependency), so this is a
 * direct Framework reference, not an opaque EntityID lookup.
 *
 * scope is kept as an opaque, nullable Value (core/value.h) for now --
 * a minimal placeholder, not a frozen schema, same treatment
 * Relationship gives its own scope field (relationship/relationship.h).
 */

typedef struct Rule Rule;

typedef enum {
    RULE_OK = 0,
    RULE_ERR_NULL_ID,
    RULE_ERR_NULL_FRAMEWORK,
    RULE_ERR_ALLOC
} RuleResult;

/*
 * Takes ownership of id (and of scope, if non-NULL) on success -- both
 * destroyed by rule_destroy. framework is required and referenced
 * only, not owned; it must outlive this Rule. scope is optional --
 * pass NULL if not supplied.
 */
RuleResult rule_create(EntityID *id, const Framework *framework, Value *scope, Rule **out);
void rule_destroy(Rule *rule);

const EntityID *rule_id(const Rule *rule);
const Framework *rule_framework(const Rule *rule);
const Value *rule_scope(const Rule *rule);

bool rule_identity_equals(const Rule *a, const Rule *b);

#endif