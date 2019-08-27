#ifndef MUSIC_CONTAINERS_MEASURE_H
#define MUSIC_CONTAINERS_MEASURE_H

#include <stdbool.h>

#include "identity/entity_id.h"

/*
 * Measure (Container): a metrical grouping, referencing a MeterMap
 * region by EntityID.
 *
 * meter_map_region_ref is a KNOWN GAP, flagged rather than silently
 * fabricated: MeterMap does not exist yet in this codebase (a later
 * pass's dependency), so this field is kept as a bare opaque
 * EntityID reference -- the same "opaque, resolved only once the real
 * target type exists" treatment core/voice_reference.h and
 * core/relationship_reference.h give references to types defined at a
 * higher layer, here applied ahead of schedule to a same-layer
 * dependency that simply is not built yet. Once MeterMap exists, this
 * field should be revisited -- it may need to become a dedicated
 * MeterMapRegionReference type rather than a bare EntityID, the same
 * way VoiceReference grew into its own type rather than staying a bare
 * EntityID. Nothing in this module should be extended to anticipate
 * MeterMap's eventual shape.
 *
 * Fields: id (EntityID), meter_map_region_ref (EntityID, opaque
 * reference to the MeterMap region this Measure spans).
 *
 * Measure has no mutator: a revision is a new Measure construction, not
 * an in-place field change (Law C2).
 *
 * Serialization: {id, meter_map_region_ref}.
 */

typedef struct Measure Measure;

typedef enum {
    MEASURE_OK = 0,
    MEASURE_ERR_NULL_ID,
    MEASURE_ERR_NULL_METER_MAP_REGION_REF,
    MEASURE_ERR_ALLOC
} MeasureResult;

/*
 * Takes ownership of id and meter_map_region_ref on success (both
 * destroyed by measure_destroy).
 */
MeasureResult measure_create(EntityID *id, EntityID *meter_map_region_ref, Measure **out);
void measure_destroy(Measure *measure);

const EntityID *measure_id(const Measure *measure);
const EntityID *measure_meter_map_region_ref(const Measure *measure);

/*
 * Identity equality: same EntityID, regardless of instance.
 * meter_map_region_ref does not factor in, same reasoning Staff's
 * identity equality excludes its referenced voice_ids/layer_ids.
 */
bool measure_identity_equals(const Measure *a, const Measure *b);

#endif