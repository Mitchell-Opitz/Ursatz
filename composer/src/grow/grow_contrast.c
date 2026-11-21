#include <ursatz_composer/grow_types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/form/hybrid_rules.h>
#include <theory/form/phrase_beginning.h>
#include <theory/form/phrase_ending.h>
#include <theory/form/phrase_level_form.h>
#include <theory/tonality/tonal_key.h>

#include <ursatz_composer/form_return.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <plan/plan_state.h>
#include <ursatz_composer/variant_registry.h>

#include "util/position.h"
#include "structure/plan_util.h"
#include "grow/grow_internal.h"
#include "harmony/harmony_internal.h"
#include "motive/motive_internal.h"
#include "util/util.h"

/* The contrasting idea and the motives that depart from the basic idea: the draws of a contrast, a distant motif and new material. */

/* Whether two developments are the same kind of change by the same amount. */
bool grow_same_kind(const MotiveRelationStep *a, const MotiveRelationStep *b) {
    return a->kind == b->kind && a->value[0] == b->value[0] && a->value[1] == b->value[1];
}

/* The most developments of m0 one contrasting idea is chosen among: inversions, interval changes everywhere and one. */
#define CONTRAST_MAX 160
/* The widest the notes of a contrasting idea may go, so the melody has room to place it. */
#define CONTRAST_LOW 58
#define CONTRAST_HIGH 84

/* Whether the last note of a development lands on the pitch class asked for (any when want is negative). */
static bool lands_on(const Motive *result, int want) {
    return want < 0 || util_pitch_class(motive_pitch(result, result->note_count - 1)) == want;
}

bool grow_shorten_source(GrowThemeSpec *spec, GrowPlace place) {
    const Motive *from = grow_idea_source(spec, place);
    Rational room = spec->layout.length[place];
    if (rational_cmp(from->length, room) <= 0) {
        return true;
    }
    for (size_t notes = from->note_count - 1; notes >= 2; notes--) {
        MotiveRelationStep step;
        Motive result;
        memset(&step, 0, sizeof(step));
        step.kind = MOTIVE_RELATION_FRAGMENT;
        step.value[0] = MOTIVE_FRAGMENT_LEADING;
        step.value[1] = (long)notes;
        step.value[2] = 1;
        if (motive_develop(from, &step, &result) == MOTIVE_DEV_OK && rational_cmp(result.length, room) <= 0) {
            spec->shortened[place] = result;
            spec->has_shortened[place] = true;
            return true;
        }
    }
    return false;
}

bool grow_draw_contrast(const GrowThemeRequest *request, FillRng *rng, const GrowThemeSpec *spec,
                        GrowPlace place, const Motive *source, int want_pc, const MotiveRelationStep *avoid,
                        MotiveRelationStep *out) {
    (void)request;
    Rational room = spec->layout.length[place];
    size_t intervals = source->note_count - 1;
    MotiveRelationStep fits[CONTRAST_MAX];
    MotiveRelationStep exact[CONTRAST_MAX];
    size_t fit_count = 0;
    size_t exact_count = 0;
    bool diatonic = grow_motive_in_range(source, 0, 127, true);

    /* In the key's scale first when the motive is; if no development of it is, in range only. */
    for (int pass = 0; pass < 2 && fit_count == 0; pass++) {
    bool in_scale = pass == 0 && diatonic;
    for (int kind = 0; kind < 3; kind++) {
        /* Mirrored in the key's steps about a note near the first, so the contour turns over; every interval
           widened or narrowed; or the one interval changed. */
        int first = motive_pitch(source, 0);
        int low = kind == 0 ? first - 2 : (kind == 1 ? -2 : 1);
        int high = kind == 0 ? first + 2 : (kind == 1 ? 2 : (int)intervals);
        for (int value = low; value <= high; value++) {
            for (int amount = (kind == 2 ? -4 : 0); amount <= (kind == 2 ? 4 : 0); amount++) {
                MotiveRelationStep dev;
                Motive result;
                memset(&dev, 0, sizeof(dev));
                if (kind == 0) {
                    dev.kind = MOTIVE_RELATION_INVERT;
                    dev.value[0] = value;
                    dev.value[1] = 1;
                } else {
                    dev.kind = MOTIVE_RELATION_INTERVAL_CHANGE;
                    dev.value[0] = kind == 1 ? 0 : value;
                    dev.value[1] = kind == 1 ? value : amount;
                }
                if ((kind != 0 && dev.value[1] == 0) ||
                    motive_develop(source, &dev, &result) != MOTIVE_DEV_OK || grow_same_contour(source, &result) ||
                    rational_cmp(result.length, room) > 0 || (avoid && grow_same_kind(avoid, &dev))) {
                    continue;
                }
                /* In range, and in the key's scale when the motive is, as the motives stage takes a form (grow_form.c). */
                if (!grow_motive_in_range(&result, CONTRAST_LOW, CONTRAST_HIGH, in_scale)) {
                    continue;
                }
                if (fit_count < CONTRAST_MAX) {
                    fits[fit_count++] = dev;
                }
                if (lands_on(&result, want_pc) && exact_count < CONTRAST_MAX) {
                    exact[exact_count++] = dev;
                }
            }
        }
    }
    }
    /* One that lands where the cadence needs it, if there is one. */
    const MotiveRelationStep *pool = exact_count > 0 ? exact : fits;
    size_t count = exact_count > 0 ? exact_count : fit_count;
    if (count == 0) {
        return false;
    }
    *out = pool[fill_rng_below(rng, count)];
    return true;
}

GrowContrastKind grow_draw_contrast_kind(const GrowThemeRequest *request, FillRng *rng, bool new_allowed) {
    uint64_t similar = request->contrast.distance[GROW_CONTRAST_SIMILAR];
    uint64_t distant = request->contrast.distance[GROW_CONTRAST_DISTANT];
    uint64_t fresh = new_allowed ? request->contrast.new_material : 0;
    if (distant == 0 && fresh == 0) {
        return GROW_CONTRAST_KIND_SIMILAR;
    }
    /* Both distance weights 0 with new material to draw: the contrast is new or, when it cannot be made, similar. */
    uint64_t total = similar + distant + fresh;
    uint64_t pick = fill_rng_below(rng, total);
    return pick < similar ? GROW_CONTRAST_KIND_SIMILAR
                          : (pick < similar + distant ? GROW_CONTRAST_KIND_DISTANT : GROW_CONTRAST_KIND_NEW);
}

/* The most rhythms a distant contrast tries. */
#define DISTANT_DRAWS 8

/* Whether two motives have the same onsets and durations over the time both have. */
static bool same_rhythm_over_common(const Motive *a, const Motive *b) {
    Rational common = rational_cmp(a->length, b->length) < 0 ? a->length : b->length;
    size_t i = 0;
    size_t j = 0;
    while (i < a->note_count && rational_cmp(a->notes[i].onset, common) < 0 && j < b->note_count &&
           rational_cmp(b->notes[j].onset, common) < 0) {
        if (rational_cmp(a->notes[i].onset, b->notes[j].onset) != 0 ||
            rational_cmp(a->notes[i].duration, b->notes[j].duration) != 0) {
            return false;
        }
        i++;
        j++;
    }
    bool a_more = i < a->note_count && rational_cmp(a->notes[i].onset, common) < 0;
    bool b_more = j < b->note_count && rational_cmp(b->notes[j].onset, common) < 0;
    return !a_more && !b_more;
}

/* The pitch one step of the key's scale from `pitch` in `direction` (the nearest scale pitch that way). */
static int scale_step(int pitch, int direction, int tonic_pc, const PitchMaterial *material) {
    unsigned scale = standard_tonality_material_pitch_classes_with_leading_tone(0, material);
    return tonic_pc + standard_tonality_mask_step(scale, pitch - tonic_pc, direction);
}

/* One draw of the motif generator for the idea at `place`: a motive of the key and the meter, one bar when the idea has
   the room for one only, else two, from the request's motif settings or the defaults. False when it makes none. */
static bool draw_motif(const GrowThemeRequest *request, FillRng *rng, const GrowThemeSpec *spec, GrowPlace place,
                       Motive *out) {
    Rational two_bars = position_times(request->bar, 2);
    MotiveRandomPreferences preferences;
    if (request->motif) {
        preferences = *request->motif;
    } else {
        motive_random_preferences_default(&preferences);
    }
    bool two = rational_cmp(spec->layout.length[place], two_bars) >= 0;
    preferences.bars[0] = two ? 0 : 1;
    preferences.bars[1] = two ? 1 : 0;
    preferences.pickup_one_in = 0;
    preferences.plan_count = 0;
    MotiveRandomOptions options;
    memset(&options, 0, sizeof(options));
    options.material = request->key.material;
    options.meter = request->meter;
    options.has_key = true;
    options.key = request->key;
    options.form = MOTIVE_SKETCH_FORM_FULL;
    options.count = 1;
    options.seed = (unsigned long)fill_rng_below(rng, 1u << 31);
    options.preferences = &preferences;
    size_t made = 0;
    return motive_random(&options, out, &made) == MOTIVE_RANDOM_OK && made >= 1 && out->note_count >= 2 &&
           rational_cmp(out->length, spec->layout.length[place]) <= 0;
}

/* Whole octaves into the contrast's range, the nearest shift first; false when no shift fits or a note is out of the
   key's scale (a motive in a key). */
static bool move_into_contrast_range(Motive *motive, bool in_scale) {
    static const int SHIFTS[] = {0, 12, -12, 24, -24, 36, -36};
    int low = 127;
    int high = 0;
    for (size_t j = 0; j < motive->note_count; j++) {
        int pitch = motive_pitch(motive, j);
        motive->notes[j].pitch = pitch;
        low = pitch < low ? pitch : low;
        high = pitch > high ? pitch : high;
    }
    for (size_t s = 0; s < sizeof(SHIFTS) / sizeof(SHIFTS[0]); s++) {
        if (low + SHIFTS[s] >= CONTRAST_LOW && high + SHIFTS[s] <= CONTRAST_HIGH) {
            for (size_t j = 0; j < motive->note_count; j++) {
                motive->notes[j].pitch += SHIFTS[s];
            }
            return grow_motive_in_range(motive, CONTRAST_LOW, CONTRAST_HIGH, in_scale);
        }
    }
    return false;
}

bool grow_make_distant(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec, GrowPlace place,
                       const Motive *source, const MotiveRelationStep *step, int want_pc) {
    Motive developed;
    if (motive_develop(source, step, &developed) != MOTIVE_DEV_OK || developed.note_count < 2) {
        return false;
    }
    bool in_scale = grow_motive_in_range(&developed, 0, 127, true);
    Motive first;
    bool have_first = false;
    for (int draw = 0; draw < DISTANT_DRAWS; draw++) {
        Motive rhythm;
        if (!draw_motif(request, rng, spec, place, &rhythm) || same_rhythm_over_common(&rhythm, source)) {
            continue;
        }
        /* The rhythm's notes take the developed motive's pitches, spread over its notes. */
        Motive result = developed;
        size_t k = rhythm.note_count;
        size_t n = developed.note_count;
        int raw[MOTIVE_SKETCH_MAX_NOTES];
        for (size_t j = 0; j < k; j++) {
            size_t from = (j * (n - 1) + (k - 1) / 2) / (k - 1);
            raw[j] = motive_pitch(&developed, from);
        }
        for (size_t j = 0; j < k; j++) {
            int pitch = raw[j];
            if (j > 0 && pitch == result.notes[j - 1].pitch) {
                int direction = 1;
                for (size_t m = j + 1; m < k; m++) {
                    if (raw[m] != pitch) {
                        direction = raw[m] > pitch ? 1 : -1;
                        break;
                    }
                }
                pitch = developed.has_scale ? scale_step(pitch, direction, developed.tonic_pc, developed.material)
                                            : pitch + 2 * direction;
            }
            result.notes[j].pitch = pitch;
            result.notes[j].onset = rhythm.notes[j].onset;
            result.notes[j].duration = rhythm.notes[j].duration;
        }
        result.note_count = k;
        result.length = rhythm.length;
        result.pickup = rhythm.pickup;
        result.chord_count = 0;
        result.feature_count = 0;
        result.tone_override_count = 0;
        if (grow_same_contour(source, &result)) {
            continue;
        }
        if (!move_into_contrast_range(&result, in_scale)) {
            continue;
        }
        if (lands_on(&result, want_pc)) {
            spec->distant[place] = result;
            spec->has_distant[place] = true;
            return true;
        }
        if (!have_first) {
            first = result;
            have_first = true;
        }
    }
    if (!have_first) {
        return false;
    }
    spec->distant[place] = first;
    spec->has_distant[place] = true;
    return true;
}

bool grow_make_new_material(const GrowThemeRequest *request, FillRng *rng, GrowThemeSpec *spec, int want_pc) {
    Motive first;
    bool have_first = false;
    for (int draw = 0; draw < DISTANT_DRAWS; draw++) {
        Motive result;
        if (!draw_motif(request, rng, spec, GROW_PLACE_CONTRAST, &result) || grow_same_contour(&spec->source, &result)) {
            continue;
        }
        result.chord_count = 0;
        result.feature_count = 0;
        result.tone_override_count = 0;
        if (!move_into_contrast_range(&result, true)) {
            continue;
        }
        if (lands_on(&result, want_pc)) {
            first = result;
            have_first = true;
            break;
        }
        if (!have_first) {
            first = result;
            have_first = true;
        }
    }
    if (!have_first) {
        return false;
    }
    spec->slot_source[GROW_SLOT_CONTRASTING] = first;
    spec->has_slot_source[GROW_SLOT_CONTRASTING] = true;
    spec->has_new_material = true;
    return true;
}

