#ifndef MUSIC_EVENTS_TIE_H
#define MUSIC_EVENTS_TIE_H

#include "identity/entity_id.h"

/*
 * Tie: connects two NoteEvents WITHOUT merging them. A Tie is its own
 * object referencing two NoteEvent ids -- it does not hold, mutate, or
 * embed either NoteEvent, and constructing one leaves both referenced
 * NoteEvents' own fields untouched (tests/events/test_tie.c).
 *
 * References the two NoteEvents by EntityID only, not by embedded
 * NoteEvent instances, so building a Tie never requires (and cannot
 * accidentally couple to) holding full NoteEvent objects.
 */

typedef struct Tie Tie;

typedef enum {
    TIE_OK = 0,
    TIE_ERR_NULL_FIRST_NOTE_EVENT_ID,
    TIE_ERR_NULL_SECOND_NOTE_EVENT_ID,
    TIE_ERR_ALLOC
} TieResult;

/* Takes ownership of first_note_event_id and second_note_event_id on success (destroyed by tie_destroy). */
TieResult tie_create(EntityID *first_note_event_id, EntityID *second_note_event_id, Tie **out);
void tie_destroy(Tie *tie);

const EntityID *tie_first_note_event_id(const Tie *tie);
const EntityID *tie_second_note_event_id(const Tie *tie);

#endif