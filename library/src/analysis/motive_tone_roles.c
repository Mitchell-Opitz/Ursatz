#include "analysis/motive_tone_roles.h"

#include <string.h>

typedef struct {
    int pcs[MOTIVE_SKETCH_MAX_NOTES][4];
    size_t count[MOTIVE_SKETCH_MAX_NOTES];
    bool chord_tone[MOTIVE_SKETCH_MAX_NOTES];
} Frame;

static int pitch_class(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

static bool has_pc(const int *pcs, size_t count, int pc) {
    for (size_t i = 0; i < count; i++) {
        if (pcs[i] == pc) {
            return true;
        }
    }
    return false;
}

static const MotiveSketchChord *chord_at(const MotiveSketch *sketch, ExactRational onset) {
    const MotiveSketchChord *found = NULL;
    for (size_t c = 0; c < sketch->chord_count; c++) {
        if (msk_cmp(sketch->chords[c].onset, onset) <= 0) {
            found = &sketch->chords[c];
        }
    }
    return found;
}

static bool known(const MotiveSketch *s, size_t i) {
    return i < s->note_count && s->notes[i].pitch != MOTIVE_SKETCH_PITCH_UNKNOWN;
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

static void build_frame(const MotiveSketch *s, Frame *f) {
    const MotiveSketchChord tonic = {{0, 1}, 1, 0, 0, false};
    for (size_t i = 0; i < s->note_count; i++) {
        const MotiveSketchChord *chord = chord_at(s, s->notes[i].onset);
        f->count[i] = motive_sketch_chord_tones(s, chord ? chord : &tonic, f->pcs[i]);
        f->chord_tone[i] =
            known(s, i) && has_pc(f->pcs[i], f->count[i], pitch_class(s->notes[i].pitch));
    }
}

/* Whether the chords of notes i and j hold different pitch classes. */
static bool chord_differs(const Frame *f, size_t i, size_t j) {
    if (f->count[i] != f->count[j]) {
        return true;
    }
    for (size_t k = 0; k < f->count[i]; k++) {
        if (!has_pc(f->pcs[j], f->count[j], f->pcs[i][k])) {
            return true;
        }
    }
    return false;
}

/* Pedal: the run of one pitch around note i crosses two or more chord changes. */
static bool in_pedal(const MotiveSketch *s, const Frame *f, size_t i) {
    size_t first = i;
    size_t last = i;
    while (first > 0 && known(s, first - 1) && s->notes[first - 1].pitch == s->notes[i].pitch) {
        first--;
    }
    while (known(s, last + 1) && s->notes[last + 1].pitch == s->notes[i].pitch) {
        last++;
    }
    size_t changes = 0;
    for (size_t j = first; j < last; j++) {
        if (chord_differs(f, j, j + 1)) {
            changes++;
        }
    }
    return changes >= 2;
}

/* The double neighbor or changing tone that note i starts (c t d c); false when it does not. */
static bool pair_at(const MotiveSketch *s, const Frame *f, size_t i, NonChordTone *type) {
    if (i == 0 || !known(s, i + 2) || !known(s, i + 1) || !known(s, i - 1)) {
        return false;
    }
    if (!f->chord_tone[i - 1] || f->chord_tone[i + 1] || !f->chord_tone[i + 2]) {
        return false;
    }
    int c = s->notes[i - 1].pitch;
    int t = s->notes[i].pitch;
    int d = s->notes[i + 1].pitch;
    if (s->notes[i + 2].pitch != c || !is_step(c, t) || !is_step(d, c)) {
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
static bool classify(const MotiveSketch *s, const Frame *f, size_t i, NonChordTone *type, size_t *partner,
                     bool *has_partner) {
    *has_partner = false;
    bool has_a = i > 0 && known(s, i - 1);
    bool has_d = known(s, i + 1);
    int t = s->notes[i].pitch;
    int a = has_a ? s->notes[i - 1].pitch : 0;
    int d = has_d ? s->notes[i + 1].pitch : 0;

    if (has_a && has_d && t == a && chord_differs(f, i - 1, i) && f->chord_tone[i + 1] && is_step(t, d)) {
        *type = d < t ? NON_CHORD_TONE_SUSPENSION : NON_CHORD_TONE_RETARDATION;
        return true;
    }
    if (has_d && d == t && has_pc(f->pcs[i + 1], f->count[i + 1], pitch_class(d)) && chord_differs(f, i, i + 1)) {
        *type = NON_CHORD_TONE_ANTICIPATION;
        return true;
    }
    if (pair_at(s, f, i, type)) {
        *partner = i + 1;
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
    if (in_pedal(s, f, i)) {
        *type = NON_CHORD_TONE_PEDAL;
        return true;
    }
    return false;
}

/* The chord tone the figure at note i moves to, or -1. */
static long resolution_for(const MotiveSketch *s, const Frame *f, size_t i, MotiveToneRole role, NonChordTone type) {
    if (role == MOTIVE_TONE_CHORD) {
        return -1;
    }
    if (role == MOTIVE_TONE_NON_CHORD && type == NON_CHORD_TONE_PEDAL) {
        size_t j = i + 1;
        while (j < s->note_count && known(s, j) && s->notes[j].pitch == s->notes[i].pitch) {
            j++;
        }
        return j < s->note_count && f->chord_tone[j] ? (long)j : -1;
    }
    bool pair = role == MOTIVE_TONE_NON_CHORD &&
                (type == NON_CHORD_TONE_DOUBLE_NEIGHBOR || type == NON_CHORD_TONE_CHANGING_TONE);
    if (i + 1 < s->note_count && f->chord_tone[i + 1]) {
        return (long)(i + 1);
    }
    if (pair && i + 2 < s->note_count && !f->chord_tone[i + 1] && f->chord_tone[i + 2]) {
        return (long)(i + 2);
    }
    return -1;
}

MotiveToneStatus motive_tone_roles(const MotiveSketch *sketch, MotiveToneLabel *out) {
    if (!sketch || !out || sketch->note_count == 0 || sketch->note_count > MOTIVE_SKETCH_MAX_NOTES) {
        return MOTIVE_TONE_INVALID;
    }
    if (!sketch->has_scale) {
        return MOTIVE_TONE_NO_SCALE;
    }
    Frame frame;
    build_frame(sketch, &frame);
    bool done[MOTIVE_SKETCH_MAX_NOTES] = {false};
    for (size_t i = 0; i < sketch->note_count; i++) {
        MotiveToneLabel *label = &out[i];
        label->type = NON_CHORD_TONE_PASSING;
        label->resolves_to = -1;
        label->unresolved = false;
        if (done[i]) {
            continue;
        }
        if (!known(sketch, i)) {
            label->role = MOTIVE_TONE_UNCLASSIFIED;
            continue;
        }
        if (frame.chord_tone[i]) {
            label->role = MOTIVE_TONE_CHORD;
            continue;
        }
        NonChordTone type = NON_CHORD_TONE_PASSING;
        size_t partner = 0;
        bool has_partner = false;
        label->role = classify(sketch, &frame, i, &type, &partner, &has_partner) ? MOTIVE_TONE_NON_CHORD
                                                                                 : MOTIVE_TONE_UNCLASSIFIED;
        if (label->role == MOTIVE_TONE_NON_CHORD) {
            label->type = type;
        }
        label->resolves_to = resolution_for(sketch, &frame, i, label->role, label->type);
        label->unresolved = label->resolves_to < 0;
        if (has_partner) {
            out[partner] = *label;
            out[partner].resolves_to = resolution_for(sketch, &frame, partner, label->role, label->type);
            out[partner].unresolved = out[partner].resolves_to < 0;
            done[partner] = true;
        }
    }
    return MOTIVE_TONE_OK;
}
