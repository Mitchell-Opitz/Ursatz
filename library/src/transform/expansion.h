#ifndef MUSIC_TRANSFORM_EXPANSION_H
#define MUSIC_TRANSFORM_EXPANSION_H

#include <stdbool.h>
#include <stddef.h>

#include "core/provenance_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "transform/transform_result.h"
#include "transform/transformation.h"

/*
 * Expansion (Transformation subtype): extends a Structure with
 * additional material by reference (TDD's Fragmentation/Expansion/
 * Contraction group). Per the Registry, Expansion operates on
 * Structure/Event refs (L5/L3) -- extend by reference, never by
 * copying full NoteEvent payloads.
 *
 * source_structure_id is a required, owned EntityID reference to the
 * Structure being extended (structure/structure.h) -- referenced by
 * id only, never embedded.
 *
 * appended_event_ids/appended_event_id_count is the material added:
 * an owned EntityID* array of event references, the same
 * array-of-references-plus-count shape and ownership rules
 * MotifOccurrence uses for its region_event_ids
 * (structure/motif_occurrence.h) and Fragmentation uses for its
 * extracted_event_ids (transform/fragmentation.h) -- ids are
 * referenced, never the NoteEvent payloads they point to. Non-empty
 * and no NULL entry.
 *
 * Owns a Transformation (transform/transformation.h) holding the base
 * §139 contract fields: input_type=output_type="Structure",
 * mapping_behavior states the original events are retained and the
 * appended events are added by reference. expansion_id forwards to
 * the owned Transformation.
 */

typedef struct Expansion Expansion;

typedef enum {
    EXPANSION_OK = 0,
    EXPANSION_ERR_NULL_ID,
    EXPANSION_ERR_NULL_SOURCE_STRUCTURE_ID,
    EXPANSION_ERR_NULL_APPENDED_EVENT_IDS,
    EXPANSION_ERR_NULL_APPENDED_EVENT_ID,
    EXPANSION_ERR_NULL_PROVENANCE,
    EXPANSION_ERR_ALLOC
} ExpansionResult;

/*
 * Takes ownership of id, source_structure_id, provenance (all
 * transitively via the owned Transformation), and appended_event_ids
 * plus each EntityID it points to, on success (all destroyed by
 * expansion_destroy). appended_event_ids must be non-NULL with
 * appended_event_id_count > 0; no entry may be NULL.
 */
ExpansionResult expansion_create(EntityID *id, EntityID *source_structure_id, EntityID **appended_event_ids,
                                  size_t appended_event_id_count, ProvenanceReference *provenance,
                                  Expansion **out);
void expansion_destroy(Expansion *expansion);

const EntityID *expansion_id(const Expansion *expansion);
const Transformation *expansion_transformation(const Expansion *expansion);
const EntityID *expansion_source_structure_id(const Expansion *expansion);
size_t expansion_appended_event_id_count(const Expansion *expansion);
const EntityID *expansion_appended_event_id_at(const Expansion *expansion, size_t index);

bool expansion_identity_equals(const Expansion *a, const Expansion *b);

typedef enum {
    EXPANSION_APPLY_OK = 0,
    EXPANSION_APPLY_ERR_NULL_EXPANSION,
    EXPANSION_APPLY_ERR_NULL_SOURCE_EVENTS,
    EXPANSION_APPLY_ERR_NULL_APPENDED_EVENTS,
    EXPANSION_APPLY_ERR_APPENDED_EVENT_COUNT_MISMATCH,
    EXPANSION_APPLY_ERR_APPENDED_EVENT_ID_MISMATCH,
    EXPANSION_APPLY_ERR_POSITION_OUT_OF_RANGE,
    EXPANSION_APPLY_ERR_ALLOC
} ExpansionApplyResult;

/*
 * Applies expansion to source_events (a borrowed array of
 * source_event_count borrowed NoteEvent pointers -- the full sequence
 * expansion_source_structure_id names), inserting appended_events (a
 * borrowed array the caller supplies, one real NoteEvent for each of
 * this Expansion's declared appended_event_ids, same order) at position
 * (0 = before every source event, source_event_count = after all of
 * them). This is the "append at a declared position" scoping decided
 * for this pass: Expansion's Registry description names extending a
 * Structure with new material by reference but does not specify an
 * insertion point, so this apply requires the caller to declare one
 * explicitly rather than guessing "always at the end."
 *
 * appended_events must have exactly appended_event_id_count entries,
 * and appended_events[i]'s id must equal
 * expansion_appended_event_id_at(expansion, i) -- Expansion's
 * constructor only stores id references, not NoteEvent payloads, so the
 * actual new material has to be supplied here, at apply time, and this
 * checks it against what was declared at construction rather than
 * trusting it blindly.
 *
 * Neither array is mutated; every event in *out carries a real
 * ProvenanceReference pointing back to whichever source or appended
 * event it was cloned from.
 */
ExpansionApplyResult expansion_apply(const Expansion *expansion, NoteEvent **source_events,
                                      size_t source_event_count, NoteEvent **appended_events,
                                      size_t appended_event_count, size_t position, TransformResult **out);

#endif