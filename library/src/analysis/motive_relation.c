#include "analysis/motive_relation.h"

#include "analysis/motive_chord_internal.h"
#include "analysis/motive_tone_roles.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef MotiveRelationResult (*ApplyFn)(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);

typedef struct {
    const char *name;
    size_t params;
    int cost;
    ApplyFn apply;
} Entry;

/* --- helpers ------------------------------------------------------------------ */

static bool pitched(const MotiveSketch *sketch) {
    for (size_t i = 0; i < sketch->note_count; i++) {
        if (sketch->notes[i].pitch == MOTIVE_SKETCH_PITCH_UNKNOWN) {
            return false;
        }
    }
    return true;
}

static ExactRational note_end(const MotiveSketchNote *note) {
    return msk_add(note->onset, note->duration);
}

/* A sketch is as long as its notes; a chord at or after the end goes. */
static void settle(MotiveSketch *sketch) {
    if (sketch->note_count == 0) {
        return;
    }
    sketch->length = note_end(&sketch->notes[sketch->note_count - 1]);
    size_t kept = 0;
    for (size_t i = 0; i < sketch->chord_count; i++) {
        if (msk_ok(sketch->length) && msk_cmp(sketch->chords[i].onset, sketch->length) < 0) {
            sketch->chords[kept++] = sketch->chords[i];
        }
    }
    sketch->chord_count = kept;
}

/*
 * The chords after a change that moved the kept notes from old_onset[j] to new_onset[j]: a chord goes to the first
 * kept note at or after it, and of several chords that land on one note the last, the one in force, stays.
 */
static void remap_chords(const MotiveSketch *in, MotiveSketch *out, const ExactRational *old_onset,
                         const ExactRational *new_onset, size_t kept) {
    out->chord_count = 0;
    for (size_t c = 0; c < in->chord_count; c++) {
        size_t j = 0;
        while (j < kept && msk_cmp(old_onset[j], in->chords[c].onset) < 0) {
            j++;
        }
        if (j == kept) {
            continue;
        }
        MotiveSketchChord chord = in->chords[c];
        chord.onset = new_onset[j];
        if (out->chord_count > 0 && msk_eq(out->chords[out->chord_count - 1].onset, chord.onset)) {
            out->chords[out->chord_count - 1] = chord;
        } else {
            out->chords[out->chord_count++] = chord;
        }
    }
}

/* --- exact ----------------------------------------------------------------------- */

static MotiveRelationResult apply_exact(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out) {
    (void)in;
    (void)step;
    (void)out;
    return MOTIVE_RELATION_OK;
}

/* --- transpose, invert -------------------------------------------------------------- */

static bool midi(long pitch) {
    return pitch >= 0 && pitch <= 127;
}

static MotiveRelationResult apply_transpose(const MotiveSketch *in, const MotiveRelationStep *step,
                                            MotiveSketch *out) {
    bool tonal = step->value[0] != 0;
    long by = step->value[1];
    long degree = step->value[2];
    if (!pitched(in)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    if (degree != 0) {
        if (degree < 1 || degree > 7) {
            return MOTIVE_RELATION_INVALID;
        }
        if (!in->has_scale) {
            return MOTIVE_RELATION_NO_SCALE;
        }
        /* The nearest way to the degree, up to three steps either way. */
        long first = motive_sketch_degree_of_step(in, motive_sketch_step(in, in->notes[0].pitch, NULL));
        long steps = (degree - first + 7) % 7;
        tonal = true;
        by = steps > 3 ? steps - 7 : steps;
    }
    if (tonal && !in->has_scale) {
        return MOTIVE_RELATION_NO_SCALE;
    }
    for (size_t i = 0; i < in->note_count; i++) {
        long pitch;
        if (tonal) {
            int alteration;
            long at = motive_sketch_step(in, in->notes[i].pitch, &alteration);
            pitch = motive_sketch_pitch_at_step(in, at + by, alteration);
        } else {
            pitch = in->notes[i].pitch + by;
        }
        if (!midi(pitch)) {
            return MOTIVE_RELATION_RANGE;
        }
        out->notes[i].pitch = (int)pitch;
    }
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_invert(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out) {
    long axis = step->value[0];
    bool tonal = step->value[1] != 0;
    if (!pitched(in)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    if (axis != -1 && !midi(axis)) {
        return MOTIVE_RELATION_INVALID;
    }
    if (tonal && !in->has_scale) {
        return MOTIVE_RELATION_NO_SCALE;
    }
    if (axis == -1) {
        axis = in->notes[0].pitch;
    }
    long axis_step = motive_sketch_step(in, (int)axis, NULL);
    for (size_t i = 0; i < in->note_count; i++) {
        long pitch;
        if (tonal) {
            int alteration;
            long at = motive_sketch_step(in, in->notes[i].pitch, &alteration);
            pitch = motive_sketch_pitch_at_step(in, 2 * axis_step - at, alteration);
        } else {
            pitch = 2 * axis - in->notes[i].pitch;
        }
        if (!midi(pitch)) {
            return MOTIVE_RELATION_RANGE;
        }
        out->notes[i].pitch = (int)pitch;
    }
    return MOTIVE_RELATION_OK;
}

/* --- retrograde ------------------------------------------------------------------------ */

static MotiveRelationResult apply_retrograde(const MotiveSketch *in, const MotiveRelationStep *step,
                                             MotiveSketch *out) {
    (void)step;
    size_t count = in->note_count;
    /* Each note moves to the mirror of its span about the middle of the notes; the order reverses. */
    ExactRational span = msk_add(in->notes[0].onset, note_end(&in->notes[count - 1]));
    for (size_t i = 0; i < count; i++) {
        const MotiveSketchNote *note = &in->notes[count - 1 - i];
        out->notes[i].pitch = note->pitch;
        out->notes[i].duration = note->duration;
        out->notes[i].onset = msk_sub(span, note_end(note));
    }
    for (size_t f = 0; f < in->feature_count; f++) {
        out->features[f].first = count - 1 - in->features[f].last;
        out->features[f].last = count - 1 - in->features[f].first;
    }
    return MOTIVE_RELATION_OK;
}

/* --- interval change ------------------------------------------------------------------- */

static int direction_of(long semitones) {
    return semitones > 0 ? 1 : (semitones < 0 ? -1 : 0);
}

static MotiveRelationResult apply_interval_change(const MotiveSketch *in, const MotiveRelationStep *step,
                                                  MotiveSketch *out) {
    long which = step->value[0];
    long amount = step->value[1];
    bool keep_contour = step->value[2] != 0;
    size_t count = in->note_count;
    if (!pitched(in) || count < 2) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    if (which < 0 || (size_t)which > count - 1) {
        return MOTIVE_RELATION_INVALID;
    }
    /* A repeated note has no direction to widen in, so its interval is left alone. */
    long offset = 0;
    for (size_t i = 1; i < count; i++) {
        long moved = in->notes[i].pitch - in->notes[i - 1].pitch;
        if ((which == 0 || (size_t)which == i) && moved != 0) {
            offset += moved > 0 ? amount : -amount;
        }
        long pitch = in->notes[i].pitch + offset;
        if (!midi(pitch)) {
            return MOTIVE_RELATION_RANGE;
        }
        out->notes[i].pitch = (int)pitch;
    }
    if (keep_contour) {
        for (size_t i = 1; i < count; i++) {
            if (direction_of(in->notes[i].pitch - in->notes[i - 1].pitch) !=
                direction_of(out->notes[i].pitch - out->notes[i - 1].pitch)) {
                return MOTIVE_RELATION_NOT_APPLICABLE;
            }
        }
    }
    return MOTIVE_RELATION_OK;
}

/* --- fragment, liquidate ---------------------------------------------------------------- */

static size_t minimum_length(long min) {
    return min > 0 ? (size_t)min : 2;
}

static MotiveRelationResult apply_fragment(const MotiveSketch *in, const MotiveRelationStep *step,
                                           MotiveSketch *out) {
    long position = step->value[0];
    size_t total = in->note_count;
    if (step->value[1] < 0) {
        return MOTIVE_RELATION_INVALID;
    }
    size_t length = (size_t)step->value[1];
    if (length < minimum_length(step->value[2]) || length >= total) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    size_t first;
    switch (position) {
    case 1:
        first = 0;
        break;
    case 2:
        first = total - length;
        break;
    case 3:
        first = (total - length) / 2;
        break;
    default:
        return MOTIVE_RELATION_INVALID;
    }
    ExactRational start = in->notes[first].onset;
    ExactRational old_onset[MOTIVE_SKETCH_MAX_NOTES];
    ExactRational new_onset[MOTIVE_SKETCH_MAX_NOTES];
    for (size_t i = 0; i < length; i++) {
        out->notes[i] = in->notes[first + i];
        old_onset[i] = in->notes[first + i].onset;
        new_onset[i] = msk_sub(old_onset[i], start);
        out->notes[i].onset = new_onset[i];
    }
    out->note_count = length;
    out->feature_count = 0;
    for (size_t f = 0; f < in->feature_count; f++) {
        if (in->features[f].first >= first && in->features[f].last < first + length) {
            out->features[out->feature_count] = in->features[f];
            out->features[out->feature_count].first -= first;
            out->features[out->feature_count].last -= first;
            out->feature_count++;
        }
    }
    remap_chords(in, out, old_onset, new_onset, length);
    return MOTIVE_RELATION_OK;
}

static bool nice_time(ExactRational duration);

static MotiveRelationResult apply_liquidate(const MotiveSketch *in, const MotiveRelationStep *step,
                                            MotiveSketch *out) {
    size_t total = in->note_count;
    bool keep_rhythm = step->value[1] != 0;
    if (step->value[0] <= 0) {
        return MOTIVE_RELATION_INVALID;
    }
    size_t drop = (size_t)step->value[0];
    if (drop >= total || total - drop < minimum_length(step->value[2])) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    /* The weakest note goes first; of two equals, the later. */
    bool dropped[MOTIVE_SKETCH_MAX_NOTES] = {false};
    int strength[MOTIVE_SKETCH_MAX_NOTES];
    for (size_t i = 0; i < total; i++) {
        strength[i] = motive_sketch_strength(in, in->notes[i].onset);
    }
    for (size_t round = 0; round < drop; round++) {
        size_t weakest = total;
        for (size_t i = 0; i < total; i++) {
            if (!dropped[i] && (weakest == total || strength[i] <= strength[weakest])) {
                weakest = i;
            }
        }
        dropped[weakest] = true;
    }
    size_t kept = 0;
    size_t new_index[MOTIVE_SKETCH_MAX_NOTES];
    ExactRational old_onset[MOTIVE_SKETCH_MAX_NOTES];
    ExactRational new_onset[MOTIVE_SKETCH_MAX_NOTES];
    ExactRational at = msk_int(0);
    for (size_t i = 0; i < total; i++) {
        new_index[i] = total;
        if (dropped[i]) {
            continue;
        }
        out->notes[kept] = in->notes[i];
        old_onset[kept] = in->notes[i].onset;
        if (!keep_rhythm) {
            out->notes[kept].onset = at;
            at = msk_add(at, in->notes[i].duration);
        }
        new_onset[kept] = out->notes[kept].onset;
        new_index[i] = kept++;
    }
    out->note_count = kept;
    if (keep_rhythm) {
        /* A dropped note's time goes to the kept note before it; a dropped first note's, to the first kept note. */
        ExactRational end = note_end(&in->notes[total - 1]);
        for (size_t k = kept; k-- > 0;) {
            if (k == 0) {
                out->notes[0].onset = in->notes[0].onset;
            }
            out->notes[k].duration = msk_sub(end, out->notes[k].onset);
            if (!nice_time(out->notes[k].duration)) {
                return MOTIVE_RELATION_NOT_EXPRESSIBLE;
            }
            end = out->notes[k].onset;
        }
    }
    out->feature_count = 0;
    for (size_t f = 0; f < in->feature_count; f++) {
        bool whole = true;
        for (size_t i = in->features[f].first; i <= in->features[f].last; i++) {
            whole = whole && !dropped[i];
        }
        if (whole) {
            out->features[out->feature_count] = in->features[f];
            out->features[out->feature_count].first = new_index[in->features[f].first];
            out->features[out->feature_count].last = new_index[in->features[f].last];
            out->feature_count++;
        }
    }
    if (!keep_rhythm) {
        remap_chords(in, out, old_onset, new_onset, kept);
    }
    return MOTIVE_RELATION_OK;
}

/* --- augment, diminish ---------------------------------------------------------------------- */

static bool whole_beats(const MotiveSketch *sketch, ExactRational time) {
    ExactRational beats = msk_div(time, sketch->beat);
    return msk_ok(beats) && beats.d == 1;
}

static MotiveRelationResult scale_time(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out,
                                       bool augment) {
    long num = step->value[0];
    long den = step->den[0];
    if (num == 0 && den == 0) {
        num = augment ? 2 : 1;
        den = augment ? 1 : 2;
    }
    if (num <= 0 || den <= 0 || (augment && num <= den) || (!augment && num >= den)) {
        return MOTIVE_RELATION_INVALID;
    }
    ExactRational factor = msk_make((unsigned long long)num, (unsigned long long)den);
    for (size_t i = 0; i < in->note_count; i++) {
        out->notes[i].onset = msk_mul(in->notes[i].onset, factor);
        out->notes[i].duration = msk_mul(in->notes[i].duration, factor);
        if (!motive_sketch_expressible(out->notes[i].duration) || !motive_sketch_on_grid(out->notes[i].onset)) {
            return MOTIVE_RELATION_NOT_EXPRESSIBLE;
        }
    }
    for (size_t c = 0; c < in->chord_count; c++) {
        out->chords[c].onset = msk_mul(in->chords[c].onset, factor);
    }
    out->pickup = msk_mul(in->pickup, factor);
    if (!msk_ok(out->pickup) || msk_cmp(out->pickup, in->bar) >= 0) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    /* A source of whole beats stays one. */
    if (whole_beats(in, in->length) && !whole_beats(in, note_end(&out->notes[in->note_count - 1]))) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_augment(const MotiveSketch *in, const MotiveRelationStep *step,
                                          MotiveSketch *out) {
    return scale_time(in, step, out, true);
}

static MotiveRelationResult apply_diminish(const MotiveSketch *in, const MotiveRelationStep *step,
                                           MotiveSketch *out) {
    return scale_time(in, step, out, false);
}

/* --- rhythm (ch. III, Exs. 17, 18, 22 to 24) ------------------------------------------------- */

static MotiveRelationResult apply_note_repeat(const MotiveSketch *in, const MotiveRelationStep *step,
                                              MotiveSketch *out) {
    long note = step->value[0];
    long count = step->value[1];
    if (note < 0 || (size_t)note >= in->note_count || count < 2 || count > 4) {
        return MOTIVE_RELATION_INVALID;
    }
    if (in->note_count + (size_t)count - 1 > MOTIVE_SKETCH_MAX_NOTES) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    ExactRational part = msk_div(in->notes[note].duration, msk_int((unsigned long long)count));
    if (!motive_sketch_expressible(part) || !motive_sketch_on_grid(part)) {
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

static MotiveRelationResult apply_rhythm_repeat(const MotiveSketch *in, const MotiveRelationStep *step,
                                                MotiveSketch *out) {
    long first = step->value[0];
    long length = step->value[1];
    long target = step->value[2];
    size_t total = in->note_count;
    if (first < 0 || length < 1 || target < 0 || (size_t)(first + length) > total ||
        (size_t)(target + length) > total) {
        return MOTIVE_RELATION_INVALID;
    }
    ExactRational at = in->notes[target].onset;
    ExactRational old_end = note_end(&in->notes[target + length - 1]);
    for (long k = 0; k < length; k++) {
        out->notes[target + k].duration = in->notes[first + k].duration;
        out->notes[target + k].onset = at;
        at = msk_add(at, in->notes[first + k].duration);
    }
    /* The notes after the run keep their distance from it. */
    for (size_t i = (size_t)(target + length); i < total; i++) {
        ExactRational gap = msk_sub(in->notes[i].onset, old_end);
        out->notes[i].onset = msk_add(at, gap);
        old_end = note_end(&in->notes[i]);
        at = note_end(&out->notes[i]);
    }
    for (size_t i = 0; i < total; i++) {
        if (!motive_sketch_expressible(out->notes[i].duration) || !motive_sketch_on_grid(out->notes[i].onset)) {
            return MOTIVE_RELATION_NOT_EXPRESSIBLE;
        }
    }
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_rhythm_shift(const MotiveSketch *in, const MotiveRelationStep *step,
                                               MotiveSketch *out) {
    if (step->value[0] <= 0 || step->den[0] <= 0) {
        return MOTIVE_RELATION_INVALID;
    }
    ExactRational offset = msk_make((unsigned long long)step->value[0], (unsigned long long)step->den[0]);
    if (!motive_sketch_on_grid(offset)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    ExactRational inside = msk_mod(offset, in->bar);
    if (msk_is_zero(inside)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    /* The motive starts later in the bar, so its first downbeat comes sooner. */
    out->pickup = msk_mod(msk_add(in->pickup, msk_sub(in->bar, inside)), in->bar);
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_add_upbeat(const MotiveSketch *in, const MotiveRelationStep *step,
                                             MotiveSketch *out) {
    long count = step->value[0];
    long approach = step->value[1];
    if (count < 1 || count > 3 || (approach != 1 && approach != -1)) {
        return MOTIVE_RELATION_INVALID;
    }
    if (in->note_count + (size_t)count > MOTIVE_SKETCH_MAX_NOTES) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    ExactRational part = msk_div(in->beat, msk_int(2));
    ExactRational total = msk_mul(part, msk_int((unsigned long long)count));
    ExactRational pickup = msk_add(in->pickup, total);
    if (!motive_sketch_expressible(part) || !motive_sketch_on_grid(part)) {
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
            if (!midi(pitch)) {
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

static MotiveRelationResult apply_change_meter(const MotiveSketch *in, const MotiveRelationStep *step,
                                               MotiveSketch *out) {
    long num = step->value[0];
    long den = step->value[1];
    if (num < 1 || num > 64 || (den != 1 && den != 2 && den != 4 && den != 8 && den != 16)) {
        return MOTIVE_RELATION_INVALID;
    }
    ExactRational bar = msk_make((unsigned long long)num, (unsigned long long)den);
    ExactRational factor = msk_div(bar, in->bar);
    out->bar = bar;
    out->beat = msk_make(1, (unsigned long long)den);
    for (size_t i = 0; i < in->note_count; i++) {
        out->notes[i].onset = msk_mul(in->notes[i].onset, factor);
        out->notes[i].duration = msk_mul(in->notes[i].duration, factor);
        if (!motive_sketch_expressible(out->notes[i].duration) || !motive_sketch_on_grid(out->notes[i].onset)) {
            return MOTIVE_RELATION_NOT_EXPRESSIBLE;
        }
    }
    for (size_t c = 0; c < in->chord_count; c++) {
        out->chords[c].onset = msk_mul(in->chords[c].onset, factor);
    }
    out->pickup = msk_mul(in->pickup, factor);
    return MOTIVE_RELATION_OK;
}

/* --- intervals (Exs. 18 to 23, ch. VIII Ex. 52) ------------------------------------------------ */

/* Features after a note was put in at index at (a note put in after the first of a feature's notes is in it). */
static void features_after_insert(const MotiveSketch *in, MotiveSketch *out, size_t at, size_t added) {
    for (size_t f = 0; f < in->feature_count; f++) {
        if (in->features[f].first >= at) {
            out->features[f].first += added;
        }
        if (in->features[f].last >= at) {
            out->features[f].last += added;
        }
    }
}

/* The sketch with notes[0..at) kept, then the added notes, then notes[at..) as they are. */
static bool insert_notes(const MotiveSketch *in, MotiveSketch *out, size_t at, const MotiveSketchNote *added,
                         size_t count) {
    if (in->note_count + count > MOTIVE_SKETCH_MAX_NOTES) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        out->notes[at + i] = added[i];
    }
    for (size_t i = at; i < in->note_count; i++) {
        out->notes[i + count] = in->notes[i];
    }
    out->note_count = in->note_count + count;
    features_after_insert(in, out, at, count);
    return true;
}

static bool nice_time(ExactRational duration) {
    return motive_sketch_expressible(duration) && motive_sketch_on_grid(duration);
}

static MotiveRelationResult apply_add_interval(const MotiveSketch *in, const MotiveRelationStep *step,
                                               MotiveSketch *out) {
    long interval = step->value[0];
    long steps = step->value[1];
    if (interval < 1 || (size_t)interval >= in->note_count || steps == 0) {
        return MOTIVE_RELATION_INVALID;
    }
    if (!pitched(in)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    size_t before = (size_t)interval - 1;
    int alteration;
    long at = motive_sketch_step(in, in->notes[before].pitch, &alteration);
    long pitch = motive_sketch_pitch_at_step(in, at + steps, alteration);
    if (!midi(pitch)) {
        return MOTIVE_RELATION_RANGE;
    }
    ExactRational half = msk_div(in->notes[before].duration, msk_int(2));
    if (!nice_time(half)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    MotiveSketchNote added;
    added.pitch = (int)pitch;
    added.onset = msk_add(in->notes[before].onset, half);
    added.duration = half;
    if (!insert_notes(in, out, (size_t)interval, &added, 1)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    out->notes[before].duration = half;
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_omit_interval(const MotiveSketch *in, const MotiveRelationStep *step,
                                                MotiveSketch *out) {
    long interval = step->value[0];
    if (interval < 1 || (size_t)interval >= in->note_count) {
        return MOTIVE_RELATION_INVALID;
    }
    if (in->note_count < 3) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    size_t gone = (size_t)interval;
    ExactRational lengthened = msk_sub(note_end(&in->notes[gone]), in->notes[gone - 1].onset);
    if (!nice_time(lengthened)) {
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

static MotiveRelationResult apply_ancillary_fill(const MotiveSketch *in, const MotiveRelationStep *step,
                                                 MotiveSketch *out) {
    long interval = step->value[0];
    long kind = step->value[1];
    if (interval < 1 || (size_t)interval >= in->note_count || (kind != 0 && kind != 1)) {
        return MOTIVE_RELATION_INVALID;
    }
    if (!pitched(in)) {
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
    ExactRational half = msk_div(in->notes[before].duration, msk_int(2));
    ExactRational part = msk_div(half, msk_int((unsigned long long)count));
    if (!nice_time(half) || !nice_time(part)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    MotiveSketchNote added[6];
    for (long k = 0; k < count; k++) {
        long pitch = motive_sketch_pitch_at_step(in, from + direction * (k + 1), kind == 1 ? alteration : 0);
        if (!midi(pitch)) {
            return MOTIVE_RELATION_RANGE;
        }
        added[k].pitch = (int)pitch;
        added[k].duration = part;
        added[k].onset = msk_add(msk_add(in->notes[before].onset, half), msk_mul(part, msk_int((unsigned long long)k)));
    }
    if (!insert_notes(in, out, (size_t)interval, added, (size_t)count)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    out->notes[before].duration = half;
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_condense(const MotiveSketch *in, const MotiveRelationStep *step,
                                           MotiveSketch *out) {
    long first = step->value[0];
    long last = step->value[1];
    long num = step->value[2];
    long den = step->den[2];
    if (first < 0 || last < first || (size_t)last >= in->note_count || num <= 0 || den <= 0 || num >= den) {
        return MOTIVE_RELATION_INVALID;
    }
    ExactRational ratio = msk_make((unsigned long long)num, (unsigned long long)den);
    ExactRational start = in->notes[first].onset;
    ExactRational old_span = msk_sub(note_end(&in->notes[last]), start);
    ExactRational saved = msk_sub(old_span, msk_mul(old_span, ratio));
    for (long i = first; i <= last; i++) {
        out->notes[i].onset = msk_add(start, msk_mul(msk_sub(in->notes[i].onset, start), ratio));
        out->notes[i].duration = msk_mul(in->notes[i].duration, ratio);
    }
    for (size_t i = (size_t)last + 1; i < in->note_count; i++) {
        out->notes[i].onset = msk_sub(in->notes[i].onset, saved);
    }
    for (size_t i = 0; i < in->note_count; i++) {
        if (!nice_time(out->notes[i].duration) || !motive_sketch_on_grid(out->notes[i].onset)) {
            return MOTIVE_RELATION_NOT_EXPRESSIBLE;
        }
    }
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_repeat_feature(const MotiveSketch *in, const MotiveRelationStep *step,
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
    ExactRational from = note_end(&in->notes[last]);
    ExactRational span = msk_sub(from, in->notes[first].onset);
    ExactRational shift = msk_mul(span, msk_int((unsigned long long)times));
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
                if (!midi(pitch)) {
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

static MotiveRelationResult apply_shift_feature(const MotiveSketch *in, const MotiveRelationStep *step,
                                                MotiveSketch *out) {
    long which = step->value[0];
    if (step->value[1] <= 0 || step->den[1] <= 0) {
        return MOTIVE_RELATION_INVALID;
    }
    if (which < 0 || (size_t)which >= in->feature_count) {
        return MOTIVE_RELATION_NO_FEATURE;
    }
    ExactRational offset = msk_make((unsigned long long)step->value[1], (unsigned long long)step->den[1]);
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
        if (!nice_time(out->notes[i].duration) || !motive_sketch_on_grid(out->notes[i].onset)) {
            return MOTIVE_RELATION_NOT_EXPRESSIBLE;
        }
    }
    return MOTIVE_RELATION_OK;
}

/* --- harmony and melody adaptation (Exs. 25 to 29) ----------------------------------------------- */

/* The chord whose triad holds the most notes (then the most time), the first of I V IV vi ii iii vii on a tie. */
static bool implied_chord(const MotiveSketch *sketch, MotiveSketchChord *out) {
    static const int ORDER[7] = {1, 5, 4, 6, 2, 3, 7};
    size_t best_hits = 0;
    ExactRational best_time = msk_int(0);
    bool found = false;
    for (int k = 0; k < 7; k++) {
        MotiveSketchChord chord = {msk_int(0), ORDER[k], 0, 0, false};
        int pcs[4];
        size_t count = motive_sketch_chord_tones(sketch, &chord, pcs);
        size_t hits = 0;
        ExactRational time = msk_int(0);
        for (size_t i = 0; i < sketch->note_count; i++) {
            if (mch_has_pc(pcs, count, mch_pitch_class(sketch->notes[i].pitch))) {
                hits++;
                time = msk_add(time, sketch->notes[i].duration);
            }
        }
        if (hits > best_hits || (hits == best_hits && hits > 0 && msk_cmp(time, best_time) > 0)) {
            best_hits = hits;
            best_time = time;
            *out = chord;
            found = true;
        }
    }
    return found;
}

/* A harmony relation's start: a scale, pitches, and chords (the best implied chord at 0 when there are none). */
static MotiveRelationResult prepare_harmony(MotiveSketch *work) {
    if (!work->has_scale) {
        return MOTIVE_RELATION_NO_SCALE;
    }
    if (!pitched(work)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    if (work->chord_count == 0) {
        if (!implied_chord(work, &work->chords[0])) {
            return MOTIVE_RELATION_NO_HARMONY;
        }
        work->chord_count = 1;
    }
    return MOTIVE_RELATION_OK;
}

static int sign_of(long value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

/* Whether a note moved to pitch keeps the direction into it and out of it that the note had. */
static bool keeps_directions(const MotiveSketch *before, const MotiveSketch *sketch, size_t index, int pitch) {
    int was = before->notes[index].pitch;
    if (index > 0 && sign_of(pitch - sketch->notes[index - 1].pitch) != sign_of(was - before->notes[index - 1].pitch)) {
        return false;
    }
    if (index + 1 < sketch->note_count) {
        int next = before->notes[index + 1].pitch;
        if (sign_of(next - pitch) != sign_of(next - was)) {
            return false;
        }
    }
    return true;
}

/* The pitch of the new chord's tones nearest note i (within an octave), by the rules of adapt_melody; -1 for none. */
static int nearest_chord_pitch(const MotiveSketch *before, const MotiveSketch *sketch, size_t i, const int *pcs,
                               size_t count) {
    int pitch = sketch->notes[i].pitch;
    int preferred = i > 0 && before->notes[i].pitch < before->notes[i - 1].pitch ? -1 : 1;
    int nearest = -1;
    for (int distance = 1; distance <= 12; distance++) {
        for (int k = 0; k < 2; k++) {
            int candidate = pitch + (k == 0 ? preferred : -preferred) * distance;
            if (!midi(candidate) || !mch_has_pc(pcs, count, mch_pitch_class(candidate))) {
                continue;
            }
            if (nearest < 0) {
                nearest = candidate;
            }
            if (keeps_directions(before, sketch, i, candidate)) {
                return candidate;
            }
            if (distance > 6) {
                return nearest;
            }
        }
    }
    return nearest;
}

/*
 * The melody after the chords changed from those of before to those of sketch, through the tone roles of the melody
 * before the change (motive_tone_roles):
 *  - a note that was a chord tone and is not one of its new chord moves to the nearest tone of the new chord (within
 *    an octave). Of the tones at one distance, the one in the direction from the previous note comes first (up for
 *    the first note); the first of the nearest that keeps the contour's directions is taken, and if none within six
 *    semitones does, the nearest;
 *  - a non-chord tone whose resolution target moved moves by the same scale steps, so it still resolves the way it
 *    did. One whose figure no longer fits after that is left as it is (it is unclassified in the result).
 */
static void adapt_melody(const MotiveSketch *before, MotiveSketch *sketch) {
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    if (motive_tone_roles(before, NULL, 0, labels) != MOTIVE_TONE_OK) {
        return;
    }
    for (size_t i = 0; i < sketch->note_count; i++) {
        MotiveSketchNote *note = &sketch->notes[i];
        if (labels[i].role != MOTIVE_TONE_CHORD) {
            continue;
        }
        const MotiveSketchChord *old_chord = mch_chord_at(before, note->onset);
        const MotiveSketchChord *new_chord = mch_chord_at(sketch, note->onset);
        if (!new_chord || (old_chord && mch_same_tones(sketch, old_chord, new_chord))) {
            continue;
        }
        int pcs[4];
        size_t count = motive_sketch_chord_tones(sketch, new_chord, pcs);
        if (mch_has_pc(pcs, count, mch_pitch_class(note->pitch))) {
            continue;
        }
        int chosen = nearest_chord_pitch(before, sketch, i, pcs, count);
        if (chosen >= 0) {
            note->pitch = chosen;
        }
    }
    for (size_t i = 0; i < sketch->note_count; i++) {
        if (labels[i].role != MOTIVE_TONE_NON_CHORD || labels[i].resolves_to < 0) {
            continue;
        }
        /* The target's move in scale steps, less the move the note has already made (dominant-form transposes). */
        size_t target = (size_t)labels[i].resolves_to;
        long delta = motive_sketch_step(sketch, sketch->notes[target].pitch, NULL) -
                     motive_sketch_step(sketch, before->notes[target].pitch, NULL) -
                     (motive_sketch_step(sketch, sketch->notes[i].pitch, NULL) -
                      motive_sketch_step(sketch, before->notes[i].pitch, NULL));
        if (delta == 0) {
            continue;
        }
        int alteration;
        long at = motive_sketch_step(sketch, sketch->notes[i].pitch, &alteration);
        long pitch = motive_sketch_pitch_at_step(sketch, at + delta, alteration);
        if (midi(pitch)) {
            sketch->notes[i].pitch = (int)pitch;
        }
    }
}

/* The span of a chord and the note onset inside it nearest its middle; false when no note starts inside. */
static bool split_point(const MotiveSketch *sketch, size_t chord, ExactRational *at) {
    ExactRational start = sketch->chords[chord].onset;
    ExactRational end = chord + 1 < sketch->chord_count ? sketch->chords[chord + 1].onset : sketch->length;
    ExactRational middle = msk_add(start, msk_div(msk_sub(end, start), msk_int(2)));
    bool found = false;
    ExactRational best_gap = msk_int(0);
    for (size_t i = 0; i < sketch->note_count; i++) {
        ExactRational onset = sketch->notes[i].onset;
        if (msk_cmp(onset, start) <= 0 || msk_cmp(onset, end) >= 0) {
            continue;
        }
        ExactRational gap = msk_cmp(onset, middle) >= 0 ? msk_sub(onset, middle) : msk_sub(middle, onset);
        if (!found || msk_cmp(gap, best_gap) < 0) {
            best_gap = gap;
            *at = onset;
            found = true;
        }
    }
    return found;
}

static bool insert_chord(MotiveSketch *sketch, size_t at, MotiveSketchChord chord) {
    if (sketch->chord_count >= MOTIVE_SKETCH_MAX_CHORDS) {
        return false;
    }
    for (size_t c = sketch->chord_count; c > at; c--) {
        sketch->chords[c] = sketch->chords[c - 1];
    }
    sketch->chords[at] = chord;
    sketch->chord_count++;
    return true;
}

/*
 * The melody's bass on a chord after its inversion changed: the lowest note that starts inside the chord, when it was
 * the old bass chord tone, moves to the nearest pitch of the new bass chord tone (the lower one when two are equally
 * near).
 */
static void move_bass(MotiveSketch *sketch, size_t chord, int old_inversion) {
    const MotiveSketchChord *c = &sketch->chords[chord];
    int pcs[4];
    size_t count = motive_sketch_chord_tones(sketch, c, pcs);
    if (old_inversion < 0 || (size_t)old_inversion >= count || (size_t)c->inversion >= count) {
        return;
    }
    ExactRational start = c->onset;
    ExactRational end = chord + 1 < sketch->chord_count ? sketch->chords[chord + 1].onset : sketch->length;
    bool found = false;
    size_t lowest = 0;
    for (size_t i = 0; i < sketch->note_count; i++) {
        if (msk_cmp(sketch->notes[i].onset, start) < 0 || msk_cmp(sketch->notes[i].onset, end) >= 0) {
            continue;
        }
        if (!found || sketch->notes[i].pitch < sketch->notes[lowest].pitch) {
            lowest = i;
            found = true;
        }
    }
    int was = pcs[old_inversion];
    int now = pcs[c->inversion];
    if (!found || was == now || mch_pitch_class(sketch->notes[lowest].pitch) != was) {
        return;
    }
    int pitch = sketch->notes[lowest].pitch;
    for (int distance = 1; distance <= 12; distance++) {
        for (int k = 0; k < 2; k++) {
            int candidate = pitch + (k == 0 ? -distance : distance);
            if (midi(candidate) && mch_pitch_class(candidate) == now) {
                sketch->notes[lowest].pitch = candidate;
                return;
            }
        }
    }
}

static MotiveRelationResult apply_harmony_inversion(const MotiveSketch *in, const MotiveRelationStep *step,
                                                    MotiveSketch *out) {
    MotiveRelationResult ready = prepare_harmony(out);
    if (ready != MOTIVE_RELATION_OK) {
        return ready;
    }
    long chord = step->value[0];
    long inversion = step->value[1];
    if (inversion < 0 || inversion > 3) {
        return MOTIVE_RELATION_INVALID;
    }
    if (chord < 0 || (size_t)chord >= out->chord_count) {
        return MOTIVE_RELATION_NO_HARMONY;
    }
    if (inversion == 3 && !out->chords[chord].seventh) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    MotiveSketch before = *out;
    int old_inversion = out->chords[chord].inversion;
    out->chords[chord].inversion = (int)inversion;
    move_bass(out, (size_t)chord, old_inversion);
    adapt_melody(&before, out);
    (void)in;
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_harmony_append(const MotiveSketch *in, const MotiveRelationStep *step,
                                                 MotiveSketch *out) {
    (void)in;
    long degree = step->value[0];
    if (degree < 1 || degree > 7 || (step->value[1] != 0 && step->value[1] != 1)) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveRelationResult ready = prepare_harmony(out);
    if (ready != MOTIVE_RELATION_OK) {
        return ready;
    }
    ExactRational at = out->notes[out->note_count - 1].onset;
    if (msk_cmp(at, out->chords[out->chord_count - 1].onset) <= 0) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    MotiveSketch before = *out;
    MotiveSketchChord chord = {at, (int)degree, 0, 0, step->value[1] != 0};
    if (!insert_chord(out, out->chord_count, chord)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    adapt_melody(&before, out);
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_harmony_insert(const MotiveSketch *in, const MotiveRelationStep *step,
                                                 MotiveSketch *out) {
    (void)in;
    long chord = step->value[0];
    long degree = step->value[1];
    if (degree < 1 || degree > 7) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveRelationResult ready = prepare_harmony(out);
    if (ready != MOTIVE_RELATION_OK) {
        return ready;
    }
    if (chord < 0 || (size_t)chord >= out->chord_count) {
        return MOTIVE_RELATION_NO_HARMONY;
    }
    ExactRational at;
    if (!split_point(out, (size_t)chord, &at)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    MotiveSketch before = *out;
    MotiveSketchChord added = {at, (int)degree, 0, 0, false};
    if (!insert_chord(out, (size_t)chord + 1, added)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    adapt_melody(&before, out);
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_harmony_substitute(const MotiveSketch *in, const MotiveRelationStep *step,
                                                     MotiveSketch *out) {
    (void)in;
    long chord = step->value[0];
    long degree = step->value[1];
    long then = step->value[2];
    if (degree < 1 || degree > 7 || then < 0 || then > 7) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveRelationResult ready = prepare_harmony(out);
    if (ready != MOTIVE_RELATION_OK) {
        return ready;
    }
    if (chord < 0 || (size_t)chord >= out->chord_count) {
        return MOTIVE_RELATION_NO_HARMONY;
    }
    MotiveSketch before = *out;
    MotiveSketchChord replaced = out->chords[chord];
    replaced.degree = (int)degree;
    replaced.alteration = 0;
    replaced.inversion = 0;
    if (then != 0) {
        ExactRational at;
        if (!split_point(out, (size_t)chord, &at)) {
            return MOTIVE_RELATION_NOT_APPLICABLE;
        }
        MotiveSketchChord second = {at, (int)then, 0, 0, false};
        if (!insert_chord(out, (size_t)chord + 1, second)) {
            return MOTIVE_RELATION_NOT_APPLICABLE;
        }
    }
    out->chords[chord] = replaced;
    adapt_melody(&before, out);
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_passing_harmony(const MotiveSketch *in, const MotiveRelationStep *step,
                                                  MotiveSketch *out) {
    (void)in;
    long chord = step->value[0];
    MotiveRelationResult ready = prepare_harmony(out);
    if (ready != MOTIVE_RELATION_OK) {
        return ready;
    }
    if (chord < 0 || (size_t)chord + 1 >= out->chord_count) {
        return MOTIVE_RELATION_NO_HARMONY;
    }
    int from = out->chords[chord].degree;
    int to = out->chords[chord + 1].degree;
    if (from == to) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    /* The weak note before the change: the last note that starts inside the first chord. */
    ExactRational start = out->chords[chord].onset;
    ExactRational change = out->chords[chord + 1].onset;
    bool found = false;
    ExactRational at = msk_int(0);
    for (size_t i = 0; i < out->note_count; i++) {
        if (msk_cmp(out->notes[i].onset, start) > 0 && msk_cmp(out->notes[i].onset, change) < 0) {
            at = out->notes[i].onset;
            found = true;
        }
    }
    if (!found) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    /* A step between the roots, or when they are a step apart the dominant of the second. */
    int distance = (to - from + 7) % 7;
    int between;
    if (distance == 1 || distance == 6) {
        between = (to - 1 + 4) % 7 + 1;
    } else if (distance <= 3) {
        between = (from - 1 + 1) % 7 + 1;
    } else {
        between = (from - 1 + 6) % 7 + 1;
    }
    MotiveSketch before = *out;
    MotiveSketchChord added = {at, between, 0, 0, false};
    if (!insert_chord(out, (size_t)chord + 1, added)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    adapt_melody(&before, out);
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_counter_line(const MotiveSketch *in, const MotiveRelationStep *step,
                                               MotiveSketch *out) {
    (void)step;
    if (!in->accompaniment) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    MotiveRelationResult ready = prepare_harmony(out);
    if (ready != MOTIVE_RELATION_OK) {
        return ready;
    }
    size_t i = 0;
    while (i < out->note_count) {
        /* A run of notes on one pitch inside one chord. */
        const MotiveSketchChord *chord = mch_chord_at(out, out->notes[i].onset);
        size_t end = i + 1;
        while (end < out->note_count && out->notes[end].pitch == out->notes[i].pitch &&
               mch_chord_at(out, out->notes[end].onset) == chord) {
            end++;
        }
        if (end - i >= 2 && chord) {
            /* Toward the nearest tone of the next chord (of its own, in the last). */
            const MotiveSketchChord *next = chord + 1 < out->chords + out->chord_count ? chord + 1 : chord;
            int pcs[4];
            size_t count = motive_sketch_chord_tones(out, next, pcs);
            int pitch = out->notes[i].pitch;
            int target = -1;
            for (int d = 1; d <= 12 && target < 0; d++) {
                for (int k = 0; k < 2 && target < 0; k++) {
                    int candidate = pitch + (k == 0 ? d : -d);
                    if (midi(candidate) && mch_has_pc(pcs, count, mch_pitch_class(candidate))) {
                        target = candidate;
                    }
                }
            }
            if (target >= 0) {
                int alteration;
                long at = motive_sketch_step(out, pitch, &alteration);
                long goal = motive_sketch_step(out, target, NULL);
                int direction = goal > at ? 1 : -1;
                for (size_t n = i + 1; n < end; n++) {
                    long stepped = at + direction * (long)(n - i);
                    if ((direction > 0 && stepped > goal) || (direction < 0 && stepped < goal)) {
                        stepped = goal;
                    }
                    long moved = motive_sketch_pitch_at_step(out, stepped, stepped == goal ? 0 : alteration);
                    out->notes[n].pitch = stepped == goal ? target : (midi(moved) ? (int)moved : pitch);
                }
            }
        }
        i = end;
    }
    return MOTIVE_RELATION_OK;
}

/*
 * The dominant form (ch. V): I becomes V; I-V, I-IV, I-II and any other harmony starting on I become V-I (its first
 * two chords' onsets); I-V-I becomes V-I-V. The melody moves a tonal fourth down, or a fifth up when that is out of
 * range, keeping rhythm and contour, and is then adapted to the new chords.
 */
static MotiveRelationResult apply_dominant_form(const MotiveSketch *in, const MotiveRelationStep *step,
                                                MotiveSketch *out) {
    (void)in;
    (void)step;
    MotiveRelationResult ready = prepare_harmony(out);
    if (ready != MOTIVE_RELATION_OK) {
        return ready;
    }
    if (out->chords[0].degree != 1 || out->chords[0].alteration != 0) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    MotiveSketch before = *out;
    size_t n = out->chord_count;
    int degrees[3];
    size_t count;
    if (n == 1) {
        degrees[0] = 5;
        count = 1;
    } else if (n == 3 && out->chords[1].degree == 5 && out->chords[2].degree == 1) {
        degrees[0] = 5;
        degrees[1] = 1;
        degrees[2] = 5;
        count = 3;
    } else {
        degrees[0] = 5;
        degrees[1] = 1;
        count = 2;
    }
    for (size_t c = 0; c < count; c++) {
        out->chords[c].degree = degrees[c];
        out->chords[c].alteration = 0;
        out->chords[c].inversion = 0;
        out->chords[c].seventh = false;
    }
    out->chord_count = count;
    static const long MOVES[2] = {-3, 4};
    bool moved = false;
    for (int m = 0; m < 2 && !moved; m++) {
        int pitches[MOTIVE_SKETCH_MAX_NOTES];
        moved = true;
        for (size_t i = 0; i < before.note_count && moved; i++) {
            int alteration;
            long at = motive_sketch_step(&before, before.notes[i].pitch, &alteration);
            long pitch = motive_sketch_pitch_at_step(&before, at + MOVES[m], alteration);
            moved = midi(pitch);
            pitches[i] = (int)pitch;
        }
        if (moved) {
            for (size_t i = 0; i < before.note_count; i++) {
                out->notes[i].pitch = pitches[i];
            }
        }
    }
    if (!moved) {
        return MOTIVE_RELATION_RANGE;
    }
    adapt_melody(&before, out);
    return MOTIVE_RELATION_OK;
}

/* --- liquidation by characteristic features (ch. VIII, R-144) ------------------------------------------ */

typedef struct {
    size_t first;
    size_t last;
} Group;

/*
 * The features of a sketch: its labelled ones, or when it has none to start with, each bar-beat group (the notes
 * from one beat onset to the next) in order.
 */
static size_t feature_groups(const MotiveSketch *sketch, bool labelled, Group *groups) {
    size_t count = 0;
    if (labelled) {
        for (size_t f = 0; f < sketch->feature_count; f++) {
            groups[count].first = sketch->features[f].first;
            groups[count].last = sketch->features[f].last;
            count++;
        }
        return count;
    }
    for (size_t i = 0; i < sketch->note_count; i++) {
        if (i == 0 || motive_sketch_strength(sketch, sketch->notes[i].onset) >= 1) {
            groups[count].first = i;
            count++;
        }
        groups[count - 1].last = i;
    }
    return count;
}

/* A feature's leaps (steps of 3 or more, semitones without a scale) and its notes of a duration found only once. */
static long feature_weight(const MotiveSketch *sketch, const Group *group) {
    long weight = 0;
    for (size_t i = group->first; i <= group->last; i++) {
        if (i > group->first) {
            long gap = motive_sketch_step(sketch, sketch->notes[i].pitch, NULL) -
                       motive_sketch_step(sketch, sketch->notes[i - 1].pitch, NULL);
            weight += (gap < 0 ? -gap : gap) >= 3 ? 1 : 0;
        }
        size_t same = 0;
        for (size_t k = 0; k < sketch->note_count; k++) {
            same += msk_eq(sketch->notes[k].duration, sketch->notes[i].duration) ? 1 : 0;
        }
        weight += same == 1 ? 1 : 0;
    }
    return weight;
}

/* Puts one note of the span of notes first..last in place of them, on the chord tone nearest the first. */
static MotiveRelationResult replace_with_chord_tone(MotiveSketch *sketch, const Group *group) {
    const MotiveSketchNote first = sketch->notes[group->first];
    ExactRational span = msk_sub(msk_add(sketch->notes[group->last].onset, sketch->notes[group->last].duration),
                                 first.onset);
    if (!motive_sketch_expressible(span)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    const MotiveSketchChord *chord = mch_chord_at(sketch, first.onset);
    int pcs[4];
    size_t count = chord ? motive_sketch_chord_tones(sketch, chord, pcs) : 0;
    int pitch = -1;
    for (int d = 0; d <= 12 && pitch < 0 && count > 0; d++) {
        for (int k = 0; k < 2 && pitch < 0; k++) {
            int candidate = first.pitch + (k == 0 ? d : -d);
            if (midi(candidate) && mch_has_pc(pcs, count, mch_pitch_class(candidate))) {
                pitch = candidate;
            }
        }
    }
    if (pitch < 0) {
        return MOTIVE_RELATION_NO_HARMONY;
    }
    size_t removed = group->last - group->first;
    sketch->notes[group->first].pitch = pitch;
    sketch->notes[group->first].duration = span;
    for (size_t i = group->last + 1; i < sketch->note_count; i++) {
        sketch->notes[i - removed] = sketch->notes[i];
    }
    sketch->note_count -= removed;
    size_t kept = 0;
    for (size_t f = 0; f < sketch->feature_count; f++) {
        MotiveSketchFeature feature = sketch->features[f];
        if (feature.last >= group->first && feature.first <= group->last) {
            continue;
        }
        if (feature.first > group->last) {
            feature.first -= removed;
            feature.last -= removed;
        }
        sketch->features[kept++] = feature;
    }
    sketch->feature_count = kept;
    return MOTIVE_RELATION_OK;
}

static MotiveRelationResult apply_liquidate_features(const MotiveSketch *in, const MotiveRelationStep *step,
                                                     MotiveSketch *out) {
    (void)in;
    long steps = step->value[0];
    if (steps < 1 || steps > 3 || (step->value[1] != 0 && step->value[1] != 1)) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveRelationResult ready = prepare_harmony(out);
    if (ready != MOTIVE_RELATION_OK) {
        return ready;
    }
    bool labelled = out->feature_count > 0;
    for (long round = 0; round < steps; round++) {
        Group groups[MOTIVE_SKETCH_MAX_NOTES];
        size_t count = feature_groups(out, labelled, groups);
        /* The heaviest feature of two notes or more goes first; of equals, the later. */
        size_t heaviest = count;
        long best = 0;
        for (size_t g = 0; g < count; g++) {
            long weight = feature_weight(out, &groups[g]);
            if (groups[g].last > groups[g].first && (heaviest == count || weight >= best)) {
                heaviest = g;
                best = weight;
            }
        }
        if (heaviest == count) {
            return MOTIVE_RELATION_NOT_APPLICABLE;
        }
        MotiveRelationResult replaced = replace_with_chord_tone(out, &groups[heaviest]);
        if (replaced != MOTIVE_RELATION_OK) {
            return replaced;
        }
    }
    if (step->value[1] != 0) {
        MotiveRelationStep half = {MOTIVE_RELATION_DIMINISH, {1}, {2}};
        settle(out);
        MotiveSketch shortened = *out;
        MotiveRelationResult scaled = scale_time(&shortened, &half, out, false);
        if (scaled != MOTIVE_RELATION_OK) {
            return scaled;
        }
    }
    return MOTIVE_RELATION_OK;
}

/* --- frame relations (R-197) -------------------------------------------------------------------- */

/*
 * The sketch without the notes marked in drop. A dropped note's time goes to the kept note before it (its duration
 * grows by the dropped notes that follow it); the dropped notes before the first kept note give theirs to it, which
 * then starts earlier by that time. A feature keeps the kept notes inside it and goes when it holds none.
 */
static MotiveRelationResult drop_notes(MotiveSketch *sketch, const bool *drop) {
    size_t kept_count = 0;
    for (size_t i = 0; i < sketch->note_count; i++) {
        if (!drop[i]) {
            kept_count++;
        }
    }
    if (kept_count == 0) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    MotiveSketchNote kept[MOTIVE_SKETCH_MAX_NOTES];
    size_t before[MOTIVE_SKETCH_MAX_NOTES + 1]; /* kept notes among the first i */
    size_t n = 0;
    ExactRational leading = msk_int(0);
    before[0] = 0;
    for (size_t i = 0; i < sketch->note_count; i++) {
        if (!drop[i]) {
            kept[n++] = sketch->notes[i];
        } else if (n == 0) {
            leading = msk_add(leading, sketch->notes[i].duration);
        } else {
            kept[n - 1].duration = msk_add(kept[n - 1].duration, sketch->notes[i].duration);
        }
        before[i + 1] = n;
    }
    if (msk_cmp(leading, kept[0].onset) > 0) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    kept[0].onset = msk_sub(kept[0].onset, leading);
    kept[0].duration = msk_add(kept[0].duration, leading);
    for (size_t k = 0; k < kept_count; k++) {
        if (!nice_time(kept[k].duration) || !msk_ok(kept[k].onset) || !motive_sketch_on_grid(kept[k].onset)) {
            return MOTIVE_RELATION_NOT_EXPRESSIBLE;
        }
    }
    for (size_t k = 0; k < kept_count; k++) {
        sketch->notes[k] = kept[k];
    }
    sketch->note_count = kept_count;
    size_t features = 0;
    for (size_t f = 0; f < sketch->feature_count; f++) {
        MotiveSketchFeature feature = sketch->features[f];
        size_t first = before[feature.first];
        size_t last = before[feature.last + 1];
        if (last <= first) {
            continue;
        }
        feature.first = first;
        feature.last = last - 1;
        sketch->features[features++] = feature;
    }
    sketch->feature_count = features;
    return MOTIVE_RELATION_OK;
}

/* A frame relation reads the tone roles: a scale and pitches; false, with the result to return, when it lacks them. */
static bool frame_roles(const MotiveSketch *sketch, MotiveToneLabel *labels, MotiveRelationResult *refused) {
    if (!sketch->has_scale) {
        *refused = MOTIVE_RELATION_NO_SCALE;
        return false;
    }
    if (!pitched(sketch)) {
        *refused = MOTIVE_RELATION_NOT_APPLICABLE;
        return false;
    }
    if (motive_tone_roles(sketch, NULL, 0, labels) != MOTIVE_TONE_OK) {
        *refused = MOTIVE_RELATION_INVALID;
        return false;
    }
    return true;
}

static MotiveRelationResult apply_reduce_to_frame(const MotiveSketch *in, const MotiveRelationStep *step,
                                                  MotiveSketch *out) {
    (void)step;
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    MotiveRelationResult refused;
    if (!frame_roles(in, labels, &refused)) {
        return refused;
    }
    bool drop[MOTIVE_SKETCH_MAX_NOTES];
    for (size_t i = 0; i < in->note_count; i++) {
        drop[i] = labels[i].role != MOTIVE_TONE_CHORD;
    }
    return drop_notes(out, drop);
}

/* Whether the tone roles of the sketch label a note as a non-chord tone of the type. */
static bool labelled_as(const MotiveSketch *sketch, size_t note, NonChordTone type) {
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    if (motive_tone_roles(sketch, NULL, 0, labels) != MOTIVE_TONE_OK) {
        return false;
    }
    return labels[note].role == MOTIVE_TONE_NON_CHORD && labels[note].type == type;
}

static bool pair_type(NonChordTone type) {
    return type == NON_CHORD_TONE_DOUBLE_NEIGHBOR || type == NON_CHORD_TONE_CHANGING_TONE;
}

/*
 * Puts a pitch on a note, from the candidates in order, the first that the tone roles label as the type. The
 * candidates are the scale steps next to the notes either side, then those notes' own pitches, then two steps away.
 * False, the note as it was, when none does.
 */
static bool form_single(MotiveSketch *sketch, size_t index, NonChordTone type) {
    if (index == 0 || index + 1 >= sketch->note_count) {
        return false;
    }
    int around[2] = {sketch->notes[index - 1].pitch, sketch->notes[index + 1].pitch};
    int candidates[16];
    size_t count = 0;
    for (int reach = 1; reach <= 2; reach++) {
        for (int k = 0; k < 2; k++) {
            int alteration;
            long at = motive_sketch_step(sketch, around[k], &alteration);
            for (int direction = 1; direction >= -1; direction -= 2) {
                long pitch = motive_sketch_pitch_at_step(sketch, at + direction * reach, alteration);
                if (midi(pitch)) {
                    candidates[count++] = (int)pitch;
                }
            }
        }
        if (reach == 1) {
            candidates[count++] = around[0];
            candidates[count++] = around[1];
        }
    }
    int was = sketch->notes[index].pitch;
    for (size_t k = 0; k < count; k++) {
        sketch->notes[index].pitch = candidates[k];
        if (labelled_as(sketch, index, type)) {
            return true;
        }
    }
    sketch->notes[index].pitch = was;
    return false;
}

/*
 * The same for the two notes at first and first + 1, between a chord tone and the same pitch again (c t d c): the
 * scale steps above and below c in either order, and, for a double neighbor, the semitones above and below (a
 * diatonic pair is a third apart, so a changing tone). False, the notes as they were, when none is labelled so.
 */
static bool form_pair(MotiveSketch *sketch, size_t first, NonChordTone type) {
    if (first == 0 || first + 2 >= sketch->note_count ||
        sketch->notes[first + 2].pitch != sketch->notes[first - 1].pitch) {
        return false;
    }
    int c = sketch->notes[first - 1].pitch;
    int alteration;
    long at = motive_sketch_step(sketch, c, &alteration);
    long above = motive_sketch_pitch_at_step(sketch, at + 1, alteration);
    long below = motive_sketch_pitch_at_step(sketch, at - 1, alteration);
    int pairs[4][2] = {{(int)above, (int)below}, {(int)below, (int)above}, {c + 1, c - 1}, {c - 1, c + 1}};
    int was[2] = {sketch->notes[first].pitch, sketch->notes[first + 1].pitch};
    for (size_t k = 0; k < (type == NON_CHORD_TONE_DOUBLE_NEIGHBOR ? 4u : 2u); k++) {
        if (!midi(pairs[k][0]) || !midi(pairs[k][1])) {
            continue;
        }
        sketch->notes[first].pitch = pairs[k][0];
        sketch->notes[first + 1].pitch = pairs[k][1];
        if (labelled_as(sketch, first, type) && labelled_as(sketch, first + 1, type)) {
            return true;
        }
    }
    sketch->notes[first].pitch = was[0];
    sketch->notes[first + 1].pitch = was[1];
    return false;
}

static MotiveRelationResult apply_add_non_chord_tone(const MotiveSketch *in, const MotiveRelationStep *step,
                                                     MotiveSketch *out) {
    long after = step->value[0];
    long type = step->value[1];
    if (after < 0 || (size_t)after >= in->note_count || type < 0 || type >= NON_CHORD_TONE_COUNT) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    MotiveRelationResult refused;
    if (!frame_roles(in, labels, &refused)) {
        return refused;
    }
    size_t before = (size_t)after;
    if (before + 1 >= in->note_count || labels[before].role != MOTIVE_TONE_CHORD) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    size_t added = pair_type((NonChordTone)type) ? 2 : 1;
    ExactRational half = msk_div(in->notes[before].duration, msk_int(2));
    ExactRational part = msk_div(half, msk_int(added));
    if (!nice_time(half) || !nice_time(part)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    MotiveSketchNote notes[2];
    for (size_t k = 0; k < added; k++) {
        notes[k].pitch = in->notes[before].pitch;
        notes[k].duration = part;
        notes[k].onset = msk_add(msk_add(in->notes[before].onset, half), msk_mul(part, msk_int(k)));
    }
    if (!insert_notes(in, out, before + 1, notes, added)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    out->notes[before].duration = half;
    bool formed = added == 2 ? form_pair(out, before + 1, (NonChordTone)type)
                             : form_single(out, before + 1, (NonChordTone)type);
    return formed ? MOTIVE_RELATION_OK : MOTIVE_RELATION_NOT_APPLICABLE;
}

static MotiveRelationResult apply_remove_non_chord_tone(const MotiveSketch *in, const MotiveRelationStep *step,
                                                        MotiveSketch *out) {
    long note = step->value[0];
    if (note < 0 || (size_t)note >= in->note_count) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    MotiveRelationResult refused;
    if (!frame_roles(in, labels, &refused)) {
        return refused;
    }
    if (labels[note].role == MOTIVE_TONE_CHORD) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    bool drop[MOTIVE_SKETCH_MAX_NOTES] = {false};
    drop[note] = true;
    return drop_notes(out, drop);
}

static MotiveRelationResult apply_change_non_chord_tone(const MotiveSketch *in, const MotiveRelationStep *step,
                                                        MotiveSketch *out) {
    long note = step->value[0];
    long type = step->value[1];
    if (note < 0 || (size_t)note >= in->note_count || type < 0 || type >= NON_CHORD_TONE_COUNT) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveToneLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    MotiveRelationResult refused;
    if (!frame_roles(in, labels, &refused)) {
        return refused;
    }
    size_t index = (size_t)note;
    if (labels[index].role == MOTIVE_TONE_CHORD) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    NonChordTone now = labels[index].type;
    bool is_figure = labels[index].role == MOTIVE_TONE_NON_CHORD;
    if (is_figure && now == (NonChordTone)type) {
        return MOTIVE_RELATION_UNCHANGED;
    }
    if (!pair_type((NonChordTone)type)) {
        return form_single(out, index, (NonChordTone)type) ? MOTIVE_RELATION_OK : MOTIVE_RELATION_NOT_APPLICABLE;
    }
    /* The partner of the pair: the neighbouring note of the same pair type, else a neighbouring non-chord note. */
    bool next_free = index + 1 < in->note_count && labels[index + 1].role != MOTIVE_TONE_CHORD;
    bool previous_free = index > 0 && labels[index - 1].role != MOTIVE_TONE_CHORD;
    bool next_pair = next_free && labels[index + 1].role == MOTIVE_TONE_NON_CHORD && labels[index + 1].type == now;
    bool previous_pair =
        previous_free && labels[index - 1].role == MOTIVE_TONE_NON_CHORD && labels[index - 1].type == now;
    size_t first;
    if (is_figure && pair_type(now) && next_pair) {
        first = index;
    } else if (is_figure && pair_type(now) && previous_pair) {
        first = index - 1;
    } else if (next_free) {
        first = index;
    } else if (previous_free) {
        first = index - 1;
    } else {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    return form_pair(out, first, (NonChordTone)type) ? MOTIVE_RELATION_OK : MOTIVE_RELATION_NOT_APPLICABLE;
}

static MotiveRelationResult apply_reharmonize_frame(const MotiveSketch *in, const MotiveRelationStep *step,
                                                    MotiveSketch *out) {
    (void)in;
    long degree = step->value[0];
    if (degree < 1 || degree > 7 || (step->value[1] != 0 && step->value[1] != 1)) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveRelationResult ready = prepare_harmony(out);
    if (ready != MOTIVE_RELATION_OK) {
        return ready;
    }
    MotiveSketch before = *out;
    MotiveSketchChord chord = {msk_int(0), (int)degree, 0, 0, step->value[1] != 0};
    out->chords[0] = chord;
    out->chord_count = 1;
    adapt_melody(&before, out);
    return MOTIVE_RELATION_OK;
}

/* --- the catalog ------------------------------------------------------------------------------ */

static const Entry TABLE[] = {
    {"exact", 0, 0, apply_exact},
    {"transpose", 3, 1, apply_transpose},
    {"invert", 2, 2, apply_invert},
    {"retrograde", 0, 2, apply_retrograde},
    {"interval-change", 3, 2, apply_interval_change},
    {"fragment", 3, 2, apply_fragment},
    {"liquidate", 3, 2, apply_liquidate},
    {"augment", 1, 1, apply_augment},
    {"diminish", 1, 1, apply_diminish},
    {"note-repeat", 2, 2, apply_note_repeat},
    {"rhythm-repeat", 3, 2, apply_rhythm_repeat},
    {"rhythm-shift", 1, 2, apply_rhythm_shift},
    {"add-upbeat", 2, 2, apply_add_upbeat},
    {"change-meter", 2, 3, apply_change_meter},
    {"add-interval", 2, 2, apply_add_interval},
    {"omit-interval", 1, 2, apply_omit_interval},
    {"ancillary-fill", 2, 2, apply_ancillary_fill},
    {"condense", 3, 2, apply_condense},
    {"repeat-feature", 3, 2, apply_repeat_feature},
    {"shift-feature", 2, 2, apply_shift_feature},
    {"harmony-inversion", 2, 2, apply_harmony_inversion},
    {"harmony-append", 2, 2, apply_harmony_append},
    {"harmony-insert", 2, 2, apply_harmony_insert},
    {"harmony-substitute", 3, 2, apply_harmony_substitute},
    {"passing-harmony", 1, 2, apply_passing_harmony},
    {"counter-line", 0, 2, apply_counter_line},
    {"dominant-form", 0, 2, apply_dominant_form},
    {"liquidate-features", 2, 2, apply_liquidate_features},
    {"reduce-to-frame", 0, 2, apply_reduce_to_frame},
    {"add-non-chord-tone", 2, 2, apply_add_non_chord_tone},
    {"remove-non-chord-tone", 1, 2, apply_remove_non_chord_tone},
    {"change-non-chord-tone", 2, 2, apply_change_non_chord_tone},
    {"reharmonize-frame", 2, 2, apply_reharmonize_frame},
};

typedef char table_has_every_kind[sizeof(TABLE) / sizeof(TABLE[0]) == MOTIVE_RELATION_KIND_COUNT ? 1 : -1];

bool motive_relation_harmony_changes_notes(const MotiveSketch *in, const MotiveRelationStep *step) {
    MotiveSketch out;
    if (!in || !step || motive_relation_apply(in, step, &out) != MOTIVE_RELATION_OK) {
        return false;
    }
    for (size_t i = 0; i < in->note_count && i < out.note_count; i++) {
        if (in->notes[i].pitch != out.notes[i].pitch) {
            return true;
        }
    }
    return false;
}

size_t motive_relation_count(void) {
    return MOTIVE_RELATION_KIND_COUNT;
}

const char *motive_relation_name(MotiveRelationKind kind) {
    return (unsigned)kind < MOTIVE_RELATION_KIND_COUNT ? TABLE[kind].name : NULL;
}

bool motive_relation_find(const char *name, MotiveRelationKind *kind) {
    if (!name || !kind) {
        return false;
    }
    for (size_t i = 0; i < MOTIVE_RELATION_KIND_COUNT; i++) {
        if (strcmp(TABLE[i].name, name) == 0) {
            *kind = (MotiveRelationKind)i;
            return true;
        }
    }
    return false;
}

size_t motive_relation_param_count(MotiveRelationKind kind) {
    return (unsigned)kind < MOTIVE_RELATION_KIND_COUNT ? TABLE[kind].params : 0;
}

int motive_relation_cost(MotiveRelationKind kind) {
    return (unsigned)kind < MOTIVE_RELATION_KIND_COUNT ? TABLE[kind].cost : 0;
}

static const char *const RESULT_NAMES[] = {"ok",       "not applicable", "unchanged", "range",   "not expressible",
                                           "no scale", "no harmony",     "no feature", "invalid"};

const char *motive_relation_result_name(MotiveRelationResult result) {
    return (size_t)result < sizeof(RESULT_NAMES) / sizeof(RESULT_NAMES[0]) ? RESULT_NAMES[result] : "unknown";
}

MotiveRelationResult motive_relation_apply(const MotiveSketch *in, const MotiveRelationStep *step,
                                           MotiveSketch *out) {
    if (!in || !step || !out || (unsigned)step->kind >= MOTIVE_RELATION_KIND_COUNT || !motive_sketch_validate(in)) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveSketch work = *in;
    MotiveRelationResult result = TABLE[step->kind].apply(in, step, &work);
    if (result != MOTIVE_RELATION_OK) {
        return result;
    }
    settle(&work);
    if (!motive_sketch_validate(&work)) {
        return MOTIVE_RELATION_INVALID;
    }
    if (step->kind != MOTIVE_RELATION_EXACT) {
        MotiveSketch base = *in;
        settle(&base);
        if (motive_sketch_equal(&base, &work)) {
            return MOTIVE_RELATION_UNCHANGED;
        }
    }
    *out = work;
    return MOTIVE_RELATION_OK;
}

/* --- steps as text ------------------------------------------------------------------------------ */

static const char *const PARAM_NAMES[MOTIVE_RELATION_KIND_COUNT][MOTIVE_RELATION_MAX_PARAMS] = {
    [MOTIVE_RELATION_EXACT] = {NULL},
    [MOTIVE_RELATION_TRANSPOSE] = {"tonal", "by", "degree"},
    [MOTIVE_RELATION_INVERT] = {"axis", "tonal"},
    [MOTIVE_RELATION_RETROGRADE] = {NULL},
    [MOTIVE_RELATION_INTERVAL_CHANGE] = {"interval", "amount", "keep-contour"},
    [MOTIVE_RELATION_FRAGMENT] = {"position", "length", "min"},
    [MOTIVE_RELATION_LIQUIDATE] = {"drop", "keep-rhythm", "min"},
    [MOTIVE_RELATION_AUGMENT] = {"ratio"},
    [MOTIVE_RELATION_DIMINISH] = {"ratio"},
    [MOTIVE_RELATION_NOTE_REPEAT] = {"note", "count"},
    [MOTIVE_RELATION_RHYTHM_REPEAT] = {"first", "length", "target"},
    [MOTIVE_RELATION_RHYTHM_SHIFT] = {"offset"},
    [MOTIVE_RELATION_ADD_UPBEAT] = {"count", "approach"},
    [MOTIVE_RELATION_CHANGE_METER] = {"num", "den"},
    [MOTIVE_RELATION_ADD_INTERVAL] = {"interval", "steps"},
    [MOTIVE_RELATION_OMIT_INTERVAL] = {"interval"},
    [MOTIVE_RELATION_ANCILLARY_FILL] = {"interval", "kind"},
    [MOTIVE_RELATION_CONDENSE] = {"first", "last", "ratio"},
    [MOTIVE_RELATION_REPEAT_FEATURE] = {"feature", "times", "steps"},
    [MOTIVE_RELATION_SHIFT_FEATURE] = {"feature", "offset"},
    [MOTIVE_RELATION_HARMONY_INVERSION] = {"chord", "inversion"},
    [MOTIVE_RELATION_HARMONY_APPEND] = {"degree", "seventh"},
    [MOTIVE_RELATION_HARMONY_INSERT] = {"chord", "degree"},
    [MOTIVE_RELATION_HARMONY_SUBSTITUTE] = {"chord", "degree", "then"},
    [MOTIVE_RELATION_PASSING_HARMONY] = {"chord"},
    [MOTIVE_RELATION_COUNTER_LINE] = {NULL},
    [MOTIVE_RELATION_DOMINANT_FORM] = {NULL},
    [MOTIVE_RELATION_LIQUIDATE_FEATURES] = {"steps", "shorten"},
    [MOTIVE_RELATION_REDUCE_TO_FRAME] = {NULL},
    [MOTIVE_RELATION_ADD_NON_CHORD_TONE] = {"after", "type"},
    [MOTIVE_RELATION_REMOVE_NON_CHORD_TONE] = {"note"},
    [MOTIVE_RELATION_CHANGE_NON_CHORD_TONE] = {"note", "type"},
    [MOTIVE_RELATION_REHARMONIZE_FRAME] = {"degree", "seventh"},
};

const char *motive_relation_param_name(MotiveRelationKind kind, size_t index) {
    if ((unsigned)kind >= MOTIVE_RELATION_KIND_COUNT || index >= motive_relation_param_count(kind)) {
        return NULL;
    }
    return PARAM_NAMES[kind][index];
}

static bool param_is_ratio(MotiveRelationKind kind, size_t index) {
    switch (kind) {
    case MOTIVE_RELATION_AUGMENT:
    case MOTIVE_RELATION_DIMINISH:
    case MOTIVE_RELATION_RHYTHM_SHIFT:
        return index == 0;
    case MOTIVE_RELATION_CONDENSE:
        return index == 2;
    case MOTIVE_RELATION_SHIFT_FEATURE:
        return index == 1;
    default:
        return false;
    }
}

static long param_default(MotiveRelationKind kind, size_t index) {
    return kind == MOTIVE_RELATION_INVERT && index == 0 ? -1 : 0;
}

bool motive_relation_step_text(const MotiveRelationStep *step, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return false;
    }
    buffer[0] = '\0';
    if (!step || (unsigned)step->kind >= MOTIVE_RELATION_KIND_COUNT) {
        return false;
    }
    size_t used = 0;
    int written = snprintf(buffer, size, "%s", TABLE[step->kind].name);
    if (written < 0 || (size_t)written >= size) {
        buffer[0] = '\0';
        return false;
    }
    used = (size_t)written;
    for (size_t i = 0; i < TABLE[step->kind].params; i++) {
        if (step->value[i] == param_default(step->kind, i)) {
            continue;
        }
        if (param_is_ratio(step->kind, i)) {
            written = snprintf(buffer + used, size - used, " %s=%ld:%ld", PARAM_NAMES[step->kind][i], step->value[i],
                               step->den[i]);
        } else {
            written = snprintf(buffer + used, size - used, " %s=%ld", PARAM_NAMES[step->kind][i], step->value[i]);
        }
        if (written < 0 || (size_t)written >= size - used) {
            buffer[used] = '\0';
            return false;
        }
        used += (size_t)written;
    }
    return true;
}

/* The next space-separated token of *cursor, cut in place; NULL at the end. */
static char *next_token(char **cursor) {
    char *start = *cursor;
    while (*start == ' ') {
        start++;
    }
    if (*start == '\0') {
        *cursor = start;
        return NULL;
    }
    char *end = start;
    while (*end != '\0' && *end != ' ') {
        end++;
    }
    if (*end != '\0') {
        *end++ = '\0';
    }
    *cursor = end;
    return start;
}

static bool parse_whole(const char *text, long *out) {
    if (*text == '\0') {
        return false;
    }
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || *end != '\0') {
        return false;
    }
    *out = value;
    return true;
}

bool motive_relation_step_parse(const char *text, MotiveRelationStep *out) {
    if (!text || !out) {
        return false;
    }
    char copy[160];
    if (strlen(text) >= sizeof(copy)) {
        return false;
    }
    strcpy(copy, text);
    char *save = copy;
    char *name = next_token(&save);
    MotiveRelationStep step;
    memset(&step, 0, sizeof(step));
    if (!name || !motive_relation_find(name, &step.kind)) {
        return false;
    }
    for (size_t i = 0; i < MOTIVE_RELATION_MAX_PARAMS; i++) {
        step.value[i] = param_default(step.kind, i);
    }
    bool seen[MOTIVE_RELATION_MAX_PARAMS] = {false};
    for (char *token = next_token(&save); token; token = next_token(&save)) {
        char *equals = strchr(token, '=');
        if (!equals) {
            return false;
        }
        *equals = '\0';
        const char *value = equals + 1;
        size_t index = TABLE[step.kind].params;
        for (size_t i = 0; i < TABLE[step.kind].params; i++) {
            if (strcmp(token, PARAM_NAMES[step.kind][i]) == 0) {
                index = i;
                break;
            }
        }
        if (index == TABLE[step.kind].params || seen[index]) {
            return false;
        }
        seen[index] = true;
        if (param_is_ratio(step.kind, index)) {
            char *colon = strchr(value, ':');
            long n = 0;
            long d = 0;
            if (!colon) {
                return false;
            }
            *colon = '\0';
            if (!parse_whole(value, &n) || !parse_whole(colon + 1, &d) || n <= 0 || d <= 0) {
                return false;
            }
            step.value[index] = n;
            step.den[index] = d;
        } else if (!parse_whole(value, &step.value[index])) {
            return false;
        }
    }
    *out = step;
    return true;
}
