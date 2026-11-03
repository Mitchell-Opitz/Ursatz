#include <ursatz_composer/motive_random.h>

#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/motive_analysis.h>

#include "motive/motive_internal.h"

/*
 * Random motives: a rhythm drawn in half-beats, then a frame of chord tones of I (a walk on the tonic triad) on the beats
 * of the meter and on the first and last note, then each note between two frame notes either another chord tone or a
 * non-chord tone that the tone roles label as the type drawn for it. A motive is kept only if it passes the melodic rules
 * of motive invention. The defaults below are the whole of what shapes a random motive; tune them here.
 */

#define MIN_BARS 1
#define MAX_BARS 2
#define PICKUP_ONE_IN 4        /* a pickup (half a beat or a beat) in one motive of this many */
#define MIN_NOTES 2
#define MAX_NOTES 12
#define MAX_RANGE_SEMITONES 19 /* a twelfth */
#define PITCH_LOW 48           /* C3 */
#define PITCH_HIGH 84          /* C6 */
#define BASE_POSITION 35       /* degree 1 of octave 0: the tonic in MIDI 60 to 71 */
#define BIG_LEAP_RUNGS 2       /* a frame move of this many chord tones or more; the next move is one chord tone back */
#define REPEAT_ONE_IN 8        /* a repeated frame pitch in one move of this many */
#define OFF_FRAME_REACH 5      /* scale steps either side of the frame note before, searched for a non-chord tone */
#define ATTEMPTS_PER_MOTIVE 256
#define STYLE_CANDIDATES 8   /* candidates drawn per motive when there is a style */

/* Lengths in half-beats (1, 2 and 4: the beat unit's half, itself and double) and how often each is drawn. */
static const long DURATIONS[] = {1, 2, 4};
static const unsigned long DURATION_WEIGHTS[] = {2, 4, 1};
#define DURATION_KINDS (sizeof(DURATIONS) / sizeof(DURATIONS[0]))

/*
 * The frame is a walk on the chord tones of I, the scale steps 0, 2 and 4 of each octave (rungs of a ladder, three to an
 * octave). A move is a number of rungs, one most often, and a motive opens on degree 1, 3 or 5.
 */
static const long TRIAD_STEPS[] = {0, 2, 4};
#define TRIAD_SIZE (sizeof(TRIAD_STEPS) / sizeof(TRIAD_STEPS[0]))
static const int FRAME_MOVES[] = {1, 1, 1, 1, 1, 1, 2, 2};
#define FRAME_MOVE_KINDS (sizeof(FRAME_MOVES) / sizeof(FRAME_MOVES[0]))
#define BASE_RUNG ((BASE_POSITION / 7) * (long)TRIAD_SIZE)

/*
 * What a note between two frame notes may be, drawn with equal weight: another chord tone, or a non-chord tone of one of
 * these types. The harmony stays I and the beats stay on chord tones, so a suspension, retardation and anticipation (they
 * need a chord change), a double neighbor and a changing tone (they need two non-chord notes side by side) and a pedal
 * cannot form, and are not drawn.
 */
static const NonChordTone DECORATIONS[] = {NON_CHORD_TONE_PASSING, NON_CHORD_TONE_NEIGHBOR, NON_CHORD_TONE_APPOGGIATURA,
                                           NON_CHORD_TONE_ESCAPE_TONE};
#define DECORATION_KINDS (sizeof(DECORATIONS) / sizeof(DECORATIONS[0]))

const char *motive_random_result_name(MotiveRandomResult result) {
    switch (result) {
    case MOTIVE_RANDOM_OK:
        return "ok";
    case MOTIVE_RANDOM_ERR_NULL:
        return "null";
    case MOTIVE_RANDOM_ERR_ARGUMENT:
        return "argument";
    case MOTIVE_RANDOM_ERR_METER:
        return "meter";
    case MOTIVE_RANDOM_ERR_KEY:
        return "key";
    case MOTIVE_RANDOM_ERR_UNSATISFIABLE:
        return "unsatisfiable";
    case MOTIVE_RANDOM_ERR_ALLOC:
        return "alloc";
    }
    return "unknown";
}

/* ---------------------------------------------------------------- rhythm */

typedef struct {
    long ticks[MAX_NOTES];
    bool on_beat[MAX_NOTES];
    size_t count;
    long pickup;
    long total;
} Rhythm;

/*
 * Tiles the pickup and the bars with notes of 1, 2 or 4 half-beats (ticks). A note stays inside its bar, and a note of
 * a beat or more starts on a beat. Positions are counted from the barline, so the pickup is negative.
 */
static bool draw_rhythm(const PlanMeter *meter, FillRng *rng, Rhythm *out) {
    long bar = 2L * meter->beats_per_bar;
    long bars = MIN_BARS + (long)fill_rng_below(rng, MAX_BARS - MIN_BARS + 1);
    memset(out, 0, sizeof(*out));
    if (fill_rng_below(rng, PICKUP_ONE_IN) == 0) {
        out->pickup = 1 + (long)fill_rng_below(rng, 2);
    }
    out->total = out->pickup + bars * bar;
    for (long position = 0; position < out->total;) {
        long from_barline = position - out->pickup;
        long room = from_barline < 0 ? -from_barline : bar - from_barline % bar;
        bool on_beat = (from_barline % 2 + 2) % 2 == 0;
        unsigned long weights[DURATION_KINDS];
        unsigned long sum = 0;
        for (size_t i = 0; i < DURATION_KINDS; i++) {
            bool fits = DURATIONS[i] <= room && (DURATIONS[i] == 1 || on_beat);
            weights[i] = fits ? DURATION_WEIGHTS[i] : 0;
            sum += weights[i];
        }
        unsigned long pick = (unsigned long)fill_rng_below(rng, sum);
        size_t chosen = 0;
        while (pick >= weights[chosen]) {
            pick -= weights[chosen++];
        }
        if (out->count == MAX_NOTES) {
            return false;
        }
        out->on_beat[out->count] = on_beat;
        out->ticks[out->count++] = DURATIONS[chosen];
        position += DURATIONS[chosen];
    }
    return out->count >= MIN_NOTES;
}

/* ----------------------------------------------------------------- pitch */

typedef struct {
    int pitches[MAX_NOTES];
    bool decorated[MAX_NOTES]; /* a non-chord tone of types[i] */
    NonChordTone types[MAX_NOTES];
} Melody;

static long rung_position(long rung) {
    return (rung / (long)TRIAD_SIZE) * 7 + TRIAD_STEPS[rung % (long)TRIAD_SIZE];
}

static bool is_chord_step(long position) {
    long step = ((position % 7) + 7) % 7;
    return step == TRIAD_STEPS[0] || step == TRIAD_STEPS[1] || step == TRIAD_STEPS[2];
}

static bool in_range(int pitch) {
    return pitch >= PITCH_LOW && pitch <= PITCH_HIGH;
}

/* The figure a note makes between the pitch before and the pitch after it, by the semitone rules of the tone roles. */
static bool forms(NonChordTone type, int before, int pitch, int after) {
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
    default:
        return false;
    }
}

/*
 * The frame: a walk on the chord tones of I from degree 1, 3 or 5, for the first and last note and every note that starts
 * on a beat. Steps (one chord tone) most often, a repeated pitch now and then, and a move of two or more chord tones is
 * followed by one chord tone back. The draw fails when a note leaves PITCH_LOW to PITCH_HIGH.
 */
static bool draw_frame(const AnalysisKey *key, const Rhythm *rhythm, FillRng *rng, long *positions, Melody *melody) {
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
        positions[i] = rung_position(rung);
        melody->pitches[i] = motive_position_pitch(key, positions[i], 0);
        if (!in_range(melody->pitches[i])) {
            return false;
        }
    }
    return true;
}

/*
 * Fills the notes between two frame notes: a type is drawn for each (or none), and a note takes a non-chord pitch near the
 * frame note before it that makes the figure with the frame notes either side, else a chord tone near it.
 */
static bool draw_between(const AnalysisKey *key, const Rhythm *rhythm, FillRng *rng, long *positions, Melody *melody) {
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
                if (!is_chord_step(p) && in_range(pitch) &&
                    forms(DECORATIONS[kind - 1], melody->pitches[i - 1], pitch, after_pitch)) {
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
                if (is_chord_step(p) && in_range(motive_position_pitch(key, p, 0))) {
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

/* The frame, the notes between, and a range of no more than a twelfth. */
static bool draw_melody(const AnalysisKey *key, const Rhythm *rhythm, FillRng *rng, Melody *melody) {
    long positions[MAX_NOTES];
    memset(melody, 0, sizeof(*melody));
    if (!draw_frame(key, rhythm, rng, positions, melody) || !draw_between(key, rhythm, rng, positions, melody)) {
        return false;
    }
    int low = 127;
    int high = 0;
    for (size_t i = 0; i < rhythm->count; i++) {
        low = melody->pitches[i] < low ? melody->pitches[i] : low;
        high = melody->pitches[i] > high ? melody->pitches[i] : high;
    }
    return high - low <= MAX_RANGE_SEMITONES;
}

/* -------------------------------------------------------------- candidate */

static bool build_motive(const MotiveRandomOptions *options, const AnalysisKey *key, const Rhythm *rhythm,
                         const Melody *melody, Motive *out) {
    unsigned long den = 2UL * (unsigned long)options->meter.beat_unit;
    memset(out, 0, sizeof(*out));
    out->meter = options->meter;
    out->key = *key;
    out->key_source = MOTIVE_KEY_GIVEN;
    long onset = 0;
    for (size_t i = 0; i < rhythm->count; i++) {
        out->notes[i].midi = melody->pitches[i];
        out->notes[i].spelled = false;
        if (rational_make(onset, den, &out->notes[i].offset) != RATIONAL_OK ||
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

/*
 * True when the tone roles read the motive as it was drawn: the base harmony is I, proposed, every frame or chord note is
 * a chord tone, and every other note is a non-chord tone of its type that resolves.
 */
static bool tones_as_drawn(const Motive *motive, const Melody *melody) {
    MotiveAnalysis analysis;
    if (!motive_analyze(motive, &analysis) || !analysis.base_harmony_known || !analysis.tones_known ||
        analysis.base_harmony.given || analysis.base_harmony.count != 1 || analysis.base_harmony.chords[0].degree != 1) {
        return false;
    }
    for (size_t i = 0; i < motive->note_count; i++) {
        const MotiveToneLabel *label = &analysis.tones[i];
        bool expected = melody->decorated[i] ? label->role == MOTIVE_TONE_NON_CHORD && label->type == melody->types[i] &&
                                                   !label->unresolved
                                             : label->role == MOTIVE_TONE_CHORD;
        if (!expected) {
            return false;
        }
    }
    return true;
}

/*
 * One full motive that passes the melodic rules. *found is false when the draw did not (try again); the result is false
 * only when the check itself could not run.
 */
static bool draw_candidate(const MotiveRandomOptions *options, const AnalysisKey *key, FillRng *rng, Motive *out,
                           bool *found) {
    Rhythm rhythm;
    Melody melody;
    const char *fault = NULL;
    *found = false;
    if (!draw_rhythm(&options->meter, rng, &rhythm) || !draw_melody(key, &rhythm, rng, &melody) ||
        !build_motive(options, key, &rhythm, &melody, out) || !tones_as_drawn(out, &melody)) {
        return true;
    }
    if (!motive_melodic_fault(out, &fault)) {
        return false;
    }
    *found = fault == NULL;
    return true;
}

/* The form of a full motive, read back by the reader of that form. */
static bool apply_form(const MotiveRandomOptions *options, const Motive *full, Motive *out) {
    if (options->form == MOTIVE_FORM_FULL) {
        *out = *full;
        return true;
    }
    MotiveOptions read;
    memset(&read, 0, sizeof(read));
    read.has_meter = true;
    read.meter = full->meter;
    read.has_key = true;
    read.key = full->key;
    bool rhythm = options->form == MOTIVE_FORM_RHYTHM;
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

static bool already_made(const Motive *made, size_t count, const Motive *motive) {
    for (size_t i = 0; i < count; i++) {
        if (motive_equal(&made[i], motive)) {
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ style */

/*
 * How close a full candidate is to the style: its melody labels, and of those only the category the form keeps (the
 * durations of a rhythm, the contour of a contour), scored by composer_style_closeness. False on allocation failure.
 */
static bool candidate_closeness(const MotiveRandomOptions *options, const Motive *full, ComposerRatio *out) {
    ComposerStyleNote notes[MAX_NOTES];
    for (size_t i = 0; i < full->note_count; i++) {
        notes[i].pitch = full->notes[i].midi;
        notes[i].duration_numerator = (unsigned long)full->notes[i].duration.num;
        notes[i].duration_denominator = full->notes[i].duration.den;
    }
    ComposerStyleLabels *all = composer_style_labels_create();
    ComposerStyleLabels *kept = options->form == MOTIVE_FORM_FULL ? all : composer_style_labels_create();
    const char *category_kept = options->form == MOTIVE_FORM_RHYTHM ? "duration" : "contour";
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
static int ratio_compare(const ComposerRatio *a, const ComposerRatio *b) {
    __extension__ typedef unsigned __int128 wide;
    wide left = (wide)a->numerator * b->denominator;
    wide right = (wide)b->numerator * a->denominator;
    return left < right ? -1 : left > right ? 1 : 0;
}

/* ---------------------------------------------------------------- the call */

static bool valid_beat_unit(int unit) {
    return unit == 1 || unit == 2 || unit == 4 || unit == 8 || unit == 16;
}

MotiveRandomResult motive_random(const MotiveRandomOptions *options, Motive *out, size_t *out_count) {
    if (!options || !out || !out_count) {
        return MOTIVE_RANDOM_ERR_NULL;
    }
    *out_count = 0;
    if ((options->mode != KEY_STRUCTURE_MODE_MAJOR && options->mode != KEY_STRUCTURE_MODE_MINOR) ||
        (options->form != MOTIVE_FORM_FULL && options->form != MOTIVE_FORM_RHYTHM &&
         options->form != MOTIVE_FORM_CONTOUR) ||
        options->count < 1 || options->count > MOTIVE_RANDOM_MAX_COUNT) {
        return MOTIVE_RANDOM_ERR_ARGUMENT;
    }
    if (options->meter.beats_per_bar < 1 || options->meter.beats_per_bar > 64 ||
        !valid_beat_unit(options->meter.beat_unit)) {
        return MOTIVE_RANDOM_ERR_METER;
    }
    AnalysisKey key;
    if (options->has_key) {
        key = options->key;
        if (key.mode != options->mode || analysis_key_tonic_pitch_class(&key) < 0 || analysis_key_tonic_pitch_class(&key) > 11) {
            return MOTIVE_RANDOM_ERR_KEY;
        }
    } else {
        key = plan_key_of_pitch_class(options->mode == KEY_STRUCTURE_MODE_MAJOR ? 0 : 9, options->mode);
    }

    FillRng rng;
    fill_rng_seed(&rng, options->seed);
    size_t want = options->style ? STYLE_CANDIDATES : 1;
    size_t made = 0;
    while (made < options->count) {
        Motive pool[STYLE_CANDIDATES];
        size_t have = 0;
        size_t best = 0;
        size_t ties = 0;
        ComposerRatio best_closeness = {0, 1};
        for (size_t attempt = 0; have < want && attempt < ATTEMPTS_PER_MOTIVE; attempt++) {
            Motive full;
            bool found;
            if (!draw_candidate(options, &key, &rng, &full, &found)) {
                return MOTIVE_RANDOM_ERR_ALLOC;
            }
            if (!found || !apply_form(options, &full, &pool[have]) || already_made(out, made, &pool[have]) ||
                already_made(pool, have, &pool[have])) {
                continue;
            }
            if (options->style) {
                ComposerRatio closeness;
                if (!candidate_closeness(options, &full, &closeness)) {
                    return MOTIVE_RANDOM_ERR_ALLOC;
                }
                int order = have == 0 ? 1 : ratio_compare(&closeness, &best_closeness);
                if (order > 0) {
                    best = have;
                    best_closeness = closeness;
                    ties = 1;
                } else if (order == 0 && fill_rng_below(&rng, ++ties) == 0) {
                    best = have;
                }
            }
            have++;
        }
        if (have == 0) {
            return MOTIVE_RANDOM_ERR_UNSATISFIABLE;
        }
        out[made++] = pool[best];
    }
    *out_count = made;
    return MOTIVE_RANDOM_OK;
}
