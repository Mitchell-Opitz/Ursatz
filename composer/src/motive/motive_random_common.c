#include "motive/motive_random_internal.h"

#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>

/* The steps the random motive strategies share (motive_random_internal.h). */

static const long TRIAD_STEPS[] = {0, 2, 4};
static const int FRAME_MOVES[] = {1, 1, 1, 1, 1, 1, 2, 2};
#define FRAME_MOVE_KINDS (sizeof(FRAME_MOVES) / sizeof(FRAME_MOVES[0]))

/*
 * What a note between two frame notes may be, drawn with equal weight: another chord tone, or a non-chord tone of one of
 * these types. The harmony stays I and the beats stay on chord tones, so a suspension, retardation and anticipation (they
 * need a chord change), a double neighbor and a changing tone (they need two non-chord notes side by side) and a pedal
 * cannot form, and are not drawn.
 */
const NonChordTone MOTIVE_RANDOM_DECORATIONS[DECORATION_KINDS] = {
    NON_CHORD_TONE_PASSING, NON_CHORD_TONE_NEIGHBOR, NON_CHORD_TONE_APPOGGIATURA, NON_CHORD_TONE_ESCAPE_TONE};
#define DECORATIONS MOTIVE_RANDOM_DECORATIONS

const long MOTIVE_RANDOM_DURATIONS[DURATION_KINDS] = {1, 2, 4};
const unsigned long MOTIVE_RANDOM_DURATION_WEIGHTS[DURATION_KINDS] = {2, 4, 1};

long motive_random_rung_position(long rung) {
    return (rung / (long)TRIAD_SIZE) * 7 + TRIAD_STEPS[rung % (long)TRIAD_SIZE];
}

bool motive_random_is_chord_step(long position) {
    long step = ((position % 7) + 7) % 7;
    return step == TRIAD_STEPS[0] || step == TRIAD_STEPS[1] || step == TRIAD_STEPS[2];
}

bool motive_random_in_range(int pitch) {
    return pitch >= PITCH_LOW && pitch <= PITCH_HIGH;
}

/* The figure a note makes between the pitch before and the pitch after it, by the semitone rules of the tone roles. */
bool motive_random_forms(NonChordTone type, int before, int pitch, int after) {
    int in = pitch - before;
    int out = after - pitch;
    bool in_step = abs(in) >= 1 && abs(in) <= 2;
    bool out_step = abs(out) >= 1 && abs(out) <= 2;
    bool same_way = in != 0 && out != 0 && (in > 0) == (out > 0);
    switch (type) {
    case NON_CHORD_TONE_PASSING:
        return in_step && out_step && same_way;
    case NON_CHORD_TONE_NEIGHBOR:
        return in_step && out_step && !same_way;
    case NON_CHORD_TONE_APPOGGIATURA:
        return abs(in) > 2 && out_step && !same_way;
    case NON_CHORD_TONE_ESCAPE_TONE:
        return in_step && abs(out) > 2 && !same_way;
    case NON_CHORD_TONE_SUSPENSION: /* held over from the note before, then down by step (the caller checks the chord change) */
        return in == 0 && out < 0 && out_step;
    default:
        return false;
    }
}

/*
 * The frame: a walk on the chord tones of I from degree 1, 3 or 5, for the first and last note and every note that starts
 * on a beat. Steps (one chord tone) most often, a repeated pitch now and then, and a move of two or more chord tones is
 * followed by one chord tone back. The draw fails when a note leaves PITCH_LOW to PITCH_HIGH.
 */
bool motive_random_draw_frame(const AnalysisKey *key, const Rhythm *rhythm, FillRng *rng, long *positions, Melody *melody) {
    long rung = BASE_RUNG + (long)fill_rng_below(rng, TRIAD_SIZE);
    int step_back = 0;
    bool first = true;
    for (size_t i = 0; i < rhythm->count; i++) {
        if (!(rhythm->on_beat[i] || i == 0 || i + 1 == rhythm->count)) {
            continue;
        }
        if (!first) {
            int move;
            if (step_back != 0) {
                move = step_back;
                step_back = 0;
            } else if (fill_rng_below(rng, REPEAT_ONE_IN) == 0) {
                move = 0;
            } else {
                int size = FRAME_MOVES[fill_rng_below(rng, FRAME_MOVE_KINDS)];
                move = fill_rng_below(rng, 2) == 0 ? size : -size;
                if (size >= BIG_LEAP_RUNGS) {
                    step_back = move > 0 ? -1 : 1;
                }
            }
            rung += move;
        }
        first = false;
        if (rung < 0) {
            return false;
        }
        positions[i] = motive_random_rung_position(rung);
        melody->pitches[i] = motive_position_pitch(key, positions[i], 0);
        if (!motive_random_in_range(melody->pitches[i])) {
            return false;
        }
    }
    return true;
}

/*
 * Fills the notes between two frame notes: a type is drawn for each (or none), and a note takes a non-chord pitch near the
 * frame note before it that makes the figure with the frame notes either side, else a chord tone near it.
 */
bool motive_random_draw_between(const AnalysisKey *key, const Rhythm *rhythm, FillRng *rng, long *positions, Melody *melody) {
    for (size_t i = 1; i + 1 < rhythm->count; i++) {
        if (rhythm->on_beat[i]) {
            continue;
        }
        long before = positions[i - 1];
        int after_pitch = melody->pitches[i + 1];
        size_t kind = (size_t)fill_rng_below(rng, DECORATION_KINDS + 1);
        long options[2 * OFF_FRAME_REACH + 1];
        size_t option_count = 0;
        if (kind > 0) {
            for (long p = before - OFF_FRAME_REACH; p <= before + OFF_FRAME_REACH; p++) {
                int pitch = motive_position_pitch(key, p, 0);
                if (!motive_random_is_chord_step(p) && motive_random_in_range(pitch) &&
                    motive_random_forms(DECORATIONS[kind - 1], melody->pitches[i - 1], pitch, after_pitch)) {
                    options[option_count++] = p;
                }
            }
        }
        if (option_count > 0) {
            positions[i] = options[fill_rng_below(rng, option_count)];
            melody->decorated[i] = true;
            melody->types[i] = DECORATIONS[kind - 1];
        } else {
            for (long p = before - 4; p <= before + 4; p++) {
                if (motive_random_is_chord_step(p) && motive_random_in_range(motive_position_pitch(key, p, 0))) {
                    options[option_count++] = p;
                }
            }
            if (option_count == 0) {
                return false;
            }
            positions[i] = options[fill_rng_below(rng, option_count)];
        }
        melody->pitches[i] = motive_position_pitch(key, positions[i], 0);
    }
    return true;
}

bool motive_random_build(const MotiveRandomOptions *options, const AnalysisKey *key, const Rhythm *rhythm,
                         const Melody *melody, Motive *out) {
    unsigned long den = (unsigned long)(rhythm->ticks_per_unit > 0 ? rhythm->ticks_per_unit : 2) * (unsigned long)options->meter.beat_unit;
    motive_init(out, options->meter);
    motive_set_key(out, key, MOTIVE_SKETCH_KEY_GIVEN);
    long onset = 0;
    for (size_t i = 0; i < rhythm->count; i++) {
        out->notes[i].pitch = melody->pitches[i];
        if (rational_make(onset, den, &out->notes[i].onset) != RATIONAL_OK ||
            rational_make(rhythm->ticks[i], den, &out->notes[i].duration) != RATIONAL_OK) {
            return false;
        }
        onset += rhythm->ticks[i];
    }
    out->note_count = rhythm->count;
    if (rhythm->pickup > 0 && rational_make(rhythm->pickup, den, &out->pickup) != RATIONAL_OK) {
        return false;
    }
    return rational_make(onset, den, &out->length) == RATIONAL_OK;
}

/* The form of a full motive, read back by the reader of that form. */
bool motive_random_apply_form(const MotiveRandomOptions *options, const Motive *full, Motive *out) {
    if (options->form == MOTIVE_SKETCH_FORM_FULL) {
        *out = *full;
        return true;
    }
    MotiveOptions read;
    memset(&read, 0, sizeof(read));
    read.has_meter = true;
    read.meter = motive_meter(full);
    read.has_key = true;
    read.key = motive_key(full);
    if (full->chord_count > 0) {
        read.has_harmony = true;
        read.harmony_count = full->chord_count;
        memcpy(read.harmony, full->chords, full->chord_count * sizeof(read.harmony[0]));
    }
    if (full->feature_count > 0) {
        read.has_features = true;
        read.feature_count = full->feature_count;
        memcpy(read.features, full->features, full->feature_count * sizeof(read.features[0]));
    }
    bool rhythm = options->form == MOTIVE_SKETCH_FORM_RHYTHM;
    if (rhythm && motive_pickup(full).num != 0) {
        read.has_pickup = true;
        read.pickup = full->pickup;
    }
    char *text = rhythm ? motive_to_rhythm_text(full) : motive_to_contour_text(full);
    bool ok = text && (rhythm ? motive_from_rhythm_text(text, &read, out, NULL)
                              : motive_from_contour_text(text, &read, out, NULL)) == MOTIVE_OK;
    free(text);
    return ok;
}

bool motive_random_already_made(const Motive *made, size_t count, const Motive *motive) {
    for (size_t i = 0; i < count; i++) {
        if (motive_equal(&made[i], motive)) {
            return true;
        }
    }
    return false;
}

bool motive_random_candidate_closeness(const MotiveRandomOptions *options, const Motive *full, ComposerRatio *out) {
    ComposerStyleNote notes[MAX_NOTES];
    for (size_t i = 0; i < full->note_count; i++) {
        notes[i].pitch = full->notes[i].pitch;
        notes[i].duration_numerator = (unsigned long)full->notes[i].duration.num;
        notes[i].duration_denominator = full->notes[i].duration.den;
    }
    ComposerStyleLabels *all = composer_style_labels_create();
    ComposerStyleLabels *kept = options->form == MOTIVE_SKETCH_FORM_FULL ? all : composer_style_labels_create();
    const char *category_kept = options->form == MOTIVE_SKETCH_FORM_RHYTHM ? "duration" : "contour";
    bool ok = all && kept && composer_style_labels_add_melody(all, NULL, notes, full->note_count);
    for (size_t i = 0; ok && kept != all && i < composer_style_labels_count(all); i++) {
        const char *category;
        const char *label;
        ok = composer_style_labels_at(all, i, &category, &label) &&
             (strcmp(category, category_kept) != 0 || composer_style_labels_add(kept, category, label));
    }
    ok = ok && composer_style_closeness(options->style, kept, out);
    if (kept != all) {
        composer_style_labels_destroy(kept);
    }
    composer_style_labels_destroy(all);
    return ok;
}

/* -1, 0 or 1 as a is less than, equal to or more than b. */
int motive_random_ratio_compare(const ComposerRatio *a, const ComposerRatio *b) {
    __extension__ typedef unsigned __int128 wide;
    wide left = (wide)a->numerator * b->denominator;
    wide right = (wide)b->numerator * a->denominator;
    return left < right ? -1 : left > right ? 1 : 0;
}
