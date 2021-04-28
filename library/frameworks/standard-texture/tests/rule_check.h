#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_TESTS_RULE_CHECK_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_TESTS_RULE_CHECK_H

#include <assert.h>

#include "framework.h"

/* Every rule/constraint of a group references standard-texture-v1, and the
   constraint is hard. `rule_fn` / `constraint_fn` take the group's enum. */
#define CHECK_RULES_REFERENCE_FRAMEWORK(rule_fn, constraint_fn, count)                                   \
    do {                                                                                                  \
        for (int i = 0; i < (count); i++) {                                                               \
            const Rule *rule = rule_fn(i);                                                                \
            const Constraint *constraint = constraint_fn(i);                                              \
            assert(rule != NULL && constraint != NULL);                                                   \
            assert(framework_identity_equals(rule_framework(rule), standard_texture_framework()));         \
            assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);                                  \
        }                                                                                                 \
        assert(rule_fn(count) == NULL);                                                                   \
        assert(constraint_fn(count) == NULL);                                                             \
    } while (0)

#endif
