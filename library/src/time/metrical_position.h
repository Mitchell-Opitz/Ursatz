#ifndef MUSIC_TIME_METRICAL_POSITION_H
#define MUSIC_TIME_METRICAL_POSITION_H

#include <stdbool.h>

#include "time/duration.h"

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
 * value cannot know on its own: metrical_position_from_time (time/metrical_position.h, with a
 * MeterMap) produces only positions inside their measure, and metrical_position_to_time rejects
 * one that is not.
 *
 * Value type: immutable, value equality on (measure, offset).
 */

typedef struct MetricalPosition MetricalPosition;

typedef enum {
    METRICAL_POSITION_OK = 0,
    METRICAL_POSITION_ERR_NULL_ARGUMENT,
    METRICAL_POSITION_ERR_ALLOC
} MetricalPositionResult;

/* measure may be 0 (a pickup) or any other int. offset is read and copied, not owned. */
MetricalPositionResult metrical_position_create(int measure, const Duration *offset, MetricalPosition **out);
void metrical_position_destroy(MetricalPosition *position);

int metrical_position_measure(const MetricalPosition *position);

/* The offset from the start of the measure, in whole notes. Borrowed from the position. */
const Duration *metrical_position_offset(const MetricalPosition *position);

bool metrical_position_equals(const MetricalPosition *a, const MetricalPosition *b);

#endif
