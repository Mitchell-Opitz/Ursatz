#ifndef MUSIC_STRUCTURE_MOTIF_OCCURRENCE_H
#define MUSIC_STRUCTURE_MOTIF_OCCURRENCE_H

#include <stddef.h>

#include "core/occurrence.h"
#include "identity/entity_id.h"

/*
 * MotifOccurrence: a situated instance of a Motif.
 *
 * Composes a core/occurrence.h Occurrence (its own id, and the id of
 * the Motif it is an occurrence of, as Occurrence's target_id) with
 * region_event_ids -- EntityID pointers at the concrete events present
 * at this occurrence, referenced not duplicated (TDD Sec56, Sec152).
 * This mirrors how Event (music-events) composes Occurrence with a
 * TemporalAnchor: both are the next object built directly on top of
 * Occurrence, so both embed it rather than flattening past it.
 *
 * Key pattern (TDD Sec58): the Motif is abstract; occurrences point at
 * concrete regions of musical material without the Motif itself being
 * duplicated per occurrence. A single Motif may have many
 * MotifOccurrences, each a distinct MotifOccurrence instance (distinct
 * own id) sharing the same target Motif id.
 *
 * MotifOccurrence has no mutator: a revision is a new construction, not
 * an in-place field change (Law C2).
 */

typedef struct MotifOccurrence MotifOccurrence;

typedef enum {
    MOTIF_OCCURRENCE_OK = 0,
    MOTIF_OCCURRENCE_ERR_NULL_OCCURRENCE,
    MOTIF_OCCURRENCE_ERR_NULL_REGION_EVENT_IDS,
    MOTIF_OCCURRENCE_ERR_NULL_REGION_EVENT_ID,
    MOTIF_OCCURRENCE_ERR_ALLOC
} MotifOccurrenceResult;

/*
 * Takes ownership of occurrence and of region_event_ids (and each
 * EntityID it points to) on success -- all destroyed by
 * motif_occurrence_destroy. region_event_ids must be non-NULL with
 * region_event_id_count > 0; no entry may be NULL. occurrence's
 * target_id is the Motif this is an occurrence of.
 */
MotifOccurrenceResult motif_occurrence_create(Occurrence *occurrence, EntityID **region_event_ids,
                                               size_t region_event_id_count, MotifOccurrence **out);
void motif_occurrence_destroy(MotifOccurrence *motif_occurrence);

const EntityID *motif_occurrence_id(const MotifOccurrence *motif_occurrence);
/* The id of the Motif this is an occurrence of. */
const EntityID *motif_occurrence_motif_id(const MotifOccurrence *motif_occurrence);
size_t motif_occurrence_region_event_id_count(const MotifOccurrence *motif_occurrence);
const EntityID *motif_occurrence_region_event_id_at(const MotifOccurrence *motif_occurrence, size_t index);

#endif