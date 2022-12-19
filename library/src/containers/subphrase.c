#include "containers/subphrase.h"

#include <stdlib.h>

struct Subphrase {
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

SubphraseResult subphrase_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Subphrase **out) {
    if (id == NULL) {
        return SUBPHRASE_ERR_NULL_ID;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        if (event_ids[i] == NULL) {
            return SUBPHRASE_ERR_NULL_EVENT_ID;
        }
    }

    Subphrase *subphrase = (Subphrase *)malloc(sizeof(Subphrase));
    if (subphrase == NULL) {
        return SUBPHRASE_ERR_ALLOC;
    }

    subphrase->event_ids = copy_id_array(event_ids, event_id_count);
    if (event_id_count > 0 && subphrase->event_ids == NULL) {
        free(subphrase);
        return SUBPHRASE_ERR_ALLOC;
    }

    subphrase->id = id;
    subphrase->event_id_count = event_id_count;

    *out = subphrase;
    return SUBPHRASE_OK;
}

void subphrase_destroy(Subphrase *subphrase) {
    if (subphrase == NULL) {
        return;
    }
    entity_id_destroy(subphrase->id);
    destroy_id_array(subphrase->event_ids, subphrase->event_id_count);
    free(subphrase);
}

const EntityID *subphrase_id(const Subphrase *subphrase) {
    return subphrase->id;
}

size_t subphrase_event_id_count(const Subphrase *subphrase) {
    return subphrase->event_id_count;
}

const EntityID *subphrase_event_id_at(const Subphrase *subphrase, size_t index) {
    return subphrase->event_ids[index];
}

bool subphrase_identity_equals(const Subphrase *a, const Subphrase *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}