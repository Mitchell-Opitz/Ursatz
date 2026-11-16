#include "identity/schema_version.h"

#include <assert.h>
#include <stddef.h>

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

int main(void) {
    test_create_rejects_zero();
    test_create_accepts_valid();
    return 0;
}