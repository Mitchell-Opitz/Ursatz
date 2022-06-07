#ifndef URSATZ_SCALE_HARMONIZATION_TESTS_SCALE_RULE_CHECK_H
#define URSATZ_SCALE_HARMONIZATION_TESTS_SCALE_RULE_CHECK_H

#include <assert.h>

#include "scale_framework.h"

/* Every rule/constraint of a group references scale-harmonization-v1, and the
   constraint is hard or soft. `rule_fn` / `constraint_fn` take the group's
   enum. */
#define CHECK_RULES_REFERENCE_FRAMEWORK(rule_fn, constraint_fn, count)                                    \
    do {                                                                                                   \
        for (int i = 0; i < (count); i++) {                                                                \
            const Rule *rule = rule_fn(i);                                                                 \
            const Constraint *constraint = constraint_fn(i);                                               \
            assert(rule != NULL && constraint != NULL);                                                    \
            assert(framework_identity_equals(rule_framework(rule), scale_harmonization_framework()));      \
            assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD ||                                  \
                   constraint_type(constraint) == CONSTRAINT_TYPE_SOFT);                                   \
        }                                                                                                  \
        assert(rule_fn(count) == NULL);                                                                    \
        assert(constraint_fn(count) == NULL);                                                              \
    } while (0)

#endif
