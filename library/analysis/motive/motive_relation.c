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

_Static_assert(MOTIVE_SKETCH_MAX_NOTES == MOTIVE_RELATION_NOTE_LIMIT, "the vocabulary limits are the sketch's");

_Static_assert(MOTIVE_SKETCH_MAX_CHORDS == MOTIVE_RELATION_CHORD_LIMIT, "the vocabulary limits are the sketch's");

_Static_assert(MOTIVE_SKETCH_MAX_FEATURES == MOTIVE_RELATION_FEATURE_LIMIT, "the vocabulary limits are the sketch's");

typedef MotiveRelationResult (*ApplyFn)(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out);

typedef struct {
    int cost;
    ApplyFn apply;
} Entry;

/* --- helpers ------------------------------------------------------------------ */

bool motive_relation_pitched(const MotiveSketch *sketch) {
    for (size_t i = 0; i < sketch->note_count; i++) {
        if (sketch->notes[i].pitch == MOTIVE_SKETCH_PITCH_UNKNOWN) {
            return false;
        }
    }
    return true;
}

Rational motive_relation_note_end(const MotiveSketchNote *note) {
    return msk_add(note->onset, note->duration);
}

/* A sketch is as long as its notes; a chord at or after the end goes. */
void motive_relation_settle(MotiveSketch *sketch) {
    if (sketch->note_count == 0) {
        return;
    }
    sketch->length = motive_relation_note_end(&sketch->notes[sketch->note_count - 1]);
    size_t kept = 0;
    for (size_t i = 0; i < sketch->chord_count; i++) {
        if (msk_ok(sketch->length) && msk_cmp(sketch->chords[i].onset, sketch->length) < 0) {
            sketch->chords[kept++] = sketch->chords[i];
        }
    }
    sketch->chord_count = kept;
}

/* --- transpose, invert -------------------------------------------------------------- */

bool motive_relation_midi(long pitch) {
    return pitch >= 0 && pitch <= 127;
}

bool motive_relation_nice_time(const MotiveSketch *source, Rational duration);

/* --- augment, diminish ---------------------------------------------------------------------- */

static bool whole_beats(const MotiveSketch *sketch, Rational time) {
    Rational beats = msk_div(time, sketch->beat);
    return msk_ok(beats) && beats.den == 1;
}

MotiveRelationResult motive_relation_scale_time(const MotiveSketch *in, const MotiveRelationStep *step, MotiveSketch *out,
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
    Rational factor = msk_make((unsigned long long)num, (unsigned long long)den);
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
    if (!msk_ok(out->pickup) || msk_cmp(out->pickup, in->bar) >= 0) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    /* A source of whole beats stays one. */
    if (whole_beats(in, in->length) && !whole_beats(in, motive_relation_note_end(&out->notes[in->note_count - 1]))) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
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
bool motive_relation_insert_notes(const MotiveSketch *in, MotiveSketch *out, size_t at, const MotiveSketchNote *added,
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

bool motive_relation_nice_time(const MotiveSketch *source, Rational duration) {
    return motive_sketch_time_writable(source, duration) && motive_sketch_time_on_grid(source, duration);
}

/* --- harmony and melody adaptation (Exs. 25 to 29) ----------------------------------------------- */

/* A harmony relation's start: a scale, pitches, and chords (the best implied chord at 0 when there are none). */
MotiveRelationResult motive_relation_prepare_harmony(MotiveSketch *work) {
    if (!work->has_scale) {
        return MOTIVE_RELATION_NO_SCALE;
    }
    if (!motive_relation_pitched(work)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    if (work->chord_count == 0) {
        if (!motive_sketch_implied_chord(work, &work->chords[0])) {
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

/* The pitch of the new chord's tones nearest note i (within an octave), by the rules of motive_relation_adapt_melody; -1 for none. */
static int nearest_chord_pitch(const MotiveSketch *before, const MotiveSketch *sketch, size_t i, const int *pcs,
                               size_t count) {
    int pitch = sketch->notes[i].pitch;
    int preferred = i > 0 && before->notes[i].pitch < before->notes[i - 1].pitch ? -1 : 1;
    int nearest = -1;
    for (int distance = 1; distance <= 12; distance++) {
        for (int k = 0; k < 2; k++) {
            int candidate = pitch + (k == 0 ? preferred : -preferred) * distance;
            if (!motive_relation_midi(candidate) || !mch_has_pc(pcs, count, pitch_class_of_midi(candidate))) {
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
void motive_relation_adapt_melody(const MotiveSketch *before, MotiveSketch *sketch) {
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
        int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
        size_t count = motive_sketch_chord_tones(sketch, new_chord, pcs);
        if (mch_has_pc(pcs, count, pitch_class_of_midi(note->pitch))) {
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
        if (motive_relation_midi(pitch)) {
            sketch->notes[i].pitch = (int)pitch;
        }
    }
}

/* --- the catalog ------------------------------------------------------------------------------ */

static const Entry TABLE[] = {
    {0, motive_relation_apply_exact},
    {1, motive_relation_apply_transpose},
    {2, motive_relation_apply_invert},
    {2, motive_relation_apply_retrograde},
    {2, motive_relation_apply_interval_change},
    {2, motive_relation_apply_fragment},
    {2, motive_relation_apply_liquidate},
    {1, motive_relation_apply_augment},
    {1, motive_relation_apply_diminish},
    {2, motive_relation_apply_note_repeat},
    {2, motive_relation_apply_rhythm_repeat},
    {2, motive_relation_apply_rhythm_shift},
    {2, motive_relation_apply_add_upbeat},
    {3, motive_relation_apply_change_meter},
    {2, motive_relation_apply_add_interval},
    {2, motive_relation_apply_omit_interval},
    {2, motive_relation_apply_ancillary_fill},
    {2, motive_relation_apply_condense},
    {2, motive_relation_apply_repeat_feature},
    {2, motive_relation_apply_shift_feature},
    {2, motive_relation_apply_harmony_inversion},
    {2, motive_relation_apply_harmony_append},
    {2, motive_relation_apply_harmony_insert},
    {2, motive_relation_apply_harmony_substitute},
    {2, motive_relation_apply_passing_harmony},
    {2, motive_relation_apply_counter_line},
    {2, motive_relation_apply_dominant_form},
    {2, motive_relation_apply_liquidate_features},
    {2, motive_relation_apply_reduce_to_frame},
    {2, motive_relation_apply_add_non_chord_tone},
    {2, motive_relation_apply_remove_non_chord_tone},
    {2, motive_relation_apply_change_non_chord_tone},
    {2, motive_relation_apply_reharmonize_frame},
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

int motive_relation_cost(MotiveRelationKind kind) {
    return (unsigned)kind < MOTIVE_RELATION_KIND_COUNT ? TABLE[kind].cost : 0;
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
    motive_relation_settle(&work);
    if (!motive_sketch_validate(&work)) {
        return MOTIVE_RELATION_INVALID;
    }
    if (step->kind != MOTIVE_RELATION_EXACT) {
        MotiveSketch base = *in;
        motive_relation_settle(&base);
        if (motive_sketch_equal(&base, &work)) {
            return MOTIVE_RELATION_UNCHANGED;
        }
    }
    *out = work;
    return MOTIVE_RELATION_OK;
}
