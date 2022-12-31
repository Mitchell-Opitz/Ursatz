#include "containers/idea.h"

#include <stdlib.h>

struct Idea {
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

IdeaResult idea_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Idea **out) {
    if (id == NULL) {
        return IDEA_ERR_NULL_ID;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        if (event_ids[i] == NULL) {
            return IDEA_ERR_NULL_EVENT_ID;
        }
    }

    Idea *idea = (Idea *)malloc(sizeof(Idea));
    if (idea == NULL) {
        return IDEA_ERR_ALLOC;
    }

    idea->event_ids = copy_id_array(event_ids, event_id_count);
    if (event_id_count > 0 && idea->event_ids == NULL) {
        free(idea);
        return IDEA_ERR_ALLOC;
    }

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
    destroy_id_array(idea->event_ids, idea->event_id_count);
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