#ifndef MUSIC_PERSISTENCE_NOTE_EVENTS_H
#define MUSIC_PERSISTENCE_NOTE_EVENTS_H

#include <stddef.h>

#include "events/note_event.h"
#include "persistence/persistence.h"

/*
 * NoteEvent persistence: stores/reloads a time-ordered NoteEvent
 * sequence into the `note_events` table (schema created by
 * persistence_open, see persistence.c).
 *
 * Only the fields NoteEvent itself carries are persisted: id,
 * temporal_anchor (exact rational), duration (exact rational), pitch
 * identity token, optional voice reference (opaque EntityID only, per
 * note_event.h's own serialization note), and provenance reference
 * (explicit present/absent, never omitted). optional_pitch_spelling is
 * out of scope for this pass (MIDIImporter never populates it; no
 * producer in this codebase does yet).
 *
 * Sequence order is preserved via an explicit sequence_order column,
 * not row id -- callers get back events in the same order they were
 * stored in.
 */

typedef enum {
    PERSISTENCE_NOTE_EVENTS_OK = 0,
    PERSISTENCE_NOTE_EVENTS_ERR_NULL_ARG,
    PERSISTENCE_NOTE_EVENTS_ERR_DB,
    PERSISTENCE_NOTE_EVENTS_ERR_ALLOC
} PersistenceNoteEventsResult;

/* Stores events[0..count) in order. Does not take ownership of events. */
PersistenceNoteEventsResult persistence_store_note_events(PersistenceHandle *handle, NoteEvent *const *events,
                                                           size_t count);

/*
 * Loads all stored NoteEvents in original sequence order into a newly
 * allocated array. Caller owns the array and every NoteEvent in it
 * (destroy each with note_event_destroy, then free the array).
 */
PersistenceNoteEventsResult persistence_load_note_events(PersistenceHandle *handle, NoteEvent ***out_events,
                                                          size_t *out_count);

#endif
