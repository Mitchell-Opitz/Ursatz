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
static MotiveRelationResult replace_with_chord_tone(const MotiveSketch *source, MotiveSketch *sketch,
                                                    const Group *group) {
    const MotiveSketchNote first = sketch->notes[group->first];
    Rational span = msk_sub(msk_add(sketch->notes[group->last].onset, sketch->notes[group->last].duration),
                                 first.onset);
    if (!motive_sketch_time_writable(source, span)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    const MotiveSketchChord *chord = mch_chord_at(sketch, first.onset);
    int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
    size_t count = chord ? motive_sketch_chord_tones(sketch, chord, pcs) : 0;
    int pitch = -1;
    for (int d = 0; d <= 12 && pitch < 0 && count > 0; d++) {
        for (int k = 0; k < 2 && pitch < 0; k++) {
            int candidate = first.pitch + (k == 0 ? d : -d);
            if (motive_relation_midi(candidate) && mch_has_pc(pcs, count, pitch_class_of_midi(candidate))) {
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

MotiveRelationResult motive_relation_apply_liquidate_features(const MotiveSketch *in, const MotiveRelationStep *step,
                                                     MotiveSketch *out) {
    long steps = step->value[0];
    if (steps < 1 || steps > 3 || (step->value[1] != 0 && step->value[1] != 1)) {
        return MOTIVE_RELATION_INVALID;
    }
    MotiveRelationResult ready = motive_relation_prepare_harmony(out);
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
        MotiveRelationResult replaced = replace_with_chord_tone(in, out, &groups[heaviest]);
        if (replaced != MOTIVE_RELATION_OK) {
            return replaced;
        }
    }
    if (step->value[1] != 0) {
        MotiveRelationStep half = {MOTIVE_RELATION_DIMINISH, {1}, {2}};
        motive_relation_settle(out);
        MotiveSketch shortened = *out;
        MotiveRelationResult scaled = motive_relation_scale_time(&shortened, &half, out, false);
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
static MotiveRelationResult drop_notes(const MotiveSketch *source, MotiveSketch *sketch, const bool *drop) {
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
    Rational leading = msk_int(0);
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
        if (!motive_relation_nice_time(source, kept[k].duration) || !msk_ok(kept[k].onset) ||
            !motive_sketch_time_on_grid(source, kept[k].onset)) {
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
    if (!motive_relation_pitched(sketch)) {
        *refused = MOTIVE_RELATION_NOT_APPLICABLE;
        return false;
    }
    if (motive_tone_roles(sketch, NULL, 0, labels) != MOTIVE_TONE_OK) {
        *refused = MOTIVE_RELATION_INVALID;
        return false;
    }
    return true;
}

MotiveRelationResult motive_relation_apply_reduce_to_frame(const MotiveSketch *in, const MotiveRelationStep *step,
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
    return drop_notes(in, out, drop);
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
                if (motive_relation_midi(pitch)) {
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
        if (!motive_relation_midi(pairs[k][0]) || !motive_relation_midi(pairs[k][1])) {
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

MotiveRelationResult motive_relation_apply_add_non_chord_tone(const MotiveSketch *in, const MotiveRelationStep *step,
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
    Rational half = msk_div(in->notes[before].duration, msk_int(2));
    Rational part = msk_div(half, msk_int(added));
    if (!motive_relation_nice_time(in, half) || !motive_relation_nice_time(in, part)) {
        return MOTIVE_RELATION_NOT_EXPRESSIBLE;
    }
    MotiveSketchNote notes[2];
    for (size_t k = 0; k < added; k++) {
        notes[k].pitch = in->notes[before].pitch;
        notes[k].duration = part;
        notes[k].onset = msk_add(msk_add(in->notes[before].onset, half), msk_mul(part, msk_int(k)));
    }
    if (!motive_relation_insert_notes(in, out, before + 1, notes, added)) {
        return MOTIVE_RELATION_NOT_APPLICABLE;
    }
    out->notes[before].duration = half;
    bool formed = added == 2 ? form_pair(out, before + 1, (NonChordTone)type)
                             : form_single(out, before + 1, (NonChordTone)type);
    return formed ? MOTIVE_RELATION_OK : MOTIVE_RELATION_NOT_APPLICABLE;
}

MotiveRelationResult motive_relation_apply_remove_non_chord_tone(const MotiveSketch *in, const MotiveRelationStep *step,
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
    return drop_notes(in, out, drop);
}

MotiveRelationResult motive_relation_apply_change_non_chord_tone(const MotiveSketch *in, const MotiveRelationStep *step,
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

MotiveRelationResult motive_relation_apply_reharmonize_frame(const MotiveSketch *in, const MotiveRelationStep *step,
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
    MotiveSketch before = *out;
    MotiveSketchChord chord = {msk_int(0), (int)degree, 0, 0, step->value[1] != 0};
    out->chords[0] = chord;
    out->chord_count = 1;
    motive_relation_adapt_melody(&before, out);
    return MOTIVE_RELATION_OK;
}
