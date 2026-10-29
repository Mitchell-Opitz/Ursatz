#include "containers/idea.h"

#include <stdlib.h>

#include "containers/id_array.h"

struct Idea {
    EntityID *id;
    EntityID **event_ids;
    size_t event_id_count;
};

IdeaResult idea_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Idea **out) {
    EntityID **copy = NULL;
    switch (container_shell_create(id, event_ids, event_id_count, &copy)) {
        case CONTAINER_SHELL_OK:
            break;
        case CONTAINER_SHELL_ERR_NULL_OWNER:
            return IDEA_ERR_NULL_ID;
        case CONTAINER_SHELL_ERR_NULL_ID:
            return IDEA_ERR_NULL_EVENT_ID;
        default:
            return IDEA_ERR_ALLOC;
    }

    Idea *idea = (Idea *)malloc(sizeof(Idea));
    if (idea == NULL) {
        free(copy);
        return IDEA_ERR_ALLOC;
    }

    idea->event_ids = copy;
    idea->id = id;
    idea->event_id_count = event_id_count;

    *out = idea;
    return IDEA_OK;
}

void idea_destroy(Idea *idea) {
    if (idea == NULL) {
        return;
    }
    entity_id_destroy(idea->id);
    id_array_destroy(idea->event_ids, idea->event_id_count);
    free(idea);
}

const EntityID *idea_id(const Idea *idea) {
    return idea->id;
}

size_t idea_event_id_count(const Idea *idea) {
    return idea->event_id_count;
}

const EntityID *idea_event_id_at(const Idea *idea, size_t index) {
    return idea->event_ids[index];
}

bool idea_identity_equals(const Idea *a, const Idea *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}