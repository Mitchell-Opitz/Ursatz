#include "analysis/motive/motive_relation.h"
#include "analysis/motive/motive_chord_internal.h"
#include "analysis/motive/motive_tone_roles.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/pitch/midi_pitch.h"
#include "analysis/motive/motive_relation_internal.h"

MotiveRelationResult motive_relation_apply_augment(const MotiveSketch *in, const MotiveRelationStep *step,
                                          MotiveSketch *out) {
    return motive_relation_scale_time(in, step, out, true);
}

MotiveRelationResult motive_relation_apply_diminish(const MotiveSketch *in, const MotiveRelationStep *step,
                                           MotiveSketch *out) {
    return motive_relation_scale_time(in, step, out, false);
}

/* --- rhythm (ch. III, Exs. 17, 18, 22 to 24) ------------------------------------------------- */

MotiveRelationResult motive_relation_apply_note_repeat(const MotiveSketch *in, const MotiveRelationStep *step,
                                              MotiveSketch *out) {
    long note = step->value[0];
    long count = step->value[1];
    if (note < 0 || (size_t)note >= in->note_count || count < 2 || count > 4) {
        return MOTIVE_RELATION_INVALID;
    }
    if (in->note_count + (size_t)count - 1 > MOTIVE_SKETCH_MAX_NOTES) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    Rational part = msk_div(in->notes[note].duration, msk_int((unsigned long long)count));
    if (!motive_sketch_time_writable(in, part) || !motive_sketch_time_on_grid(in, part)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    size_t at = 0;
    for (size_t i = 0; i < in->note_count; i++) {
        if (i != (size_t)note) {
            out->notes[at++] = in->notes[i];
            continue;
        }
        for (long k = 0; k < count; k++) {
            out->notes[at].pitch = in->notes[i].pitch;
            out->notes[at].duration = part;
            out->notes[at].onset = msk_add(in->notes[i].onset, msk_mul(part, msk_int((unsigned long long)k)));
            at++;
        }
    }
    out->note_count = at;
    for (size_t f = 0; f < in->feature_count; f++) {
        if (in->features[f].first > (size_t)note) {
            out->features[f].first += (size_t)count - 1;
        }
        if (in->features[f].last >= (size_t)note) {
            out->features[f].last += (size_t)count - 1;
        }
    }
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_rhythm_repeat(const MotiveSketch *in, const MotiveRelationStep *step,
                                                MotiveSketch *out) {
    long first = step->value[0];
    long length = step->value[1];
    long target = step->value[2];
    size_t total = in->note_count;
    if (first < 0 || length < 1 || target < 0 || (size_t)(first + length) > total ||
        (size_t)(target + length) > total) {
        return MOTIVE_RELATION_INVALID;
    }
    Rational at = in->notes[target].onset;
    Rational old_end = motive_relation_note_end(&in->notes[target + length - 1]);
    for (long k = 0; k < length; k++) {
        out->notes[target + k].duration = in->notes[first + k].duration;
        out->notes[target + k].onset = at;
        at = msk_add(at, in->notes[first + k].duration);
    }
    /* The notes after the run keep their distance from it. */
    for (size_t i = (size_t)(target + length); i < total; i++) {
        Rational gap = msk_sub(in->notes[i].onset, old_end);
        out->notes[i].onset = msk_add(at, gap);
        old_end = motive_relation_note_end(&in->notes[i]);
        at = motive_relation_note_end(&out->notes[i]);
    }
    for (size_t i = 0; i < total; i++) {
        if (!motive_sketch_time_writable(in, out->notes[i].duration) ||
            !motive_sketch_time_on_grid(in, out->notes[i].onset)) {
            return MOTIVE_RELATION_NOT_EXPRESSIBLE;
        }
    }
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_rhythm_shift(const MotiveSketch *in, const MotiveRelationStep *step,
                                               MotiveSketch *out) {
    if (step->value[0] <= 0 || step->den[0] <= 0) {
        return MOTIVE_RELATION_INVALID;
    }
    Rational offset = msk_make((unsigned long long)step->value[0], (unsigned long long)step->den[0]);
    if (!motive_sketch_time_on_grid(in, offset)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    Rational inside = msk_mod(offset, in->bar);
    if (msk_is_zero(inside)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    /* The motive starts later in the bar, so its first downbeat comes sooner. */
    out->pickup = msk_mod(msk_add(in->pickup, msk_sub(in->bar, inside)), in->bar);
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_add_upbeat(const MotiveSketch *in, const MotiveRelationStep *step,
                                             MotiveSketch *out) {
    long count = step->value[0];
    long approach = step->value[1];
    if (count < 1 || count > 3 || (approach != 1 && approach != -1)) {
        return MOTIVE_RELATION_INVALID;
    }
    if (in->note_count + (size_t)count > MOTIVE_SKETCH_MAX_NOTES) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    Rational part = msk_div(in->beat, msk_int(2));
    Rational total = msk_mul(part, msk_int((unsigned long long)count));
    Rational pickup = msk_add(in->pickup, total);
    if (!motive_sketch_time_writable(in, part) || !motive_sketch_time_on_grid(in, part)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    if (!msk_ok(pickup) || msk_cmp(pickup, in->bar) >= 0) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    bool known = in->notes[0].pitch != MOTIVE_SKETCH_PITCH_UNKNOWN;
    long into = known ? motive_sketch_step(in, in->notes[0].pitch, NULL) : 0;
    for (long k = 0; k < count; k++) {
        MotiveSketchNote *note = &out->notes[k];
        note->pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
        if (known) {
            long pitch = motive_sketch_pitch_at_step(in, into - approach * (count - k), 0);
            if (!motive_relation_midi(pitch)) {
                return MOTIVE_RELATION_RANGE;
            }
            note->pitch = (int)pitch;
        }
        note->duration = part;
        note->onset = msk_mul(part, msk_int((unsigned long long)k));
    }
    for (size_t i = 0; i < in->note_count; i++) {
        out->notes[(size_t)count + i] = in->notes[i];
        out->notes[(size_t)count + i].onset = msk_add(in->notes[i].onset, total);
    }
    out->note_count = in->note_count + (size_t)count;
    for (size_t f = 0; f < in->feature_count; f++) {
        out->features[f].first += (size_t)count;
        out->features[f].last += (size_t)count;
    }
    /* The first chord keeps covering the upbeat. */
    for (size_t c = 0; c < in->chord_count; c++) {
        out->chords[c].onset = msk_is_zero(in->chords[c].onset) ? in->chords[c].onset
                                                                : msk_add(in->chords[c].onset, total);
    }
    out->pickup = pickup;
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_change_meter(const MotiveSketch *in, const MotiveRelationStep *step,
                                               MotiveSketch *out) {
    long num = step->value[0];
    long den = step->value[1];
    if (num < 1 || num > 64 || (den != 1 && den != 2 && den != 4 && den != 8 && den != 16)) {
        return MOTIVE_RELATION_INVALID;
    }
    Rational bar = msk_make((unsigned long long)num, (unsigned long long)den);
    Rational factor = msk_div(bar, in->bar);
    out->bar = bar;
    out->beat = msk_make(1, (unsigned long long)den);
    for (size_t i = 0; i < in->note_count; i++) {
        out->notes[i].onset = msk_mul(in->notes[i].onset, factor);
        out->notes[i].duration = msk_mul(in->notes[i].duration, factor);
        if (!motive_sketch_time_writable(in, out->notes[i].duration) ||
            !motive_sketch_time_on_grid(in, out->notes[i].onset)) {
            return MOTIVE_RELATION_NOT_EXPRESSIBLE;
        }
    }
    for (size_t c = 0; c < in->chord_count; c++) {
        out->chords[c].onset = msk_mul(in->chords[c].onset, factor);
    }
    out->pickup = msk_mul(in->pickup, factor);
    return MOTIVE_RELATION_OK;
}
