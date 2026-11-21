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

/*
 * The chords after a change that moved the kept notes from old_onset[j] to new_onset[j]: a chord goes to the first
 * kept note at or after it, and of several chords that land on one note the last, the one in force, stays.
 */
static void remap_chords(const MotiveSketch *in, MotiveSketch *out, const Rational *old_onset,
                         const Rational *new_onset, size_t kept) {
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

MotiveRelationResult motive_relation_apply_exact(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out) {
    (void)in;
    (void)step;
    (void)out;
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_transpose(const MotiveSketch *in, const MotiveRelationStep *step,
                                            MotiveSketch *out) {
    bool tonal = step->value[0] != 0;
    long by = step->value[1];
    long degree = step->value[2];
    if (!motive_relation_pitched(in)) {
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
        if (!motive_relation_midi(pitch)) {
            return MOTIVE_RELATION_RANGE;
        }
        out->notes[i].pitch = (int)pitch;
    }
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_invert(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out) {
    long axis = step->value[0];
    bool tonal = step->value[1] != 0;
    if (!motive_relation_pitched(in)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    if (axis != -1 && !motive_relation_midi(axis)) {
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
        if (!motive_relation_midi(pitch)) {
            return MOTIVE_RELATION_RANGE;
        }
        out->notes[i].pitch = (int)pitch;
    }
    return MOTIVE_RELATION_OK;
}

/* --- retrograde ------------------------------------------------------------------------ */

MotiveRelationResult motive_relation_apply_retrograde(const MotiveSketch *in, const MotiveRelationStep *step,
                                             MotiveSketch *out) {
    (void)step;
    size_t count = in->note_count;
    /* Each note moves to the mirror of its span about the middle of the notes; the order reverses. */
    Rational span = msk_add(in->notes[0].onset, motive_relation_note_end(&in->notes[count - 1]));
    for (size_t i = 0; i < count; i++) {
        const MotiveSketchNote *note = &in->notes[count - 1 - i];
        out->notes[i].pitch = note->pitch;
        out->notes[i].duration = note->duration;
        out->notes[i].onset = msk_sub(span, motive_relation_note_end(note));
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

MotiveRelationResult motive_relation_apply_interval_change(const MotiveSketch *in, const MotiveRelationStep *step,
                                                  MotiveSketch *out) {
    long which = step->value[0];
    long amount = step->value[1];
    bool keep_contour = step->value[2] != 0;
    size_t count = in->note_count;
    if (!motive_relation_pitched(in) || count < 2) {
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
        if (!motive_relation_midi(pitch)) {
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

MotiveRelationResult motive_relation_apply_fragment(const MotiveSketch *in, const MotiveRelationStep *step,
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
    Rational start = in->notes[first].onset;
    Rational old_onset[MOTIVE_SKETCH_MAX_NOTES];
    Rational new_onset[MOTIVE_SKETCH_MAX_NOTES];
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

MotiveRelationResult motive_relation_apply_liquidate(const MotiveSketch *in, const MotiveRelationStep *step,
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
    Rational old_onset[MOTIVE_SKETCH_MAX_NOTES];
    Rational new_onset[MOTIVE_SKETCH_MAX_NOTES];
    Rational at = msk_int(0);
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
        Rational end = motive_relation_note_end(&in->notes[total - 1]);
        for (size_t k = kept; k-- > 0;) {
            if (k == 0) {
                out->notes[0].onset = in->notes[0].onset;
            }
            out->notes[k].duration = msk_sub(end, out->notes[k].onset);
            if (!motive_relation_nice_time(in, out->notes[k].duration)) {
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
