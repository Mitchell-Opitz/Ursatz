#include "containers/phrase.h"

#include <stdlib.h>

struct Phrase {
    EntityID *id;
    EntityID **event_ids;
    size_t event_id_count;
};

static EntityID **copy_id_array(EntityID **ids, size_t count) {
    if (count == 0) {
        return NULL;
    }
    EntityID **copy = (EntityID **)malloc(sizeof(EntityID *) * count);
    if (copy == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        copy[i] = ids[i];
    }
    return copy;
}

static void destroy_id_array(EntityID **ids, size_t count) {
    if (ids == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        entity_id_destroy(ids[i]);
    }
    free(ids);
}

PhraseResult phrase_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Phrase **out) {
    if (id == NULL) {
        return PHRASE_ERR_NULL_ID;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        if (event_ids[i] == NULL) {
            return PHRASE_ERR_NULL_EVENT_ID;
        }
    }

    Phrase *phrase = (Phrase *)malloc(sizeof(Phrase));
    if (phrase == NULL) {
        return PHRASE_ERR_ALLOC;
    }

    phrase->event_ids = copy_id_array(event_ids, event_id_count);
    if (event_id_count > 0 && phrase->event_ids == NULL) {
        free(phrase);
        return PHRASE_ERR_ALLOC;
    }

    phrase->id = id;
    phrase->event_id_count = event_id_count;

    *out = phrase;
    return PHRASE_OK;
}

void phrase_destroy(Phrase *phrase) {
    if (phrase == NULL) {
        return;
    }
    entity_id_destroy(phrase->id);
    destroy_id_array(phrase->event_ids, phrase->event_id_count);
    free(phrase);
}

const EntityID *phrase_id(const Phrase *phrase) {
    return phrase->id;
}

size_t phrase_event_id_count(const Phrase *phrase) {
    return phrase->event_id_count;
}

const EntityID *phrase_event_id_at(const Phrase *phrase, size_t index) {
    return phrase->event_ids[index];
}

bool phrase_identity_equals(const Phrase *a, const Phrase *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}