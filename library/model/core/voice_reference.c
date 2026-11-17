#include "model/core/voice_reference.h"

#include <stdlib.h>

struct VoiceReference {
    EntityID *target_id;
};

VoiceReferenceResult voice_reference_create(EntityID *target_id, VoiceReference **out) {
    if (target_id == NULL) {
        return VOICE_REFERENCE_ERR_NULL_TARGET_ID;
    }

    VoiceReference *reference = (VoiceReference *)malloc(sizeof(VoiceReference));
    if (reference == NULL) {
        return VOICE_REFERENCE_ERR_ALLOC;
    }

    reference->target_id = target_id;

    *out = reference;
    return VOICE_REFERENCE_OK;
}

void voice_reference_destroy(VoiceReference *reference) {
    if (reference == NULL) {
        return;
    }
    entity_id_destroy(reference->target_id);
    free(reference);
}

const EntityID *voice_reference_target_id(const VoiceReference *reference) {
    return reference->target_id;
}