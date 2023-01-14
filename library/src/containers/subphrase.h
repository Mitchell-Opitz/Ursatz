#ifndef MUSIC_CONTAINERS_SUBPHRASE_H
#define MUSIC_CONTAINERS_SUBPHRASE_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"

/*
 * Subphrase (Container): a mid-level grouping of Event references (events/event.h), by
 * EntityID, not by embedding the referenced Event objects. Same reference-only containment
 * pattern Idea and Phrase use (containers/idea.h, containers/phrase.h). A subphrase is made
 * of ideas and sits inside a phrase, between an Idea and a Phrase in the form hierarchy
 * (FormLevel in semantics/form_level.h). Unlike a phrase it need not end with a cadence.
 *
 * THIS IS NOT containers/phrase.h's Phrase: a Phrase must end with closure, usually a
 * cadence; a Subphrase does not. What role a subphrase plays (presentation or continuation)
 * is a SubphraseRole (semantics/subphrase_role.h) assigned by an analysis under a Framework
 * (frameworks/standard-form), not carried by this type.
 *
 * Fields: id (EntityID), event_ids (owned array of EntityID references to Event). May be
 * empty (count == 0, pass NULL), same treatment Idea gives its own array.
 *
 * Subphrase does not itself resolve event_ids to actual Event instances -- that lookup is a
 * registry/repository concern (L9, out of scope here).
 *
 * Subphrase has no mutator: a revision is a new Subphrase construction, not an in-place field
 * change (Law C2).
 *
 * Serialization: {id, event_ids}.
 */

typedef struct Subphrase Subphrase;

typedef enum {
    SUBPHRASE_OK = 0,
    SUBPHRASE_ERR_NULL_ID,
    SUBPHRASE_ERR_NULL_EVENT_ID,
    SUBPHRASE_ERR_ALLOC
} SubphraseResult;

/*
 * Takes ownership of id and each EntityID in event_ids (if any) on
 * success (all destroyed by subphrase_destroy). event_ids is optional --
 * pass NULL with count == 0 if not supplied. No entry of a non-empty
 * event_ids array may be NULL.
 */
SubphraseResult subphrase_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Subphrase **out);
void subphrase_destroy(Subphrase *subphrase);

const EntityID *subphrase_id(const Subphrase *subphrase);
size_t subphrase_event_id_count(const Subphrase *subphrase);
const EntityID *subphrase_event_id_at(const Subphrase *subphrase, size_t index);

/*
 * Identity equality: same EntityID, regardless of instance. Referenced
 * event_ids do not factor in, same reasoning Part's identity equality
 * excludes its referenced staff_ids.
 */
bool subphrase_identity_equals(const Subphrase *a, const Subphrase *b);

#endif