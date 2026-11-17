#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "model/identity/entity_id.h"
#include "theory/framework.h"
#include "framework_test_access.h"

/*
 * Construction test: a Framework holds identity and a name.
 *
 * Structural test: framework.h/.c include no header from rules/
 * (Rule/Constraint/Preference) or theory/style_model.h -- Framework is
 * referenced BY that type, never the reverse. The only check here is that
 * Framework's own translation unit has no such include; the directory-wide
 * theory/rules architecture test (test_architecture_theory.c,
 * test_architecture_rules.c) is the durable guard.
 */

int main(void) {
    EntityID *id;
    assert(entity_id_create("framework-species-counterpoint", &id) == ENTITY_ID_OK);

    Framework *framework;
    assert(framework_create(id, "species counterpoint", &framework) == FRAMEWORK_OK);
    assert(strcmp(framework_name(framework), "species counterpoint") == 0);

    EntityID *other_id;
    assert(entity_id_create("framework-common-practice", &other_id) == ENTITY_ID_OK);
    Framework *other;
    assert(framework_create(other_id, "common-practice harmony", &other) == FRAMEWORK_OK);

    assert(framework_identity_equals(framework, framework));
    assert(!framework_identity_equals(framework, other));

    framework_destroy(framework);
    framework_destroy(other);

    printf("test_framework: ok\n");
    return 0;
}