#include "identity/schema_version.h"

#include <stdlib.h>

struct SchemaVersion {
    unsigned long value;
};

SchemaVersionResult schema_version_create(unsigned long value, SchemaVersion **out) {
    if (value == 0) {
        return SCHEMA_VERSION_ERR_NON_POSITIVE;
    }

    SchemaVersion *v = (SchemaVersion *)malloc(sizeof(SchemaVersion));
    if (v == NULL) {
        return SCHEMA_VERSION_ERR_ALLOC;
    }

    v->value = value;
    *out = v;
    return SCHEMA_VERSION_OK;
}

void schema_version_destroy(SchemaVersion *version) {
    free(version);
}

bool schema_version_equals(const SchemaVersion *a, const SchemaVersion *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->value == b->value;
}

unsigned long schema_version_value(const SchemaVersion *version) {
    return version->value;
}