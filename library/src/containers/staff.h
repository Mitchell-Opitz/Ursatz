#ifndef MUSIC_CONTAINERS_STAFF_H
#define MUSIC_CONTAINERS_STAFF_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"

/*
 * Staff (Container): groups Voice(s) and Layer(s) by reference, not by
 * embedding the referenced objects. Same reference-only containment
 * pattern used throughout this codebase (e.g. Candidate's material_id,
 * CandidateSet's input_ids array -- generation/candidate.h).
 *
 * Fields: id (EntityID), voice_ids (owned array of EntityID references
 * to Voice, containers/voice.h), layer_ids (owned array of EntityID
 * references to Layer, containers/layer.h). Both arrays may be empty
 * (count == 0, pass NULL for the corresponding array) -- a Staff with
 * no voices or layers assigned yet is still a valid Staff, the same
 * "found nothing is not an error" treatment CandidateSet gives its own
 * arrays.
 *
 * Staff does not itself resolve these references to actual Voice/Layer
 * instances -- that lookup is a registry/repository concern (L9,
 * out of scope here), same as VoiceReference's own resolution.
 *
 * Staff has no mutator: a revision is a new Staff construction, not an
 * in-place field change (Law C2).
 *
 * Serialization: {id, voice_ids, layer_ids}.
 */

typedef struct Staff Staff;

typedef enum {
    STAFF_OK = 0,
    STAFF_ERR_NULL_ID,
    STAFF_ERR_NULL_VOICE_ID,
    STAFF_ERR_NULL_LAYER_ID,
    STAFF_ERR_ALLOC
} StaffResult;

/*
 * Takes ownership of id, each EntityID in voice_ids (if any), and each
 * EntityID in layer_ids (if any) on success (all destroyed by
 * staff_destroy). voice_ids/layer_ids are optional -- pass NULL with
 * the matching count == 0 for either not supplied. No entry of a
 * non-empty voice_ids or layer_ids array may be NULL.
 */
StaffResult staff_create(EntityID *id, EntityID **voice_ids, size_t voice_id_count, EntityID **layer_ids,
                          size_t layer_id_count, Staff **out);
void staff_destroy(Staff *staff);

const EntityID *staff_id(const Staff *staff);
size_t staff_voice_id_count(const Staff *staff);
const EntityID *staff_voice_id_at(const Staff *staff, size_t index);
size_t staff_layer_id_count(const Staff *staff);
const EntityID *staff_layer_id_at(const Staff *staff, size_t index);

/*
 * Identity equality: same EntityID, regardless of instance. Referenced
 * voice_ids/layer_ids do not factor in, same reasoning Layer's identity
 * equality excludes its descriptive grouping field.
 */
bool staff_identity_equals(const Staff *a, const Staff *b);

#endif