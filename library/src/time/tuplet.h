#ifndef MUSIC_TIME_TUPLET_H
#define MUSIC_TIME_TUPLET_H

#include <stdbool.h>

/*
 * Tuplet: a ratio that fits `actual` notes into the time normally taken
 * by `normal` notes of the same written value -- 3:2 is a triplet (three
 * in the time of two), 2:3 a duplet (two in the time of three), 5:4 a
 * quintuplet. A Tuplet is only the ratio; it holds no note value and no
 * position. Which written value it applies to is the caller's, and how a
 * ratio scales a Duration is tuplet_scale_duration (time/tuplet.h).
 *
 * Both counts are bounded (2 to 64 actual, 1 to 64 normal) to keep the
 * exact rationals small, and the two differ -- a ratio of equal counts
 * changes nothing and is not a tuplet. Names (triplet, duplet) and the
 * notation of the number above the group are
 * frameworks/standard-rhythm/; this type makes no claim about them.
 *
 * Value type: immutable, value equality on (actual, normal). 6:4 and 3:2
 * are different Tuplets: the ratio is not reduced, because a sextuplet
 * is written 6, not 3.
 */

typedef struct Tuplet Tuplet;

#define TUPLET_MIN_ACTUAL 2
#define TUPLET_MAX_COUNT 64

typedef enum {
    TUPLET_OK = 0,
    TUPLET_ERR_ACTUAL_OUT_OF_RANGE,
    TUPLET_ERR_NORMAL_OUT_OF_RANGE,
    TUPLET_ERR_EQUAL_COUNTS,
    TUPLET_ERR_ALLOC
} TupletResult;

TupletResult tuplet_create(unsigned actual, unsigned normal, Tuplet **out);
void tuplet_destroy(Tuplet *tuplet);

/* How many notes are played. */
unsigned tuplet_actual(const Tuplet *tuplet);

/* How many notes of the same value they replace. */
unsigned tuplet_normal(const Tuplet *tuplet);

bool tuplet_equals(const Tuplet *a, const Tuplet *b);

#endif
