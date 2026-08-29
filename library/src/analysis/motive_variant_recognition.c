#include "analysis/motive_variant_recognition.h"

#include <string.h>

/* matched out of total pairs; a dimension with no pairs on either side is matched whole. */
typedef struct {
    size_t matched;
    size_t total;
} Share;

static bool pitched(const MotiveSketch *sketch) {
    for (size_t i = 0; i < sketch->note_count; i++) {
        if (sketch->notes[i].pitch == MOTIVE_SKETCH_PITCH_UNKNOWN) {
            return false;
        }
    }
    return true;
}

static size_t longer(size_t a, size_t b) {
    return a > b ? a : b;
}

/* The time from a note's onset to the next onset; the last note's duration for the last. */
static ExactRational inter_onset(const MotiveSketch *sketch, size_t index) {
    if (index + 1 < sketch->note_count) {
        return msk_sub(sketch->notes[index + 1].onset, sketch->notes[index].onset);
    }
    return sketch->notes[index].duration;
}

static bool same_ratio(ExactRational a_next, ExactRational a, ExactRational b_next, ExactRational b) {
    ExactRational ra = msk_div(a_next, a);
    ExactRational rb = msk_div(b_next, b);
    return msk_ok(ra) && msk_ok(rb) && msk_eq(ra, rb);
}

static Share rhythm_share(const MotiveSketch *a, const MotiveSketch *b) {
    Share share = {0, longer(a->note_count, b->note_count) - 1};
    size_t common = a->note_count < b->note_count ? a->note_count : b->note_count;
    for (size_t i = 0; i + 1 < common; i++) {
        if (same_ratio(inter_onset(a, i + 1), inter_onset(a, i), inter_onset(b, i + 1), inter_onset(b, i)) &&
            same_ratio(a->notes[i + 1].duration, a->notes[i].duration, b->notes[i + 1].duration,
                       b->notes[i].duration)) {
            share.matched++;
        }
    }
    if (share.total == 0) {
        share.total = 1;
        share.matched = 1;
    }
    return share;
}

static long interval_at(const MotiveSketch *sketch, size_t index) {
    return motive_sketch_step(sketch, sketch->notes[index + 1].pitch, NULL) -
           motive_sketch_step(sketch, sketch->notes[index].pitch, NULL);
}

static int sign_of(long value) {
    return (value > 0) - (value < 0);
}

static Share interval_share(const MotiveSketch *a, const MotiveSketch *b, bool contour) {
    Share share = {0, longer(a->note_count, b->note_count) - 1};
    size_t common = a->note_count < b->note_count ? a->note_count : b->note_count;
    for (size_t i = 0; i + 1 < common; i++) {
        long x = interval_at(a, i);
        long y = interval_at(b, i);
        if (contour ? sign_of(x) == sign_of(y) : x == y) {
            share.matched++;
        }
    }
    if (share.total == 0) {
        share.total = 1;
        share.matched = 1;
    }
    return share;
}

static bool same_chord(const MotiveSketchChord *a, const MotiveSketchChord *b) {
    return a->degree == b->degree && a->alteration == b->alteration && a->inversion == b->inversion &&
           a->seventh == b->seventh;
}

static Share harmony_share(const MotiveSketch *a, const MotiveSketch *b) {
    Share share = {0, longer(a->chord_count, b->chord_count)};
    size_t common = a->chord_count < b->chord_count ? a->chord_count : b->chord_count;
    for (size_t i = 0; i < common; i++) {
        if (same_chord(&a->chords[i], &b->chords[i])) {
            share.matched++;
        }
    }
    return share;
}

static void set_dimension(MotiveDimensions *out, MotiveDimension dimension, Share share) {
    out->kept[dimension] = share.matched == share.total;
    out->changed[dimension] = !out->kept[dimension];
    out->similarity[dimension] = share.total == 0 ? 1.0 : (double)share.matched / (double)share.total;
}

void motive_variant_compare(const MotiveSketch *a, const MotiveSketch *b, MotiveDimensions *out) {
    memset(out, 0, sizeof(*out));
    set_dimension(out, MOTIVE_DIMENSION_RHYTHM, rhythm_share(a, b));
    bool a_pitched = pitched(a);
    bool b_pitched = pitched(b);
    Share none = {0, 1};
    Share whole = {1, 1};
    if (a_pitched && b_pitched) {
        set_dimension(out, MOTIVE_DIMENSION_INTERVALS, interval_share(a, b, false));
        set_dimension(out, MOTIVE_DIMENSION_CONTOUR, interval_share(a, b, true));
    } else {
        Share pitches = !a_pitched && !b_pitched ? whole : none;
        set_dimension(out, MOTIVE_DIMENSION_INTERVALS, pitches);
        set_dimension(out, MOTIVE_DIMENSION_CONTOUR, pitches);
    }
    Share harmony = harmony_share(a, b);
    if (harmony.total == 0) {
        harmony = whole;
    }
    set_dimension(out, MOTIVE_DIMENSION_HARMONY, harmony);
}

double motive_variant_nearness(const MotiveDimensions *dimensions) {
    double sum = 0.0;
    for (size_t i = 0; i < MOTIVE_DIMENSION_COUNT; i++) {
        sum += dimensions->similarity[i];
    }
    return sum / MOTIVE_DIMENSION_COUNT;
}
