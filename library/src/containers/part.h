#ifndef MUSIC_CONTAINERS_PART_H
#define MUSIC_CONTAINERS_PART_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"

/*
 * Part (Container): represents an instrument/performer grouping. Groups
 * Staff(s) by reference, not by embedding the referenced objects -- same
 * reference-only containment pattern Staff itself uses toward Voice and
 * Layer (containers/staff.h).
 *
 * Fields: id (EntityID), staff_ids (owned array of EntityID references
 * to Staff, containers/staff.h). May be empty (count == 0, pass NULL)
 * -- a Part with no staves assigned yet is still a valid Part, same
 * "found nothing is not an error" treatment CandidateSet gives its own
 * arrays.
 *
 * Part does not itself resolve staff_ids to actual Staff instances --
 * that lookup is a registry/repository concern (L9, out of scope here).
 *
 * Part has no mutator: a revision is a new Part construction, not an
 * in-place field change (Law C2).
 *
 * Serialization: {id, staff_ids}.
 */

typedef struct Part Part;

typedef enum {
    PART_OK = 0,
    PART_ERR_NULL_ID,
    PART_ERR_NULL_STAFF_ID,
    PART_ERR_ALLOC
} PartResult;

/*
 * Takes ownership of id and each EntityID in staff_ids (if any) on
 * success (all destroyed by part_destroy). staff_ids is optional --
 * pass NULL with count == 0 if not supplied. No entry of a non-empty
 * staff_ids array may be NULL.
 */
PartResult part_create(EntityID *id, EntityID **staff_ids, size_t staff_id_count, Part **out);
void part_destroy(Part *part);

const EntityID *part_id(const Part *part);
size_t part_staff_id_count(const Part *part);
const EntityID *part_staff_id_at(const Part *part, size_t index);

/*
 * Identity equality: same EntityID, regardless of instance. Referenced
 * staff_ids do not factor in, same reasoning Staff's identity equality
 * excludes its referenced voice_ids/layer_ids.
 */
bool part_identity_equals(const Part *a, const Part *b);

#endif