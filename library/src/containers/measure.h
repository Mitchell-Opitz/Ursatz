#ifndef MUSIC_CONTAINERS_MEASURE_H
#define MUSIC_CONTAINERS_MEASURE_H

#include <stdbool.h>

#include <stddef.h>

#include "identity/entity_id.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/time_span.h"

/*
 * Measure (Container): a metrical grouping, referencing a MeterMap
 * region by EntityID.
 *
 * meter_map_region_ref names a region of a MeterMap (time/meter_map.h) by that region's EntityID,
 * and resolves against it: measure_resolve_region, or meter_map_region_by_id directly. The ref
 * stays a bare EntityID, the same reference form VoiceReference's target uses, so a Measure can
 * be built before, or without, the MeterMap in hand, and measure_create is unchanged.
 *
 * A Measure holds only {id, meter_map_region_ref}. Its number and its start and end are not stored
 * on it: the MeterMap is the single source for measure boundaries, and storing a copy would let
 * the two disagree. measure_enumerate returns every measure of a MeterMap as {index, TimeSpan,
 * region ref}; a caller that wants Measure containers builds one per entry from the region ref.
 * A region holds many measures, so the ref alone does not say which one a Measure is: that is
 * what the caller's id for it, and the enumerated index, carry.
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
    MEASURE_ERR_ALLOC,
    MEASURE_ERR_NULL_ARGUMENT,
    MEASURE_ERR_REGION_NOT_FOUND,
    MEASURE_ERR_METER_MAP
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

/* Resolves the Measure's meter_map_region_ref against map. *out is borrowed from the map.
   MEASURE_ERR_REGION_NOT_FOUND if the map has no region with that id. */
MeasureResult measure_resolve_region(const Measure *measure, const MeterMap *map, const MeterMapRegion **out);

/*
 * One measure of a MeterMap: its number (0 for a pickup), its span [start, end), and the id of the
 * region it lies in (the value a Measure's meter_map_region_ref holds).
 */
typedef struct {
    int index;
    TimeSpan *span;         /* owned */
    EntityID *region_ref;   /* owned */
} MeasureEntry;

typedef struct {
    MeasureEntry *entries; /* owned; NULL when count is 0 */
    size_t count;
} MeasureEntries;

/*
 * Every measure of map that starts before end_time, in order, from the first measure number: see
 * meter_map_measure_count for what that includes. An end_time on a barline adds no empty measure.
 * On OK, *out owns its entries (measure_entries_destroy); otherwise *out is untouched.
 */
MeasureResult measure_enumerate(const MeterMap *map, const MusicalTime *end_time, MeasureEntries *out);

/* Destroys every entry's span and region ref, and the list. NULL is a no-op. */
void measure_entries_destroy(MeasureEntries *entries);

#endif
