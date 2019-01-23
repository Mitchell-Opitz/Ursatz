#ifndef MUSIC_STRUCTURE_CADENCE_H
#define MUSIC_STRUCTURE_CADENCE_H

#include <stdbool.h>
#include <stddef.h>

#include "core/value.h"
#include "identity/entity_id.h"

/*
 * Cadence (Structure): a formal closure unit.
 *
 * Fields: id (EntityID), event refs (EntityID pointers at the defining
 * events, referenced not duplicated -- TDD Sec56, Sec152), and an
 * optional harmonic context.
 *
 * PHASE-5 STRUCTURAL SHELL ONLY. The System Registry flags Cadence as
 * an explicit Phase 5/8 deliverable: this pass builds identity, event
 * refs, and a placeholder harmonic-context field only. Cadence
 * DETECTION (recognizing that a given region of events forms a
 * cadence) is Phase 8 (music-analysis) work and is out of scope here --
 * nothing in this file analyzes harmony or infers cadence type.
 *
 * optional_harmonic_context is deliberately opaque: a Value
 * (core/value.h) carrying arbitrary content bytes under a caller-chosen
 * type tag, the same opaque-token approach PitchIdentity uses
 * (pitch/pitch_identity.h). Full harmonic-context semantics belong to
 * L6+/L7, not built yet -- this field exists so Cadence's shape is
 * complete without requiring that machinery, and is nullable so
 * construction is never blocked on it.
 *
 * Cadence has no mutator: a revision is a new construction, not an
 * in-place field change (Law C2).
 */

typedef struct Cadence Cadence;

typedef enum {
    CADENCE_OK = 0,
    CADENCE_ERR_NULL_ID,
    CADENCE_ERR_NULL_EVENT_IDS,
    CADENCE_ERR_NULL_EVENT_ID,
    CADENCE_ERR_ALLOC
} CadenceResult;

/*
 * Takes ownership of id, of event_ids (and each EntityID it points to),
 * and (when non-NULL) of optional_harmonic_context, on success -- all
 * destroyed by cadence_destroy. event_ids must be non-NULL with
 * event_id_count > 0; no entry may be NULL. optional_harmonic_context
 * may be NULL.
 */
CadenceResult cadence_create(EntityID *id, EntityID **event_ids, size_t event_id_count,
                              Value *optional_harmonic_context, Cadence **out);
void cadence_destroy(Cadence *cadence);

const EntityID *cadence_id(const Cadence *cadence);
size_t cadence_event_id_count(const Cadence *cadence);
const EntityID *cadence_event_id_at(const Cadence *cadence, size_t index);
/* NULL if not present. */
const Value *cadence_optional_harmonic_context(const Cadence *cadence);

/* Identity equality: same EntityID, regardless of instance. */
bool cadence_identity_equals(const Cadence *a, const Cadence *b);

#endif