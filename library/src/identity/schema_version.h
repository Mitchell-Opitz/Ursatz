#ifndef MUSIC_IDENTITY_SCHEMA_VERSION_H
#define MUSIC_IDENTITY_SCHEMA_VERSION_H

#include <stdbool.h>

typedef struct SchemaVersion SchemaVersion;

typedef enum {
    SCHEMA_VERSION_OK = 0,
    SCHEMA_VERSION_ERR_NON_POSITIVE,
    SCHEMA_VERSION_ERR_ALLOC
} SchemaVersionResult;

SchemaVersionResult schema_version_create(unsigned long value, SchemaVersion **out);
void schema_version_destroy(SchemaVersion *version);
bool schema_version_equals(const SchemaVersion *a, const SchemaVersion *b);
unsigned long schema_version_value(const SchemaVersion *version);

#endif