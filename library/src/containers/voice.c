#include "containers/voice.h"

#include <stdlib.h>

struct Voice {
    EntityID *id;
};

VoiceResult voice_create(EntityID *id, Voice **out) {
    if (id == NULL) {
        return VOICE_ERR_NULL_ID;
    }

    Voice *voice = (Voice *)malloc(sizeof(Voice));
    if (voice == NULL) {
        return VOICE_ERR_ALLOC;
    }

    voice->id = id;

    *out = voice;
    return VOICE_OK;
}

void voice_destroy(Voice *voice) {
    if (voice == NULL) {
        return;
    }
    entity_id_destroy(voice->id);
    free(voice);
}

const EntityID *voice_id(const Voice *voice) {
    return voice->id;
}

bool voice_identity_equals(const Voice *a, const Voice *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}