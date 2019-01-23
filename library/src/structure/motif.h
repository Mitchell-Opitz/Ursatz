#ifndef MUSIC_STRUCTURE_MOTIF_H
#define MUSIC_STRUCTURE_MOTIF_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"

/*
 * Motif (Structure): an abstract recurring pattern.
 *
 * Fields: id (EntityID) and event refs -- EntityID pointers at the
 * NoteEvents that define the pattern, referenced not duplicated (TDD
 * Sec56, Sec152). Motif does not embed the Structure/Entity wrapper
 * types directly -- it flattens to the same field shape (id plus
 * event-id references), matching how concrete types elsewhere in this
 * codebase (e.g. NoteEvent relative to Event, Voice relative to
 * Container) implement their own fields rather than composing the
 * base type's wrapper struct.
 *
 * Motif is abstract: it describes the pattern, not any place it
 * occurs. A situated instance is a MotifOccurrence
 * (structure/motif_occurrence.h), which references a Motif by id
 * without the Motif itself being duplicated per occurrence -- a single
 * Motif may have many MotifOccurrences. Motif's own identity depends
 * only on its id, never on how many occurrences (if any) exist.
 *
 * Motif has no mutator: a revision is a new Motif construction, not an
 * in-place field change (Law C2).
 */

typedef struct Motif Motif;

typedef enum {
    MOTIF_OK = 0,
    MOTIF_ERR_NULL_ID,
    MOTIF_ERR_NULL_EVENT_IDS,
    MOTIF_ERR_NULL_EVENT_ID,
    MOTIF_ERR_ALLOC
} MotifResult;

/*
 * Takes ownership of id and of event_ids (and each EntityID it points
 * to) on success -- all destroyed by motif_destroy. event_ids must be
 * non-NULL with event_id_count > 0; no entry may be NULL.
 */
MotifResult motif_create(EntityID *id, EntityID **event_ids, size_t event_id_count, Motif **out);
void motif_destroy(Motif *motif);

const EntityID *motif_id(const Motif *motif);
size_t motif_event_id_count(const Motif *motif);
const EntityID *motif_event_id_at(const Motif *motif, size_t index);

/*
 * Identity equality: same EntityID, regardless of instance or how many
 * MotifOccurrences (if any) point at it.
 */
bool motif_identity_equals(const Motif *a, const Motif *b);

#endif