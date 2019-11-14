#ifndef MUSIC_TRANSFORM_FRAGMENTATION_H
#define MUSIC_TRANSFORM_FRAGMENTATION_H

#include <stdbool.h>
#include <stddef.h>

#include "core/provenance_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "transform/transform_result.h"
#include "transform/transformation.h"

/*
 * Fragmentation (Transformation subtype): extracts a subset of
 * material from a Structure by reference (TDD's Fragmentation/
 * Expansion/Contraction group). Per the Registry, Fragmentation
 * operates on Structure/Event refs (L5/L3) -- extract by reference,
 * never by copying full NoteEvent payloads.
 *
 * source_structure_id is a required, owned EntityID reference to the
 * Structure this fragment is extracted from (structure/structure.h) --
 * referenced by id only, never embedded.
 *
 * extracted_event_ids/extracted_event_id_count is the extracted
 * subset: an owned EntityID* array of event references, the same
 * array-of-references-plus-count shape and ownership rules
 * MotifOccurrence uses for its region_event_ids
 * (structure/motif_occurrence.h) -- ids are referenced, never the
 * NoteEvent payloads they point to. Non-empty and no NULL entry.
 *
 * Owns a Transformation (transform/transformation.h) holding the base
 * §139 contract fields: input_type=output_type="Structure",
 * mapping_behavior states extraction is by reference.
 * fragmentation_id forwards to the owned Transformation.
 */

typedef struct Fragmentation Fragmentation;

typedef enum {
    FRAGMENTATION_OK = 0,
    FRAGMENTATION_ERR_NULL_ID,
    FRAGMENTATION_ERR_NULL_SOURCE_STRUCTURE_ID,
    FRAGMENTATION_ERR_NULL_EXTRACTED_EVENT_IDS,
    FRAGMENTATION_ERR_NULL_EXTRACTED_EVENT_ID,
    FRAGMENTATION_ERR_NULL_PROVENANCE,
    FRAGMENTATION_ERR_ALLOC
} FragmentationResult;

/*
 * Takes ownership of id, source_structure_id, provenance (all
 * transitively via the owned Transformation), and extracted_event_ids
 * plus each EntityID it points to, on success (all destroyed by
 * fragmentation_destroy). extracted_event_ids must be non-NULL with
 * extracted_event_id_count > 0; no entry may be NULL.
 */
FragmentationResult fragmentation_create(EntityID *id, EntityID *source_structure_id, EntityID **extracted_event_ids,
                                          size_t extracted_event_id_count, ProvenanceReference *provenance,
                                          Fragmentation **out);
void fragmentation_destroy(Fragmentation *fragmentation);

const EntityID *fragmentation_id(const Fragmentation *fragmentation);
const Transformation *fragmentation_transformation(const Fragmentation *fragmentation);
const EntityID *fragmentation_source_structure_id(const Fragmentation *fragmentation);
size_t fragmentation_extracted_event_id_count(const Fragmentation *fragmentation);
const EntityID *fragmentation_extracted_event_id_at(const Fragmentation *fragmentation, size_t index);

bool fragmentation_identity_equals(const Fragmentation *a, const Fragmentation *b);

typedef enum {
    FRAGMENTATION_APPLY_OK = 0,
    FRAGMENTATION_APPLY_ERR_NULL_FRAGMENTATION,
    FRAGMENTATION_APPLY_ERR_NULL_SOURCE_EVENTS,
    FRAGMENTATION_APPLY_ERR_EVENT_NOT_FOUND,
    FRAGMENTATION_APPLY_ERR_ALLOC
} FragmentationApplyResult;

/*
 * Applies fragmentation to source_events (a borrowed array of
 * source_event_count borrowed NoteEvent pointers -- the full sequence
 * fragmentation_source_structure_id names): looks up each of this
 * Fragmentation's declared extracted_event_ids by id equality against
 * source_events, and clones the matches into *out, in the declared
 * extracted_event_ids order (not necessarily source_events' own order --
 * the caller's declared order is the extraction's order).
 *
 * If any extracted_event_id has no matching id in source_events, this
 * fails with FRAGMENTATION_APPLY_ERR_EVENT_NOT_FOUND -- a declared
 * extraction target that does not exist in the named source is a real
 * error, not something to skip silently.
 *
 * source_events is never mutated; every event in *out carries a real
 * ProvenanceReference pointing back to the source event it was cloned
 * from.
 */
FragmentationApplyResult fragmentation_apply(const Fragmentation *fragmentation, NoteEvent **source_events,
                                              size_t source_event_count, TransformResult **out);

#endif