#ifndef MUSIC_TIME_TEMPO_MAP_H
#define MUSIC_TIME_TEMPO_MAP_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"
#include "time/musical_time.h"
#include "time/performance_time.h"
#include "time/time_span.h"

/*
 * TempoMap (music-time, Entity): the mapping from MusicalTime (symbolic time, in whole notes) to
 * performance time (seconds). It is an ordered, gap-free list of regions, each {start MusicalTime,
 * microseconds per quarter note}. The first region starts at 0 and the last is open (it has no
 * end). Within a region the tempo is constant.
 *
 * Symbolic time never depends on tempo. A MusicalTime or Duration means the same thing with or
 * without a TempoMap; this entity only answers "how many seconds is that?". It is also not a tempo
 * marking: MetronomeMarking and TempoMarking are notation-level values and are neither replaced
 * nor produced here. It depends on identity/ and time/ only.
 *
 * Exact seconds. For a whole-note span d at T microseconds per quarter note:
 *
 *     seconds = d * 4 * T / 1,000,000
 *
 * (a quarter note is a quarter of a whole note, so a whole note lasts 4 * T microseconds). Seconds
 * at a time are the sum of that over every region before it plus the part of its own region
 * reached. All of it is exact rational arithmetic on unsigned 64-bit integers, in lowest terms and
 * never floating point. Every product and sum is checked: a value that does not fit is
 * TEMPO_MAP_ERR_OVERFLOW, never a wrapped number. A map whose total seconds at some change point
 * would not fit is rejected when it is built.
 *
 * Inverse. time_at_seconds is the exact inverse of seconds_at: within the region that holds the
 * seconds, time = start + (seconds - seconds at start) * 1,000,000 / (4 * T). So
 * time_at_seconds(seconds_at(t)) is t exactly, for every t.
 *
 * Change points. A change point is a region. A tempo that repeats the previous one is still its own
 * region: nothing is merged, so the region list says what the file said. Change points must be
 * strictly ascending, since two at one time leave no way to say which wins; a tempo of 0 is an
 * error (no quarter note can last no time). If the first change point is after 0, the span before
 * it is a defaulted region at TEMPO_MAP_DEFAULT_MICROSECONDS_PER_QUARTER.
 *
 * Defaulted. A region is defaulted when the file declared no tempo for it.
 * tempo_map_create_defaulted is one defaulted region from 0 at the default tempo (MIDI's default,
 * 500,000 microseconds per quarter note, 120 quarter notes per minute). tempo_map_is_defaulted is
 * true when every region is defaulted, so a consumer can tell a declared tempo from a default.
 *
 * Each region has its own EntityID (the map's id, "/region-" and the region index), resolved by
 * tempo_map_region_by_id, as MeterMap's regions are.
 */

#define TEMPO_MAP_DEFAULT_MICROSECONDS_PER_QUARTER 500000UL

typedef struct TempoMap TempoMap;
typedef struct TempoMapRegion TempoMapRegion;

typedef struct {
    const MusicalTime *start;           /* read, not owned */
    unsigned long microseconds_per_quarter; /* 1 or more */
} TempoMapChangePoint;

typedef enum {
    TEMPO_MAP_OK = 0,
    TEMPO_MAP_ERR_NULL_ARGUMENT,
    TEMPO_MAP_ERR_NO_CHANGE_POINTS,
    TEMPO_MAP_ERR_NEGATIVE_START,
    TEMPO_MAP_ERR_NOT_ASCENDING,
    TEMPO_MAP_ERR_ZERO_TEMPO,
    TEMPO_MAP_ERR_TIME_BEFORE_START,
    TEMPO_MAP_ERR_NOT_FOUND,
    TEMPO_MAP_ERR_OVERFLOW,
    TEMPO_MAP_ERR_ALLOC
} TempoMapResult;

/*
 * Builds a TempoMap from change points in strictly ascending order of start, all 0 or later.
 * Takes ownership of id on success only. The times are copied. TEMPO_MAP_ERR_OVERFLOW if the
 * seconds at some change point do not fit.
 */
TempoMapResult tempo_map_create(EntityID *id, const TempoMapChangePoint *change_points, size_t count, TempoMap **out);

/* One defaulted region from 0 at the default tempo. Takes ownership of id on success only. */
TempoMapResult tempo_map_create_defaulted(EntityID *id, TempoMap **out);

void tempo_map_destroy(TempoMap *map);

const EntityID *tempo_map_id(const TempoMap *map);

/* True when every region is defaulted. */
bool tempo_map_is_defaulted(const TempoMap *map);

size_t tempo_map_region_count(const TempoMap *map);

/* The i-th region in time order, or NULL if i is out of range. Borrowed from the map. */
const TempoMapRegion *tempo_map_region(const TempoMap *map, size_t index);

/* The region containing time: start <= time < end, so a change point belongs to the new region.
   TEMPO_MAP_ERR_TIME_BEFORE_START for a time before 0. *out is borrowed from the map. */
TempoMapResult tempo_map_region_at(const TempoMap *map, const MusicalTime *time, const TempoMapRegion **out);

/* TEMPO_MAP_ERR_NOT_FOUND if no region has this id. */
TempoMapResult tempo_map_region_by_id(const TempoMap *map, const EntityID *id, const TempoMapRegion **out);

/* The tempo in force at time, in microseconds per quarter note. */
TempoMapResult tempo_map_tempo_at(const TempoMap *map, const MusicalTime *time, unsigned long *out_microseconds_per_quarter);

/* Seconds from 0 to time, exact. *out is new; the caller destroys it. */
TempoMapResult tempo_map_seconds_at(const TempoMap *map, const MusicalTime *time, PerformanceTime **out);

/* The exact inverse of tempo_map_seconds_at: the MusicalTime that is `seconds` from 0. *out is new;
   the caller destroys it. */
TempoMapResult tempo_map_time_at_seconds(const TempoMap *map, const PerformanceTime *seconds, MusicalTime **out);

/* Seconds the span lasts: seconds_at(end) less seconds_at(start), across any number of tempo
   changes. The span's boundary type is not read: a span's length does not depend on which of its
   ends are included. */
TempoMapResult tempo_map_duration_seconds(const TempoMap *map, const TimeSpan *span, PerformanceTime **out);

const EntityID *tempo_map_region_id(const TempoMapRegion *region);
const MusicalTime *tempo_map_region_start(const TempoMapRegion *region);
/* NULL for the open (last) region. */
const MusicalTime *tempo_map_region_end(const TempoMapRegion *region);
unsigned long tempo_map_region_microseconds_per_quarter(const TempoMapRegion *region);
bool tempo_map_region_is_defaulted(const TempoMapRegion *region);
/* Seconds from 0 to the start of the region, exact. *out is new; the caller destroys it. */
TempoMapResult tempo_map_region_seconds_at_start(const TempoMapRegion *region, PerformanceTime **out);

#endif
