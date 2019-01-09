#include "events/tie.h"

#include <stdlib.h>

struct Tie {
    EntityID *first_note_event_id;
    EntityID *second_note_event_id;
};

TieResult tie_create(EntityID *first_note_event_id, EntityID *second_note_event_id, Tie **out) {
    if (first_note_event_id == NULL) {
        return TIE_ERR_NULL_FIRST_NOTE_EVENT_ID;
    }
    if (second_note_event_id == NULL) {
        return TIE_ERR_NULL_SECOND_NOTE_EVENT_ID;
    }

    Tie *tie = (Tie *)malloc(sizeof(Tie));
    if (tie == NULL) {
        return TIE_ERR_ALLOC;
    }

    tie->first_note_event_id = first_note_event_id;
    tie->second_note_event_id = second_note_event_id;

    *out = tie;
    return TIE_OK;
}

void tie_destroy(Tie *tie) {
    if (tie == NULL) {
        return;
    }
    entity_id_destroy(tie->first_note_event_id);
    entity_id_destroy(tie->second_note_event_id);
    free(tie);
}

const EntityID *tie_first_note_event_id(const Tie *tie) {
    return tie->first_note_event_id;
}

const EntityID *tie_second_note_event_id(const Tie *tie) {
    return tie->second_note_event_id;
}