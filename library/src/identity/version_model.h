#ifndef MUSIC_IDENTITY_VERSION_MODEL_H
#define MUSIC_IDENTITY_VERSION_MODEL_H

#include <stdbool.h>
#include "identity/entity_id.h"
#include "identity/version.h"

/*
 * VersionModel is stateless: it enforces identity/revision/derivation
 * semantics (TDD §16) over EntityID/Version, not a constructed object.
 */

bool version_model_is_valid_revision(const Version *source, const Version *candidate);
bool version_model_is_valid_derivation(const EntityID *source_entity_id, const EntityID *derived_entity_id);

#endif