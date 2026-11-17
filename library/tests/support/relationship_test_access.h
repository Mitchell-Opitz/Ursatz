#ifndef URSATZ_TESTS_SUPPORT_RELATIONSHIP_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_RELATIONSHIP_TEST_ACCESS_H

#include "model/relationship/relationship.h"
#include "model/relationship/relationship_internal.h"

/* What the tests read of a Relationship. No application reads these, so they are not in relationship/relationship.h. */

static inline const char *relationship_predicate_name(const Relationship *relationship) {
    return relationship->predicate_name;
}

static inline const Value *relationship_scope(const Relationship *relationship) {
    return relationship->scope;
}

#endif
