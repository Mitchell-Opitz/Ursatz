#ifndef MUSIC_TIME_METER_MAP_H
#define MUSIC_TIME_METER_MAP_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"
#include "time/musical_time.h"
#include "time/time_signature.h"
#include "time/time_span.h"

/*
 * MeterMap (music-time, Entity): the TimeSignatures of a piece laid over its time. It is an
 * ordered, non-overlapping, gap-free list of regions, each {TimeSignature, start, end or open,
 * first measure number}. It is the single source for measure boundaries: where a measure starts
 * and ends, and what its number is, is computed here and nowhere else.
 *
 * The map holds meter only. It makes no claim about beats, subdivisions or beat strength, and it
 * does not read the TimeSignature as simple or compound: that is frameworks/standard-meter. It
 * depends on identity/ and time/ only.
 *
 * Regions. The first region starts at 0 and the last is open (it has no end). A region's
 * measures are laid from its start in steps of its TimeSignature's measure duration. If the next
 * region begins before the last measure is complete, that last measure is a SHORT measure: it is
 * counted as one measure and ends where the next region begins, and the next region starts a
 * fresh grid there.
 *
 * Measure numbering follows the score: the first full measure is 1. Each region carries the number
 * of its first measure; numbers run on across regions. A short measure keeps its number (a 4/4
 * measure cut short by a change is still measure N, and the new region begins at N+1).
 *
 * Pickup. A pickup is detected only from what the file says, never from note content. The first
 * region is a pickup, numbered measure 0, if a later region follows it and either:
 *   - its length is less than one measure of its own signature (a partial first measure ended by a
 *     later time signature event), or
 *   - its length is exactly one measure of its own signature, its signature differs from the next
 *     region's, and its measure is less than half of the next region's measure (a short leading
 *     signature such as 1/8 or 3/8 before 12/8).
 * The second rule is a convention: its half threshold is what keeps a genuine 3/4 first bar before
 * a 4/4 from reading as a pickup. A pickup region spans exactly one measure, number 0.
 *
 * Barlines. A barline time belongs to the FOLLOWING measure: measures are left-closed, so a
 * measure's span is [start, end) and measure_index_at(barline) is the measure that starts there.
 * Where a region changes, the change point is both the end of the earlier measure and the start of
 * the next.
 *
 * Defaulted. A region is defaulted when the file declared no signature for it. A map built with
 * meter_map_create_defaulted is one 4/4 region marked defaulted. If the first declared signature
 * is after tick 0, the span before it is a defaulted 4/4 region. meter_map_is_defaulted is true
 * when every region is defaulted, so a consumer can tell a declared meter from a default.
 *
 * A change point whose signature equals the previous region's and that falls on one of its
 * barlines adds nothing and is dropped. One that falls mid-measure is kept: it starts a new region
 * and so ends a short measure.
 *
 * Each region has its own EntityID (the map's id, "/region-" and the region index), which is what
 * a Measure's meter_map_region_ref names; meter_map_region_by_id resolves it.
 */

typedef struct MeterMap MeterMap;
typedef struct MeterMapRegion MeterMapRegion;

typedef struct {
    const TimeSignature *time_signature; /* read, not owned */
    const MusicalTime *start;            /* read, not owned */
} MeterMapChangePoint;

typedef enum {
    METER_MAP_OK = 0,
    METER_MAP_ERR_NULL_ARGUMENT,
    METER_MAP_ERR_NO_CHANGE_POINTS,
    METER_MAP_ERR_NEGATIVE_START,
    METER_MAP_ERR_NOT_ASCENDING,
    METER_MAP_ERR_TIME_BEFORE_START,
    METER_MAP_ERR_INDEX_OUT_OF_RANGE,
    METER_MAP_ERR_NOT_FOUND,
    METER_MAP_ERR_ALLOC
} MeterMapResult;

/*
 * Builds a MeterMap from change points, in ascending order of start (strictly ascending: two
 * change points at one time are an error, since the caller has no way to say which wins). Start
 * times must be 0 or more. Takes ownership of id on success only. The signatures and times are
 * copied.
 */
MeterMapResult meter_map_create(EntityID *id, const MeterMapChangePoint *change_points, size_t count, MeterMap **out);

/* A single 4/4 region from 0, open, marked defaulted. Takes ownership of id on success only. */
MeterMapResult meter_map_create_defaulted(EntityID *id, MeterMap **out);

void meter_map_destroy(MeterMap *map);

const EntityID *meter_map_id(const MeterMap *map);

/* True when every region is defaulted. */
bool meter_map_is_defaulted(const MeterMap *map);

/* True when the first region is a pickup (measure 0). */
bool meter_map_has_pickup(const MeterMap *map);

/* The number of the first measure: 0 with a pickup, otherwise 1. */
int meter_map_first_measure_number(const MeterMap *map);

size_t meter_map_region_count(const MeterMap *map);

/* The i-th region in time order, or NULL if i is out of range. Borrowed from the map. */
const MeterMapRegion *meter_map_region(const MeterMap *map, size_t index);

/* The region containing time: start <= time < end, so a change point belongs to the new region.
   METER_MAP_ERR_TIME_BEFORE_START for a time before 0. *out is borrowed from the map. */
MeterMapResult meter_map_region_at(const MeterMap *map, const MusicalTime *time, const MeterMapRegion **out);

/* METER_MAP_ERR_NOT_FOUND if no region has this id. */
MeterMapResult meter_map_region_by_id(const MeterMap *map, const EntityID *id, const MeterMapRegion **out);

/* The number of the measure containing time. A barline time is the measure that starts there. */
MeterMapResult meter_map_measure_index_at(const MeterMap *map, const MusicalTime *time, int *out_index);

/* The span [start, end) of measure `index`. A short measure's span is its real, short extent.
   The last region is open, so its measures continue without end.
   METER_MAP_ERR_INDEX_OUT_OF_RANGE if the index is below the first measure number. *out is a new
   TimeSpan, left-closed, that the caller destroys. */
MeterMapResult meter_map_measure_span(const MeterMap *map, int index, TimeSpan **out);

/* The region that holds measure `index`. */
MeterMapResult meter_map_region_of_measure(const MeterMap *map, int index, const MeterMapRegion **out);

/* The number of measures that start before end_time, a pickup included: an end_time on a barline
   adds no empty measure after it, and one inside a measure counts that measure. 0 for end_time 0.
   METER_MAP_ERR_TIME_BEFORE_START for a negative end_time. */
MeterMapResult meter_map_measure_count(const MeterMap *map, const MusicalTime *end_time, size_t *out_count);

const EntityID *meter_map_region_id(const MeterMapRegion *region);
const TimeSignature *meter_map_region_time_signature(const MeterMapRegion *region);
const MusicalTime *meter_map_region_start(const MeterMapRegion *region);
/* NULL for the open (last) region. */
const MusicalTime *meter_map_region_end(const MeterMapRegion *region);
int meter_map_region_first_measure_number(const MeterMapRegion *region);
bool meter_map_region_is_defaulted(const MeterMapRegion *region);
bool meter_map_region_is_pickup(const MeterMapRegion *region);

#endif
