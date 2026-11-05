#include "analysis/motive_variant_recognition.h"
#include "analysis/motive_chord_internal.h"

#include "analysis/motive_tone_roles.h"

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

static size_t longer(size_t a, size_t b) { return a > b ? a : b; }

/* The time from a note's onset to the next onset; the last note's duration for the last. */
static Rational inter_onset(const MotiveSketch *sketch, size_t index) {
    if (index + 1 < sketch->note_count) {
        return msk_sub(sketch->notes[index + 1].onset, sketch->notes[index].onset);
    }
    return sketch->notes[index].duration;
}

static bool same_ratio(Rational a_next, Rational a, Rational b_next,
                       Rational b) {
    Rational ra = msk_div(a_next, a);
    Rational rb = msk_div(b_next, b);
    return msk_ok(ra) && msk_ok(rb) && msk_eq(ra, rb);
}

/* How far a sketch may be shifted against the other, in notes, when looking for the alignment that
 * matches best. */
#define MAX_SHIFT 3

static bool rhythm_pair_matches(const MotiveSketch *a, size_t i, const MotiveSketch *b, size_t j) {
    return same_ratio(inter_onset(a, i + 1), inter_onset(a, i), inter_onset(b, j + 1),
                      inter_onset(b, j)) &&
           same_ratio(a->notes[i + 1].duration, a->notes[i].duration, b->notes[j + 1].duration,
                      b->notes[j].duration);
}

static long interval_at(const MotiveSketch *sketch, size_t index) {
    return motive_sketch_step(sketch, sketch->notes[index + 1].pitch, NULL) -
           motive_sketch_step(sketch, sketch->notes[index].pitch, NULL);
}

static int sign_of(long value) { return (value > 0) - (value < 0); }

static bool interval_pair_matches(const MotiveSketch *a, size_t i, const MotiveSketch *b,
                                  size_t j) {
    return interval_at(a, i) == interval_at(b, j);
}

static bool contour_pair_matches(const MotiveSketch *a, size_t i, const MotiveSketch *b, size_t j) {
    return sign_of(interval_at(a, i)) == sign_of(interval_at(b, j));
}

typedef bool (*PairMatch)(const MotiveSketch *a, size_t i, const MotiveSketch *b, size_t j);

/*
 * The share of neighbouring note pairs that match, over the longer sketch. The sketches are
 * compared at the shift (up to MAX_SHIFT notes either way) that matches most, so a fragment, an
 * added upbeat or an omitted note still shows what it kept. Only the unshifted comparison can match
 * every pair, so kept is unchanged.
 */
static Share pair_share(const MotiveSketch *a, const MotiveSketch *b, PairMatch matches) {
    Share best = {0, longer(a->note_count, b->note_count) - 1};
    if (best.total == 0) {
        best.total = 1;
        best.matched = 1;
        return best;
    }
    for (int shift = 0; shift <= 2 * MAX_SHIFT; shift++) {
        int delta = (shift % 2 == 1 ? 1 : -1) * ((shift + 1) / 2);
        Share share = {0, best.total};
        for (size_t i = 0; i + 1 < a->note_count; i++) {
            long j = (long)i + delta;
            if (j >= 0 && (size_t)j + 1 < b->note_count && matches(a, i, b, (size_t)j)) {
                share.matched++;
            }
        }
        if (share.matched > best.matched) {
            best = share;
        }
    }
    return best;
}

static bool same_chord(const MotiveSketchChord *a, const MotiveSketchChord *b) {
    return a->degree == b->degree && a->alteration == b->alteration &&
           a->inversion == b->inversion && a->seventh == b->seventh;
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
    out->similarity[dimension] =
        share.total == 0 ? 1.0 : (double)share.matched / (double)share.total;
}

void motive_variant_compare(const MotiveSketch *a, const MotiveSketch *b, MotiveDimensions *out) {
    memset(out, 0, sizeof(*out));
    set_dimension(out, MOTIVE_DIMENSION_RHYTHM, pair_share(a, b, rhythm_pair_matches));
    bool a_pitched = pitched(a);
    bool b_pitched = pitched(b);
    Share none = {0, 1};
    Share whole = {1, 1};
    if (a_pitched && b_pitched) {
        set_dimension(out, MOTIVE_DIMENSION_INTERVALS, pair_share(a, b, interval_pair_matches));
        set_dimension(out, MOTIVE_DIMENSION_CONTOUR, pair_share(a, b, contour_pair_matches));
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

#define PROPOSAL_MAX 64

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
                      Rational ratio) {
    MotiveRelationStep step = make_step(kind, a, b, c);
    step.value[which] = (long)ratio.num;
    step.den[which] = (long)ratio.den;
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
        /* When the target moved by other means too: the usual steps. */
        static const long USUAL[] = {1, -1, 2, -2, 4, -4};
        for (size_t i = 0; i < sizeof(USUAL) / sizeof(USUAL[0]); i++) {
            put(p, MOTIVE_RELATION_TRANSPOSE, 1, USUAL[i], 0);
        }
    }
    put(p, MOTIVE_RELATION_TRANSPOSE, 0, 12, 0);
    put(p, MOTIVE_RELATION_TRANSPOSE, 0, -12, 0);
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

/* The amounts that turn the source's intervals into the target's, when the two have the same
 * length. */
static void infer_interval_change(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
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

static void propose_interval_change(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    infer_interval_change(s, t, p);
    /* Otherwise a widening or narrowing of every interval, then of each one. */
    static const long USUAL[] = {1, -1, 2, -2};
    for (size_t i = 0; i < sizeof(USUAL) / sizeof(USUAL[0]); i++) {
        put(p, MOTIVE_RELATION_INTERVAL_CHANGE, 0, USUAL[i], 0);
    }
    for (size_t interval = 1; interval < s->note_count; interval++) {
        put(p, MOTIVE_RELATION_INTERVAL_CHANGE, (long)interval, 1, 0);
        put(p, MOTIVE_RELATION_INTERVAL_CHANGE, (long)interval, -1, 0);
    }
}

static void propose_fragment(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (s->note_count < 3) {
        return;
    }
    if (t->note_count < s->note_count) {
        long min = t->note_count < 2 ? 1 : 0;
        for (long position = 1; position <= 3; position++) {
            put(p, MOTIVE_RELATION_FRAGMENT, position, (long)t->note_count, min);
        }
    }
    /* Otherwise the longest fragments. */
    for (size_t length = s->note_count - 1; length >= 2 && length + 5 > s->note_count; length--) {
        for (long position = 1; position <= 3; position++) {
            put(p, MOTIVE_RELATION_FRAGMENT, position, (long)length, 0);
        }
    }
}

static void propose_liquidate(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (t->note_count < s->note_count) {
        long drop = (long)(s->note_count - t->note_count);
        long min = t->note_count < 2 ? 1 : 0;
        put(p, MOTIVE_RELATION_LIQUIDATE, drop, 0, min);
        put(p, MOTIVE_RELATION_LIQUIDATE, drop, 1, min);
    }
    /* Otherwise one to three of the weakest notes. */
    for (long drop = 1; drop <= 3; drop++) {
        put(p, MOTIVE_RELATION_LIQUIDATE, drop, 0, 0);
        put(p, MOTIVE_RELATION_LIQUIDATE, drop, 1, 0);
    }
}

/* The terms of a ratio parameter a composer writes (1 to 3 for a scaling, 1 to 4 for an offset): the registry's
 * range. A ratio outside it is not proposed, since no registry step could name it. */
static bool ratio_within(Rational ratio, unsigned long long limit) {
    return msk_ok(ratio) && ratio.num != 0 && ratio.num <= limit && ratio.den <= limit;
}

static void propose_scale_time(const MotiveSketch *s, const MotiveSketch *t, Proposals *p,
                               MotiveRelationKind kind) {
    Rational factor = msk_div(t->length, s->length);
    bool above = kind == MOTIVE_RELATION_AUGMENT;
    bool wanted = msk_ok(factor) && factor.num != 0 &&
                  (above ? msk_cmp(factor, msk_int(1)) > 0 : msk_cmp(factor, msk_int(1)) < 0);
    if (wanted && ratio_within(factor, 3)) {
        put_ratio(p, kind, 0, 0, 0, 0, factor);
    }
    /* The ratios a chain draws, so a later step's effect on the length does not hide the factor. */
    static const unsigned long long TERMS[3][2] = {{2, 1}, {3, 2}, {3, 1}};
    for (size_t i = 0; i < 3; i++) {
        Rational ratio = above ? msk_make(TERMS[i][0], TERMS[i][1]) : msk_make(TERMS[i][1], TERMS[i][0]);
        if (!(wanted && msk_eq(ratio, factor))) {
            put_ratio(p, kind, 0, 0, 0, 0, ratio);
        }
    }
}

static void propose_augment(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    propose_scale_time(s, t, p, MOTIVE_RELATION_AUGMENT);
}

static void propose_diminish(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    propose_scale_time(s, t, p, MOTIVE_RELATION_DIMINISH);
}

static void propose_note_repeat(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    long added = t->note_count > s->note_count ? (long)(t->note_count - s->note_count) : 0;
    for (size_t note = 0; note < s->note_count && added + 1 >= 2 && added + 1 <= 4; note++) {
        put(p, MOTIVE_RELATION_NOTE_REPEAT, (long)note, added + 1, 0);
    }
    for (size_t note = 0; note < s->note_count; note++) {
        put(p, MOTIVE_RELATION_NOTE_REPEAT, (long)note, 2, 0);
    }
}

static void propose_rhythm_repeat(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    size_t total = s->note_count;
    if (total < 2) {
        return;
    }
    size_t differing = total;
    if (t->note_count == total) {
        for (size_t i = 0; i < total; i++) {
            if (!msk_eq(s->notes[i].duration, t->notes[i].duration)) {
                differing = i;
                break;
            }
        }
    }
    for (size_t length = 1; length <= 2; length++) {
        for (size_t target = 0; target + length <= total; target++) {
            if (differing != total && target != differing) {
                continue;
            }
            for (size_t first = 0; first + length <= total; first++) {
                if (first != target) {
                    put(p, MOTIVE_RELATION_RHYTHM_REPEAT, (long)first, (long)length, (long)target);
                }
            }
        }
    }
}

static void propose_rhythm_shift(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    Rational inside = msk_mod(msk_sub(msk_add(s->pickup, s->bar), t->pickup), s->bar);
    if (msk_ok(inside) && !msk_is_zero(inside)) {
        put_ratio(p, MOTIVE_RELATION_RHYTHM_SHIFT, 0, 0, 0, 0, inside);
    }
    Rational half = msk_div(s->beat, msk_int(2));
    if (msk_ok(half)) {
        put_ratio(p, MOTIVE_RELATION_RHYTHM_SHIFT, 0, 0, 0, 0, half);
    }
    put_ratio(p, MOTIVE_RELATION_RHYTHM_SHIFT, 0, 0, 0, 0, s->beat);
}

static void propose_add_upbeat(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    long added = t->note_count > s->note_count ? (long)(t->note_count - s->note_count) : 0;
    for (long approach = 1; approach >= -1 && added >= 1 && added <= 3; approach -= 2) {
        put(p, MOTIVE_RELATION_ADD_UPBEAT, added, approach, 0);
    }
    for (long count = 1; count <= 3; count++) {
        for (long approach = 1; approach >= -1; approach -= 2) {
            put(p, MOTIVE_RELATION_ADD_UPBEAT, count, approach, 0);
        }
    }
}

static bool meter_den(unsigned long long den) {
    return den == 1 || den == 2 || den == 4 || den == 8 || den == 16;
}

static void propose_change_meter(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (t->beat.num == 1 && meter_den(t->beat.den)) {
        Rational beats = msk_div(t->bar, t->beat);
        if (msk_ok(beats) && beats.den == 1 && beats.num >= 1 && beats.num <= 64) {
            put(p, MOTIVE_RELATION_CHANGE_METER, (long)beats.num, (long)t->beat.den, 0);
        }
    }
    if (meter_den(t->bar.den) && t->bar.num <= 64 && !(t->beat.num == 1 && t->bar.den == t->beat.den)) {
        put(p, MOTIVE_RELATION_CHANGE_METER, (long)t->bar.num, (long)t->bar.den, 0);
    }
    /* Otherwise the usual meters other than the state's. */
    static const long USUAL[][2] = {{2, 4}, {3, 4}, {4, 4}};
    for (size_t i = 0; i < sizeof(USUAL) / sizeof(USUAL[0]); i++) {
        Rational bar =
            msk_make((unsigned long long)USUAL[i][0], (unsigned long long)USUAL[i][1]);
        if (!msk_eq(bar, s->bar)) {
            put(p, MOTIVE_RELATION_CHANGE_METER, USUAL[i][0], USUAL[i][1], 0);
        }
    }
}

/* True when the target without its note skip orders its pitches exactly as the state does (every pair: the same sign). */
static bool orders_like_without(const MotiveSketch *s, const MotiveSketch *t, size_t skip) {
    for (size_t i = 0; i < s->note_count; i++) {
        size_t ti = i < skip ? i : i + 1;
        for (size_t j = i + 1; j < s->note_count; j++) {
            size_t tj = j < skip ? j : j + 1;
            long ds = pitch_of(s, j) - pitch_of(s, i);
            long dt = pitch_of(t, tj) - pitch_of(t, ti);
            if ((ds > 0) != (dt > 0) || (ds < 0) != (dt < 0)) {
                return false;
            }
        }
    }
    return true;
}

static void propose_add_interval(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    if (t->note_count == s->note_count + 1) {
        /* An added note that becomes the lowest or highest of a contour form shifts every rank, so the first difference
           names only interval 1 (R-153): every interval the target orders like the state without that note, every steps. */
        for (size_t interval = 1; interval < s->note_count; interval++) {
            if (!orders_like_without(s, t, interval)) {
                continue;
            }
            for (long steps = -7; steps <= 7; steps++) {
                if (steps != 0) {
                    put(p, MOTIVE_RELATION_ADD_INTERVAL, (long)interval, steps, 0);
                }
            }
        }
        size_t at = 1;
        while (at < s->note_count && pitch_of(s, at) == pitch_of(t, at)) {
            at++;
        }
        for (size_t interval = at; interval >= 1 && interval + 3 > at && interval < s->note_count;
             interval--) {
            long steps = step_of(t, interval) - step_of(s, interval - 1);
            if (steps != 0) {
                put(p, MOTIVE_RELATION_ADD_INTERVAL, (long)interval, steps, 0);
            }
        }
    }
    for (size_t interval = 1; interval < s->note_count; interval++) {
        put(p, MOTIVE_RELATION_ADD_INTERVAL, (long)interval, 1, 0);
        put(p, MOTIVE_RELATION_ADD_INTERVAL, (long)interval, -1, 0);
    }
}

static void propose_omit_interval(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    (void)t;
    for (size_t interval = 1; interval < s->note_count; interval++) {
        put(p, MOTIVE_RELATION_OMIT_INTERVAL, (long)interval, 0, 0);
    }
}

static void propose_ancillary_fill(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    (void)t;
    for (size_t interval = 1; interval < s->note_count; interval++) {
        for (long kind = 0; kind <= 1; kind++) {
            put(p, MOTIVE_RELATION_ANCILLARY_FILL, (long)interval, kind, 0);
        }
    }
}

static Rational note_end(const MotiveSketch *sketch, size_t index) {
    return msk_add(sketch->notes[index].onset, sketch->notes[index].duration);
}

static void propose_condense(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    /* With the same notes, the run whose durations changed, compressed by the ratio of its spans. */
    if (s->note_count == t->note_count) {
        size_t first = s->note_count;
        size_t last = 0;
        for (size_t i = 0; i < s->note_count; i++) {
            if (!msk_eq(s->notes[i].duration, t->notes[i].duration)) {
                first = first == s->note_count ? i : first;
                last = i;
            }
        }
        if (first < s->note_count) {
            Rational ratio = msk_div(msk_sub(note_end(t, last), t->notes[first].onset),
                                          msk_sub(note_end(s, last), s->notes[first].onset));
            if (ratio_within(ratio, 3) && msk_cmp(ratio, msk_int(1)) < 0) {
                put_ratio(p, MOTIVE_RELATION_CONDENSE, (long)first, (long)last, 0, 2, ratio);
            }
        }
    }
    if (msk_cmp(t->length, s->length) < 0) {
        Rational saved = msk_sub(s->length, t->length);
        for (size_t span = 1; span < s->note_count; span++) {
            for (size_t first = 0; first + span < s->note_count; first++) {
                size_t last = first + span;
                Rational old_span = msk_sub(note_end(s, last), s->notes[first].onset);
                Rational ratio = msk_div(msk_sub(old_span, saved), old_span);
                if (ratio_within(ratio, 3) && msk_cmp(ratio, msk_int(1)) < 0) {
                    put_ratio(p, MOTIVE_RELATION_CONDENSE, (long)first, (long)last, 0, 2, ratio);
                }
            }
        }
    }
    /* Otherwise a single note, then a run of two or three, at half the time. */
    for (size_t span = 0; span <= 2; span++) {
        for (size_t first = 0; first + span < s->note_count; first++) {
            put_ratio(p, MOTIVE_RELATION_CONDENSE, (long)first, (long)(first + span), 0, 2,
                      msk_make(1, 2));
        }
    }
}

static void propose_repeat_feature(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    for (size_t f = 0; f < s->feature_count; f++) {
        size_t first = s->features[f].first;
        size_t last = s->features[f].last;
        size_t length = last - first + 1;
        size_t added = t->note_count > s->note_count ? t->note_count - s->note_count : 0;
        if (added % length == 0 && added / length >= 1 && added / length <= 2 &&
            last + 1 < t->note_count && t->notes[last + 1].pitch != MOTIVE_SKETCH_PITCH_UNKNOWN &&
            s->notes[first].pitch != MOTIVE_SKETCH_PITCH_UNKNOWN) {
            put(p, MOTIVE_RELATION_REPEAT_FEATURE, (long)f, (long)(added / length),
                step_of(t, last + 1) - step_of(s, first));
        }
    }
    for (size_t f = 0; f < s->feature_count; f++) {
        /* The registry's draw (R-153), so a first step the target cannot show is in level 1. */
        for (long times = 1; times <= 2; times++) {
            for (long steps = -7; steps <= 7; steps++) {
                put(p, MOTIVE_RELATION_REPEAT_FEATURE, (long)f, times, steps);
            }
        }
    }
}

static void propose_shift_feature(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    for (size_t f = 0; f < s->feature_count; f++) {
        size_t first = s->features[f].first;
        if (s->note_count == t->note_count &&
            msk_cmp(t->notes[first].onset, s->notes[first].onset) > 0) {
            Rational offset = msk_sub(t->notes[first].onset, s->notes[first].onset);
            put_ratio(p, MOTIVE_RELATION_SHIFT_FEATURE, (long)f, 0, 0, 1, offset);
        }
    }
    for (size_t f = 0; f < s->feature_count; f++) {
        put_ratio(p, MOTIVE_RELATION_SHIFT_FEATURE, (long)f, 0, 0, 1, msk_make(1, 16));
    }
}

static void propose_harmony_inversion(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    for (size_t chord = 0; chord < chords_or_one(s) && chord < t->chord_count; chord++) {
        put(p, MOTIVE_RELATION_HARMONY_INVERSION, (long)chord, t->chords[chord].inversion, 0);
    }
    for (size_t chord = 0; chord < chords_or_one(s); chord++) {
        for (long inversion = 1; inversion <= 2; inversion++) {
            put(p, MOTIVE_RELATION_HARMONY_INVERSION, (long)chord, inversion, 0);
        }
    }
}

static void propose_harmony_append(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    (void)s;
    if (t->chord_count > 0) {
        const MotiveSketchChord *last = &t->chords[t->chord_count - 1];
        put(p, MOTIVE_RELATION_HARMONY_APPEND, last->degree, last->seventh ? 1 : 0, 0);
        put(p, MOTIVE_RELATION_HARMONY_APPEND, last->degree, last->seventh ? 0 : 1, 0);
    }
    static const long USUAL[] = {1, 4, 5, 2, 6};
    for (size_t i = 0; i < sizeof(USUAL) / sizeof(USUAL[0]); i++) {
        put(p, MOTIVE_RELATION_HARMONY_APPEND, USUAL[i], 0, 0);
    }
}

static void propose_harmony_insert(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    for (size_t chord = 0; chord < t->chord_count; chord++) {
        put(p, MOTIVE_RELATION_HARMONY_INSERT, (long)chord, t->chords[chord].degree, 0);
    }
    static const long USUAL[] = {4, 5, 2};
    for (size_t chord = 0; chord < chords_or_one(s); chord++) {
        for (size_t i = 0; i < sizeof(USUAL) / sizeof(USUAL[0]); i++) {
            put(p, MOTIVE_RELATION_HARMONY_INSERT, (long)chord, USUAL[i], 0);
        }
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
    static const long USUAL[] = {4, 5, 1};
    for (size_t chord = 0; chord < chords_or_one(s); chord++) {
        for (size_t i = 0; i < sizeof(USUAL) / sizeof(USUAL[0]); i++) {
            put(p, MOTIVE_RELATION_HARMONY_SUBSTITUTE, (long)chord, USUAL[i], 0);
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

static void propose_plain(MotiveRelationKind kind, Proposals *p) { put(p, kind, 0, 0, 0); }

static void propose_liquidate_features(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    (void)s;
    (void)t;
    for (long steps = 1; steps <= 3; steps++) {
        for (long shorten = 0; shorten <= 1; shorten++) {
            put(p, MOTIVE_RELATION_LIQUIDATE_FEATURES, steps, shorten, 0);
        }
    }
}

/* A proposal not already in the list (the frame relations propose from several sources). */
static void put_once(Proposals *p, MotiveRelationKind kind, long a, long b) {
    for (size_t i = 0; i < p->count; i++) {
        if (p->list[i].kind == kind && p->list[i].value[0] == a && p->list[i].value[1] == b) {
            return;
        }
    }
    put(p, kind, a, b, 0);
}

static bool roles_of(const MotiveSketch *sketch, MotiveToneLabel *labels) {
    return sketch->has_scale && motive_tone_roles(sketch, NULL, 0, labels) == MOTIVE_TONE_OK;
}

/* The figure the target has after each of the source's chord tones first, then every type after every note. */
static void propose_add_non_chord_tone(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    unsigned long seen = 0;
    if (roles_of(t, labels)) {
        for (size_t j = 1; j < t->note_count; j++) {
            if (labels[j].role == MOTIVE_TONE_NON_CHORD) {
                put_once(p, MOTIVE_RELATION_ADD_NON_CHORD_TONE, (long)j - 1, labels[j].type);
                seen |= 1ul << labels[j].type;
            }
        }
    }
    for (int pass = 0; pass < 2; pass++) {
        for (long after = 0; after + 1 < (long)s->note_count; after++) {
            for (long type = 0; type < NON_CHORD_TONE_COUNT; type++) {
                if (pass == 1 || (seen & (1ul << type)) != 0) {
                    put_once(p, MOTIVE_RELATION_ADD_NON_CHORD_TONE, after, type);
                }
            }
        }
    }
}

static void propose_remove_non_chord_tone(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    (void)t;
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    if (!roles_of(s, labels)) {
        return;
    }
    for (size_t i = 0; i < s->note_count; i++) {
        if (labels[i].role != MOTIVE_TONE_CHORD) {
            put_once(p, MOTIVE_RELATION_REMOVE_NON_CHORD_TONE, (long)i, 0);
        }
    }
}

/* For each note of the source that is not a chord tone, the type the target has there first, then every type. */
static void propose_change_non_chord_tone(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    MotiveToneLabel wanted[MOTIVE_SKETCH_MAX_NOTES];
    if (!roles_of(s, labels)) {
        return;
    }
    bool compare = s->note_count == t->note_count && roles_of(t, wanted);
    for (size_t i = 0; compare && i < s->note_count; i++) {
        if (labels[i].role != MOTIVE_TONE_CHORD && wanted[i].role == MOTIVE_TONE_NON_CHORD) {
            put_once(p, MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, (long)i, wanted[i].type);
        }
    }
    for (size_t i = 0; i < s->note_count; i++) {
        for (long type = 0; labels[i].role != MOTIVE_TONE_CHORD && type < NON_CHORD_TONE_COUNT; type++) {
            put_once(p, MOTIVE_RELATION_CHANGE_NON_CHORD_TONE, (long)i, type);
        }
    }
}

static void propose_reharmonize_frame(const MotiveSketch *s, const MotiveSketch *t, Proposals *p) {
    (void)s;
    for (size_t chord = 0; chord < t->chord_count; chord++) {
        put_once(p, MOTIVE_RELATION_REHARMONIZE_FRAME, t->chords[chord].degree, t->chords[chord].seventh ? 1 : 0);
    }
    for (long degree = 1; degree <= 7; degree++) {
        for (long seventh = 0; seventh <= 1; seventh++) {
            put_once(p, MOTIVE_RELATION_REHARMONIZE_FRAME, degree, seventh);
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
    case MOTIVE_RELATION_REDUCE_TO_FRAME:
    case MOTIVE_RELATION_ADD_NON_CHORD_TONE:
    case MOTIVE_RELATION_REMOVE_NON_CHORD_TONE:
    case MOTIVE_RELATION_CHANGE_NON_CHORD_TONE:
    case MOTIVE_RELATION_REHARMONIZE_FRAME:
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
    case MOTIVE_RELATION_REDUCE_TO_FRAME:
    case MOTIVE_RELATION_ADD_NON_CHORD_TONE:
    case MOTIVE_RELATION_REMOVE_NON_CHORD_TONE:
    case MOTIVE_RELATION_CHANGE_NON_CHORD_TONE:
    case MOTIVE_RELATION_REHARMONIZE_FRAME:
        return true;
    default:
        return false;
    }
}

static void propose(MotiveRelationKind kind, const MotiveSketch *s, const MotiveSketch *t,
                    Proposals *p) {
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
    case MOTIVE_RELATION_REDUCE_TO_FRAME:
        propose_plain(kind, p);
        break;
    case MOTIVE_RELATION_ADD_NON_CHORD_TONE:
        propose_add_non_chord_tone(s, t, p);
        break;
    case MOTIVE_RELATION_REMOVE_NON_CHORD_TONE:
        propose_remove_non_chord_tone(s, t, p);
        break;
    case MOTIVE_RELATION_CHANGE_NON_CHORD_TONE:
        propose_change_non_chord_tone(s, t, p);
        break;
    case MOTIVE_RELATION_REHARMONIZE_FRAME:
        propose_reharmonize_frame(s, t, p);
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
    bool leader; /* the nearest state its last relation reached at this depth: never pushed out of
                    the beam */
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
    options->contour = false;
    options->implied_start = true;
    options->admit = NULL;
    options->admit_context = NULL;
}

static Rational distance_of(int cost) {
    return msk_make((unsigned long long)cost, (unsigned long long)cost + 4);
}

static void fill_chain(MotiveRecognition *out, const MotiveRelationStep *steps, size_t count) {
    memcpy(out->steps, steps, count * sizeof(steps[0]));
    out->step_count = count;
}

/* The last relation of a state's chain. */
static MotiveRelationKind last_kind(const State *state) {
    return state->steps[state->count - 1].kind;
}

/*
 * Puts a candidate into the next level, unless it repeats a state or does not make the beam. The
 * beam keeps the nearest states, except that the nearest state each relation reached stays (a
 * relation the nearness cannot see, such as a change of meter, would otherwise never survive the
 * beam). Leaders are only kept when the beam has room for all of them and as many other states.
 */
static void insert_candidate(State *const *levels, const size_t *caps, size_t *counts, size_t depth,
                             size_t beam, State *candidate) {
    for (size_t d = 0; d <= depth; d++) {
        State *level = levels[d];
        for (size_t i = 0; i < counts[d]; i++) {
            if (motive_sketch_equal(&level[i].sketch, &candidate->sketch)) {
                return;
            }
        }
    }
    State *next = levels[depth + 1];
    size_t count = counts[depth + 1];
    for (size_t i = 0; i < count; i++) {
        if (motive_sketch_equal(&next[i].sketch, &candidate->sketch)) {
            if (candidate->cost < next[i].cost) {
                size_t seq = next[i].seq;
                bool leader = next[i].leader;
                next[i] = *candidate;
                next[i].seq = seq;
                next[i].leader = leader;
            }
            return;
        }
    }
    bool diverse = depth + 1 >= 2 && beam >= 2 * MOTIVE_RELATION_KIND_COUNT;
    candidate->leader = false;
    size_t old_leader = count;
    if (diverse) {
        for (size_t i = 0; i < count; i++) {
            if (next[i].leader && last_kind(&next[i]) == last_kind(candidate)) {
                old_leader = i;
                break;
            }
        }
        if (old_leader == count || candidate->nearness > next[old_leader].nearness) {
            candidate->leader = true;
        }
    }
    size_t slot;
    if (count < caps[depth + 1]) {
        slot = count;
        counts[depth + 1]++;
    } else {
        /* The worst state that is not a leader. */
        slot = count;
        for (size_t i = 0; i < count; i++) {
            if (!next[i].leader && (slot == count || search_better(&next[slot], &next[i]))) {
                slot = i;
            }
        }
        if (slot == count || (!candidate->leader && candidate->nearness <= next[slot].nearness)) {
            return;
        }
    }
    if (candidate->leader && old_leader < count) {
        next[old_leader].leader = false;
    }
    next[slot] = *candidate;
}

MotiveRecognizeResult motive_variant_recognize(const MotiveSketch *source,
                                               const MotiveSketch *target,
                                               const MotiveRecognizeOptions *options,
                                               MotiveRecognition *out) {
    MotiveRecognizeOptions defaults;
    motive_recognize_options_default(&defaults);
    if (!options) {
        options = &defaults;
    }
    if (!out || !source || !target || options->max_depth < 1 ||
        options->max_depth > MOTIVE_RECOGNIZE_MAX_DEPTH || options->beam < 1 ||
        options->beam > MOTIVE_RECOGNIZE_MAX_BEAM) {
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
    if (options->contour &&
        (!motive_sketch_contour_normalize(&from, &from) || !motive_sketch_contour_normalize(&to, &to))) {
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
    /* Level 1 holds every distinct state one relation from the source (R-149); the beam cuts from level 2 on. */
    size_t caps[MOTIVE_RECOGNIZE_MAX_DEPTH + 1];
    State *level[MOTIVE_RECOGNIZE_MAX_DEPTH + 1];
    size_t total = 0;
    for (size_t d = 0; d <= depth_limit; d++) {
        caps[d] = d == 0 ? 2 : (d == 1 ? MOTIVE_RELATION_KIND_COUNT * PROPOSAL_MAX : beam);
        total += caps[d];
    }
    State *storage = malloc(total * sizeof(State));
    if (!storage) {
        return MOTIVE_RECOGNIZE_NO_MEMORY;
    }
    for (size_t d = 0, at = 0; d <= depth_limit; at += caps[d], d++) {
        level[d] = storage + at;
    }
    size_t counts[MOTIVE_RECOGNIZE_MAX_DEPTH + 1] = {0};
    memset(&level[0][0], 0, sizeof(level[0][0]));
    level[0][0].sketch = from;
    motive_variant_compare(&from, &to, &dimensions);
    level[0][0].nearness = motive_variant_nearness(&dimensions);
    counts[0] = 1;
    /*
     * A source without chords of its own is also tried with its best implied chord at 0, at no cost, when the target has
     * chords: a relation that keeps the chords (fragment, omit-interval, ancillary-fill and most others) leaves a
     * chordless source chordless, so a variant that carries the source's implied chord is otherwise only reachable
     * through a harmony relation that does nothing but add it.
     */
    MotiveSketchChord implied;
    if (options->implied_start && !options->contour && from.chord_count == 0 && to.chord_count > 0 && from.has_scale &&
        from.form == MOTIVE_SKETCH_FORM_FULL && mch_implied_chord(&from, &implied)) {
        level[0][1] = level[0][0];
        level[0][1].sketch.chords[0] = implied;
        level[0][1].sketch.chord_count = 1;
        motive_variant_compare(&level[0][1].sketch, &to, &dimensions);
        level[0][1].nearness = motive_variant_nearness(&dimensions);
        counts[0] = 2;
    }
    Chain closest = {{{0}}, 0, 0, true};
    double closest_nearness = level[0][0].nearness;

    for (size_t depth = 0; depth < depth_limit; depth++) {
        State *current = level[depth];
        Chain solution = {{{0}}, 0, 0, false};
        size_t seq = 0;
        for (size_t s = 0; s < counts[depth]; s++) {
            for (int k = 1; k < MOTIVE_RELATION_KIND_COUNT; k++) {
                Proposals proposals;
                propose((MotiveRelationKind)k, &current[s].sketch, &to, &proposals);
                for (size_t i = 0; i < proposals.count; i++) {
                    State candidate;
                    if (options->admit && !options->admit(&proposals.list[i], options->admit_context)) {
                        continue;
                    }
                    if (motive_relation_apply(&current[s].sketch, &proposals.list[i],
                                              &candidate.sketch) != MOTIVE_RELATION_OK) {
                        continue;
                    }
                    if (options->contour &&
                        !motive_sketch_contour_normalize(&candidate.sketch, &candidate.sketch)) {
                        continue;
                    }
                    memcpy(candidate.steps, current[s].steps, depth * sizeof(candidate.steps[0]));
                    candidate.steps[depth] = proposals.list[i];
                    candidate.count = depth + 1;
                    candidate.cost = current[s].cost + motive_relation_cost((MotiveRelationKind)k);
                    if (motive_sketch_equal(&candidate.sketch, &to)) {
                        if (chain_better(candidate.cost, candidate.steps, candidate.count,
                                         &solution)) {
                            memcpy(solution.steps, candidate.steps,
                                   candidate.count * sizeof(candidate.steps[0]));
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
                    insert_candidate(level, caps, counts, depth, beam, &candidate);
                }
            }
        }
        if (solution.set) {
            out->found = true;
            fill_chain(out, solution.steps, solution.count);
            out->distance = distance_of(solution.cost);
            free(storage);
            return MOTIVE_RECOGNIZE_OK;
        }
        State *next = level[depth + 1];
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
    free(storage);
    return MOTIVE_RECOGNIZE_OK;
}
