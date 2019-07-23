#ifndef MUSIC_PITCH_TUNING_SYSTEM_H
#define MUSIC_PITCH_TUNING_SYSTEM_H

#include <stdbool.h>

#include "identity/entity_id.h"

/*
 * TuningSystem (Entity category): a tuning definition (12-TET, just
 * intonation, a custom scheme, etc.) that PitchRealization measures its
 * ratios against. This base type only holds identity and a label; it
 * does not encode any particular tuning's actual pitch values here --
 * defining 12-TET's specific ratio set or JI's specific ratio set is out
 * of scope for this type (Deliverable D / Phase 2-3), matching this
 * platform's "no single theory baked in" principle.
 *
 * TuningSystem does not build on core/entity.h's Entity: the Entity/
 * Occurrence/Event category boundary is still contested upstream
 * (Domain Spec, System Registry), and building on Entity here would
 * itself be an assumption about how that boundary settles. This mirrors
 * theory/framework.h's Framework, which makes the same choice for the
 * same reason.
 */

typedef struct TuningSystem TuningSystem;

typedef enum {
    TUNING_SYSTEM_OK = 0,
    TUNING_SYSTEM_ERR_NULL_ID,
    TUNING_SYSTEM_ERR_NULL_NAME,
    TUNING_SYSTEM_ERR_ALLOC
} TuningSystemResult;

/* Takes ownership of id on success (destroyed by tuning_system_destroy). name is copied, required, non-empty. */
TuningSystemResult tuning_system_create(EntityID *id, const char *name, TuningSystem **out);
void tuning_system_destroy(TuningSystem *tuning_system);

const EntityID *tuning_system_id(const TuningSystem *tuning_system);
const char *tuning_system_name(const TuningSystem *tuning_system);

/* Identity equality: same EntityID, regardless of instance. */
bool tuning_system_identity_equals(const TuningSystem *a, const TuningSystem *b);

#endif