#ifndef MUSIC_CORE_PROVENANCE_REFERENCE_H
#define MUSIC_CORE_PROVENANCE_REFERENCE_H

#include <stdbool.h>

#include "identity/entity_id.h"

/* Opaque, cross-cutting reference to a Provenance/Derivation record
   (ceiling L0-L1). Three-state presence model: present, explicitly
   absent, or never silently omitted — there is no default/uninitialized
   state, only the two explicit constructors below. Resolved only by
   provenance-aware code. Value equality on state and, when present, the
   referenced EntityID only. */

typedef struct ProvenanceReference ProvenanceReference;

typedef enum {
    PROVENANCE_REFERENCE_PRESENT,
    PROVENANCE_REFERENCE_ABSENT
} ProvenanceReferenceState;

typedef enum {
    PROVENANCE_REFERENCE_OK = 0,
    PROVENANCE_REFERENCE_ERR_NULL_TARGET_ID,
    PROVENANCE_REFERENCE_ERR_ALLOC
} ProvenanceReferenceResult;

/* Takes ownership of target_id on success. */
ProvenanceReferenceResult provenance_reference_create_present(EntityID *target_id, ProvenanceReference **out);
ProvenanceReferenceResult provenance_reference_create_absent(ProvenanceReference **out);
void provenance_reference_destroy(ProvenanceReference *reference);

ProvenanceReferenceState provenance_reference_state(const ProvenanceReference *reference);
/* NULL when state is PROVENANCE_REFERENCE_ABSENT. */
const EntityID *provenance_reference_target_id(const ProvenanceReference *reference);

bool provenance_reference_equals(const ProvenanceReference *a, const ProvenanceReference *b);

#endif