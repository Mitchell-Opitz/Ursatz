#ifndef MUSIC_CONTAINERS_MOVEMENT_H
#define MUSIC_CONTAINERS_MOVEMENT_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"

/*
 * Movement (Container): a work subdivision. Groups Section(s)
 * (containers/section.h) by reference, not by embedding the referenced
 * objects. Same reference-only containment pattern Section uses toward
 * Phrase (containers/section.h).
 *
 * Fields: id (EntityID), section_ids (owned array of EntityID
 * references to Section). May be empty (count == 0, pass NULL) -- a
 * Movement with no sections assigned yet is still a valid Movement,
 * same "found nothing is not an error" treatment CandidateSet gives its
 * own arrays.
 *
 * Movement does not itself resolve section_ids to actual Section
 * instances -- that lookup is a registry/repository concern (L9, out of
 * scope here).
 *
 * Movement has no mutator: a revision is a new Movement construction,
 * not an in-place field change (Law C2).
 *
 * Serialization: {id, section_ids}.
 */

typedef struct Movement Movement;

typedef enum {
    MOVEMENT_OK = 0,
    MOVEMENT_ERR_NULL_ID,
    MOVEMENT_ERR_NULL_SECTION_ID,
    MOVEMENT_ERR_ALLOC
} MovementResult;

/*
 * Takes ownership of id and each EntityID in section_ids (if any) on
 * success (all destroyed by movement_destroy). section_ids is
 * optional -- pass NULL with count == 0 if not supplied. No entry of a
 * non-empty section_ids array may be NULL.
 */
MovementResult movement_create(EntityID *id, EntityID **section_ids, size_t section_id_count, Movement **out);
void movement_destroy(Movement *movement);

const EntityID *movement_id(const Movement *movement);
size_t movement_section_id_count(const Movement *movement);
const EntityID *movement_section_id_at(const Movement *movement, size_t index);

/*
 * Identity equality: same EntityID, regardless of instance. Referenced
 * section_ids do not factor in, same reasoning Section's identity
 * equality excludes its referenced phrase_ids.
 */
bool movement_identity_equals(const Movement *a, const Movement *b);

#endif