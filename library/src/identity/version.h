#ifndef MUSIC_IDENTITY_VERSION_H
#define MUSIC_IDENTITY_VERSION_H

#include <stdbool.h>
#include "identity/entity_id.h"

typedef struct Version Version;

typedef enum {
    VERSION_OK = 0,
    VERSION_ERR_NULL_ENTITY_ID,
    VERSION_ERR_NON_POSITIVE_REVISION,
    VERSION_ERR_ALLOC
} VersionResult;

VersionResult version_create(const EntityID *entity_id, unsigned long revision, Version **out);
void version_destroy(Version *version);
bool version_equals(const Version *a, const Version *b);
const EntityID *version_entity_id(const Version *version);
unsigned long version_revision(const Version *version);

#endif