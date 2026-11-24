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

/* The span of a chord and the note onset inside it nearest its middle; false when no note starts inside. */
static bool split_point(const MotiveSketch *sketch, size_t chord, Rational *at) {
    Rational start = sketch->chords[chord].onset;
    Rational end = chord + 1 < sketch->chord_count ? sketch->chords[chord + 1].onset : sketch->length;
    Rational middle = msk_add(start, msk_div(msk_sub(end, start), msk_int(2)));
    bool found = false;
    Rational best_gap = msk_int(0);
    for (size_t i = 0; i < sketch->note_count; i++) {
        Rational onset = sketch->notes[i].onset;
        if (msk_cmp(onset, start) <= 0 || msk_cmp(onset, end) >= 0) {
            continue;
        }
        Rational gap = msk_cmp(onset, middle) >= 0 ? msk_sub(onset, middle) : msk_sub(middle, onset);
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
    int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
    size_t count = motive_sketch_chord_tones(sketch, c, pcs);
    if (old_inversion < 0 || (size_t)old_inversion >= count || (size_t)c->inversion >= count) {
        return;
    }
    Rational start = c->onset;
    Rational end = chord + 1 < sketch->chord_count ? sketch->chords[chord + 1].onset : sketch->length;
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
    if (!found || was == now || pitch_class_of_midi(sketch->notes[lowest].pitch) != was) {
        return;
    }
    int pitch = sketch->notes[lowest].pitch;
    for (int distance = 1; distance <= 12; distance++) {
        for (int k = 0; k < 2; k++) {
            int candidate = pitch + (k == 0 ? -distance : distance);
            if (motive_relation_midi(candidate) && pitch_class_of_midi(candidate) == now) {
                sketch->notes[lowest].pitch = candidate;
                return;
            }
        }
    }
}

MotiveRelationResult motive_relation_apply_harmony_inversion(const MotiveSketch *in, const MotiveRelationStep *step,
                                                    MotiveSketch *out) {
    MotiveRelationResult ready = motive_relation_prepare_harmony(out);
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
    motive_relation_adapt_melody(&before, out);
    (void)in;
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_harmony_append(const MotiveSketch *in, const MotiveRelationStep *step,
                                                 MotiveSketch *out) {
    (void)in;
    long degree = step->value[0];
    if (degree < 1 || degree > 7 || (step->value[1] != 0 && step->value[1] != 1)) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveRelationResult ready = motive_relation_prepare_harmony(out);
    if (ready != MOTIVE_RELATION_OK) {
        return ready;
    }
    Rational at = out->notes[out->note_count - 1].onset;
    if (msk_cmp(at, out->chords[out->chord_count - 1].onset) <= 0) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    MotiveSketch before = *out;
    MotiveSketchChord chord = {at, (int)degree, 0, 0, step->value[1] != 0};
    if (!insert_chord(out, out->chord_count, chord)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    motive_relation_adapt_melody(&before, out);
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_harmony_insert(const MotiveSketch *in, const MotiveRelationStep *step,
                                                 MotiveSketch *out) {
    (void)in;
    long chord = step->value[0];
    long degree = step->value[1];
    if (degree < 1 || degree > 7) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveRelationResult ready = motive_relation_prepare_harmony(out);
    if (ready != MOTIVE_RELATION_OK) {
        return ready;
    }
    if (chord < 0 || (size_t)chord >= out->chord_count) {
        return MOTIVE_RELATION_NO_HARMONY;
    }
    Rational at;
    if (!split_point(out, (size_t)chord, &at)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    MotiveSketch before = *out;
    MotiveSketchChord added = {at, (int)degree, 0, 0, false, NULL};
    if (!insert_chord(out, (size_t)chord + 1, added)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    motive_relation_adapt_melody(&before, out);
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_harmony_substitute(const MotiveSketch *in, const MotiveRelationStep *step,
                                                     MotiveSketch *out) {
    (void)in;
    long chord = step->value[0];
    long degree = step->value[1];
    long then = step->value[2];
    if (degree < 1 || degree > 7 || then < 0 || then > 7) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveRelationResult ready = motive_relation_prepare_harmony(out);
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
        Rational at;
        if (!split_point(out, (size_t)chord, &at)) {
            return MOTIVE_RELATION_NOT_APPLICABLE;
        }
        MotiveSketchChord second = {at, (int)then, 0, 0, false, NULL};
        if (!insert_chord(out, (size_t)chord + 1, second)) {
            return MOTIVE_RELATION_NOT_APPLICABLE;
        }
    }
    out->chords[chord] = replaced;
    motive_relation_adapt_melody(&before, out);
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_passing_harmony(const MotiveSketch *in, const MotiveRelationStep *step,
                                                  MotiveSketch *out) {
    (void)in;
    long chord = step->value[0];
    MotiveRelationResult ready = motive_relation_prepare_harmony(out);
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
    Rational start = out->chords[chord].onset;
    Rational change = out->chords[chord + 1].onset;
    bool found = false;
    Rational at = msk_int(0);
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
    MotiveSketchChord added = {at, between, 0, 0, false, NULL};
    if (!insert_chord(out, (size_t)chord + 1, added)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    motive_relation_adapt_melody(&before, out);
    return MOTIVE_RELATION_OK;
}

MotiveRelationResult motive_relation_apply_counter_line(const MotiveSketch *in, const MotiveRelationStep *step,
                                               MotiveSketch *out) {
    (void)step;
    if (!in->accompaniment) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    MotiveRelationResult ready = motive_relation_prepare_harmony(out);
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
            int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
            size_t count = motive_sketch_chord_tones(out, next, pcs);
            int pitch = out->notes[i].pitch;
            int target = -1;
            for (int d = 1; d <= 12 && target < 0; d++) {
                for (int k = 0; k < 2 && target < 0; k++) {
                    int candidate = pitch + (k == 0 ? d : -d);
                    if (motive_relation_midi(candidate) && mch_has_pc(pcs, count, pitch_class_of_midi(candidate))) {
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
                    out->notes[n].pitch = stepped == goal ? target : (motive_relation_midi(moved) ? (int)moved : pitch);
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
MotiveRelationResult motive_relation_apply_dominant_form(const MotiveSketch *in, const MotiveRelationStep *step,
                                                MotiveSketch *out) {
    (void)in;
    (void)step;
    MotiveRelationResult ready = motive_relation_prepare_harmony(out);
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
            moved = motive_relation_midi(pitch);
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
    motive_relation_adapt_melody(&before, out);
    return MOTIVE_RELATION_OK;
}
