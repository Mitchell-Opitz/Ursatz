#ifndef MUSIC_CORE_VOICE_REFERENCE_H
#define MUSIC_CORE_VOICE_REFERENCE_H

#include <stdbool.h>

#include "identity/entity_id.h"

/* Opaque EntityID-shaped pointer to a Voice (L4). Carries no Voice
   fields/semantics; resolved only at L4+. Value equality on the
   referenced EntityID only. */

typedef struct VoiceReference VoiceReference;

typedef enum {
    VOICE_REFERENCE_OK = 0,
    VOICE_REFERENCE_ERR_NULL_TARGET_ID,
    VOICE_REFERENCE_ERR_ALLOC
} VoiceReferenceResult;

/* Takes ownership of target_id on success. */
VoiceReferenceResult voice_reference_create(EntityID *target_id, VoiceReference **out);
void voice_reference_destroy(VoiceReference *reference);

const EntityID *voice_reference_target_id(const VoiceReference *reference);
bool voice_reference_equals(const VoiceReference *a, const VoiceReference *b);

#endif