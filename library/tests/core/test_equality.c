#include "core/equality.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

static Entity *make_entity(const char *id_value, unsigned long schema_value, const char *type,
                            const char *metadata) {
    EntityID *id = NULL;
    entity_id_create(id_value, &id);

    SchemaVersion *version = NULL;
    schema_version_create(schema_value, &version);

    ProvenanceReference *provenance = NULL;
    provenance_reference_create_absent(&provenance);

    Entity *entity = NULL;
    entity_create(id, type, version, metadata, provenance, &entity);
    return entity;
}

/* StructuralEquality: isomorphic structure with a different EntityID is
   structurally equal but NOT identity-equal. */
static void test_structural_equals_ignores_entity_id(void) {
    Entity *a = make_entity("work-1", 1, "Work", "key=C");
    Entity *b = make_entity("work-2", 1, "Work", "key=C");

    assert(structural_equals(a, b));
    assert(!entity_identity_equals(a, b));

    entity_destroy(a);
    entity_destroy(b);
}

static void test_structural_inequals_different_metadata(void) {
    Entity *a = make_entity("work-1", 1, "Work", "key=C");
    Entity *b = make_entity("work-2", 1, "Work", "key=D");

    assert(!structural_equals(a, b));

    entity_destroy(a);
    entity_destroy(b);
}

/*
 * SemanticEquality: contract only at L1 — no real comparison is possible
 * without Context (L6). This just confirms the function-pointer contract
 * is usable; it is not a semantic equality test and is intentionally
 * partial/deferred (Test Spec §9.2).
 */
static bool stub_semantic_equals(const void *a, const void *b, const void *context) {
    (void)context;
    return a == b;
}

static void test_semantic_equals_contract_is_callable(void) {
    SemanticEqualsFn fn = stub_semantic_equals;
    int value = 1;
    assert(fn(&value, &value, NULL));
    assert(!fn(&value, NULL, NULL));
}

static Value *canonicalize_round_to_tens(const Value *value, unsigned long policy_version) {
    size_t size = 0;
    const void *content = value_content(value, &size);
    int original = *(const int *)content;

    int canonical_int = (policy_version == 1) ? (original / 10) * 10 : original;

    Value *out = NULL;
    value_create(value_type(value), &canonical_int, sizeof(canonical_int), &out);
    return out;
}

/* CanonicalEquality: same normal form under policy version V is equal. */
static void test_canonical_equals_same_form_under_policy(void) {
    int content_a = 14;
    int content_b = 18;
    Value *a = NULL;
    Value *b = NULL;
    value_create("Duration", &content_a, sizeof(content_a), &a);
    value_create("Duration", &content_b, sizeof(content_b), &b);

    assert(canonical_equals(a, b, 1, canonicalize_round_to_tens));

    value_destroy(a);
    value_destroy(b);
}

/* Same objects MAY be canonical-unequal under a different policy version. */
static void test_canonical_unequal_under_different_policy_version(void) {
    int content_a = 14;
    int content_b = 18;
    Value *a = NULL;
    Value *b = NULL;
    value_create("Duration", &content_a, sizeof(content_a), &a);
    value_create("Duration", &content_b, sizeof(content_b), &b);

    assert(canonical_equals(a, b, 1, canonicalize_round_to_tens));
    assert(!canonical_equals(a, b, 2, canonicalize_round_to_tens));

    value_destroy(a);
    value_destroy(b);
}

/*
 * Canonical-equal does not imply identity-equal (Law 23). Value has no
 * identity field, so canonical equality between Values never carries an
 * identity claim by construction — the cross-check against Entity
 * identity applies once Entities embed canonicalizable values (L2+).
 */
static void test_canonical_equal_does_not_imply_identity(void) {
    int content_a = 10;
    int content_b = 10;
    Value *a = NULL;
    Value *b = NULL;
    value_create("Duration", &content_a, sizeof(content_a), &a);
    value_create("Duration", &content_b, sizeof(content_b), &b);

    assert(canonical_equals(a, b, 1, canonicalize_round_to_tens));
    assert(a != b);

    value_destroy(a);
    value_destroy(b);
}

int main(void) {
    test_structural_equals_ignores_entity_id();
    test_structural_inequals_different_metadata();
    test_semantic_equals_contract_is_callable();
    test_canonical_equals_same_form_under_policy();
    test_canonical_unequal_under_different_policy_version();
    test_canonical_equal_does_not_imply_identity();
    return 0;
}