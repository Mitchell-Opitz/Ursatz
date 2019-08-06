#ifndef MUSIC_EVENTS_SLUR_H
#define MUSIC_EVENTS_SLUR_H

#include <stddef.h>

#include "identity/entity_id.h"

/*
 * Slur: a phrasing/legato/analytical grouping over a range of
 * NoteEvents. A Slur is its own object referencing NoteEvent ids -- it
 * does not hold, mutate, or embed any NoteEvent, and constructing one
 * leaves every referenced NoteEvent's own fields untouched
 * (tests/events/test_slur.c), the same principle events/tie.h's Tie
 * follows.
 *
 * Unlike Tie (always exactly two NoteEvents), a Slur spans a variable-
 * length range, so its shape follows structure/structure.h's Structure
 * instead: an array of EntityID pointers plus a count, referencing the
 * NoteEvents by id only, never by embedded instance.
 *
 * note_event_id_count must be at least 2: a "range" of one NoteEvent is
 * degenerate -- a slur groups multiple notes, or it isn't a slur.
 */

typedef struct Slur Slur;

typedef enum {
    SLUR_OK = 0,
    SLUR_ERR_NULL_NOTE_EVENT_IDS,
    SLUR_ERR_TOO_FEW_NOTE_EVENT_IDS,
    SLUR_ERR_NULL_NOTE_EVENT_ID,
    SLUR_ERR_ALLOC
} SlurResult;

/*
 * Takes ownership of note_event_ids (and each EntityID it points to) on
 * success -- all destroyed by slur_destroy. note_event_ids must be
 * non-NULL with note_event_id_count >= 2; no entry may be NULL.
 */
SlurResult slur_create(EntityID **note_event_ids, size_t note_event_id_count, Slur **out);
void slur_destroy(Slur *slur);

size_t slur_note_event_id_count(const Slur *slur);
const EntityID *slur_note_event_id_at(const Slur *slur, size_t index);

#endif