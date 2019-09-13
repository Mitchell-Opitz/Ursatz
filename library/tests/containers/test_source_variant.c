#include "containers/source_variant.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static SourceVariant *make_variant(const char *source_id_value, const char *descriptor) {
    EntityID *source_id = NULL;
    entity_id_create(source_id_value, &source_id);

    SourceVariant *variant = NULL;
    source_variant_create(source_id, descriptor, &variant);
    return variant;
}

static void test_create_rejects_null_source_id(void) {
    SourceVariant *variant = NULL;
    SourceVariantResult r = source_variant_create(NULL, "autograph", &variant);
    assert(r == SOURCE_VARIANT_ERR_NULL_SOURCE_ID);
    assert(variant == NULL);
}

static void test_create_rejects_null_descriptor(void) {
    EntityID *source_id = NULL;
    entity_id_create("source-1", &source_id);

    SourceVariant *variant = NULL;
    SourceVariantResult r = source_variant_create(source_id, NULL, &variant);
    assert(r == SOURCE_VARIANT_ERR_NULL_DESCRIPTOR);
    assert(variant == NULL);

    entity_id_destroy(source_id);
}

static void test_create_rejects_empty_descriptor(void) {
    EntityID *source_id = NULL;
    entity_id_create("source-1", &source_id);

    SourceVariant *variant = NULL;
    SourceVariantResult r = source_variant_create(source_id, "", &variant);
    assert(r == SOURCE_VARIANT_ERR_EMPTY_DESCRIPTOR);
    assert(variant == NULL);

    entity_id_destroy(source_id);
}

static void test_create_accepts_valid(void) {
    SourceVariant *variant = make_variant("source-1", "autograph");
    assert(variant != NULL);
    source_variant_destroy(variant);
}

static void test_source_id_accessor_returns_constructed_value(void) {
    SourceVariant *variant = make_variant("source-1", "autograph");
    const EntityID *source_id = source_variant_source_id(variant);
    assert(source_id != NULL);
    assert(strcmp(entity_id_value(source_id), "source-1") == 0);
    source_variant_destroy(variant);
}

static void test_descriptor_accessor_returns_constructed_value(void) {
    SourceVariant *variant = make_variant("source-1", "autograph");
    assert(strcmp(source_variant_descriptor(variant), "autograph") == 0);
    source_variant_destroy(variant);
}

static void test_equals_same_content(void) {
    SourceVariant *a = make_variant("source-1", "autograph");
    SourceVariant *b = make_variant("source-1", "autograph");
    assert(a != b);
    assert(source_variant_equals(a, b));
    source_variant_destroy(a);
    source_variant_destroy(b);
}

static void test_not_equal_with_different_source_id(void) {
    SourceVariant *a = make_variant("source-1", "autograph");
    SourceVariant *b = make_variant("source-2", "autograph");
    assert(!source_variant_equals(a, b));
    source_variant_destroy(a);
    source_variant_destroy(b);
}

static void test_not_equal_with_different_descriptor(void) {
    SourceVariant *a = make_variant("source-1", "autograph");
    SourceVariant *b = make_variant("source-1", "first-print");
    assert(!source_variant_equals(a, b));
    source_variant_destroy(a);
    source_variant_destroy(b);
}

int main(void) {
    test_create_rejects_null_source_id();
    test_create_rejects_null_descriptor();
    test_create_rejects_empty_descriptor();
    test_create_accepts_valid();
    test_source_id_accessor_returns_constructed_value();
    test_descriptor_accessor_returns_constructed_value();
    test_equals_same_content();
    test_not_equal_with_different_source_id();
    test_not_equal_with_different_descriptor();
    return 0;
}