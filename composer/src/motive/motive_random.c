#include <ursatz_composer/motive_random.h>

#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/fill_rng.h>

#include "motive/motive_internal.h"

/*
 * Random motives: a rhythm drawn in half-beats, then a walk on the key's scale, kept only if it passes the melodic
 * rules of motive invention. The defaults below are the whole of what shapes a random motive; tune them here.
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
#define BIG_LEAP_STEPS 4       /* a leap over a fourth, in scale steps; the next move is one step back */
#define REPEAT_ONE_IN 8        /* a repeated pitch in one move of this many */
#define ATTEMPTS_PER_MOTIVE 256

/* Lengths in half-beats (1, 2 and 4: the beat unit's half, itself and double) and how often each is drawn. */
static const long DURATIONS[] = {1, 2, 4};
static const unsigned long DURATION_WEIGHTS[] = {2, 4, 1};
#define DURATION_KINDS (sizeof(DURATIONS) / sizeof(DURATIONS[0]))

/* Scale steps a move may take, steps most often, and the degrees (1, 3, 5) a motive may open on. */
static const int MOVES[] = {1, 1, 1, 1, 1, 1, 2, 2, 3, 4, 5};
#define MOVE_KINDS (sizeof(MOVES) / sizeof(MOVES[0]))
static const long OPENINGS[] = {0, 2, 4};
#define OPENING_KINDS (sizeof(OPENINGS) / sizeof(OPENINGS[0]))

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
        out->ticks[out->count++] = DURATIONS[chosen];
        position += DURATIONS[chosen];
    }
    return out->count >= MIN_NOTES;
}

/* ----------------------------------------------------------------- pitch */

/*
 * A walk on the key's scale from degree 1, 3 or 5: steps most often, a repeated pitch now and then, leaps up to a sixth,
 * and a leap over a fourth is followed by a step the other way. The draw fails when the walk leaves PITCH_LOW to
 * PITCH_HIGH or spreads over more than a twelfth.
 */
static bool draw_pitches(const PlanKey *key, size_t count, FillRng *rng, int *pitches) {
    long position = BASE_POSITION + OPENINGS[fill_rng_below(rng, OPENING_KINDS)];
    int step_back = 0; /* the direction the next move must take, after a big leap; 0 when free */
    int low = 127;
    int high = 0;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            int move;
            if (step_back != 0) {
                move = step_back;
                step_back = 0;
            } else if (fill_rng_below(rng, REPEAT_ONE_IN) == 0) {
                move = 0;
            } else {
                int size = MOVES[fill_rng_below(rng, MOVE_KINDS)];
                move = fill_rng_below(rng, 2) == 0 ? size : -size;
                if (size >= BIG_LEAP_STEPS) {
                    step_back = move > 0 ? -1 : 1;
                }
            }
            position += move;
        }
        int pitch = motive_position_pitch(key, position, 0);
        if (pitch < PITCH_LOW || pitch > PITCH_HIGH) {
            return false;
        }
        pitches[i] = pitch;
        low = pitch < low ? pitch : low;
        high = pitch > high ? pitch : high;
    }
    return high - low <= MAX_RANGE_SEMITONES;
}

/* -------------------------------------------------------------- candidate */

static bool build_motive(const MotiveRandomOptions *options, const PlanKey *key, const Rhythm *rhythm,
                         const int *pitches, Motive *out) {
    unsigned long den = 2UL * (unsigned long)options->meter.beat_unit;
    memset(out, 0, sizeof(*out));
    out->meter = options->meter;
    out->key = *key;
    out->key_source = MOTIVE_KEY_GIVEN;
    long onset = 0;
    for (size_t i = 0; i < rhythm->count; i++) {
        out->notes[i].midi_pitch = pitches[i];
        if (plan_time_make(onset, den, &out->notes[i].offset) != PLAN_OK ||
            plan_time_make(rhythm->ticks[i], den, &out->notes[i].duration) != PLAN_OK) {
            return false;
        }
        onset += rhythm->ticks[i];
    }
    out->note_count = rhythm->count;
    if (rhythm->pickup > 0 && plan_time_make(rhythm->pickup, den, &out->pickup) != PLAN_OK) {
        return false;
    }
    return plan_time_make(onset, den, &out->length) == PLAN_OK;
}

/*
 * One full motive that passes the melodic rules. *found is false when the draw did not (try again); the result is false
 * only when the check itself could not run.
 */
static bool draw_candidate(const MotiveRandomOptions *options, const PlanKey *key, FillRng *rng, Motive *out,
                           bool *found) {
    Rhythm rhythm;
    int pitches[MAX_NOTES];
    const char *fault = NULL;
    *found = false;
    if (!draw_rhythm(&options->meter, rng, &rhythm) || !draw_pitches(key, rhythm.count, rng, pitches) ||
        !build_motive(options, key, &rhythm, pitches, out)) {
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

/* ---------------------------------------------------------------- the call */

static bool valid_beat_unit(int unit) {
    return unit == 1 || unit == 2 || unit == 4 || unit == 8 || unit == 16;
}

MotiveRandomResult motive_random(const MotiveRandomOptions *options, Motive *out, size_t *out_count) {
    if (!options || !out || !out_count) {
        return MOTIVE_RANDOM_ERR_NULL;
    }
    *out_count = 0;
    if ((options->mode != PLAN_MODE_MAJOR && options->mode != PLAN_MODE_MINOR) ||
        (options->form != MOTIVE_FORM_FULL && options->form != MOTIVE_FORM_RHYTHM &&
         options->form != MOTIVE_FORM_CONTOUR) ||
        options->count < 1 || options->count > MOTIVE_RANDOM_MAX_COUNT) {
        return MOTIVE_RANDOM_ERR_ARGUMENT;
    }
    if (options->meter.beats_per_bar < 1 || options->meter.beats_per_bar > 64 ||
        !valid_beat_unit(options->meter.beat_unit)) {
        return MOTIVE_RANDOM_ERR_METER;
    }
    PlanKey key;
    if (options->has_key) {
        key = options->key;
        if (key.mode != options->mode || key.tonic_pitch_class < 0 || key.tonic_pitch_class > 11) {
            return MOTIVE_RANDOM_ERR_KEY;
        }
    } else {
        key.tonic_pitch_class = options->mode == PLAN_MODE_MAJOR ? 0 : 9;
        key.mode = options->mode;
    }

    FillRng rng;
    fill_rng_seed(&rng, options->seed);
    size_t made = 0;
    while (made < options->count) {
        bool kept = false;
        for (size_t attempt = 0; !kept && attempt < ATTEMPTS_PER_MOTIVE; attempt++) {
            Motive full;
            Motive shown;
            bool found;
            if (!draw_candidate(options, &key, &rng, &full, &found)) {
                return MOTIVE_RANDOM_ERR_ALLOC;
            }
            if (found && apply_form(options, &full, &shown) && !already_made(out, made, &shown)) {
                out[made] = shown;
                kept = true;
            }
        }
        if (!kept) {
            return MOTIVE_RANDOM_ERR_UNSATISFIABLE;
        }
        made++;
    }
    *out_count = made;
    return MOTIVE_RANDOM_OK;
}
