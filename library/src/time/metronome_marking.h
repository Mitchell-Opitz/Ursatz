#ifndef MUSIC_TIME_METRONOME_MARKING_H
#define MUSIC_TIME_METRONOME_MARKING_H

#include <stdbool.h>

#include "time/duration.h"

/*
 * MetronomeMarking: a specific tempo given as a beat unit and a rate in
 * beats per minute -- "quarter note = 60" is a beat unit of 1/4 and 60
 * bpm, so one quarter note lasts one second.
 *
 * Both parts are exact rationals in lowest terms, never float/double
 * (Law 19): the beat unit is a Duration's numerator/denominator, and bpm
 * is a numerator/denominator pair so a marking such as 72.5 bpm (145/2)
 * is representable exactly. The beat unit must be non-zero and bpm
 * positive.
 *
 * What a marking means in seconds is derived, not stored: one beat unit
 * lasts 60/bpm seconds. Seconds are returned as an exact rational too.
 * Textual tempo words (allegro, andante) are NOT metronome markings --
 * they are less specific and live in events/tempo_marking.h and the
 * standard-markings framework.
 *
 * Value type: immutable, value equality on (beat unit, bpm).
 */

typedef struct MetronomeMarking MetronomeMarking;

typedef enum {
    METRONOME_MARKING_OK = 0,
    METRONOME_MARKING_ERR_NULL_BEAT_UNIT,
    METRONOME_MARKING_ERR_ZERO_BEAT_UNIT,
    METRONOME_MARKING_ERR_NON_POSITIVE_BPM,
    METRONOME_MARKING_ERR_ALLOC
} MetronomeMarkingResult;

/* beat_unit is read, not owned. bpm = bpm_numerator / bpm_denominator,
   both must be non-zero. */
MetronomeMarkingResult metronome_marking_create(const Duration *beat_unit, unsigned long bpm_numerator,
                                                unsigned long bpm_denominator, MetronomeMarking **out);
void metronome_marking_destroy(MetronomeMarking *marking);

unsigned long metronome_marking_beat_unit_numerator(const MetronomeMarking *marking);
unsigned long metronome_marking_beat_unit_denominator(const MetronomeMarking *marking);
unsigned long metronome_marking_bpm_numerator(const MetronomeMarking *marking);
unsigned long metronome_marking_bpm_denominator(const MetronomeMarking *marking);

/* Seconds one beat unit lasts, as an exact rational in lowest terms: 60/bpm. */
void metronome_marking_seconds_per_beat_unit(const MetronomeMarking *marking, unsigned long *out_numerator,
                                             unsigned long *out_denominator);

/* Seconds a note of the given Duration lasts at this marking, as an exact
   rational in lowest terms. A zero Duration lasts 0/1. */
void metronome_marking_seconds_for_duration(const MetronomeMarking *marking, const Duration *duration,
                                            unsigned long *out_numerator, unsigned long *out_denominator);

/* Value equality on the reduced (beat unit, bpm) pair. */
bool metronome_marking_equals(const MetronomeMarking *a, const MetronomeMarking *b);

#endif
