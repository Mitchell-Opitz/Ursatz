#ifndef MUSIC_TIME_TEMPO_MAP_H
#define MUSIC_TIME_TEMPO_MAP_H

#include <stdbool.h>
#include <stddef.h>

#include "model/identity/entity_id.h"
#include "model/time/musical_time.h"

/*
 * TempoMap (music-time, Entity): the tempo of a piece over MusicalTime (symbolic time, in whole
 * notes). It is an ordered, gap-free list of regions, each {start MusicalTime,
 * microseconds per quarter note}. The first region starts at 0 and the last is open (it has no
 * end). Within a region the tempo is constant.
 *
 * Symbolic time never depends on tempo. A MusicalTime or Duration means the same thing with or
 * without a TempoMap; this entity only says how fast it is played. It is also not a tempo
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
 * Metric modulation. A change point may say "this much of the old beat is that much of the new" instead of a
 * tempo; the map resolves it to the exact tempo at creation and the region keeps that.
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
 */

#define TEMPO_MAP_DEFAULT_MICROSECONDS_PER_QUARTER 500000UL

typedef struct TempoMap TempoMap;
typedef struct TempoMapRegion TempoMapRegion;

typedef enum {
    TEMPO_MAP_CHANGE_ABSOLUTE = 0, /* the default: microseconds_per_quarter says the tempo */
    TEMPO_MAP_CHANGE_MODULATION    /* a metric modulation: old_value and new_value say it */
} TempoMapChangeKind;

/*
 * A change point is absolute (microseconds_per_quarter, 1 or more, as always) or a metric modulation: the new tempo
 * makes new_value last as long as old_value did at the tempo in force, so a quarter note that becomes a dotted
 * quarter ({1/4, 3/8}) leaves the tempo two thirds as long. Both are lengths in whole notes, above 0. tempo_map_create
 * resolves a modulation against the previous region's exact tempo (a first region at 0 has none: a modulation there
 * is TEMPO_MAP_ERR_BAD_MODULATION). The kind is the last member after the two the struct always had, so a positional
 * {start, microseconds} is an absolute change point; code that sets the members one by one starts from a zeroed struct.
 */
typedef struct {
    MusicalTime start;
    unsigned long microseconds_per_quarter; /* absolute: 1 or more; ignored for a modulation */
    TempoMapChangeKind kind;
    Rational old_value; /* modulation only */
    Rational new_value; /* modulation only */
} TempoMapChangePoint;

typedef enum {
    TEMPO_MAP_OK = 0,
    TEMPO_MAP_ERR_NULL_ARGUMENT,
    TEMPO_MAP_ERR_NO_CHANGE_POINTS,
    TEMPO_MAP_ERR_NEGATIVE_START,
    TEMPO_MAP_ERR_NOT_ASCENDING,
    TEMPO_MAP_ERR_ZERO_TEMPO,
    TEMPO_MAP_ERR_OVERFLOW,
    TEMPO_MAP_ERR_ALLOC,
    TEMPO_MAP_ERR_BAD_MODULATION /* a modulation with a length that is not above 0, or one with no tempo before it */
} TempoMapResult;

/*
 * Builds a TempoMap from change points in strictly ascending order of start, all 0 or later.
 * Takes ownership of id on success only. The times are copied. TEMPO_MAP_ERR_OVERFLOW if the
 * seconds at some change point do not fit.
 */
TempoMapResult tempo_map_create(EntityID *id, const TempoMapChangePoint *change_points, size_t count, TempoMap **out);

/*
 * The exact seconds at a time, and the time at exact seconds, the inverse (a change of tempo is continuous in
 * seconds, so the two are one-to-one). TEMPO_MAP_ERR_NEGATIVE_START for a negative time or seconds,
 * TEMPO_MAP_ERR_OVERFLOW where the exact value does not fit; *out is written only on TEMPO_MAP_OK.
 */
TempoMapResult tempo_map_seconds_at(const TempoMap *map, MusicalTime t, Rational *seconds);
TempoMapResult tempo_map_time_at(const TempoMap *map, Rational seconds, MusicalTime *out);

/* One defaulted region from 0 at the default tempo. Takes ownership of id on success only. */
TempoMapResult tempo_map_create_defaulted(EntityID *id, TempoMap **out);

void tempo_map_destroy(TempoMap *map);

/* True when every region is defaulted. */
bool tempo_map_is_defaulted(const TempoMap *map);

size_t tempo_map_region_count(const TempoMap *map);

/* The i-th region in time order, or NULL if i is out of range. Borrowed from the map. */
const TempoMapRegion *tempo_map_region(const TempoMap *map, size_t index);

MusicalTime tempo_map_region_start(const TempoMapRegion *region);
/* The region's tempo; a modulation's resolved tempo is rounded to a whole microsecond here (never below 1), while the
   seconds above stay exact. */
unsigned long tempo_map_region_microseconds_per_quarter(const TempoMapRegion *region);
bool tempo_map_region_is_defaulted(const TempoMapRegion *region);

#endif
