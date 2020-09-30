#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_BEAT_GRID_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_BEAT_GRID_H

#include <stdbool.h>

#include "time/duration.h"
#include "time/time_signature.h"

/*
 * The beat grid of a measure: where beats fall inside it. Beats come from
 * the signature's meter class (meter_class_rules.h) -- the bottom unit in
 * simple meter, a dotted unit in compound -- so the same lookups serve
 * both: 4/4 beats fall every 1/4 of a whole note, 6/8 beats every 3/8.
 * Positions are offsets from the start of the measure (zero is the
 * downbeat). Beaming, beat-boundary ties, and counting are built on this.
 *
 * Every function is false for an unclassified signature (no beat grid).
 */

/*
 * The beat `offset` falls in: *out_index is 0-based, *out_on_beat is true
 * when the offset is exactly the start of that beat. False, leaving the
 * outputs untouched, when the offset is at or past the end of the
 * measure.
 */
bool standard_meter_beat_at(const TimeSignature *time_signature, const Duration *offset, unsigned *out_index,
                            bool *out_on_beat);

/*
 * The start of beat `index` as an offset from the start of the measure;
 * index equal to the beats per measure is the end of the measure. False,
 * leaving *out untouched, past that or on allocation failure. On success
 * the caller owns *out.
 */
bool standard_meter_beat_start(const TimeSignature *time_signature, unsigned index, Duration **out);

#endif
