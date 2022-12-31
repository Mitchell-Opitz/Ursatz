#ifndef MUSIC_CONTAINERS_IDEA_H
#define MUSIC_CONTAINERS_IDEA_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"

/*
 * Idea (Container): the smallest grouping unit of a segmentation analysis -- a short group of
 * Event references (events/event.h), by EntityID, not by embedding the referenced Event
 * objects. Same reference-only containment pattern Phrase uses (containers/phrase.h). Ideas
 * hold the motivic material of a work and are often two measures long, but may be longer or
 * shorter; an Idea sits between a Motif and a Phrase in the form hierarchy (FormLevel in
 * semantics/form_level.h).
 *
 * THIS IS NOT structure/motif.h's Motif: a Motif is an abstract recurring pattern, an Idea is
 * a located group of events that may contain motives. Where ideas group into subphrases and
 * phrases is a convention of a Framework (frameworks/standard-form), not of this type.
 *
 * Fields: id (EntityID), event_ids (owned array of EntityID references to Event). May be
 * empty (count == 0, pass NULL), same treatment Phrase gives its own array.
 *
 * Idea does not itself resolve event_ids to actual Event instances -- that lookup is a
 * registry/repository concern (L9, out of scope here).
 *
 * Idea has no mutator: a revision is a new Idea construction, not an in-place field change
 * (Law C2).
 *
 * Serialization: {id, event_ids}.
 */

typedef struct Idea Idea;

typedef enum {
    IDEA_OK = 0,
    IDEA_ERR_NULL_ID,
    IDEA_ERR_NULL_EVENT_ID,
    IDEA_ERR_ALLOC
} IdeaResult;

/*
 * Takes ownership of id and each EntityID in event_ids (if any) on
 * success (all destroyed by idea_destroy). event_ids is optional --
 * pass NULL with count == 0 if not supplied. No entry of a non-empty
 * event_ids array may be NULL.
 */
IdeaResult idea_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Idea **out);
void idea_destroy(Idea *idea);

const EntityID *idea_id(const Idea *idea);
size_t idea_event_id_count(const Idea *idea);
const EntityID *idea_event_id_at(const Idea *idea, size_t index);

/*
 * Identity equality: same EntityID, regardless of instance. Referenced
 * event_ids do not factor in, same reasoning Part's identity equality
 * excludes its referenced staff_ids.
 */
bool idea_identity_equals(const Idea *a, const Idea *b);

#endif