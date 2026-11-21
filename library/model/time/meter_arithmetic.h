#ifndef MUSIC_TIME_METER_ARITHMETIC_H
#define MUSIC_TIME_METER_ARITHMETIC_H

#include <stdbool.h>

#include "model/core/rational.h"
#include "model/time/meter_map.h"

/*
 * Exact measure and beat arithmetic over a MeterMap. Times and lengths are Rationals of whole notes
 * (core/rational.h); nothing here uses floating point. Every function returns a MeterMapResult and
 * writes its outputs only on METER_MAP_OK. A time or length that does not fit the integer fields of a
 * MusicalTime or of a Rational is METER_MAP_ERR_OVERFLOW; a negative time is
 * METER_MAP_ERR_TIME_BEFORE_START.
 *
 * Barlines. A barline time belongs to the measure that starts there ([start, end) measures), as in
 * meter_map_measure_index_at.
 *
 * Beats. The beat length is the time signature's (time_signature_beat_length): its group when every
 * group has the same size, else its bottom unit. Beats are counted from 1 at the start of the measure,
 * so a short measure holds the beats that start inside it and a pickup counts its own beats from the
 * pickup's start. A notated position counts the signature's groups (3+2 over 8 has two beats).
 */

/* The measure that contains `time`. */
MeterMapResult meter_map_measure_of(const MeterMap *map, Rational time, int *measure);

/* The last measure a span ending at `end` touches: the measure of `end`, or the one before when `end`
   is a barline (and that measure is not the first). */
MeterMapResult meter_map_last_measure_touched(const MeterMap *map, Rational end, int *measure);

/* The [start, end) of measure `measure`; either output may be NULL. */
MeterMapResult meter_map_measure_bounds(const MeterMap *map, int measure, Rational *start, Rational *end);

/* The length of one beat in measure `measure`. */
MeterMapResult meter_map_beat_length(const MeterMap *map, int measure, Rational *beat);

/* A notated position (R-588): the measure, the beat group (counted from 1 within the measure) and the offset from
   that beat's start, in whole notes. The beat is the signature's group, so 3+2 over 8 has beat 2 at three eighths. */
typedef struct {
    int measure;
    int beat;
    Rational offset;
} NotatedPosition;

/* The notated position of `t`: the one conversion from absolute time to a score position. A pickup counts its own beats
   from its start, as meter_map_measure_of finds its measure. METER_MAP_ERR_TIME_BEFORE_START before 0. */
MeterMapResult meter_map_position_of(const MeterMap *map, MusicalTime t, NotatedPosition *out);

/* The local rhythmic grid at a point (R-588): the measure's signature (borrowed from the map), the measure's start, the
   length of its beat (time_signature_beat_length) and of its division: half the beat in a simple meter, a third of it
   in a compound one. */
typedef struct {
    const TimeSignature *signature;
    Rational measure_start;
    Rational beat;
    Rational division;
} MeterGrid;

/* The grid of the measure that holds `t`. METER_MAP_ERR_TIME_BEFORE_START before 0. */
MeterMapResult meter_map_grid_at(const MeterMap *map, MusicalTime t, MeterGrid *out);

/* Whether `length` is a whole number of one of the grid's levels no shorter than `finest`: the beat, the division, the
   division halved down to `finest`, and the tuplets of the beat and of the division (the 3:2 of each in a simple meter:
   two thirds of it) or of the beat (the 2:3 in a compound meter: a half of the beat). False for a length that is not
   positive. */
bool meter_grid_holds(const MeterGrid *grid, Rational length, Rational finest);

/* The first beat of measure `measure` at or after `time`, never later than the measure's end. A time
   at or before the start of the measure snaps to the start. */
MeterMapResult meter_map_snap_to_beat(const MeterMap *map, int measure, Rational time, Rational *snapped);

/* True when `time` falls on a beat of its measure (the measure's start included). False on any error. */
bool meter_map_on_beat(const MeterMap *map, Rational time);

/* The first measure that counts toward lengths: 1 when the map has a pickup (measure 0 is not
   counted), else the first measure. 1 for a NULL map. */
int meter_map_first_counted_measure(const MeterMap *map);

#endif
