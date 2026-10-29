#include "analysis/motive_tone_roles.h"

#include "analysis/motive_chord_internal.h"
#include "semantics/analysis_note.h"

#include <string.h>

/* The rule core: a line of notes (pitch and the chord mask of each), any length. */
typedef struct {
    const ToneLineNote *notes;
    size_t count;
    ToneLineLabel *out;
} Line;

static bool known(const Line *l, size_t i) {
    return i < l->count && l->notes[i].midi != ANALYSIS_PITCH_UNKNOWN;
}

static int sign_of(int value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

static int span(int a, int b) {
    return a > b ? a - b : b - a;
}

static bool is_step(int a, int b) {
    int d = span(a, b);
    return d == 1 || d == 2;
}

static bool is_leap(int a, int b) {
    return span(a, b) > 2;
}

static int pitch_class(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

static bool in_mask(unsigned mask, int pitch) {
    return (mask >> pitch_class(pitch) & 1u) != 0;
}

/* Whether the chords of notes i and j hold different pitch classes. */
static bool chord_differs(const Line *l, size_t i, size_t j) {
    return (l->notes[i].chord_pitch_classes & 0xFFFu) != (l->notes[j].chord_pitch_classes & 0xFFFu);
}

/* Pedal: the run of one pitch around note i crosses two or more chord changes. */
static bool in_pedal(const Line *l, size_t i) {
    size_t first = i;
    size_t last = i;
    while (first > 0 && known(l, first - 1) && l->notes[first - 1].midi == l->notes[i].midi) {
        first--;
    }
    while (known(l, last + 1) && l->notes[last + 1].midi == l->notes[i].midi) {
        last++;
    }
    size_t changes = 0;
    for (size_t j = first; j < last; j++) {
        if (chord_differs(l, j, j + 1)) {
            changes++;
        }
    }
    return changes >= 2;
}

/* The double neighbor or changing tone that note i starts (c t d c); false when it does not. */
static bool pair_at(const Line *l, size_t i, NonChordTone *type) {
    if (i == 0 || !known(l, i + 2) || !known(l, i + 1) || !known(l, i - 1)) {
        return false;
    }
    if (!l->out[i - 1].chord_tone || l->out[i + 1].chord_tone || !l->out[i + 2].chord_tone) {
        return false;
    }
    int c = l->notes[i - 1].midi;
    int t = l->notes[i].midi;
    int d = l->notes[i + 1].midi;
    if (l->notes[i + 2].midi != c || !is_step(c, t) || !is_step(d, c)) {
        return false;
    }
    if (sign_of(t - c) == sign_of(d - c)) {
        return false;
    }
    int gap = span(t, d);
    *type = (gap == 3 || gap == 4) ? NON_CHORD_TONE_CHANGING_TONE : NON_CHORD_TONE_DOUBLE_NEIGHBOR;
    return true;
}

/* The figure of a non-chord note t = i by the rules, in order; false when none fits. */
static bool classify(const Line *l, size_t i, NonChordTone *type, bool *has_partner) {
    *has_partner = false;
    bool has_a = i > 0 && known(l, i - 1);
    bool has_d = known(l, i + 1);
    int t = l->notes[i].midi;
    int a = has_a ? l->notes[i - 1].midi : 0;
    int d = has_d ? l->notes[i + 1].midi : 0;

    if (has_a && has_d && t == a && chord_differs(l, i - 1, i) && l->out[i + 1].chord_tone && is_step(t, d)) {
        *type = d < t ? NON_CHORD_TONE_SUSPENSION : NON_CHORD_TONE_RETARDATION;
        return true;
    }
    if (has_d && d == t && in_mask(l->notes[i + 1].chord_pitch_classes, d) && chord_differs(l, i, i + 1)) {
        *type = NON_CHORD_TONE_ANTICIPATION;
        return true;
    }
    if (pair_at(l, i, type)) {
        *has_partner = true;
        return true;
    }
    if (has_a && has_d) {
        if (is_step(a, t) && is_step(t, d)) {
            *type = sign_of(t - a) == sign_of(d - t) ? NON_CHORD_TONE_PASSING : NON_CHORD_TONE_NEIGHBOR;
            return true;
        }
        if (is_leap(a, t) && is_step(t, d) && sign_of(t - a) != sign_of(d - t)) {
            *type = NON_CHORD_TONE_APPOGGIATURA;
            return true;
        }
        if (is_step(a, t) && is_leap(t, d) && sign_of(t - a) != sign_of(d - t)) {
            *type = NON_CHORD_TONE_ESCAPE_TONE;
            return true;
        }
    }
    if (in_pedal(l, i)) {
        *type = NON_CHORD_TONE_PEDAL;
        return true;
    }
    return false;
}

/* The chord tone the figure at note i moves to, or -1; non_chord says the note holds a figure. */
static long resolution_for(const Line *l, size_t i, bool non_chord, NonChordTone type) {
    if (non_chord && type == NON_CHORD_TONE_PEDAL) {
        size_t j = i + 1;
        while (j < l->count && known(l, j) && l->notes[j].midi == l->notes[i].midi) {
            j++;
        }
        return j < l->count && l->out[j].chord_tone ? (long)j : -1;
    }
    bool pair = non_chord && (type == NON_CHORD_TONE_DOUBLE_NEIGHBOR || type == NON_CHORD_TONE_CHANGING_TONE);
    if (i + 1 < l->count && l->out[i + 1].chord_tone) {
        return (long)(i + 1);
    }
    if (pair && i + 2 < l->count && !l->out[i + 1].chord_tone && l->out[i + 2].chord_tone) {
        return (long)(i + 2);
    }
    return -1;
}

/* Labels every note of the line by the rules; out has count entries. */
static void label_line(const Line *l) {
    for (size_t i = 0; i < l->count; i++) {
        tone_line_label_clear(&l->out[i]);
        l->out[i].chord_tone = known(l, i) && in_mask(l->notes[i].chord_pitch_classes, l->notes[i].midi);
    }
    bool skip = false;
    for (size_t i = 0; i < l->count; i++) {
        ToneLineLabel *label = &l->out[i];
        if (skip) {
            skip = false;
            continue;
        }
        if (!known(l, i) || label->chord_tone) {
            label->classified = label->chord_tone;
            continue;
        }
        NonChordTone type = NON_CHORD_TONE_PASSING;
        bool has_partner = false;
        label->classified = classify(l, i, &type, &has_partner);
        if (label->classified) {
            label->type = type;
        }
        label->resolves_to = resolution_for(l, i, label->classified, label->type);
        label->unresolved = label->resolves_to < 0;
        if (has_partner) {
            ToneLineLabel *partner = &l->out[i + 1];
            *partner = *label;
            partner->resolves_to = resolution_for(l, i + 1, true, label->type);
            partner->unresolved = partner->resolves_to < 0;
            skip = true;
        }
    }
}

/* Maps a rule-core label to a motive label. */
static void to_motive_label(const ToneLineLabel *from, MotiveToneLabel *to) {
    to->role = from->chord_tone ? MOTIVE_TONE_CHORD
                                : (from->classified ? MOTIVE_TONE_NON_CHORD : MOTIVE_TONE_UNCLASSIFIED);
    to->type = from->classified && !from->chord_tone ? from->type : NON_CHORD_TONE_PASSING;
    to->resolves_to = from->resolves_to;
    to->unresolved = from->unresolved;
    to->overridden = false;
}

bool motive_tone_roles_line(const ToneLineNote *notes, size_t count, ToneLineLabel *out, void *context) {
    (void)context;
    if (!out || (count > 0 && !notes)) {
        return false;
    }
    Line line = {notes, count, out};
    label_line(&line);
    return true;
}

MotiveToneStatus motive_tone_roles(const MotiveSketch *sketch, const MotiveToneOverride *overrides,
                                   size_t override_count, MotiveToneLabel *out) {
    if (!sketch || !out || sketch->note_count == 0 || sketch->note_count > MOTIVE_SKETCH_MAX_NOTES ||
        (override_count > 0 && !overrides)) {
        return MOTIVE_TONE_INVALID;
    }
    if (!sketch->has_scale) {
        return MOTIVE_TONE_NO_SCALE;
    }
    for (size_t k = 0; k < override_count; k++) {
        const MotiveToneOverride *o = &overrides[k];
        if (o->note >= sketch->note_count || (int)o->role < MOTIVE_TONE_CHORD ||
            o->role > MOTIVE_TONE_UNCLASSIFIED ||
            (o->role == MOTIVE_TONE_NON_CHORD && !non_chord_tone_valid(o->type))) {
            return MOTIVE_TONE_BAD_OVERRIDE;
        }
    }
    ToneLineNote notes[MOTIVE_SKETCH_MAX_NOTES];
    ToneLineLabel labels[MOTIVE_SKETCH_MAX_NOTES];
    const MotiveSketchChord tonic = {{0, 1}, 1, 0, 0, false};
    for (size_t i = 0; i < sketch->note_count; i++) {
        const MotiveSketchChord *chord = mch_chord_at(sketch, sketch->notes[i].onset);
        int pcs[4];
        size_t n = motive_sketch_chord_tones(sketch, chord ? chord : &tonic, pcs);
        notes[i].midi = sketch->notes[i].pitch;
        notes[i].onset = sketch->notes[i].onset;
        notes[i].duration = sketch->notes[i].duration;
        notes[i].chord_pitch_classes = 0;
        for (size_t k = 0; k < n; k++) {
            notes[i].chord_pitch_classes |= 1u << mch_pitch_class(pcs[k]);
        }
    }
    Line line = {notes, sketch->note_count, labels};
    label_line(&line);
    for (size_t i = 0; i < sketch->note_count; i++) {
        to_motive_label(&labels[i], &out[i]);
    }
    for (size_t k = 0; k < override_count; k++) {
        const MotiveToneOverride *o = &overrides[k];
        MotiveToneLabel *label = &out[o->note];
        label->role = o->role;
        label->type = o->role == MOTIVE_TONE_NON_CHORD ? o->type : NON_CHORD_TONE_PASSING;
        label->overridden = true;
        label->resolves_to = o->role == MOTIVE_TONE_CHORD
                                 ? -1
                                 : resolution_for(&line, o->note, o->role == MOTIVE_TONE_NON_CHORD, label->type);
        label->unresolved = label->role != MOTIVE_TONE_CHORD && label->resolves_to < 0 && known(&line, o->note);
    }
    return MOTIVE_TONE_OK;
}

MotiveToneStatus motive_tone_roles_of(const MotiveSketch *sketch, MotiveToneLabel *out) {
    if (!sketch) {
        return MOTIVE_TONE_INVALID;
    }
    if (sketch->tone_override_count > MOTIVE_SKETCH_MAX_NOTES) {
        return MOTIVE_TONE_BAD_OVERRIDE;
    }
    return motive_tone_roles(sketch, sketch->tone_overrides, sketch->tone_override_count, out);
}
