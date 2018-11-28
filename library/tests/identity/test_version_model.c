#include "identity/entity_id.h"
#include "identity/version.h"
#include "identity/version_model.h"

#include <assert.h>
#include <stddef.h>

static void test_revision_valid_when_same_entity_and_incremented(void) {
    EntityID *id = NULL;
    entity_id_create("motif-1", &id);

    Version *source = NULL;
    Version *candidate = NULL;
    version_create(id, 1, &source);
    version_create(id, 2, &candidate);

    assert(version_model_is_valid_revision(source, candidate));

    entity_id_destroy(id);
    version_destroy(source);
    version_destroy(candidate);
}

static void test_revision_invalid_when_different_entity(void) {
    EntityID *id_a = NULL;
    EntityID *id_b = NULL;
    entity_id_create("motif-1", &id_a);
    entity_id_create("motif-2", &id_b);

    Version *source = NULL;
    Version *candidate = NULL;
    version_create(id_a, 1, &source);
    version_create(id_b, 2, &candidate);

    assert(!version_model_is_valid_revision(source, candidate));

    entity_id_destroy(id_a);
    entity_id_destroy(id_b);
    version_destroy(source);
    version_destroy(candidate);
}

static void test_revision_invalid_when_not_incremented(void) {
    EntityID *id = NULL;
    entity_id_create("motif-1", &id);

    Version *source = NULL;
    Version *candidate = NULL;
    version_create(id, 2, &source);
    version_create(id, 2, &candidate);

    assert(!version_model_is_valid_revision(source, candidate));

    entity_id_destroy(id);
    version_destroy(source);
    version_destroy(candidate);
}

static void test_revision_invalid_when_revision_decreases(void) {
    EntityID *id = NULL;
    entity_id_create("motif-1", &id);

    Version *source = NULL;
    Version *candidate = NULL;
    version_create(id, 3, &source);
    version_create(id, 2, &candidate);

    assert(!version_model_is_valid_revision(source, candidate));

    entity_id_destroy(id);
    version_destroy(source);
    version_destroy(candidate);
}

static void test_revision_invalid_when_null(void) {
    EntityID *id = NULL;
    entity_id_create("motif-1", &id);

    Version *v = NULL;
    version_create(id, 1, &v);

    assert(!version_model_is_valid_revision(NULL, v));
    assert(!version_model_is_valid_revision(v, NULL));

    entity_id_destroy(id);
    version_destroy(v);
}

static void test_derivation_valid_when_different_entity(void) {
    EntityID *source_id = NULL;
    EntityID *derived_id = NULL;
    entity_id_create("motif-1", &source_id);
    entity_id_create("motif-2", &derived_id);

    assert(version_model_is_valid_derivation(source_id, derived_id));

    entity_id_destroy(source_id);
    entity_id_destroy(derived_id);
}

static void test_derivation_invalid_when_same_entity(void) {
    EntityID *source_id = NULL;
    EntityID *derived_id = NULL;
    entity_id_create("motif-1", &source_id);
    entity_id_create("motif-1", &derived_id);

    assert(!version_model_is_valid_derivation(source_id, derived_id));

    entity_id_destroy(source_id);
    entity_id_destroy(derived_id);
}

static void test_derivation_invalid_when_null(void) {
    EntityID *id = NULL;
    entity_id_create("motif-1", &id);

    assert(!version_model_is_valid_derivation(NULL, id));
    assert(!version_model_is_valid_derivation(id, NULL));

    entity_id_destroy(id);
}

int main(void) {
    test_revision_valid_when_same_entity_and_incremented();
    test_revision_invalid_when_different_entity();
    test_revision_invalid_when_not_incremented();
    test_revision_invalid_when_revision_decreases();
    test_revision_invalid_when_null();
    test_derivation_valid_when_different_entity();
    test_derivation_invalid_when_same_entity();
    test_derivation_invalid_when_null();
    return 0;
}