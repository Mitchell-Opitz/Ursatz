#include "events/articulation.h"

#include <stdlib.h>
#include <string.h>

struct Articulation {
    char *kind;
    EntityID *event_id;
};

ArticulationResult articulation_create(const char *kind, EntityID *event_id, Articulation **out) {
    if (kind == NULL || kind[0] == '\0') {
        return ARTICULATION_ERR_NULL_KIND;
    }
    if (event_id == NULL) {
        return ARTICULATION_ERR_NULL_EVENT_ID;
    }

    Articulation *articulation = (Articulation *)malloc(sizeof(Articulation));
    if (articulation == NULL) {
        return ARTICULATION_ERR_ALLOC;
    }

    size_t len = strlen(kind);
    articulation->kind = (char *)malloc(len + 1);
    if (articulation->kind == NULL) {
        free(articulation);
        return ARTICULATION_ERR_ALLOC;
    }
    memcpy(articulation->kind, kind, len + 1);

    articulation->event_id = event_id;

    *out = articulation;
    return ARTICULATION_OK;
}

void articulation_destroy(Articulation *articulation) {
    if (articulation == NULL) {
        return;
    }
    free(articulation->kind);
    entity_id_destroy(articulation->event_id);
    free(articulation);
}

const char *articulation_kind(const Articulation *articulation) {
    return articulation->kind;
}

const EntityID *articulation_event_id(const Articulation *articulation) {
    return articulation->event_id;
}