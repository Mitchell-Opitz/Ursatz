#ifndef MUSIC_CONTAINERS_PHRASE_H
#define MUSIC_CONTAINERS_PHRASE_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"

/*
 * Phrase (Container): a mid-level grouping of Event references
 * (events/event.h), by EntityID -- not by embedding the referenced
 * Event objects. Same reference-only containment pattern Part uses
 * toward Staff (containers/part.h). An Event's own EntityID is its
 * composed Occurrence's id (core/occurrence.h, event_occurrence() +
 * occurrence_id()); this type does not duplicate that lookup, it only
 * carries the ids.
 *
 * THIS IS NOT structure/motif_occurrence.h's occurrence-based Phrase
 * concept (referred to elsewhere as PhraseOccurrence, the L5
 * Structure-layer type: a situated instance of an abstract Phrase
 * Structure). This is the L4 Container-layer sense: a grouping object
 * that holds Event references directly. Same English word, two
 * distinct concepts at two different layers -- do not conflate or
 * merge them. Anything needing the L5 occurrence-based concept belongs
 * in structure/, not here.
 *
 * Fields: id (EntityID), event_ids (owned array of EntityID references
 * to Event). May be empty (count == 0, pass NULL) -- a Phrase with no
 * events assigned yet is still a valid Phrase, same "found nothing is
 * not an error" treatment CandidateSet gives its own arrays.
 *
 * Phrase does not itself resolve event_ids to actual Event instances --
 * that lookup is a registry/repository concern (L9, out of scope here).
 *
 * Phrase has no mutator: a revision is a new Phrase construction, not
 * an in-place field change (Law C2).
 *
 * Serialization: {id, event_ids}.
 */

typedef struct Phrase Phrase;

typedef enum {
    PHRASE_OK = 0,
    PHRASE_ERR_NULL_ID,
    PHRASE_ERR_NULL_EVENT_ID,
    PHRASE_ERR_ALLOC
} PhraseResult;

/*
 * Takes ownership of id and each EntityID in event_ids (if any) on
 * success (all destroyed by phrase_destroy). event_ids is optional --
 * pass NULL with count == 0 if not supplied. No entry of a non-empty
 * event_ids array may be NULL.
 */
PhraseResult phrase_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Phrase **out);
void phrase_destroy(Phrase *phrase);

const EntityID *phrase_id(const Phrase *phrase);
size_t phrase_event_id_count(const Phrase *phrase);
const EntityID *phrase_event_id_at(const Phrase *phrase, size_t index);

/*
 * Identity equality: same EntityID, regardless of instance. Referenced
 * event_ids do not factor in, same reasoning Part's identity equality
 * excludes its referenced staff_ids.
 */
bool phrase_identity_equals(const Phrase *a, const Phrase *b);

#endif