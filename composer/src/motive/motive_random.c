#include <ursatz_composer/motive_random.h>

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/motive_analysis.h>

#include "motive/motive_random_internal.h"

/*
 * Random motives: a rhythm drawn in half-beats, then a frame of chord tones of I (a walk on the tonic triad) on the beats
 * of the meter and on the first and last note, then each note between two frame notes either another chord tone or a
 * non-chord tone that the tone roles label as the type drawn for it. A motive is kept only if it passes the melodic rules
 * of motive invention. The defaults below are the whole of what shapes a random motive; tune them here.
 */

#define MIN_BARS 1
#define MAX_BARS 2
#define PICKUP_ONE_IN 4        /* a pickup (half a beat or a beat) in one motive of this many */

#define DURATIONS MOTIVE_RANDOM_DURATIONS
#define DURATION_WEIGHTS MOTIVE_RANDOM_DURATION_WEIGHTS

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

const char *motive_random_strategy_name(MotiveRandomStrategy strategy) {
    return strategy == MOTIVE_RANDOM_STRATEGY_DRAWN ? "drawn" : "segments";
}

bool motive_random_strategy_parse(const char *name, MotiveRandomStrategy *out) {
    if (name && strcasecmp(name, "segments") == 0) {
        *out = MOTIVE_RANDOM_STRATEGY_SEGMENTS;
        return true;
    }
    if (name && strcasecmp(name, "drawn") == 0) {
        *out = MOTIVE_RANDOM_STRATEGY_DRAWN;
        return true;
    }
    return false;
}

/* ---------------------------------------------------------------- rhythm */

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

/* The frame, the notes between, and a range of no more than a twelfth. */
static bool draw_melody(const AnalysisKey *key, const Rhythm *rhythm, FillRng *rng, Melody *melody) {
    long positions[MAX_NOTES];
    memset(melody, 0, sizeof(*melody));
    if (!motive_random_draw_frame(key, rhythm, rng, positions, melody) || !motive_random_draw_between(key, rhythm, rng, positions, melody)) {
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
static bool draw_drawn_candidate(const MotiveRandomOptions *options, const AnalysisKey *key, FillRng *rng, Motive *out,
                           bool *found) {
    Rhythm rhythm;
    Melody melody;
    const char *fault = NULL;
    *found = false;
    if (!draw_rhythm(&options->meter, rng, &rhythm) || !draw_melody(key, &rhythm, rng, &melody) ||
        !motive_random_build(options, key, &rhythm, &melody, out) || !tones_as_drawn(out, &melody)) {
        return true;
    }
    if (!motive_melodic_fault(out, &fault)) {
        return false;
    }
    *found = fault == NULL;
    return true;
}
/* One candidate by the strategy asked for. */
static bool draw_candidate(const MotiveRandomOptions *options, const AnalysisKey *key, FillRng *rng, Motive *out,
                           bool *found) {
    return options->strategy == MOTIVE_RANDOM_STRATEGY_DRAWN ? draw_drawn_candidate(options, key, rng, out, found)
                                                              : motive_random_segments_candidate(options, key, rng, out, found);
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
        (options->form != MOTIVE_SKETCH_FORM_FULL && options->form != MOTIVE_SKETCH_FORM_RHYTHM &&
         options->form != MOTIVE_SKETCH_FORM_CONTOUR) ||
        (options->strategy != MOTIVE_RANDOM_STRATEGY_SEGMENTS && options->strategy != MOTIVE_RANDOM_STRATEGY_DRAWN) ||
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
            if (!found || !motive_random_apply_form(options, &full, &pool[have]) || motive_random_already_made(out, made, &pool[have]) ||
                motive_random_already_made(pool, have, &pool[have])) {
                continue;
            }
            if (options->style) {
                ComposerRatio closeness;
                if (!motive_random_candidate_closeness(options, &full, &closeness)) {
                    return MOTIVE_RANDOM_ERR_ALLOC;
                }
                int order = have == 0 ? 1 : motive_random_ratio_compare(&closeness, &best_closeness);
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
