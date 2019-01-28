#include "semantics/interval.h"

#include <stdlib.h>

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

static unsigned long abs_long(long value) {
    return value < 0 ? (unsigned long)(-value) : (unsigned long)value;
}

IntervalResult interval_create(const PitchIdentity *pitch_a, const PitchIdentity *pitch_b,
                                bool has_directed_semitones, long directed_semitones,
                                bool has_undirected_semitones, unsigned long undirected_semitones,
                                bool has_chromatic_distance, unsigned long chromatic_distance, Interval **out) {
    if (pitch_a == NULL || pitch_b == NULL) {
        return INTERVAL_ERR_NULL_PITCH;
    }
    if (has_directed_semitones && has_undirected_semitones &&
        abs_long(directed_semitones) != undirected_semitones) {
        return INTERVAL_ERR_INCONSISTENT_DIRECTED_UNDIRECTED;
    }

    Interval *interval = (Interval *)malloc(sizeof(Interval));
    if (interval == NULL) {
        return INTERVAL_ERR_ALLOC;
    }

    interval->pitch_a = pitch_a;
    interval->pitch_b = pitch_b;
    interval->has_directed_semitones = has_directed_semitones;
    interval->directed_semitones = has_directed_semitones ? directed_semitones : 0;
    interval->has_undirected_semitones = has_undirected_semitones;
    interval->undirected_semitones = has_undirected_semitones ? undirected_semitones : 0;
    interval->has_chromatic_distance = has_chromatic_distance;
    interval->chromatic_distance = has_chromatic_distance ? chromatic_distance : 0;

    *out = interval;
    return INTERVAL_OK;
}

void interval_destroy(Interval *interval) {
    free(interval);
}

const PitchIdentity *interval_pitch_a(const Interval *interval) {
    return interval->pitch_a;
}

const PitchIdentity *interval_pitch_b(const Interval *interval) {
    return interval->pitch_b;
}

bool interval_has_directed_semitones(const Interval *interval) {
    return interval->has_directed_semitones;
}

long interval_directed_semitones(const Interval *interval) {
    return interval->directed_semitones;
}

bool interval_has_undirected_semitones(const Interval *interval) {
    return interval->has_undirected_semitones;
}

unsigned long interval_undirected_semitones(const Interval *interval) {
    return interval->undirected_semitones;
}

bool interval_has_chromatic_distance(const Interval *interval) {
    return interval->has_chromatic_distance;
}

unsigned long interval_chromatic_distance(const Interval *interval) {
    return interval->chromatic_distance;
}