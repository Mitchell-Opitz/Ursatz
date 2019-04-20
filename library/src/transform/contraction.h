#ifndef MUSIC_TRANSFORM_CONTRACTION_H
#define MUSIC_TRANSFORM_CONTRACTION_H

#include <stdbool.h>
#include <stddef.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/transformation.h"

/*
 * Contraction (Transformation subtype): compresses a Structure by
 * dropping material by reference (TDD's Fragmentation/Expansion/
 * Contraction group). Per the Registry, Contraction operates on
 * Structure/Event refs (L5/L3) -- compress by reference, never by
 * copying full NoteEvent payloads.
 *
 * source_structure_id is a required, owned EntityID reference to the
 * Structure being compressed (structure/structure.h) -- referenced by
 * id only, never embedded.
 *
 * removed_event_ids/removed_event_id_count is the material dropped:
 * an owned EntityID* array of event references, the same
 * array-of-references-plus-count shape and ownership rules
 * MotifOccurrence uses for its region_event_ids
 * (structure/motif_occurrence.h) and Fragmentation/Expansion use for
 * their own event-id arrays (transform/fragmentation.h,
 * transform/expansion.h) -- ids are referenced, never the NoteEvent
 * payloads they point to. Non-empty and no NULL entry.
 *
 * Owns a Transformation (transform/transformation.h) holding the base
 * §139 contract fields: input_type=output_type="Structure",
 * mapping_behavior states the remaining events are retained by
 * reference and the removed events are dropped by reference.
 * contraction_id forwards to the owned Transformation.
 */

typedef struct Contraction Contraction;

typedef enum {
    CONTRACTION_OK = 0,
    CONTRACTION_ERR_NULL_ID,
    CONTRACTION_ERR_NULL_SOURCE_STRUCTURE_ID,
    CONTRACTION_ERR_NULL_REMOVED_EVENT_IDS,
    CONTRACTION_ERR_NULL_REMOVED_EVENT_ID,
    CONTRACTION_ERR_NULL_PROVENANCE,
    CONTRACTION_ERR_ALLOC
} ContractionResult;

/*
 * Takes ownership of id, source_structure_id, provenance (all
 * transitively via the owned Transformation), and removed_event_ids
 * plus each EntityID it points to, on success (all destroyed by
 * contraction_destroy). removed_event_ids must be non-NULL with
 * removed_event_id_count > 0; no entry may be NULL.
 */
ContractionResult contraction_create(EntityID *id, EntityID *source_structure_id, EntityID **removed_event_ids,
                                      size_t removed_event_id_count, ProvenanceReference *provenance,
                                      Contraction **out);
void contraction_destroy(Contraction *contraction);

const EntityID *contraction_id(const Contraction *contraction);
/* The owned base Transformation -- input/output type, dimensions, mapping behavior, determinism, provenance. */
const Transformation *contraction_transformation(const Contraction *contraction);
const EntityID *contraction_source_structure_id(const Contraction *contraction);
size_t contraction_removed_event_id_count(const Contraction *contraction);
const EntityID *contraction_removed_event_id_at(const Contraction *contraction, size_t index);

bool contraction_identity_equals(const Contraction *a, const Contraction *b);

#endif