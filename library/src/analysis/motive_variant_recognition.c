#include "analysis/motive_variant_recognition.h"

#include <stdlib.h>
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

/* --- proposals ------------------------------------------------------------------------------ */

#define PROPOSAL_MAX 16

typedef struct {
    MotiveRelationStep list[PROPOSAL_MAX];
    size_t count;
} Proposals;

static MotiveRelationStep make_step(MotiveRelationKind kind, long a, long b, long c) {
    MotiveRelationStep step;
    memset(&step, 0, sizeof(step));
    step.kind = kind;
    step.value[0] = a;
    step.value[1] = b;
    step.value[2] = c;
    return step;
}

static void put_step(Proposals *p, MotiveRelationStep step) {
    if (p->count < PROPOSAL_MAX) {
        p->list[p->count++] = step;
    }
}

static void put(Proposals *p, MotiveRelationKind kind, long a, long b, long c) {
    put_step(p, make_step(kind, a, b, c));
}

/* A step whose ratio parameter is at index which. */
static void put_ratio(Proposals *p, MotiveRelationKind kind, long a, long b, long c, size_t which,
                      ExactRational ratio) {
    MotiveRelationStep step = make_step(kind, a, b, c);
    step.value[which] = (long)ratio.n;
    step.den[which] = (long)ratio.d;
    put_step(p, step);
}

static long pitch_of(const MotiveSketch *sketch, size_t index) {
    return sketch->notes[index].pitch;
}

static long step_of(const MotiveSketch *sketch, size_t index) {
    return motive_sketch_step(sketch, sketch->notes[index].pitch, NULL);
}

static size_t chords_or_one(const MotiveSketch *sketch) {
    return sketch->chord_count > 0 ? sketch->chord_count : 1;
}

static void propose_transpose(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    size_t sl = s->note_count - 1;
    size_t tl = t->note_count - 1;
    long first = pitch_of(t, 0) - pitch_of(s, 0);
    long last = pitch_of(t, tl) - pitch_of(s, sl);
    if (first != 0) {
        put(p, MOTIVE_RELATION_TRANSPOSE, 0, first, 0);
    }
    if (last != 0 && last != first) {
        put(p, MOTIVE_RELATION_TRANSPOSE, 0, last, 0);
    }
    if (s->has_scale) {
        long by_first = step_of(t, 0) - step_of(s, 0);
        long by_last = step_of(t, tl) - step_of(s, sl);
        if (by_first != 0) {
            put(p, MOTIVE_RELATION_TRANSPOSE, 1, by_first, 0);
        }
        if (by_last != 0 && by_last != by_first) {
            put(p, MOTIVE_RELATION_TRANSPOSE, 1, by_last, 0);
        }
        put(p, MOTIVE_RELATION_TRANSPOSE, 1, 0, motive_sketch_degree_of_step(s, step_of(t, 0)));
    }
}

static void propose_invert(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    put(p, MOTIVE_RELATION_INVERT, -1, 0, 0);
    long sum = pitch_of(s, 0) + pitch_of(t, 0);
    if (sum % 2 == 0 && sum / 2 != pitch_of(s, 0)) {
        put(p, MOTIVE_RELATION_INVERT, sum / 2, 0, 0);
    }
    long sum_last = pitch_of(s, s->note_count - 1) + pitch_of(t, t->note_count - 1);
    if (sum_last % 2 == 0 && sum_last / 2 != sum / 2) {
        put(p, MOTIVE_RELATION_INVERT, sum_last / 2, 0, 0);
    }
    if (s->has_scale) {
        put(p, MOTIVE_RELATION_INVERT, -1, 1, 0);
        long steps = step_of(s, 0) + step_of(t, 0);
        if (steps % 2 == 0) {
            long axis = motive_sketch_pitch_at_step(s, steps / 2, 0);
            if (axis >= 0 && axis <= 127 && axis != pitch_of(s, 0)) {
                put(p, MOTIVE_RELATION_INVERT, axis, 1, 0);
            }
        }
    }
}

static void propose_interval_change(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (s->note_count < 2 || s->note_count != t->note_count) {
        return;
    }
    long amounts[MOTIVE_SKETCH_MAX_NOTES];
    size_t differing = 0;
    size_t first = 0;
    bool common = true;
    long shared = 0;
    for (size_t i = 0; i + 1 < s->note_count; i++) {
        long from = pitch_of(s, i + 1) - pitch_of(s, i);
        long to = pitch_of(t, i + 1) - pitch_of(t, i);
        amounts[i] = from > 0 ? to - from : (from < 0 ? from - to : 0);
        if (from == 0 && to != 0) {
            return;
        }
        if (amounts[i] == 0) {
            continue;
        }
        if (differing == 0) {
            first = i;
            shared = amounts[i];
        } else if (amounts[i] != shared) {
            common = false;
        }
        differing++;
    }
    if (differing == 0) {
        return;
    }
    for (long keep = 0; keep <= 1; keep++) {
        put(p, MOTIVE_RELATION_INTERVAL_CHANGE, (long)first + 1, amounts[first], keep);
        if (common && differing > 1) {
            put(p, MOTIVE_RELATION_INTERVAL_CHANGE, 0, shared, keep);
        }
    }
}

static void propose_fragment(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (t->note_count >= s->note_count) {
        return;
    }
    long min = t->note_count < 2 ? 1 : 0;
    for (long position = 1; position <= 3; position++) {
        put(p, MOTIVE_RELATION_FRAGMENT, position, (long)t->note_count, min);
    }
}

static void propose_liquidate(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (t->note_count >= s->note_count) {
        return;
    }
    long drop = (long)(s->note_count - t->note_count);
    long min = t->note_count < 2 ? 1 : 0;
    put(p, MOTIVE_RELATION_LIQUIDATE, drop, 0, min);
    put(p, MOTIVE_RELATION_LIQUIDATE, drop, 1, min);
}

static void propose_scale_time(const MotiveSketch *s, const MotiveSketch *t, Proposals *p, MotiveRelationKind kind) {
    ExactRational factor = msk_div(t->length, s->length);
    bool wanted = msk_ok(factor) && factor.n != 0 &&
                  (kind == MOTIVE_RELATION_AUGMENT ? msk_cmp(factor, msk_int(1)) > 0
                                                   : msk_cmp(factor, msk_int(1)) < 0);
    if (wanted) {
        put_ratio(p, kind, 0, 0, 0, 0, factor);
    }
    put(p, kind, 0, 0, 0);
}

static void propose_augment(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    propose_scale_time(s, t, p, MOTIVE_RELATION_AUGMENT);
}

static void propose_diminish(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    propose_scale_time(s, t, p, MOTIVE_RELATION_DIMINISH);
}

static void propose_note_repeat(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (t->note_count <= s->note_count) {
        return;
    }
    long added = (long)(t->note_count - s->note_count);
    for (size_t note = 0; note < s->note_count; note++) {
        if (added + 1 >= 2 && added + 1 <= 4) {
            put(p, MOTIVE_RELATION_NOTE_REPEAT, (long)note, added + 1, 0);
        } else {
            for (long count = 2; count <= 4; count++) {
                put(p, MOTIVE_RELATION_NOTE_REPEAT, (long)note, count, 0);
            }
        }
    }
}

static void propose_rhythm_repeat(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (s->note_count != t->note_count || s->note_count < 2) {
        return;
    }
    size_t target = s->note_count;
    for (size_t i = 0; i < s->note_count; i++) {
        if (!msk_eq(s->notes[i].duration, t->notes[i].duration)) {
            target = i;
            break;
        }
    }
    if (target == s->note_count) {
        return;
    }
    for (size_t length = 1; length <= 3; length++) {
        if (target + length > s->note_count) {
            break;
        }
        for (size_t first = 0; first + length <= s->note_count; first++) {
            if (first != target) {
                put(p, MOTIVE_RELATION_RHYTHM_REPEAT, (long)first, (long)length, (long)target);
            }
        }
    }
}

static void propose_rhythm_shift(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    ExactRational inside = msk_mod(msk_sub(msk_add(s->pickup, s->bar), t->pickup), s->bar);
    if (msk_ok(inside) && !msk_is_zero(inside)) {
        put_ratio(p, MOTIVE_RELATION_RHYTHM_SHIFT, 0, 0, 0, 0, inside);
    }
}

static void propose_add_upbeat(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    long added = t->note_count > s->note_count ? (long)(t->note_count - s->note_count) : 0;
    for (long approach = 1; approach >= -1; approach -= 2) {
        if (added >= 1 && added <= 3) {
            put(p, MOTIVE_RELATION_ADD_UPBEAT, added, approach, 0);
        } else {
            for (long count = 1; count <= 3; count++) {
                put(p, MOTIVE_RELATION_ADD_UPBEAT, count, approach, 0);
            }
        }
    }
}

static bool meter_den(unsigned long long den) {
    return den == 1 || den == 2 || den == 4 || den == 8 || den == 16;
}

static void propose_change_meter(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    (void)s;
    if (t->beat.n == 1 && meter_den(t->beat.d)) {
        ExactRational beats = msk_div(t->bar, t->beat);
        if (msk_ok(beats) && beats.d == 1 && beats.n >= 1 && beats.n <= 64) {
            put(p, MOTIVE_RELATION_CHANGE_METER, (long)beats.n, (long)t->beat.d, 0);
        }
    }
    if (meter_den(t->bar.d) && t->bar.n <= 64 && !(t->beat.n == 1 && t->bar.d == t->beat.d)) {
        put(p, MOTIVE_RELATION_CHANGE_METER, (long)t->bar.n, (long)t->bar.d, 0);
    }
}

static void propose_add_interval(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (t->note_count != s->note_count + 1) {
        return;
    }
    size_t at = 1;
    while (at < s->note_count && pitch_of(s, at) == pitch_of(t, at)) {
        at++;
    }
    for (size_t interval = at; interval >= 1 && interval + 3 > at && interval < s->note_count; interval--) {
        long steps = step_of(t, interval) - step_of(s, interval - 1);
        if (steps != 0) {
            put(p, MOTIVE_RELATION_ADD_INTERVAL, (long)interval, steps, 0);
        }
    }
}

static void propose_omit_interval(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (t->note_count + 1 != s->note_count) {
        return;
    }
    for (size_t interval = 1; interval < s->note_count; interval++) {
        put(p, MOTIVE_RELATION_OMIT_INTERVAL, (long)interval, 0, 0);
    }
}

static void propose_ancillary_fill(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (t->note_count <= s->note_count) {
        return;
    }
    for (size_t interval = 1; interval < s->note_count; interval++) {
        for (long kind = 0; kind <= 1; kind++) {
            put(p, MOTIVE_RELATION_ANCILLARY_FILL, (long)interval, kind, 0);
        }
    }
}

static void propose_condense(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (msk_cmp(t->length, s->length) >= 0) {
        return;
    }
    ExactRational saved = msk_sub(s->length, t->length);
    for (size_t span = 1; span < s->note_count; span++) {
        for (size_t first = 0; first + span < s->note_count; first++) {
            size_t last = first + span;
            ExactRational old_span = msk_sub(msk_add(s->notes[last].onset, s->notes[last].duration),
                                             s->notes[first].onset);
            ExactRational ratio = msk_div(msk_sub(old_span, saved), old_span);
            if (msk_ok(ratio) && ratio.n != 0 && msk_cmp(ratio, msk_int(1)) < 0) {
                put_ratio(p, MOTIVE_RELATION_CONDENSE, (long)first, (long)last, 0, 2, ratio);
            }
        }
    }
}

static void propose_repeat_feature(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (t->note_count <= s->note_count) {
        return;
    }
    size_t added = t->note_count - s->note_count;
    for (size_t f = 0; f < s->feature_count; f++) {
        size_t first = s->features[f].first;
        size_t last = s->features[f].last;
        size_t length = last - first + 1;
        if (added % length != 0 || added / length < 1 || added / length > 2 || last + 1 >= t->note_count) {
            continue;
        }
        long steps = t->notes[last + 1].pitch == MOTIVE_SKETCH_PITCH_UNKNOWN ? 0 : step_of(t, last + 1) - step_of(s, first);
        put(p, MOTIVE_RELATION_REPEAT_FEATURE, (long)f, (long)(added / length), steps);
    }
}

static void propose_shift_feature(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (s->note_count != t->note_count) {
        return;
    }
    for (size_t f = 0; f < s->feature_count; f++) {
        size_t first = s->features[f].first;
        if (msk_cmp(t->notes[first].onset, s->notes[first].onset) > 0) {
            ExactRational offset = msk_sub(t->notes[first].onset, s->notes[first].onset);
            put_ratio(p, MOTIVE_RELATION_SHIFT_FEATURE, (long)f, 0, 0, 1, offset);
        }
    }
}

static void propose_harmony_inversion(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    for (size_t chord = 0; chord < chords_or_one(s); chord++) {
        if (chord < t->chord_count) {
            put(p, MOTIVE_RELATION_HARMONY_INVERSION, (long)chord, t->chords[chord].inversion, 0);
        } else {
            for (long inversion = 0; inversion <= 3; inversion++) {
                put(p, MOTIVE_RELATION_HARMONY_INVERSION, (long)chord, inversion, 0);
            }
        }
    }
}

static void propose_harmony_append(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (t->chord_count == 0) {
        return;
    }
    (void)s;
    const MotiveSketchChord *last = &t->chords[t->chord_count - 1];
    put(p, MOTIVE_RELATION_HARMONY_APPEND, last->degree, last->seventh ? 1 : 0, 0);
    put(p, MOTIVE_RELATION_HARMONY_APPEND, last->degree, last->seventh ? 0 : 1, 0);
}

static void propose_harmony_insert(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    (void)s;
    for (size_t chord = 0; chord < t->chord_count; chord++) {
        put(p, MOTIVE_RELATION_HARMONY_INSERT, (long)chord, t->chords[chord].degree, 0);
    }
}

static void propose_harmony_substitute(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    for (size_t chord = 0; chord < chords_or_one(s) && chord < t->chord_count; chord++) {
        put(p, MOTIVE_RELATION_HARMONY_SUBSTITUTE, (long)chord, t->chords[chord].degree, 0);
        if (chord + 1 < t->chord_count && t->chord_count == chords_or_one(s) + 1) {
            put(p, MOTIVE_RELATION_HARMONY_SUBSTITUTE, (long)chord, t->chords[chord].degree,
                t->chords[chord + 1].degree);
        }
    }
}

static void propose_passing_harmony(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    (void)t;
    for (size_t chord = 0; chord < chords_or_one(s); chord++) {
        put(p, MOTIVE_RELATION_PASSING_HARMONY, (long)chord, 0, 0);
    }
}

static void propose_none(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    (void)s;
    (void)t;
    (void)p;
}

static void propose_plain(MotiveRelationKind kind, Proposals *p) {
    put(p, kind, 0, 0, 0);
}

static void propose_liquidate_features(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    (void)s;
    (void)t;
    for (long steps = 1; steps <= 3; steps++) {
        for (long shorten = 0; shorten <= 1; shorten++) {
            put(p, MOTIVE_RELATION_LIQUIDATE_FEATURES, steps, shorten, 0);
        }
    }
}

/* Whether a relation reads pitches, and whether it needs a scale. */
static bool needs_pitch(MotiveRelationKind kind) {
    switch (kind) {
    case MOTIVE_RELATION_TRANSPOSE:
    case MOTIVE_RELATION_INVERT:
    case MOTIVE_RELATION_INTERVAL_CHANGE:
    case MOTIVE_RELATION_ADD_INTERVAL:
    case MOTIVE_RELATION_ANCILLARY_FILL:
    case MOTIVE_RELATION_HARMONY_INVERSION:
    case MOTIVE_RELATION_HARMONY_APPEND:
    case MOTIVE_RELATION_HARMONY_INSERT:
    case MOTIVE_RELATION_HARMONY_SUBSTITUTE:
    case MOTIVE_RELATION_PASSING_HARMONY:
    case MOTIVE_RELATION_COUNTER_LINE:
    case MOTIVE_RELATION_DOMINANT_FORM:
    case MOTIVE_RELATION_LIQUIDATE_FEATURES:
        return true;
    default:
        return false;
    }
}

static bool needs_scale(MotiveRelationKind kind) {
    switch (kind) {
    case MOTIVE_RELATION_HARMONY_INVERSION:
    case MOTIVE_RELATION_HARMONY_APPEND:
    case MOTIVE_RELATION_HARMONY_INSERT:
    case MOTIVE_RELATION_HARMONY_SUBSTITUTE:
    case MOTIVE_RELATION_PASSING_HARMONY:
    case MOTIVE_RELATION_COUNTER_LINE:
    case MOTIVE_RELATION_DOMINANT_FORM:
    case MOTIVE_RELATION_LIQUIDATE_FEATURES:
        return true;
    default:
        return false;
    }
}

static void propose(MotiveRelationKind kind, const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    p->count = 0;
    bool pitches = pitched(s) && pitched(t);
    if (needs_pitch(kind) && !pitches) {
        return;
    }
    if (needs_scale(kind) && !(s->has_scale && t->has_scale)) {
        return;
    }
    switch (kind) {
    case MOTIVE_RELATION_TRANSPOSE:
        propose_transpose(s, t, p);
        break;
    case MOTIVE_RELATION_INVERT:
        propose_invert(s, t, p);
        break;
    case MOTIVE_RELATION_RETROGRADE:
        propose_plain(kind, p);
        break;
    case MOTIVE_RELATION_INTERVAL_CHANGE:
        propose_interval_change(s, t, p);
        break;
    case MOTIVE_RELATION_FRAGMENT:
        propose_fragment(s, t, p);
        break;
    case MOTIVE_RELATION_LIQUIDATE:
        propose_liquidate(s, t, p);
        break;
    case MOTIVE_RELATION_AUGMENT:
        propose_augment(s, t, p);
        break;
    case MOTIVE_RELATION_DIMINISH:
        propose_diminish(s, t, p);
        break;
    case MOTIVE_RELATION_NOTE_REPEAT:
        propose_note_repeat(s, t, p);
        break;
    case MOTIVE_RELATION_RHYTHM_REPEAT:
        propose_rhythm_repeat(s, t, p);
        break;
    case MOTIVE_RELATION_RHYTHM_SHIFT:
        propose_rhythm_shift(s, t, p);
        break;
    case MOTIVE_RELATION_ADD_UPBEAT:
        propose_add_upbeat(s, t, p);
        break;
    case MOTIVE_RELATION_CHANGE_METER:
        propose_change_meter(s, t, p);
        break;
    case MOTIVE_RELATION_ADD_INTERVAL:
        propose_add_interval(s, t, p);
        break;
    case MOTIVE_RELATION_OMIT_INTERVAL:
        propose_omit_interval(s, t, p);
        break;
    case MOTIVE_RELATION_ANCILLARY_FILL:
        propose_ancillary_fill(s, t, p);
        break;
    case MOTIVE_RELATION_CONDENSE:
        propose_condense(s, t, p);
        break;
    case MOTIVE_RELATION_REPEAT_FEATURE:
        propose_repeat_feature(s, t, p);
        break;
    case MOTIVE_RELATION_SHIFT_FEATURE:
        propose_shift_feature(s, t, p);
        break;
    case MOTIVE_RELATION_HARMONY_INVERSION:
        propose_harmony_inversion(s, t, p);
        break;
    case MOTIVE_RELATION_HARMONY_APPEND:
        propose_harmony_append(s, t, p);
        break;
    case MOTIVE_RELATION_HARMONY_INSERT:
        propose_harmony_insert(s, t, p);
        break;
    case MOTIVE_RELATION_HARMONY_SUBSTITUTE:
        propose_harmony_substitute(s, t, p);
        break;
    case MOTIVE_RELATION_PASSING_HARMONY:
        propose_passing_harmony(s, t, p);
        break;
    case MOTIVE_RELATION_COUNTER_LINE:
    case MOTIVE_RELATION_DOMINANT_FORM:
        propose_plain(kind, p);
        break;
    case MOTIVE_RELATION_LIQUIDATE_FEATURES:
        propose_liquidate_features(s, t, p);
        break;
    default:
        propose_none(s, t, p);
        break;
    }
}

/* --- search ---------------------------------------------------------------------------------- */

typedef struct {
    MotiveSketch sketch;
    MotiveRelationStep steps[MOTIVE_RECOGNIZE_MAX_DEPTH];
    size_t count;
    int cost;
    double nearness;
    size_t seq;
} State;

typedef struct {
    MotiveRelationStep steps[MOTIVE_RECOGNIZE_MAX_DEPTH];
    size_t count;
    int cost;
    bool set;
} Chain;

/* True when chain a (cost, kinds) beats b: lower cost, then the earlier relation order. */
static bool chain_better(int cost, const MotiveRelationStep *steps, size_t count, const Chain *b) {
    if (!b->set || cost != b->cost) {
        return !b->set || cost < b->cost;
    }
    for (size_t i = 0; i < count && i < b->count; i++) {
        if (steps[i].kind != b->steps[i].kind) {
            return steps[i].kind < b->steps[i].kind;
        }
    }
    return false;
}

static bool search_better(const State *a, const State *b) {
    return a->nearness > b->nearness || (a->nearness == b->nearness && a->seq < b->seq);
}

static void sort_level(State *level, size_t count) {
    for (size_t i = 1; i < count; i++) {
        State held = level[i];
        size_t j = i;
        while (j > 0 && search_better(&held, &level[j - 1])) {
            level[j] = level[j - 1];
            j--;
        }
        level[j] = held;
    }
}

void motive_recognize_options_default(MotiveRecognizeOptions *options) {
    options->max_depth = 4;
    options->beam = 64;
}

static ExactRational distance_of(int cost) {
    return msk_make((unsigned long long)cost, (unsigned long long)cost + 4);
}

static void fill_chain(MotiveRecognition *out, const MotiveRelationStep *steps, size_t count) {
    memcpy(out->steps, steps, count * sizeof(steps[0]));
    out->step_count = count;
}

/* Puts a candidate into the next level, unless it repeats a state or does not make the beam. */
static void insert_candidate(State *levels, size_t *counts, size_t depth, size_t beam, const State *candidate) {
    for (size_t d = 0; d <= depth; d++) {
        State *level = levels + d * beam;
        for (size_t i = 0; i < counts[d]; i++) {
            if (motive_sketch_equal(&level[i].sketch, &candidate->sketch)) {
                return;
            }
        }
    }
    State *next = levels + (depth + 1) * beam;
    size_t count = counts[depth + 1];
    for (size_t i = 0; i < count; i++) {
        if (motive_sketch_equal(&next[i].sketch, &candidate->sketch)) {
            if (candidate->cost < next[i].cost) {
                size_t seq = next[i].seq;
                next[i] = *candidate;
                next[i].seq = seq;
            }
            return;
        }
    }
    if (count < beam) {
        next[count] = *candidate;
        counts[depth + 1]++;
        return;
    }
    size_t worst = 0;
    for (size_t i = 1; i < count; i++) {
        if (search_better(&next[worst], &next[i])) {
            worst = i;
        }
    }
    if (candidate->nearness > next[worst].nearness) {
        next[worst] = *candidate;
    }
}

MotiveRecognizeResult motive_variant_recognize(const MotiveSketch *source, const MotiveSketch *target,
                                               const MotiveRecognizeOptions *options, MotiveRecognition *out) {
    MotiveRecognizeOptions defaults;
    motive_recognize_options_default(&defaults);
    if (!options) {
        options = &defaults;
    }
    if (!out || !source || !target || options->max_depth < 1 || options->max_depth > MOTIVE_RECOGNIZE_MAX_DEPTH ||
        options->beam < 1 || options->beam > MOTIVE_RECOGNIZE_MAX_BEAM) {
        return MOTIVE_RECOGNIZE_INVALID;
    }
    /* Both sketches settled the way every relation's result is, so equal means equal. */
    MotiveSketch from;
    MotiveSketch to;
    MotiveRelationStep exact = make_step(MOTIVE_RELATION_EXACT, 0, 0, 0);
    if (motive_relation_apply(source, &exact, &from) != MOTIVE_RELATION_OK ||
        motive_relation_apply(target, &exact, &to) != MOTIVE_RELATION_OK) {
        return MOTIVE_RECOGNIZE_INVALID;
    }
    memset(out, 0, sizeof(*out));
    MotiveDimensions dimensions;
    motive_variant_compare(&from, &to, &dimensions);
    memcpy(out->kept, dimensions.kept, sizeof(out->kept));
    memcpy(out->changed, dimensions.changed, sizeof(out->changed));
    memcpy(out->similarity, dimensions.similarity, sizeof(out->similarity));
    out->distance = msk_int(1);
    if (motive_sketch_equal(&from, &to)) {
        out->found = true;
        out->steps[0] = exact;
        out->step_count = 1;
        out->distance = msk_int(0);
        return MOTIVE_RECOGNIZE_OK;
    }

    size_t beam = options->beam;
    size_t depth_limit = options->max_depth;
    State *levels = malloc((depth_limit + 1) * beam * sizeof(State));
    if (!levels) {
        return MOTIVE_RECOGNIZE_NO_MEMORY;
    }
    size_t counts[MOTIVE_RECOGNIZE_MAX_DEPTH + 1] = {0};
    memset(&levels[0], 0, sizeof(levels[0]));
    levels[0].sketch = from;
    motive_variant_compare(&from, &to, &dimensions);
    levels[0].nearness = motive_variant_nearness(&dimensions);
    counts[0] = 1;
    Chain closest = {{{0}}, 0, 0, true};
    double closest_nearness = levels[0].nearness;

    for (size_t depth = 0; depth < depth_limit; depth++) {
        State *current = levels + depth * beam;
        Chain solution = {{{0}}, 0, 0, false};
        size_t seq = 0;
        for (size_t s = 0; s < counts[depth]; s++) {
            for (int k = 1; k < MOTIVE_RELATION_KIND_COUNT; k++) {
                Proposals proposals;
                propose((MotiveRelationKind)k, &current[s].sketch, &to, &proposals);
                for (size_t i = 0; i < proposals.count; i++) {
                    State candidate;
                    if (motive_relation_apply(&current[s].sketch, &proposals.list[i], &candidate.sketch) !=
                        MOTIVE_RELATION_OK) {
                        continue;
                    }
                    memcpy(candidate.steps, current[s].steps, depth * sizeof(candidate.steps[0]));
                    candidate.steps[depth] = proposals.list[i];
                    candidate.count = depth + 1;
                    candidate.cost = current[s].cost + motive_relation_cost((MotiveRelationKind)k);
                    if (motive_sketch_equal(&candidate.sketch, &to)) {
                        if (chain_better(candidate.cost, candidate.steps, candidate.count, &solution)) {
                            memcpy(solution.steps, candidate.steps, candidate.count * sizeof(candidate.steps[0]));
                            solution.count = candidate.count;
                            solution.cost = candidate.cost;
                            solution.set = true;
                        }
                        continue;
                    }
                    if (solution.set) {
                        continue;
                    }
                    motive_variant_compare(&candidate.sketch, &to, &dimensions);
                    candidate.nearness = motive_variant_nearness(&dimensions);
                    candidate.seq = seq++;
                    insert_candidate(levels, counts, depth, beam, &candidate);
                }
            }
        }
        if (solution.set) {
            out->found = true;
            fill_chain(out, solution.steps, solution.count);
            out->distance = distance_of(solution.cost);
            free(levels);
            return MOTIVE_RECOGNIZE_OK;
        }
        State *next = levels + (depth + 1) * beam;
        sort_level(next, counts[depth + 1]);
        if (counts[depth + 1] > 0 && next[0].nearness > closest_nearness) {
            closest_nearness = next[0].nearness;
            closest.count = next[0].count;
            closest.cost = next[0].cost;
            memcpy(closest.steps, next[0].steps, next[0].count * sizeof(next[0].steps[0]));
        }
        if (counts[depth + 1] == 0) {
            break;
        }
    }
    fill_chain(out, closest.steps, closest.count);
    free(levels);
    return MOTIVE_RECOGNIZE_OK;
}
