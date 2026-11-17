#ifndef MUSIC_SEMANTICS_INTERVAL_INTERNAL_H
#define MUSIC_SEMANTICS_INTERVAL_INTERNAL_H

#include "analysis/notes/interval.h"

/* The fields of an Interval: interval.c writes them; tests read them through tests/support/semantics_test_access.h. */
struct Interval {
    const PitchIdentity *pitch_a;
    const PitchIdentity *pitch_b;

    bool has_directed_semitones;
    long directed_semitones;

    bool has_undirected_semitones;
    unsigned long undirected_semitones;

    bool has_chromatic_distance;
    unsigned long chromatic_distance;
};

#endif
