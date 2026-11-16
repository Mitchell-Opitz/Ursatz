#ifndef URSATZ_TESTS_SUPPORT_FRAMEWORK_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_FRAMEWORK_TEST_ACCESS_H

#include <stdbool.h>
#include <stdlib.h>

#include "identity/entity_id.h"
#include "theory/framework.h"
#include "theory/framework_internal.h"

/* What the tests do with a Framework: destroy it and compare identities. No application does, so these are not in theory/framework.h. */

static inline void framework_destroy(Framework *framework) {
    if (framework == NULL) {
        return;
    }
    entity_id_destroy(framework->id);
    free(framework->name);
    free(framework);
}

static inline bool framework_identity_equals(const Framework *a, const Framework *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}

#endif
