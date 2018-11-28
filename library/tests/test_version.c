#include "identity/entity_id.h"
#include "identity/version.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_null_entity_id(void) {
    Version *v = NULL;
    VersionResult r = version_create(NULL, 1, &v);
    assert(r == VERSION_ERR_NULL_ENTITY_ID);
    assert(v == NULL);
}

static void test_create_rejects_zero_revision(void) {
    EntityID *id = NULL;
    entity_id_create("motif-1", &id);

    Version *v = NULL;
    VersionResult r = version_create(id, 0, &v);
    assert(r == VERSION_ERR_NON_POSITIVE_REVISION);
    assert(v == NULL);

    entity_id_destroy(id);
}

static void test_create_accepts_valid(void) {
    EntityID *id = NULL;
    entity_id_create("motif-1", &id);

    Version *v = NULL;
    VersionResult r = version_create(id, 1, &v);
    assert(r == VERSION_OK);
    assert(v != NULL);

    entity_id_destroy(id);
    version_destroy(v);
}

static void test_equals_same_entity_and_revision(void) {
    EntityID *id_a = NULL;
    EntityID *id_b = NULL;
    entity_id_create("motif-1", &id_a);
    entity_id_create("motif-1", &id_b);

    Version *a = NULL;
    Version *b = NULL;
    version_create(id_a, 4, &a);
    version_create(id_b, 4, &b);

    assert(version_equals(a, b));

    entity_id_destroy(id_a);
    entity_id_destroy(id_b);
    version_destroy(a);
    version_destroy(b);
}

static void test_equals_different_revision(void) {
    EntityID *id = NULL;
    entity_id_create("motif-1", &id);

    Version *a = NULL;
    Version *b = NULL;
    version_create(id, 1, &a);
    version_create(id, 2, &b);

    assert(!version_equals(a, b));

    entity_id_destroy(id);
    version_destroy(a);
    version_destroy(b);
}

static void test_equals_different_entity_id(void) {
    EntityID *id_a = NULL;
    EntityID *id_b = NULL;
    entity_id_create("motif-1", &id_a);
    entity_id_create("motif-2", &id_b);

    Version *a = NULL;
    Version *b = NULL;
    version_create(id_a, 1, &a);
    version_create(id_b, 1, &b);

    assert(!version_equals(a, b));

    entity_id_destroy(id_a);
    entity_id_destroy(id_b);
    version_destroy(a);
    version_destroy(b);
}

static void test_identity_stable_across_revision(void) {
    /* Domain Spec: identity (EntityID) is stable across revisions (TDD §16). */
    EntityID *id = NULL;
    entity_id_create("motif-1", &id);

    Version *v1 = NULL;
    Version *v2 = NULL;
    version_create(id, 1, &v1);
    version_create(id, 2, &v2);

    assert(entity_id_equals(version_entity_id(v1), version_entity_id(v2)));
    assert(!version_equals(v1, v2));

    entity_id_destroy(id);
    version_destroy(v1);
    version_destroy(v2);
}

int main(void) {
    test_create_rejects_null_entity_id();
    test_create_rejects_zero_revision();
    test_create_accepts_valid();
    test_equals_same_entity_and_revision();
    test_equals_different_revision();
    test_equals_different_entity_id();
    test_identity_stable_across_revision();
    return 0;
}