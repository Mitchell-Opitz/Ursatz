#include "model/structure/motif.h"
#include "model/structure/motif_internal.h"

#include <stdlib.h>

MotifResult motif_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Motif **out) {
    if (id == NULL) {
        return MOTIF_ERR_NULL_ID;
    }
    if (event_ids == NULL || event_id_count == 0) {
        return MOTIF_ERR_NULL_EVENT_IDS;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        if (event_ids[i] == NULL) {
            return MOTIF_ERR_NULL_EVENT_ID;
        }
    }

    Motif *motif = (Motif *)malloc(sizeof(Motif));
    if (motif == NULL) {
        return MOTIF_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * event_id_count);
    if (owned_ids == NULL) {
        free(motif);
        return MOTIF_ERR_ALLOC;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        owned_ids[i] = event_ids[i];
    }

    motif->id = id;
    motif->event_ids = owned_ids;
    motif->event_id_count = event_id_count;

    *out = motif;
    return MOTIF_OK;
}

void motif_destroy(Motif *motif) {
    if (motif == NULL) {
        return;
    }
    entity_id_destroy(motif->id);
    for (size_t i = 0; i < motif->event_id_count; i++) {
        entity_id_destroy(motif->event_ids[i]);
    }
    free(motif->event_ids);
    free(motif);
}

const EntityID *motif_id(const Motif *motif) {
    return motif->id;
}
