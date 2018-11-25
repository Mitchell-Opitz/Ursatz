#include "identity/version_model.h"

bool version_model_is_valid_revision(const Version *source, const Version *candidate) {
    if (source == NULL || candidate == NULL) {
        return false;
    }
    if (!entity_id_equals(version_entity_id(source), version_entity_id(candidate))) {
        return false;
    }
    return version_revision(candidate) > version_revision(source);
}

bool version_model_is_valid_derivation(const EntityID *source_entity_id, const EntityID *derived_entity_id) {
    if (source_entity_id == NULL || derived_entity_id == NULL) {
        return false;
    }
    return !entity_id_equals(source_entity_id, derived_entity_id);
}