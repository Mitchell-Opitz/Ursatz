#include <ursatz_composer/motive_tones.h>

#include <stdio.h>
#include <string.h>

#include <analysis/motive_sketch.h>


#include "motive/motive_internal.h"
#include "util/util.h"

/* The order of the proposed triads, which breaks ties (I first). */
static const int TRIAD_ORDER[7] = {1, 5, 4, 6, 2, 3, 7};

static bool is_chord_tone(const MotiveSketch *sketch, const MotiveSketchChord *chord, int pitch) {
    int pcs[4];
    size_t count = motive_sketch_chord_tones(sketch, chord, pcs);
    for (size_t i = 0; i < count; i++) {
        if (pcs[i] == util_pitch_class(pitch)) {
            return true;
        }
    }
    return false;
}

static bool has_pitch_class(const int *pcs, size_t count, int pc) {
    for (size_t i = 0; i < count; i++) {
        if (pcs[i] == pc) {
            return true;
        }
    }
    return false;
}

/*
 * The chord whose triad holds the most notes, then the most time, the first of I V IV vi ii iii vii on a tie. The
 * library's harmony relations make the same choice for a sketch without chords (motive_relation.c, not exported).
 */
bool motive_implied_chord(const Motive *motive, MotiveSketchChord *out) {
    static const int ORDER[7] = {1, 5, 4, 6, 2, 3, 7};
    if (!motive || !out || !motive->has_scale || motive->form != MOTIVE_SKETCH_FORM_FULL || !motive_sketch_validate(motive)) {
        return false;
    }
    size_t best_hits = 0;
    Rational best_time = msk_int(0);
    bool found = false;
    for (int k = 0; k < 7; k++) {
        MotiveSketchChord chord = {msk_int(0), ORDER[k], 0, 0, false};
        int pcs[4];
        size_t count = motive_sketch_chord_tones(motive, &chord, pcs);
        size_t hits = 0;
        Rational time = msk_int(0);
        for (size_t i = 0; i < motive->note_count; i++) {
            if (has_pitch_class(pcs, count, util_pitch_class(motive->notes[i].pitch))) {
                hits++;
                time = msk_add(time, motive->notes[i].duration);
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

static MotiveSketchChord triad(int degree, Rational onset) {
    MotiveSketchChord chord = {onset, degree, 0, 0, false};
    return chord;
}

typedef struct {
    size_t notes[MOTIVE_SKETCH_MAX_NOTES]; /* the strong-beat notes of the bar */
    size_t count;
} StrongNotes;

/* How many of the strong notes are chord tones when the notes from split on take second and the rest first. */
static size_t strong_chord_tones(const MotiveSketch *sketch, const StrongNotes *strong, const MotiveSketchChord *first,
                                 const MotiveSketchChord *second, Rational split) {
    size_t tones = 0;
    for (size_t i = 0; i < strong->count; i++) {
        const MotiveSketchNote *note = &sketch->notes[strong->notes[i]];
        const MotiveSketchChord *chord = second && msk_cmp(note->onset, split) >= 0 ? second : first;
        if (is_chord_tone(sketch, chord, note->pitch)) {
            tones++;
        }
    }
    return tones;
}

static bool append_chord(MotiveBaseHarmony *out, const MotiveSketchChord *chord) {
    if (out->count > 0) {
        const MotiveSketchChord *last = &out->chords[out->count - 1];
        if (last->degree == chord->degree && last->alteration == chord->alteration && last->inversion == chord->inversion &&
            last->seventh == chord->seventh) {
            return true;
        }
    }
    if (out->count == MOTIVE_MAX_CHORDS) {
        return false;
    }
    MotiveSketchChord *slot = &out->chords[out->count++];
    memset(slot, 0, sizeof(*slot));
    slot->onset = motive_time((long)chord->onset.num, (unsigned long)chord->onset.den);
    slot->degree = chord->degree;
    slot->alteration = chord->alteration;
    slot->inversion = chord->inversion;
    slot->seventh = chord->seventh;
    return true;
}

static void propose(const MotiveSketch *sketch, MotiveBaseHarmony *out) {
    out->count = 0;
    out->given = false;
    long first_bar = motive_sketch_bar_of(sketch, sketch->notes[0].onset);
    long last_bar = motive_sketch_bar_of(sketch, sketch->notes[sketch->note_count - 1].onset);
    MotiveSketchChord previous = triad(1, msk_int(0));
    bool have_previous = false;
    for (long bar = first_bar; bar <= last_bar; bar++) {
        StrongNotes strong;
        strong.count = 0;
        size_t in_bar = 0;
        for (size_t i = 0; i < sketch->note_count; i++) {
            if (motive_sketch_bar_of(sketch, sketch->notes[i].onset) != bar) {
                continue;
            }
            in_bar++;
            if (motive_sketch_on_beat(sketch, sketch->notes[i].onset)) {
                strong.notes[strong.count++] = i;
            }
        }
        if (in_bar == 0) {
            continue; /* the chord before it goes on */
        }
        /* The first chord is at 0, the others at the start of their bar. */
        Rational start = bar < 0 ? msk_int(0)
                                      : msk_add(sketch->pickup, msk_mul(msk_int((unsigned long long)bar), sketch->bar));
        Rational at = have_previous ? start : msk_int(0);
        MotiveSketchChord best = triad(1, at);
        size_t best_tones = 0;
        for (int k = 0; k < 7; k++) {
            MotiveSketchChord candidate = triad(TRIAD_ORDER[k], at);
            size_t tones = strong_chord_tones(sketch, &strong, &candidate, NULL, msk_int(0));
            if (tones > best_tones) {
                best_tones = tones;
                best = candidate;
            }
        }
        if (!have_previous || best.degree != previous.degree) {
            (void)append_chord(out, &best);
        }
        previous = best;
        have_previous = true;
        /* A second chord on a beat inside the bar (not in the short bar before a pickup's first barline). */
        if (bar < 0 || strong.count - best_tones < 2) {
            continue;
        }
        size_t best_gain = 1;
        MotiveSketchChord second = triad(1, msk_int(0));
        Rational bar_end = msk_add(start, sketch->bar);
        for (Rational split = msk_add(start, sketch->beat); msk_ok(split) && msk_cmp(split, bar_end) < 0;
             split = msk_add(split, sketch->beat)) {
            for (int k = 0; k < 7; k++) {
                MotiveSketchChord candidate = triad(TRIAD_ORDER[k], split);
                if (candidate.degree == best.degree) {
                    continue;
                }
                size_t tones = strong_chord_tones(sketch, &strong, &best, &candidate, split);
                size_t gain = tones > best_tones ? tones - best_tones : 0;
                if (gain > best_gain) {
                    best_gain = gain;
                    second = candidate;
                }
            }
        }
        if (best_gain >= 2) {
            (void)append_chord(out, &second);
            previous = second;
        }
    }
}

bool motive_base_harmony(const Motive *motive, MotiveBaseHarmony *out) {
    if (!motive || !out || motive->form != MOTIVE_SKETCH_FORM_FULL || motive->key_source == MOTIVE_SKETCH_KEY_UNSPECIFIED) {
        return false;
    }
    MotiveBaseHarmony result;
    memset(&result, 0, sizeof(result));
    if (motive->chord_count > 0) {
        memcpy(result.chords, motive->chords, motive->chord_count * sizeof(motive->chords[0]));
        result.count = motive->chord_count;
        result.given = true;
        *out = result;
        return true;
    }
    if (!motive_sketch_validate(motive) || !motive->has_scale) {
        return false;
    }
    propose(motive, &result);
    if (result.count == 0) {
        return false;
    }
    *out = result;
    return true;
}

bool motive_harmony_bar_limit_ok(const Motive *motive) {
    if (!motive || motive->chord_count == 0) {
        return true;
    }
    for (size_t i = 0; i < motive->chord_count; i++) {
        long bar_i = motive_sketch_bar_of(motive, motive->chords[i].onset);
        size_t in_bar = 0;
        for (size_t j = 0; j < motive->chord_count; j++) {
            if (motive_sketch_bar_of(motive, motive->chords[j].onset) == bar_i) {
                in_bar++;
            }
        }
        if (in_bar > MOTIVE_MAX_CHORDS_PER_BAR) {
            return false;
        }
    }
    return true;
}

MotiveToneStatus motive_tone_labels(const Motive *motive, const MotiveSketchChord *chords, size_t chord_count,
                                    const MotiveToneOverride *overrides, size_t override_count, MotiveToneLabel *out) {
    if (!motive || (chord_count > 0 && !chords) || chord_count > MOTIVE_SKETCH_MAX_CHORDS) {
        return MOTIVE_TONE_INVALID;
    }
    MotiveSketch sketch = *motive;
    memcpy(sketch.chords, chords, chord_count * sizeof(chords[0]));
    sketch.chord_count = chord_count;
    if (!motive_sketch_validate(&sketch)) {
        return MOTIVE_TONE_INVALID;
    }
    return motive_tone_roles(&sketch, overrides, override_count, out);
}

const char *motive_tone_type_name(NonChordTone type) {
    switch (type) {
    case NON_CHORD_TONE_PASSING:
        return "passing";
    case NON_CHORD_TONE_NEIGHBOR:
        return "neighbor";
    case NON_CHORD_TONE_APPOGGIATURA:
        return "appoggiatura";
    case NON_CHORD_TONE_SUSPENSION:
        return "suspension";
    case NON_CHORD_TONE_RETARDATION:
        return "retardation";
    case NON_CHORD_TONE_ESCAPE_TONE:
        return "escape";
    case NON_CHORD_TONE_ANTICIPATION:
        return "anticipation";
    case NON_CHORD_TONE_DOUBLE_NEIGHBOR:
        return "double-neighbor";
    case NON_CHORD_TONE_CHANGING_TONE:
        return "changing-tone";
    case NON_CHORD_TONE_PEDAL:
        return "pedal";
    default:
        return NULL;
    }
}

bool motive_tone_role_parse(const char *text, MotiveToneRole *role, NonChordTone *type) {
    if (!text || !role || !type) {
        return false;
    }
    *type = NON_CHORD_TONE_PASSING;
    if (strcmp(text, "chord") == 0) {
        *role = MOTIVE_TONE_CHORD;
        return true;
    }
    *role = MOTIVE_TONE_NON_CHORD;
    return motive_tone_type_parse(text, type);
}

bool motive_tone_type_parse(const char *name, NonChordTone *out) {
    if (!name || !out) {
        return false;
    }
    if (strcmp(name, "escape-tone") == 0) {
        *out = NON_CHORD_TONE_ESCAPE_TONE;
        return true;
    }
    for (int type = 0; type < NON_CHORD_TONE_COUNT; type++) {
        if (strcmp(name, motive_tone_type_name((NonChordTone)type)) == 0) {
            *out = (NonChordTone)type;
            return true;
        }
    }
    return false;
}

bool motive_tones_constraint(const Motive *motive, PlanConstraint *out) {
    MotiveBaseHarmony harmony;
    MotiveToneLabel labels[MOTIVE_MAX_NOTES];
    if (!motive || !out || motive->note_count == 0 || !motive_base_harmony(motive, &harmony) ||
        motive_tone_labels(motive, harmony.chords, harmony.count, motive->tone_overrides,
                           motive->tone_override_count, labels) != MOTIVE_TONE_OK) {
        return false;
    }
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_TONES;
    PlanMotiveTones *tones = &constraint.params.motive_tones;
    if (harmony.count > PLAN_TONES_MAX_CHORDS || motive->note_count > PLAN_TONES_MAX_NOTES) {
        return false;
    }
    for (size_t i = 0; i < harmony.count; i++) {
        PlanToneChord *chord = &tones->chords[tones->chord_count++];
        Rational end = i + 1 < harmony.count ? harmony.chords[i + 1].onset : motive->length;
        Rational onset = harmony.chords[i].onset;
        if (!motive_chord_numeral(&harmony.chords[i], chord->numeral, sizeof(chord->numeral)) ||
            rational_make(onset.num, onset.den, &chord->onset) != RATIONAL_OK ||
            rational_sub(end, onset, &chord->length) !=
                RATIONAL_OK) {
            return false;
        }
    }
    for (size_t i = 0; i < motive->note_count; i++) {
        const char *label = labels[i].role == MOTIVE_TONE_CHORD         ? "chord"
                            : labels[i].role == MOTIVE_TONE_NON_CHORD   ? motive_tone_type_name(labels[i].type)
                                                                        : "unclassified";
        if (!label) {
            return false;
        }
        snprintf(tones->labels[tones->label_count++], PLAN_TONE_LABEL_MAX, "%s", label);
    }
    *out = constraint;
    return true;
}

PlanResult motive_node_write_tones(PiecePlan *plan, NodeId node, const Motive *motive) {
    PlanConstraint tones;
    if (!motive_tones_constraint(motive, &tones)) {
        return PLAN_OK;
    }
    for (size_t i = plan_node_constraint_count(plan, node); i > 0; i--) {
        if (plan_node_constraint_at(plan, node, i - 1)->kind == PLAN_CONSTRAINT_MOTIVE_TONES) {
            PlanResult removed = plan_node_remove_constraint(plan, node, i - 1);
            if (removed != PLAN_OK) {
                return removed;
            }
        }
    }
    return plan_node_add_constraint(plan, node, &tones);
}
