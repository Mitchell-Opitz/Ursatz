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

MotiveRelationResult motive_relation_apply_add_interval(const MotiveSketch *in, const MotiveRelationStep *step,
                                               MotiveSketch *out) {
    long interval = step->value[0];
    long steps = step->value[1];
    if (interval < 1 || (size_t)interval >= in->note_count || steps == 0) {
        return MOTIVE_RELATION_INVALID;
    }
    if (!motive_relation_pitched(in)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    size_t before = (size_t)interval - 1;
    int alteration;
    long at = motive_sketch_step(in, in->notes[before].pitch, &alteration);
    long pitch = motive_sketch_pitch_at_step(in, at + steps, alteration);
    if (!motive_relation_midi(pitch)) {
        return MOTIVE_RELATION_RANGE;
    }
    Rational half = msk_div(in->notes[before].duration, msk_int(2));
    if (!motive_relation_nice_time(in, half)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    MotiveSketchNote added;
    added.pitch = (int)pitch;
    added.onset = msk_add(in->notes[before].onset, half);
    added.duration = half;
    if (!motive_relation_insert_notes(in, out, (size_t)interval, &added, 1)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    out->notes[before].duration = half;
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_omit_interval(const MotiveSketch *in, const MotiveRelationStep *step,
                                                MotiveSketch *out) {
    long interval = step->value[0];
    if (interval < 1 || (size_t)interval >= in->note_count) {
        return MOTIVE_RELATION_INVALID;
    }
    if (in->note_count < 3) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    size_t gone = (size_t)interval;
    Rational lengthened = msk_sub(motive_relation_note_end(&in->notes[gone]), in->notes[gone - 1].onset);
    if (!motive_relation_nice_time(in, lengthened)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    for (size_t i = gone; i + 1 < in->note_count; i++) {
        out->notes[i] = in->notes[i + 1];
    }
    out->note_count = in->note_count - 1;
    out->notes[gone - 1].duration = lengthened;
    size_t kept = 0;
    for (size_t f = 0; f < in->feature_count; f++) {
        MotiveSketchFeature feature = in->features[f];
        if (feature.first == gone && feature.last == gone) {
            continue;
        }
        if (feature.first > gone) {
            feature.first--;
        }
        if (feature.last >= gone) {
            feature.last--;
        }
        out->features[kept++] = feature;
    }
    out->feature_count = kept;
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_ancillary_fill(const MotiveSketch *in, const MotiveRelationStep *step,
                                                 MotiveSketch *out) {
    long interval = step->value[0];
    long kind = step->value[1];
    if (interval < 1 || (size_t)interval >= in->note_count || (kind != 0 && kind != 1)) {
        return MOTIVE_RELATION_INVALID;
    }
    if (!motive_relation_pitched(in)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    size_t before = (size_t)interval - 1;
    int alteration;
    long from = motive_sketch_step(in, in->notes[before].pitch, &alteration);
    long to = motive_sketch_step(in, in->notes[interval].pitch, NULL);
    long distance = to - from;
    long direction = distance < 0 ? -1 : 1;
    long count = kind == 1 ? 1 : (distance < 0 ? -distance : distance) - 1;
    if (count < 1 || count > 6) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    /* The added notes share the second half of the first note. */
    Rational half = msk_div(in->notes[before].duration, msk_int(2));
    Rational part = msk_div(half, msk_int((unsigned long long)count));
    if (!motive_relation_nice_time(in, half) || !motive_relation_nice_time(in, part)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    MotiveSketchNote added[6];
    for (long k = 0; k < count; k++) {
        long pitch = motive_sketch_pitch_at_step(in, from + direction * (k + 1), kind == 1 ? alteration : 0);
        if (!motive_relation_midi(pitch)) {
            return MOTIVE_RELATION_RANGE;
        }
        added[k].pitch = (int)pitch;
        added[k].duration = part;
        added[k].onset = msk_add(msk_add(in->notes[before].onset, half), msk_mul(part, msk_int((unsigned long long)k)));
    }
    if (!motive_relation_insert_notes(in, out, (size_t)interval, added, (size_t)count)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    out->notes[before].duration = half;
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_condense(const MotiveSketch *in, const MotiveRelationStep *step,
                                           MotiveSketch *out) {
    long first = step->value[0];
    long last = step->value[1];
    long num = step->value[2];
    long den = step->den[2];
    if (first < 0 || last < first || (size_t)last >= in->note_count || num <= 0 || den <= 0 || num >= den) {
        return MOTIVE_RELATION_INVALID;
    }
    Rational ratio = msk_make((unsigned long long)num, (unsigned long long)den);
    Rational start = in->notes[first].onset;
    Rational old_span = msk_sub(motive_relation_note_end(&in->notes[last]), start);
    Rational saved = msk_sub(old_span, msk_mul(old_span, ratio));
    for (long i = first; i <= last; i++) {
        out->notes[i].onset = msk_add(start, msk_mul(msk_sub(in->notes[i].onset, start), ratio));
        out->notes[i].duration = msk_mul(in->notes[i].duration, ratio);
    }
    for (size_t i = (size_t)last + 1; i < in->note_count; i++) {
        out->notes[i].onset = msk_sub(in->notes[i].onset, saved);
    }
    for (size_t i = 0; i < in->note_count; i++) {
        if (!motive_relation_nice_time(in, out->notes[i].duration) || !motive_sketch_time_on_grid(in, out->notes[i].onset)) {
            return MOTIVE_RELATION_NOT_EXPRESSIBLE;
        }
    }
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_repeat_feature(const MotiveSketch *in, const MotiveRelationStep *step,
                                                 MotiveSketch *out) {
    long which = step->value[0];
    long times = step->value[1];
    long steps = step->value[2];
    if (times < 1 || times > 2) {
        return MOTIVE_RELATION_INVALID;
    }
    if (which < 0 || (size_t)which >= in->feature_count) {
        return MOTIVE_RELATION_NO_FEATURE;
    }
    size_t first = in->features[which].first;
    size_t last = in->features[which].last;
    size_t length = last - first + 1;
    size_t added = length * (size_t)times;
    if (in->note_count + added > MOTIVE_SKETCH_MAX_NOTES) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    Rational from = motive_relation_note_end(&in->notes[last]);
    Rational span = msk_sub(from, in->notes[first].onset);
    Rational shift = msk_mul(span, msk_int((unsigned long long)times));
    for (size_t i = last + 1; i < in->note_count; i++) {
        out->notes[i + added] = in->notes[i];
        out->notes[i + added].onset = msk_add(in->notes[i].onset, shift);
    }
    for (long t = 1; t <= times; t++) {
        for (size_t k = 0; k < length; k++) {
            MotiveSketchNote note = in->notes[first + k];
            if (note.pitch != MOTIVE_SKETCH_PITCH_UNKNOWN) {
                int alteration;
                long at = motive_sketch_step(in, note.pitch, &alteration);
                long pitch = motive_sketch_pitch_at_step(in, at + t * steps, alteration);
                if (!motive_relation_midi(pitch)) {
                    return MOTIVE_RELATION_RANGE;
                }
                note.pitch = (int)pitch;
            }
            note.onset = msk_add(note.onset, msk_mul(span, msk_int((unsigned long long)t)));
            out->notes[last + 1 + (size_t)(t - 1) * length + k] = note;
        }
    }
    out->note_count = in->note_count + added;
    for (size_t f = 0; f < in->feature_count; f++) {
        if (in->features[f].first > last) {
            out->features[f].first += added;
        }
        if (in->features[f].last > last) {
            out->features[f].last += added;
        }
    }
    for (size_t c = 0; c < in->chord_count; c++) {
        if (msk_cmp(in->chords[c].onset, from) >= 0) {
            out->chords[c].onset = msk_add(in->chords[c].onset, shift);
        }
    }
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_shift_feature(const MotiveSketch *in, const MotiveRelationStep *step,
                                                MotiveSketch *out) {
    long which = step->value[0];
    if (step->value[1] <= 0 || step->den[1] <= 0) {
        return MOTIVE_RELATION_INVALID;
    }
    if (which < 0 || (size_t)which >= in->feature_count) {
        return MOTIVE_RELATION_NO_FEATURE;
    }
    Rational offset = msk_make((unsigned long long)step->value[1], (unsigned long long)step->den[1]);
    size_t first = in->features[which].first;
    size_t last = in->features[which].last;
    if (msk_cmp(offset, in->notes[last].duration) >= 0) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    for (size_t i = first; i <= last; i++) {
        out->notes[i].onset = msk_add(in->notes[i].onset, offset);
    }
    out->notes[last].duration = msk_sub(in->notes[last].duration, offset);
    if (first > 0) {
        out->notes[first - 1].duration = msk_add(in->notes[first - 1].duration, offset);
    }
    for (size_t i = (first > 0 ? first - 1 : first); i <= last; i++) {
        if (!motive_relation_nice_time(in, out->notes[i].duration) || !motive_sketch_time_on_grid(in, out->notes[i].onset)) {
            return MOTIVE_RELATION_NOT_EXPRESSIBLE;
        }
    }
    return MOTIVE_RELATION_OK;
}
