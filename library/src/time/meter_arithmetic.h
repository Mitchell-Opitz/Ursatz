#ifndef MUSIC_TIME_METER_ARITHMETIC_H
#define MUSIC_TIME_METER_ARITHMETIC_H

#include <stdbool.h>

#include "core/rational.h"
#include "time/meter_map.h"

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
 * Beats. A beat is the unit of the measure's time signature, or three units in a compound meter: a
 * top number of 6, 9 or 12 over an eighth or smaller unit. Beats are counted from 1 at the start of
 * the measure, so a short measure holds the beats that start inside it and a pickup counts its own
 * beats from the pickup's start.
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

/* The number of beats that start in measure `measure`: its real length over the beat length, rounded up. */
MeterMapResult meter_map_beat_count(const MeterMap *map, int measure, int *count);

/* The measure of `time`, its beat (1-based) and the offset from the start of that beat, in whole notes.
   Any output may be NULL. */
MeterMapResult meter_map_beat_of(const MeterMap *map, Rational time, int *measure, int *beat, Rational *offset);

/* The first beat of measure `measure` at or after `time`, never later than the measure's end. A time
   at or before the start of the measure snaps to the start. */
MeterMapResult meter_map_snap_to_beat(const MeterMap *map, int measure, Rational time, Rational *snapped);

/* True when `time` falls on a beat of its measure (the measure's start included). False on any error. */
bool meter_map_on_beat(const MeterMap *map, Rational time);

/* The first measure that counts toward lengths: 1 when the map has a pickup (measure 0 is not
   counted), else the first measure. 1 for a NULL map. */
int meter_map_first_counted_measure(const MeterMap *map);

#endif
