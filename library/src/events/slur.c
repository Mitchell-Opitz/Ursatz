#include "events/slur.h"

#include <stdlib.h>

struct Slur {
    EntityID **note_event_ids;
    size_t note_event_id_count;
};

SlurResult slur_create(EntityID **note_event_ids, size_t note_event_id_count, Slur **out) {
    if (note_event_ids == NULL) {
        return SLUR_ERR_NULL_NOTE_EVENT_IDS;
    }
    if (note_event_id_count < 2) {
        return SLUR_ERR_TOO_FEW_NOTE_EVENT_IDS;
    }
    for (size_t i = 0; i < note_event_id_count; i++) {
        if (note_event_ids[i] == NULL) {
            return SLUR_ERR_NULL_NOTE_EVENT_ID;
        }
    }

    Slur *slur = (Slur *)malloc(sizeof(Slur));
    if (slur == NULL) {
        return SLUR_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * note_event_id_count);
    if (owned_ids == NULL) {
        free(slur);
        return SLUR_ERR_ALLOC;
    }
    for (size_t i = 0; i < note_event_id_count; i++) {
        owned_ids[i] = note_event_ids[i];
    }

    slur->note_event_ids = owned_ids;
    slur->note_event_id_count = note_event_id_count;

    *out = slur;
    return SLUR_OK;
}

void slur_destroy(Slur *slur) {
    if (slur == NULL) {
        return;
    }
    for (size_t i = 0; i < slur->note_event_id_count; i++) {
        entity_id_destroy(slur->note_event_ids[i]);
    }
    free(slur->note_event_ids);
    free(slur);
}

size_t slur_note_event_id_count(const Slur *slur) {
    return slur->note_event_id_count;
}

const EntityID *slur_note_event_id_at(const Slur *slur, size_t index) {
    return slur->note_event_ids[index];
}