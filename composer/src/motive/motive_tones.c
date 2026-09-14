#include <ursatz_composer/motive_tones.h>

#include <string.h>

#include "analysis/motive_sketch.h"

#include <ursatz_composer/motive_sketch_bridge.h>

#include "motive/motive_internal.h"

/* The order of the proposed triads, which breaks ties (I first). */
static const int TRIAD_ORDER[7] = {1, 5, 4, 6, 2, 3, 7};

static int pitch_class(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

/* The bar a time is in: bars are counted from the first barline, which is at the pickup, so a time before it is bar -1. */
static long bar_of(ExactRational time, ExactRational pickup, ExactRational bar) {
    if (msk_cmp(time, pickup) < 0) {
        return -1;
    }
    return (long)msk_floor_div(msk_sub(time, pickup), bar);
}

/* The time inside a bar, in whole notes: the bar before the first barline ends at the pickup. */
static ExactRational offset_in_bar(const MotiveSketch *sketch, long bar, ExactRational onset) {
    if (bar < 0) {
        return msk_sub(msk_add(onset, sketch->bar), sketch->pickup);
    }
    return msk_sub(msk_sub(onset, sketch->pickup), msk_mul(msk_int((unsigned long long)bar), sketch->bar));
}

/* The downbeat, in 4/4 the third beat as well, in a compound meter every third beat. */
static bool is_strong(const MotiveSketch *sketch, long bar, ExactRational onset) {
    ExactRational beats = msk_div(offset_in_bar(sketch, bar, onset), sketch->beat);
    if (!msk_ok(beats) || beats.d != 1) {
        return false;
    }
    ExactRational per_bar = msk_div(sketch->bar, sketch->beat);
    unsigned long long count = msk_ok(per_bar) ? per_bar.n / per_bar.d : 0;
    if (beats.n == 0) {
        return true;
    }
    if (count > 3 && count % 3 == 0) {
        return beats.n % 3 == 0;
    }
    return count == 4 && beats.n == 2;
}

static bool is_chord_tone(const MotiveSketch *sketch, const MotiveSketchChord *chord, int pitch) {
    int pcs[4];
    size_t count = motive_sketch_chord_tones(sketch, chord, pcs);
    for (size_t i = 0; i < count; i++) {
        if (pcs[i] == pitch_class(pitch)) {
            return true;
        }
    }
    return false;
}

static MotiveSketchChord triad(int degree, ExactRational onset) {
    MotiveSketchChord chord = {onset, degree, 0, 0, false};
    return chord;
}

typedef struct {
    size_t notes[MOTIVE_SKETCH_MAX_NOTES]; /* the strong-beat notes of the bar */
    size_t count;
} StrongNotes;

/* How many of the strong notes are chord tones when the notes from split on take second and the rest first. */
static size_t strong_chord_tones(const MotiveSketch *sketch, const StrongNotes *strong, const MotiveSketchChord *first,
                                 const MotiveSketchChord *second, ExactRational split) {
    size_t tones = 0;
    for (size_t i = 0; i < strong->count; i++) {
        const MotiveSketchNote *note = &sketch->notes[strong->notes[i]];
        const MotiveSketchChord *chord = second && msk_cmp(note->onset, split) >= 0 ? second : first;
        if (is_chord_tone(sketch, chord, note->pitch)) {
            tones++;
        }
    }
    return tones;
}

static bool append_chord(MotiveBaseHarmony *out, const MotiveSketchChord *chord) {
    if (out->count > 0) {
        const MotiveChord *last = &out->chords[out->count - 1];
        if (last->degree == chord->degree && last->alteration == chord->alteration && last->inversion == chord->inversion &&
            last->seventh == chord->seventh) {
            return true;
        }
    }
    if (out->count == MOTIVE_MAX_CHORDS) {
        return false;
    }
    MotiveChord *slot = &out->chords[out->count++];
    memset(slot, 0, sizeof(*slot));
    slot->onset = motive_time((long)chord->onset.n, (unsigned long)chord->onset.d);
    slot->degree = chord->degree;
    slot->alteration = chord->alteration;
    slot->inversion = chord->inversion;
    slot->seventh = chord->seventh;
    return true;
}

static void propose(const MotiveSketch *sketch, MotiveBaseHarmony *out) {
    out->count = 0;
    out->given = false;
    long first_bar = bar_of(sketch->notes[0].onset, sketch->pickup, sketch->bar);
    long last_bar = bar_of(sketch->notes[sketch->note_count - 1].onset, sketch->pickup, sketch->bar);
    MotiveSketchChord previous = triad(1, msk_int(0));
    bool have_previous = false;
    for (long bar = first_bar; bar <= last_bar; bar++) {
        StrongNotes strong;
        strong.count = 0;
        size_t in_bar = 0;
        for (size_t i = 0; i < sketch->note_count; i++) {
            if (bar_of(sketch->notes[i].onset, sketch->pickup, sketch->bar) != bar) {
                continue;
            }
            in_bar++;
            if (is_strong(sketch, bar, sketch->notes[i].onset)) {
                strong.notes[strong.count++] = i;
            }
        }
        if (in_bar == 0) {
            continue; /* the chord before it goes on */
        }
        /* The first chord is at 0, the others at the start of their bar. */
        ExactRational start = bar < 0 ? msk_int(0)
                                      : msk_add(sketch->pickup, msk_mul(msk_int((unsigned long long)bar), sketch->bar));
        ExactRational at = have_previous ? start : msk_int(0);
        MotiveSketchChord best = triad(1, at);
        size_t best_tones = 0;
        for (int k = 0; k < 7; k++) {
            MotiveSketchChord candidate = triad(TRIAD_ORDER[k], at);
            size_t tones = strong_chord_tones(sketch, &strong, &candidate, NULL, msk_int(0));
            if (tones > best_tones) {
                best_tones = tones;
                best = candidate;
            }
        }
        if (!have_previous || best.degree != previous.degree) {
            (void)append_chord(out, &best);
        }
        previous = best;
        have_previous = true;
        /* A second chord on a beat inside the bar (not in the short bar before a pickup's first barline). */
        if (bar < 0 || strong.count - best_tones < 2) {
            continue;
        }
        size_t best_gain = 1;
        MotiveSketchChord second = triad(1, msk_int(0));
        ExactRational bar_end = msk_add(start, sketch->bar);
        for (ExactRational split = msk_add(start, sketch->beat); msk_ok(split) && msk_cmp(split, bar_end) < 0;
             split = msk_add(split, sketch->beat)) {
            for (int k = 0; k < 7; k++) {
                MotiveSketchChord candidate = triad(TRIAD_ORDER[k], split);
                if (candidate.degree == best.degree) {
                    continue;
                }
                size_t gain = strong_chord_tones(sketch, &strong, &best, &candidate, split) - best_tones;
                if (gain > best_gain) {
                    best_gain = gain;
                    second = candidate;
                }
            }
        }
        if (best_gain >= 2) {
            (void)append_chord(out, &second);
            previous = second;
        }
    }
}

bool motive_base_harmony(const Motive *motive, MotiveBaseHarmony *out) {
    if (!motive || !out || motive->form != MOTIVE_FORM_FULL || motive->key_source == MOTIVE_KEY_UNSPECIFIED) {
        return false;
    }
    MotiveBaseHarmony result;
    memset(&result, 0, sizeof(result));
    if (motive->harmony_count > 0) {
        memcpy(result.chords, motive->harmony, motive->harmony_count * sizeof(motive->harmony[0]));
        result.count = motive->harmony_count;
        result.given = true;
        *out = result;
        return true;
    }
    MotiveSketch sketch;
    if (!motive_to_sketch(motive, &sketch) || !sketch.has_scale) {
        return false;
    }
    propose(&sketch, &result);
    if (result.count == 0) {
        return false;
    }
    *out = result;
    return true;
}

bool motive_harmony_bar_limit_ok(const Motive *motive) {
    if (!motive || motive->harmony_count == 0) {
        return true;
    }
    MotiveSketch sketch;
    memset(&sketch, 0, sizeof(sketch));
    sketch.pickup = msk_make((unsigned long long)motive_pickup(motive).num, motive_pickup(motive).den);
    sketch.bar = msk_make((unsigned long long)motive->meter.beats_per_bar, (unsigned long long)motive->meter.beat_unit);
    for (size_t i = 0; i < motive->harmony_count; i++) {
        long bar_i = bar_of(msk_make((unsigned long long)motive->harmony[i].onset.num, motive->harmony[i].onset.den),
                            sketch.pickup, sketch.bar);
        size_t in_bar = 0;
        for (size_t j = 0; j < motive->harmony_count; j++) {
            if (bar_of(msk_make((unsigned long long)motive->harmony[j].onset.num, motive->harmony[j].onset.den),
                       sketch.pickup, sketch.bar) == bar_i) {
                in_bar++;
            }
        }
        if (in_bar > MOTIVE_MAX_CHORDS_PER_BAR) {
            return false;
        }
    }
    return true;
}
