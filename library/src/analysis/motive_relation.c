#include "analysis/motive_relation.h"

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
};

typedef char table_has_every_kind[sizeof(TABLE) / sizeof(TABLE[0]) == MOTIVE_RELATION_KIND_COUNT ? 1 : -1];

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
