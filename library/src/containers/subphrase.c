#include "containers/subphrase.h"

#include <stdlib.h>

#include "containers/id_array.h"

struct Subphrase {
    EntityID *id;
    EntityID **event_ids;
    size_t event_id_count;
};

SubphraseResult subphrase_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Subphrase **out) {
    EntityID **copy = NULL;
    switch (container_shell_create(id, event_ids, event_id_count, &copy)) {
        case CONTAINER_SHELL_OK:
            break;
        case CONTAINER_SHELL_ERR_NULL_OWNER:
            return SUBPHRASE_ERR_NULL_ID;
        case CONTAINER_SHELL_ERR_NULL_ID:
            return SUBPHRASE_ERR_NULL_EVENT_ID;
        default:
            return SUBPHRASE_ERR_ALLOC;
    }

    Subphrase *subphrase = (Subphrase *)malloc(sizeof(Subphrase));
    if (subphrase == NULL) {
        free(copy);
        return SUBPHRASE_ERR_ALLOC;
    }

    subphrase->event_ids = copy;
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
    id_array_destroy(subphrase->event_ids, subphrase->event_id_count);
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