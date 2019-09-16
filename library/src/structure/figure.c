#include "structure/figure.h"

#include <stdlib.h>

struct Figure {
    EntityID *id;
    EntityID **event_ids;
    size_t event_id_count;
};

FigureResult figure_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Figure **out) {
    if (id == NULL) {
        return FIGURE_ERR_NULL_ID;
    }
    if (event_ids == NULL || event_id_count == 0) {
        return FIGURE_ERR_NULL_EVENT_IDS;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        if (event_ids[i] == NULL) {
            return FIGURE_ERR_NULL_EVENT_ID;
        }
    }

    Figure *figure = (Figure *)malloc(sizeof(Figure));
    if (figure == NULL) {
        return FIGURE_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * event_id_count);
    if (owned_ids == NULL) {
        free(figure);
        return FIGURE_ERR_ALLOC;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        owned_ids[i] = event_ids[i];
    }

    figure->id = id;
    figure->event_ids = owned_ids;
    figure->event_id_count = event_id_count;

    *out = figure;
    return FIGURE_OK;
}

void figure_destroy(Figure *figure) {
    if (figure == NULL) {
        return;
    }
    entity_id_destroy(figure->id);
    for (size_t i = 0; i < figure->event_id_count; i++) {
        entity_id_destroy(figure->event_ids[i]);
    }
    free(figure->event_ids);
    free(figure);
}

const EntityID *figure_id(const Figure *figure) {
    return figure->id;
}

size_t figure_event_id_count(const Figure *figure) {
    return figure->event_id_count;
}

const EntityID *figure_event_id_at(const Figure *figure, size_t index) {
    return figure->event_ids[index];
}

bool figure_identity_equals(const Figure *a, const Figure *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}