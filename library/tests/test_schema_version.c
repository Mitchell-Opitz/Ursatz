#include "identity/schema_version.h"

#include <assert.h>

static void test_create_rejects_zero(void) {
    SchemaVersion *v = NULL;
    SchemaVersionResult r = schema_version_create(0, &v);
    assert(r == SCHEMA_VERSION_ERR_NON_POSITIVE);
    assert(v == NULL);
}

static void test_create_accepts_valid(void) {
    SchemaVersion *v = NULL;
    SchemaVersionResult r = schema_version_create(1, &v);
    assert(r == SCHEMA_VERSION_OK);
    assert(v != NULL);
    schema_version_destroy(v);
}

static void test_equals_same_value(void) {
    SchemaVersion *a = NULL;
    SchemaVersion *b = NULL;
    schema_version_create(3, &a);
    schema_version_create(3, &b);

    assert(schema_version_equals(a, b));

    schema_version_destroy(a);
    schema_version_destroy(b);
}

static void test_equals_different_value(void) {
    SchemaVersion *a = NULL;
    SchemaVersion *b = NULL;
    schema_version_create(1, &a);
    schema_version_create(2, &b);

    assert(!schema_version_equals(a, b));

    schema_version_destroy(a);
    schema_version_destroy(b);
}

static void test_round_trip(void) {
    SchemaVersion *original = NULL;
    schema_version_create(5, &original);

    unsigned long serialized = schema_version_value(original);

    SchemaVersion *restored = NULL;
    schema_version_create(serialized, &restored);

    assert(schema_version_equals(original, restored));

    schema_version_destroy(original);
    schema_version_destroy(restored);
}

int main(void) {
    test_create_rejects_zero();
    test_create_accepts_valid();
    test_equals_same_value();
    test_equals_different_value();
    test_round_trip();
    return 0;
}