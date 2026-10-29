#include "containers/phrase.h"

#include <stdlib.h>

#include "containers/id_array.h"

struct Phrase {
    EntityID *id;
    EntityID **event_ids;
    size_t event_id_count;
};

PhraseResult phrase_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Phrase **out) {
    EntityID **copy = NULL;
    switch (container_shell_create(id, event_ids, event_id_count, &copy)) {
        case CONTAINER_SHELL_OK:
            break;
        case CONTAINER_SHELL_ERR_NULL_OWNER:
            return PHRASE_ERR_NULL_ID;
        case CONTAINER_SHELL_ERR_NULL_ID:
            return PHRASE_ERR_NULL_EVENT_ID;
        default:
            return PHRASE_ERR_ALLOC;
    }

    Phrase *phrase = (Phrase *)malloc(sizeof(Phrase));
    if (phrase == NULL) {
        free(copy);
        return PHRASE_ERR_ALLOC;
    }

    phrase->event_ids = copy;
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
    id_array_destroy(phrase->event_ids, phrase->event_id_count);
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