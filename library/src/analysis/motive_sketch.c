#include "analysis/motive_sketch.h"

#include <string.h>

static const int MAJOR_STEPS[7] = {0, 2, 4, 5, 7, 9, 11};
static const int MINOR_STEPS[7] = {0, 2, 3, 5, 7, 8, 10};

static long floor_div(long a, long b) {
    long quotient = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? quotient - 1 : quotient;
}

static const int *scale_steps(const MotiveSketch *sketch) {
    return sketch->minor ? MINOR_STEPS : MAJOR_STEPS;
}

/* --- equality and validation --------------------------------------------- */

bool motive_sketch_equal(const MotiveSketch *a, const MotiveSketch *b) {
    if (a->note_count != b->note_count || a->has_scale != b->has_scale || a->chord_count != b->chord_count ||
        a->feature_count != b->feature_count || a->accompaniment != b->accompaniment) {
        return false;
    }
    if (a->has_scale && (a->tonic_pc != b->tonic_pc || a->minor != b->minor)) {
        return false;
    }
    if (!msk_eq(a->bar, b->bar) || !msk_eq(a->beat, b->beat) || !msk_eq(a->pickup, b->pickup) ||
        !msk_eq(a->length, b->length)) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (a->notes[i].pitch != b->notes[i].pitch || !msk_eq(a->notes[i].onset, b->notes[i].onset) ||
            !msk_eq(a->notes[i].duration, b->notes[i].duration)) {
            return false;
        }
    }
    for (size_t i = 0; i < a->chord_count; i++) {
        const MotiveSketchChord *x = &a->chords[i];
        const MotiveSketchChord *y = &b->chords[i];
        if (!msk_eq(x->onset, y->onset) || x->degree != y->degree || x->alteration != y->alteration ||
            x->inversion != y->inversion || x->seventh != y->seventh) {
            return false;
        }
    }
    for (size_t i = 0; i < a->feature_count; i++) {
        if (strcmp(a->features[i].label, b->features[i].label) != 0 || a->features[i].first != b->features[i].first ||
            a->features[i].last != b->features[i].last) {
            return false;
        }
    }
    return true;
}

/* --- fingerprint --------------------------------------------------------------------------------- */

#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

static unsigned long long fnv_byte(unsigned long long hash, unsigned char byte) {
    return (hash ^ byte) * FNV_PRIME;
}

/* A 64-bit value as 8 bytes, least significant first, whatever the platform. */
static unsigned long long fnv_value(unsigned long long hash, unsigned long long value) {
    for (int i = 0; i < 8; i++) {
        hash = fnv_byte(hash, (unsigned char)(value >> (8 * i)));
    }
    return hash;
}

static unsigned long long fnv_signed(unsigned long long hash, long long value) {
    return fnv_value(hash, (unsigned long long)value);
}

static unsigned long long fnv_time(unsigned long long hash, Rational time) {
    return fnv_value(fnv_value(hash, time.num), time.den);
}

unsigned long long motive_sketch_fingerprint(const MotiveSketch *sketch) {
    unsigned long long hash = FNV_OFFSET;
    hash = fnv_value(hash, sketch->note_count);
    for (size_t i = 0; i < sketch->note_count; i++) {
        hash = fnv_signed(hash, sketch->notes[i].pitch);
        hash = fnv_time(hash, sketch->notes[i].onset);
        hash = fnv_time(hash, sketch->notes[i].duration);
    }
    hash = fnv_byte(hash, sketch->has_scale ? 1 : 0);
    if (sketch->has_scale) {
        hash = fnv_signed(hash, sketch->tonic_pc);
        hash = fnv_byte(hash, sketch->minor ? 1 : 0);
    }
    hash = fnv_time(hash, sketch->bar);
    hash = fnv_time(hash, sketch->beat);
    hash = fnv_time(hash, sketch->pickup);
    hash = fnv_time(hash, sketch->length);
    hash = fnv_value(hash, sketch->chord_count);
    for (size_t i = 0; i < sketch->chord_count; i++) {
        const MotiveSketchChord *chord = &sketch->chords[i];
        hash = fnv_time(hash, chord->onset);
        hash = fnv_signed(hash, chord->degree);
        hash = fnv_signed(hash, chord->alteration);
        hash = fnv_signed(hash, chord->inversion);
        hash = fnv_byte(hash, chord->seventh ? 1 : 0);
    }
    hash = fnv_value(hash, sketch->feature_count);
    for (size_t i = 0; i < sketch->feature_count; i++) {
        const MotiveSketchFeature *feature = &sketch->features[i];
        for (size_t c = 0; c < MOTIVE_SKETCH_LABEL_SIZE && feature->label[c] != '\0'; c++) {
            hash = fnv_byte(hash, (unsigned char)feature->label[c]);
        }
        hash = fnv_byte(hash, 0);
        hash = fnv_value(hash, feature->first);
        hash = fnv_value(hash, feature->last);
    }
    hash = fnv_byte(hash, sketch->accompaniment ? 1 : 0);
    return hash;
}

bool motive_sketch_validate(const MotiveSketch *sketch) {
    if (!sketch || sketch->note_count == 0 || sketch->note_count > MOTIVE_SKETCH_MAX_NOTES ||
        sketch->chord_count > MOTIVE_SKETCH_MAX_CHORDS || sketch->feature_count > MOTIVE_SKETCH_MAX_FEATURES) {
        return false;
    }
    if (sketch->has_scale && (sketch->tonic_pc < 0 || sketch->tonic_pc > 11)) {
        return false;
    }
    if (!msk_ok(sketch->bar) || !msk_ok(sketch->beat) || !msk_ok(sketch->pickup) || !msk_ok(sketch->length) ||
        sketch->bar.num == 0 || sketch->beat.num == 0 || msk_cmp(sketch->pickup, sketch->bar) >= 0) {
        return false;
    }
    Rational end = msk_int(0);
    for (size_t i = 0; i < sketch->note_count; i++) {
        const MotiveSketchNote *note = &sketch->notes[i];
        if ((note->pitch != MOTIVE_SKETCH_PITCH_UNKNOWN && (note->pitch < 0 || note->pitch > 127)) ||
            !msk_ok(note->onset) || !msk_ok(note->duration) || note->duration.num == 0 ||
            msk_cmp(note->onset, end) < 0) {
            return false;
        }
        end = msk_add(note->onset, note->duration);
        if (!msk_ok(end)) {
            return false;
        }
    }
    if (msk_cmp(sketch->length, end) < 0) {
        return false;
    }
    for (size_t i = 0; i < sketch->chord_count; i++) {
        const MotiveSketchChord *chord = &sketch->chords[i];
        if (!msk_ok(chord->onset) || msk_cmp(chord->onset, sketch->length) >= 0 ||
            (i > 0 && msk_cmp(chord->onset, sketch->chords[i - 1].onset) <= 0) || chord->degree < 1 ||
            chord->degree > 7 || chord->alteration < -1 || chord->alteration > 1 || chord->inversion < 0 ||
            chord->inversion > 3) {
            return false;
        }
    }
    for (size_t i = 0; i < sketch->feature_count; i++) {
        const MotiveSketchFeature *feature = &sketch->features[i];
        size_t label = 0;
        while (label < MOTIVE_SKETCH_LABEL_SIZE && feature->label[label] != '\0') {
            label++;
        }
        if (label == 0 || label >= MOTIVE_SKETCH_LABEL_SIZE || feature->first > feature->last ||
            feature->last >= sketch->note_count) {
            return false;
        }
    }
    return true;
}

/* --- scale ----------------------------------------------------------------- */

long motive_sketch_step(const MotiveSketch *sketch, int pitch, int *alteration) {
    if (!sketch->has_scale) {
        if (alteration) {
            *alteration = 0;
        }
        return pitch;
    }
    const int *steps = scale_steps(sketch);
    long relative = (long)pitch - sketch->tonic_pc;
    long octave = floor_div(relative, 12);
    int inside = (int)(relative - octave * 12);
    int degree = 0;
    int raised = 0;
    bool exact = false;
    for (int d = 0; d < 7 && !exact; d++) {
        if (steps[d] == inside) {
            degree = d;
            exact = true;
        }
    }
    for (int d = 0; d < 7 && !exact; d++) {
        if (steps[d] == inside - 1) {
            degree = d;
            raised = 1;
            break;
        }
    }
    if (alteration) {
        *alteration = raised;
    }
    return octave * 7 + degree;
}

int motive_sketch_pitch_at_step(const MotiveSketch *sketch, long step, int alteration) {
    if (!sketch->has_scale) {
        return (int)step + alteration;
    }
    long octave = floor_div(step, 7);
    int degree = (int)(step - octave * 7);
    return (int)(sketch->tonic_pc + octave * 12 + scale_steps(sketch)[degree] + alteration);
}

int motive_sketch_degree_of_step(const MotiveSketch *sketch, long step) {
    if (!sketch->has_scale) {
        return 0;
    }
    return (int)(step - floor_div(step, 7) * 7) + 1;
}

size_t motive_sketch_chord_tones(const MotiveSketch *sketch, const MotiveSketchChord *chord, int pcs[4]) {
    if (!sketch->has_scale || !chord) {
        return 0;
    }
    const int *steps = scale_steps(sketch);
    int root_index = chord->degree - 1;
    int root = steps[root_index] + chord->alteration;
    size_t count = chord->seventh ? 4 : 3;
    if (chord->alteration != 0) {
        static const int OFFSETS[4] = {0, 4, 7, 10};
        for (size_t i = 0; i < count; i++) {
            pcs[i] = (sketch->tonic_pc + root + OFFSETS[i] + 120) % 12;
        }
        return count;
    }
    for (size_t i = 0; i < count; i++) {
        pcs[i] = (sketch->tonic_pc + steps[(root_index + 2 * (int)i) % 7]) % 12;
    }
    if (sketch->minor && chord->degree == 5) {
        pcs[1] = (sketch->tonic_pc + 11) % 12;
    }
    return count;
}

/* --- contour ----------------------------------------------------------------- */

bool motive_sketch_contour_normalize(const MotiveSketch *in, MotiveSketch *out) {
    if (in->note_count == 0 || in->note_count > MOTIVE_SKETCH_MAX_NOTES) {
        return false;
    }
    int ranks[MOTIVE_SKETCH_MAX_NOTES];
    for (size_t i = 0; i < in->note_count; i++) {
        if (in->notes[i].pitch == MOTIVE_SKETCH_PITCH_UNKNOWN) {
            return false;
        }
        int below = 0;
        for (size_t j = 0; j < in->note_count; j++) {
            if (in->notes[j].pitch >= 0 && in->notes[j].pitch < in->notes[i].pitch) {
                bool counted = false;
                for (size_t k = 0; k < j; k++) {
                    counted = counted || in->notes[k].pitch == in->notes[j].pitch;
                }
                below += counted ? 0 : 1;
            }
        }
        ranks[i] = below;
    }
    MotiveSketch result = *in;
    MotiveSketch scale = *in;
    if (!scale.has_scale) {
        scale.has_scale = true;
        scale.tonic_pc = 0;
        scale.minor = false;
    }
    for (size_t i = 0; i < in->note_count; i++) {
        if (ranks[i] >= 64) {
            return false;
        }
        int pitch = 60 + motive_sketch_pitch_at_step(&scale, ranks[i], 0);
        if (pitch > 127) {
            return false;
        }
        result.notes[i].pitch = pitch;
        result.notes[i].onset = msk_make(i, 4);
        result.notes[i].duration = msk_make(1, 4);
    }
    result.length = msk_make(in->note_count, 4);
    *out = result;
    return true;
}

/* --- time ------------------------------------------------------------------- */

bool motive_sketch_expressible(Rational duration) {
    if (!msk_ok(duration) || duration.num == 0) {
        return false;
    }
    /* 1/2^k times 1, 3/2 or 7/4, for the values w to t. */
    for (unsigned k = 0; k < 6; k++) {
        Rational plain = rational_ratio(1, 1ULL << k);
        Rational one_dot = msk_mul(plain, rational_ratio(3, 2));
        Rational two_dots = msk_mul(plain, rational_ratio(7, 4));
        if (msk_eq(duration, plain) || msk_eq(duration, one_dot) || msk_eq(duration, two_dots)) {
            return true;
        }
    }
    return false;
}

bool motive_sketch_on_grid(Rational onset) {
    return msk_ok(onset) && 32 % onset.den == 0;
}

int motive_sketch_strength(const MotiveSketch *sketch, Rational onset) {
    Rational shift = msk_is_zero(sketch->pickup) ? msk_int(0) : msk_sub(sketch->bar, sketch->pickup);
    Rational inside = msk_mod(msk_add(onset, shift), sketch->bar);
    if (!msk_ok(inside)) {
        return 0;
    }
    if (inside.num == 0) {
        return 2;
    }
    Rational beats = msk_div(inside, sketch->beat);
    return msk_ok(beats) && beats.den == 1 ? 1 : 0;
}
