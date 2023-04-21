#ifndef MUSIC_TIME_METRICAL_POSITION_H
#define MUSIC_TIME_METRICAL_POSITION_H

#include <stdbool.h>

#include "time/duration.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/time_signature.h"

/*
 * MetricalPosition (music-time, Value): where a time falls in a measure, as the measure's number and
 * an offset into it. The offset is an exact Duration in whole notes, counted from the start of the
 * measure, so a position of measure 3 at offset 3/8 is three eighth notes into measure 3. The measure
 * number follows the score (the first full measure is 1; a pickup is measure 0), as MeterMap numbers
 * them.
 *
 * This is pure arithmetic and makes no claim about beats. It holds no TimeSignature, so it does not
 * know what a beat, a subdivision or a strength is: those readings are frameworks/standard-meter,
 * which reads a MetricalPosition together with a TimeSignature as {beat, subdivision within the
 * beat} and assigns a BeatStrength. This is why the roadmap's name, "MetricalPosition value
 * (measure, beat, subdivision)", is split here: the kernel value is measure + offset, and beat and
 * subdivision are derived from it in the framework.
 *
 * The offset is never negative. Its upper bound is the length of the measure it is in, which this
 * value cannot know on its own: metrical_position_from_time (below, with a MeterMap) produces only
 * positions inside their measure, and metrical_position_to_time rejects one that is not.
 *
 * Value type: immutable, value equality on (measure, offset).
 */

typedef struct MetricalPosition MetricalPosition;

typedef enum {
    METRICAL_POSITION_OK = 0,
    METRICAL_POSITION_ERR_NULL_ARGUMENT,
    METRICAL_POSITION_ERR_TIME_BEFORE_START,
    METRICAL_POSITION_ERR_MEASURE_OUT_OF_RANGE,
    METRICAL_POSITION_ERR_OFFSET_OUT_OF_RANGE,
    METRICAL_POSITION_ERR_OVERFLOW,
    METRICAL_POSITION_ERR_ALLOC
} MetricalPositionResult;

/* measure may be 0 (a pickup) or any other int. offset is read and copied, not owned. */
MetricalPositionResult metrical_position_create(int measure, const Duration *offset, MetricalPosition **out);
void metrical_position_destroy(MetricalPosition *position);

int metrical_position_measure(const MetricalPosition *position);

/* The offset from the start of the measure, in whole notes. Borrowed from the position. */
const Duration *metrical_position_offset(const MetricalPosition *position);

bool metrical_position_equals(const MetricalPosition *a, const MetricalPosition *b);

/*
 * Deriving a position from a MusicalTime and a MeterMap (time/meter_map.h). All arithmetic is exact
 * and checked: a value that does not fit (including a measure number past int) is
 * METRICAL_POSITION_ERR_OVERFLOW, never a wrapped number.
 *
 *   Barlines. A time exactly on a barline is the NEXT measure at offset 0, as MeterMap gives the
 *   barline to the following measure: the position of one whole note in 4/4 is measure 2, offset 0.
 *
 *   Short measures. A measure cut short by a mid-measure meter change (MeterMap: a short measure)
 *   keeps its own start: positions inside it are offsets from where that measure started, and its
 *   offsets run only up to its real, short length.
 *
 *   Pickups. A pickup is measure 0 and its offset is measured from where a FULL measure would have
 *   started, so a pickup sits at the end of an imagined measure, as in a score: a one-quarter pickup
 *   in 4/4 starts at offset 3/4 (beat 4). Which full measure is the one the file implies. When a
 *   partial first measure is ended by a later time signature, it is the measure of that region's own
 *   signature (a quarter in 4/4, then 3/4: offset 3/4 of a 4/4 measure). When a short leading
 *   signature stands before the real one (a 1/8 region before 12/8), it is the measure of the
 *   FOLLOWING region's signature: the pickup eighth is at offset 11/8 of a 12/8 measure.
 *   metrical_position_reading_signature gives that signature.
 *
 *   Open end. A MeterMap's last region is open, so every time from 0 has a position; there is no
 *   "past the end" error. A time before 0 is METRICAL_POSITION_ERR_TIME_BEFORE_START.
 */

/* The position of time. *out is new; the caller destroys it. */
MetricalPositionResult metrical_position_from_time(const MeterMap *map, const MusicalTime *time,
                                                    MetricalPosition **out);

/*
 * The inverse: the MusicalTime at position. METRICAL_POSITION_ERR_MEASURE_OUT_OF_RANGE for a measure
 * below the map's first, and METRICAL_POSITION_ERR_OFFSET_OUT_OF_RANGE for an offset that is not
 * inside the measure (at or past its length, or, in a pickup, before where the pickup starts). *out
 * is new; the caller destroys it. to_time(from_time(t)) is t, and from_time(to_time(p)) is p.
 */
MetricalPositionResult metrical_position_to_time(const MeterMap *map, const MetricalPosition *position,
                                                  MusicalTime **out);

/*
 * The TimeSignature that positions in `measure` are read in (for example, by standard-meter's
 * meter_beat_position): the signature of the region the measure is in, except for a pickup that
 * follows a short leading signature, where it is the next region's signature (see above). *out is
 * borrowed from the map.
 */
MetricalPositionResult metrical_position_reading_signature(const MeterMap *map, int measure,
                                                            const TimeSignature **out);

#endif
