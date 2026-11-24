#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>
#include <theory/tonality/roman_numeral_grammar.h>
#include <ursatz_composer/harmony_chords.h>
#include <ursatz_composer/motive_develop.h>

#include "motive/motive_random_internal.h"
#include "util/util.h"

/*
 * Random motives from segments (cli/docs/cli-random-motive.md): a motive is made of one to three segments (Schoenberg's
 * motives), the way a theme starts. Segment a is a rhythm cell with one distinctive feature; a' repeats a's rhythm and
 * contour on the next chord (the dominant form); b is a new cell. A harmony plan puts one chord on each segment, and the
 * last segment slows down to end on a chord tone of the last chord. The defaults below are the whole of what shapes a
 * segment motive; tune them here.
 */

Grid motive_random_grid_of(const MotiveRandomPreferences *preferences, const PlanMeter *meter) {
    long bar = (long)meter->beats_per_bar;
    if (preferences->compound_beats && (bar == 6 || bar == 9 || bar == 12)) {
        Grid grid = {bar / 3, 3 * TICKS_PER_UNIT, TICKS_PER_UNIT, true, ENDING_FREE, 0};
        return grid;
    }
    Grid grid = {bar, TICKS_PER_UNIT, TICKS_PER_UNIT / 2, false, ENDING_FREE, 0};
    return grid;
}

/* Syncopation needs a simple meter: not a compound grid and not 6, 9 or 12 beats a bar. */
bool motive_random_simple_meter(const Grid *grid) {
    return !grid->compound && (grid->beats == 3 || grid->beats % 3 != 0);
}

/* The short note of a figure: an eighth (a half beat) or a sixteenth (a quarter beat), 6 or 3 ticks. */
long motive_random_sync_unit(const Grid *grid, size_t level) {
    return level == 0 ? grid->half : grid->half / 2;
}

const SegmentPlan PLANS_TWO[3] = {{2, {1, 5, 0}}, {2, {1, 4, 0}}, {2, {1, 1, 0}}};
const SegmentPlan PLANS_THREE[2] = {{3, {1, 5, 1}}, {3, {1, 4, 1}}};

/* ---------------------------------------------------------------- helpers */

size_t motive_random_pick_weighted(FillRng *rng, const unsigned long *weights, size_t count) {
    unsigned long sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += weights[i];
    }
    if (sum == 0) {
        return 0;
    }
    unsigned long pick = (unsigned long)fill_rng_below(rng, sum);
    size_t chosen = 0;
    while (pick >= weights[chosen]) {
        pick -= weights[chosen++];
    }
    return chosen;
}

/* The defaults of the SEGMENTS strategy: what its draws were tuned to. */
void motive_random_preferences_default(MotiveRandomPreferences *out) {
    memset(out, 0, sizeof(*out));
    out->bars[0] = 1; /* a motive of one bar ... */
    out->bars[1] = 1; /* ... and of two bars, as often */
    out->pickup_one_in = 4; /* a pickup (half a beat or a beat), one note before segment a, in one motive of this many */
    out->shapes_one_bar[0] = 3;
    out->shapes_one_bar[1] = 2;
    out->shapes_two_bars[0] = 3;
    out->shapes_two_bars[1] = 2;
    out->shapes_two_bars[2] = 2;
    out->plans_two[0] = 3;
    out->plans_two[1] = 1;
    out->plans_two[2] = 1;
    out->plans_three[0] = 2;
    out->plans_three[1] = 1;
    for (size_t i = 0; i < 3; i++) {
        out->features[i] = 1; /* drawn with equal weight */
    }
    out->leap_low = 5; /* a leap of a fourth (5 semitones) ... */
    out->leap_high = 9; /* ... to a sixth (9 semitones) */
    out->rung_moves[0] = 6; /* one rung most often ... */
    out->rung_moves[1] = 2; /* ... two now and then */
    for (size_t i = 0; i < 5; i++) {
        out->decorations[i] = 1;
    }
    for (size_t i = 0; i < DURATION_KINDS; i++) {
        out->durations[i] = MOTIVE_RANDOM_DURATION_WEIGHTS[i];
    }
    /* The listening settings (R-420 to R-422), all off. */
    for (size_t i = 0; i < 3; i++) {
        out->accented_types[i] = 1;
    }
    out->leaps_per_motif = MAX_NOTES - 1; /* no motive has more intervals, so this never rejects */
    out->contrast_fidelity_min.den = 1;
    /* Endings and long non-chord tones (R-440, R-441), all off. */
    out->endings[0] = 1; /* free, as drawn */
    for (size_t i = 0; i < 5; i++) {
        out->long_nct_types[i] = 1;
    }
    for (size_t i = 0; i < 4; i++) {
        out->long_nct_intervals[i] = 1;
    }
    for (size_t i = 0; i < 3; i++) {
        out->long_nct_beats[i] = 1;
        out->long_nct_lengths[i] = 1;
    }
    /* Syncopation (phase L3, R-454), off. */
    for (size_t i = 0; i < 4; i++) {
        out->syncopation_types[i] = 1;
    }
    for (size_t i = 0; i < 2; i++) {
        out->syncopation_levels[i] = 1;
    }
    out->skips_per_motif = MAX_NOTES - 1; /* no motive has more intervals, so this never rejects */
    out->color_note = 4;
    out->color_chords.den = 1; /* no motif chord takes a color type */
}

/*
 * The scale-step offset of a chord's tones from the tones of I (same octave): ii 1, iii 2, IV 3, V down a fourth (-3), vi -2,
 * vii -1. The tones are the diatonic triad of the degree; in minor only V's third is raised (chord_alteration).
 */
long motive_random_chord_shift(int degree) {
    long shift = degree - 1;
    return shift > 3 ? shift - 7 : shift;
}

long motive_random_step_of(long position) {
    return ((position % 7) + 7) % 7;
}

/* The alteration of a position on the chord of a degree: in minor, the third of V is raised. */
int motive_random_chord_alteration(const SpelledKey *key, int degree, long position) {
    return composer_material_raises_seventh(key->material) && degree == 5 && motive_random_step_of(position - motive_random_chord_shift(degree)) == 2 ? 1 : 0;
}

/* ------------------------------------------------------- the motive itself */

/* The plan as the motive's own harmony (a chord on each segment's first beat) and the segments as its features. */
static bool set_plan_and_segments(const MotiveRandomOptions *options, const Layout *layout, Motive *motive) {
    unsigned long den = (unsigned long)TICKS_PER_UNIT * (unsigned long)options->meter.beat_unit;
    static const char *const LETTERS = "abcdefgh";
    char names[SEGMENT_MAX][MOTIVE_FEATURE_LABEL_MAX + 1];
    size_t letter = 0;
    for (size_t k = 0; k < layout->chord_count; k++) {
        MotiveSketchChord *chord = &motive->chords[k];
        memset(chord, 0, sizeof(*chord));
        chord->degree = layout->chord_degree[k];
        chord->color = layout->chord_color[k];
        /* The chord stays on the beat (the segment's cell start) when an anticipation sounds before it. */
        if (rational_make(layout->chord_onset[k], den, &chord->onset) != RATIONAL_OK) {
            return false;
        }
    }
    for (size_t s = 0; s < layout->segment_count; s++) {
        names[s][0] = layout->repeats[s] ? LETTERS[0] : LETTERS[letter++];
        names[s][1] = layout->repeats[s] ? '\'' : '\0';
        names[s][2] = '\0';
        MotiveSketchFeature *feature = &motive->features[s];
        memset(feature, 0, sizeof(*feature));
        strcpy(feature->label, names[s]);
        feature->first = layout->first[s];
        feature->last = layout->last[s];
    }
    motive->chord_count = layout->chord_count;
    motive->feature_count = layout->segment_count;
    return true;
}

/* True when note i is the first of a segment that arrives early (an anticipation). */
static bool anticipated(const Layout *layout, size_t i) {
    for (size_t s = 1; s < layout->segment_count; s++) {
        if (layout->first[s] == i && layout->boundary[s] == SYNC_ANTICIPATION) {
            return true;
        }
    }
    return false;
}

/*
 * True when the tone roles read the motive as it was drawn: the base harmony is the plan, given, every frame note is a
 * chord tone, and every other note is a non-chord tone of its type that resolves. An early note is read against the chord
 * before it, so like a run note it may be a chord tone or a non-chord tone that resolves.
 */
static bool tones_as_drawn(const Motive *motive, const Layout *layout, const Melody *melody) {
    MotiveAnalysis analysis;
    if (!motive_analyze(motive, &analysis) || !analysis.base_harmony_known || !analysis.tones_known ||
        !analysis.base_harmony.given || analysis.base_harmony.count != layout->chord_count) {
        return false;
    }
    for (size_t k = 0; k < layout->chord_count; k++) {
        if (analysis.base_harmony.chords[k].degree != layout->chord_degree[k] || analysis.base_harmony.chords[k].color != layout->chord_color[k]) {
            return false;
        }
    }
    for (size_t i = 0; i < motive->note_count; i++) {
        const MotiveToneLabel *label = &analysis.tones[i];
        bool expected = melody->run[i] || anticipated(layout, i) ? label->role == MOTIVE_TONE_CHORD || (label->role == MOTIVE_TONE_NON_CHORD && !label->unresolved)
                        : melody->decorated[i] ? label->role == MOTIVE_TONE_NON_CHORD && label->type == melody->types[i] &&
                                                   !label->unresolved
                                             : label->role == MOTIVE_TONE_CHORD;
        if (!expected) {
            return false;
        }
    }
    return true;
}

/* A motive of the notes first to last of a layout (no pickup), to be scored against another. */
static bool segment_motive(const MotiveRandomOptions *options, const SpelledKey *key, const Layout *layout, const Melody *melody, size_t first,
                           size_t last, Motive *out) {
    Rhythm rhythm;
    Melody notes;
    memset(&rhythm, 0, sizeof(rhythm));
    memset(&notes, 0, sizeof(notes));
    for (size_t i = first; i <= last; i++) {
        size_t k = i - first;
        rhythm.ticks[k] = layout->rhythm.ticks[i];
        rhythm.on_beat[k] = layout->rhythm.on_beat[i];
        rhythm.total += layout->rhythm.ticks[i];
        notes.pitches[k] = melody->pitches[i];
        notes.decorated[k] = melody->decorated[i];
        notes.types[k] = melody->types[i];
    }
    rhythm.count = last - first + 1;
    rhythm.ticks_per_unit = layout->rhythm.ticks_per_unit;
    return motive_random_build(options, key, &rhythm, &notes, out);
}

/*
 * The contrast distance (contrast_fidelity_min): the recognizability of the segment that is not a repeat against segment a
 * is at least the minimum. A layout with no such segment passes. Both motives are made and scored here; false when
 * the score is below the minimum or cannot be made.
 */
static bool contrast_is_close(const MotiveRandomOptions *options, const SpelledKey *key, const MotiveRandomPreferences *preferences, const Layout *layout,
                              const Melody *melody) {
    for (size_t s = 1; s < layout->segment_count; s++) {
        if (layout->repeats[s]) {
            continue;
        }
        Motive a;
        Motive b;
        MotiveRecognizability score;
        if (!segment_motive(options, key, layout, melody, layout->cell_start, layout->last[0], &a) ||
            !segment_motive(options, key, layout, melody, layout->first[s], layout->last[s], &b) ||
            !motive_recognizability(&a, &b, NULL, &score)) {
            return false;
        }
        const Rational *least = &preferences->contrast_fidelity_min;
        if ((unsigned long long)score.total.numerator * least->den < (unsigned long long)least->num * score.total.denominator) {
            return false;
        }
    }
    return true;
}

/*
 * True when the given plan (plan_count > 0) can be drawn in the meter: a count of 1 to MOTIVE_RANDOM_PLAN_MAX, degrees 1 to 7,
 * places that are all or none after the first, rising from 1:1 and inside the bars and the bar's beats, no more chords than
 * segments when unplaced, a bar count the settings allow that a shape fits (plan_bars_fit), and, when only an open ending can
 * be drawn, a last chord with a tone on degree 2, 5 or 7 (not IV or vi).
 */
bool motive_random_segments_plan_fits(const MotiveRandomPreferences *preferences, const PlanMeter *meter) {
    size_t count = preferences->plan_count;
    if (count == 0 || count > MOTIVE_RANDOM_PLAN_MAX) {
        return false;
    }
    Grid grid = motive_random_grid_of(preferences, meter);
    size_t placed = 0;
    long before = 0; /* the place of the chord before, in beats from 1:1 (the first chord is at 1:1) */
    for (size_t k = 0; k < count; k++) {
        const MotiveRandomPlanChord *chord = &preferences->plan[k];
        if (chord->degree < 1 || chord->degree > 7 || chord->bar > 2) {
            return false;
        }
        if (k == 0) {
            if (chord->bar != 0 && (chord->bar != 1 || chord->beat != 1)) {
                return false;
            }
        } else if (chord->bar > 0) {
            long at = ((long)chord->bar - 1) * grid.beats + (long)chord->beat - 1;
            if (chord->beat < 1 || (long)chord->beat > grid.beats || at <= before) {
                return false;
            }
            before = at;
            placed++;
        }
    }
    if (placed != 0 && placed != count - 1) {
        return false; /* some but not all of the chords after the first are placed */
    }
    if (placed == 0 && count > SEGMENT_MAX) {
        return false;
    }
    bool halves = motive_random_has_halves(&grid);
    if (!motive_random_plan_bars_fit(preferences, halves, 1) && !motive_random_plan_bars_fit(preferences, halves, 2)) {
        return false;
    }
    int last = preferences->plan[count - 1].degree;
    bool only_open = preferences->endings[2] > 0 && preferences->endings[0] + preferences->endings[1] + preferences->endings[3] == 0;
    return !(only_open && (last == 4 || last == 6));
}

/*
 * Color chords (R-727). A motif's color chord is a major triad of the key with a tertian color type's added members (add9, add6, six-nine,
 * the ninths); the quartal types replace the triad and are not drawn here. The type is the one a major triad takes with its suffix, as
 * motive_harmony_parse_in reads it; `*mask` gets the scale steps above the root its added members sit on. False when the chord is not a
 * major triad of the key, the type is quartal, or a member is outside the key's scale.
 */
static bool color_fits(const SpelledKey *key, int degree, const ChordType *type, unsigned *mask) {
    const PitchMaterial *material = key != NULL ? key->material : NULL;
    int root = pitch_material_step(material, degree);
    if (material == NULL || type == NULL || root < 0 || type->construction == CHORD_CONSTRUCTION_QUARTAL || type->member_count == 0) {
        return false;
    }
    int steps[7];
    for (int j = 0; j < 7; j++) {
        int step = pitch_material_step(material, (degree - 1 + j) % 7 + 1);
        if (step < 0) {
            return false;
        }
        steps[j] = util_pitch_class(step - root);
    }
    if (steps[2] != 4 || steps[4] != 7) {
        return false;
    }
    unsigned added = 0;
    for (size_t i = 0; i < type->member_count; i++) {
        int semitones = util_pitch_class(type->members[i].semitones);
        int found = -1;
        for (int j = 0; j < 7 && found < 0; j++) {
            found = steps[j] == semitones ? j : -1;
        }
        if (found < 0) {
            return false;
        }
        if (type->members[i].role != CHORD_MEMBER_ROOT && type->members[i].role != CHORD_MEMBER_THIRD && type->members[i].role != CHORD_MEMBER_FIFTH) {
            added |= 1u << found;
        }
    }
    *mask = added;
    return true;
}

/* The suffix of a plan chord as its type; false for a suffix that names none (or no knowledge) or one that does not fit the chord. */
static bool plan_chord_color(const MotiveRandomOptions *options, const SpelledKey *key, int degree, const char *suffix, const ChordType **type,
                             unsigned *mask) {
    return suffix[0] != '\0' && motive_color_type(options->knowledge, suffix, type) && color_fits(key, degree, *type, mask);
}

bool motive_random_segments_plan_colors_fit(const MotiveRandomOptions *options, const SpelledKey *key) {
    const MotiveRandomPreferences *preferences = options->preferences;
    for (size_t k = 0; preferences != NULL && k < preferences->plan_count; k++) {
        const ChordType *type;
        unsigned mask;
        if (preferences->plan[k].color[0] != '\0' && !plan_chord_color(options, key, preferences->plan[k].degree, preferences->plan[k].color, &type, &mask)) {
            return false;
        }
    }
    return true;
}

/*
 * The color of each chord of the plan (setup.chord_color): a given suffix, else (motif.color_chords above 0) one draw per chord and, when it
 * is under the share, a type drawn uniformly from those of the options' color types that fit the chord. No number is drawn when the share
 * is 0, so the default draws are unchanged.
 */
static void draw_chord_colors(const MotiveRandomOptions *options, const MotiveRandomPreferences *preferences, const SpelledKey *key, FillRng *rng,
                              Setup *setup) {
    size_t count = setup->placed ? preferences->plan_count : setup->plan.count;
    const Rational *share = &preferences->color_chords;
    bool drawn = share->den > 0 && share->num > 0 && options->knowledge != NULL && options->color != NULL;
    for (size_t k = 0; k < count && k < CHORD_MAX; k++) {
        int degree = preferences->plan_count > 0 ? preferences->plan[k].degree : setup->plan.degrees[k];
        if (preferences->plan_count > 0 && preferences->plan[k].color[0] != '\0') {
            plan_chord_color(options, key, degree, preferences->plan[k].color, &setup->chord_color[k], &setup->chord_color_mask[k]);
            continue;
        }
        if (!drawn || (long long)fill_rng_below(rng, (uint64_t)share->den) >= share->num) {
            continue;
        }
        const ChordType *fitting[COMPOSER_COLOR_TYPES_MAX];
        unsigned masks[COMPOSER_COLOR_TYPES_MAX];
        size_t fit_count = 0;
        for (size_t t = 0; t < options->color->type_count && fit_count < COMPOSER_COLOR_TYPES_MAX; t++) {
            const ChordType *given = knowledge_snapshot_chord_type(options->knowledge, options->color->types[t]);
            const char *suffix = given != NULL ? standard_tonality_color_suffix(given) : NULL;
            const ChordType *canonical;
            unsigned mask;
            if (suffix != NULL && motive_color_type(options->knowledge, suffix, &canonical) && color_fits(key, degree, canonical, &mask)) {
                fitting[fit_count] = canonical;
                masks[fit_count++] = mask;
            }
        }
        if (fit_count > 0) {
            size_t pick = fill_rng_below(rng, fit_count);
            setup->chord_color[k] = fitting[pick];
            setup->chord_color_mask[k] = masks[pick];
        }
    }
}

/*
 * Whether a note on a beat of the motif sounds one of the characteristic steps of the key's mode; with `strong_only`, a note on the first
 * beat of a bar or the third of four (R-732).
 */
static bool color_on_beat(const Motive *motive, const SpelledKey *key, bool strong_only) {
    unsigned mask = composer_key_color_mask(key);
    PlanMeter meter = motive_meter(motive);
    Rational pickup = motive_pickup(motive);
    long beats = meter.beats_per_bar;
    for (size_t i = 0; i < motive->note_count; i++) {
        Rational at = motive_time_minus(motive->notes[i].onset, pickup);
        bool on_beat = at.den != 0 && (at.num * (long)meter.beat_unit) % (long)at.den == 0;
        long beat = on_beat ? (((at.num * (long)meter.beat_unit) / (long)at.den) % beats) : -1;
        beat = beat < 0 && on_beat ? beat + beats : beat;
        bool strong = on_beat && (beat == 0 || (beats % 2 == 0 && beat % 2 == 0));
        if ((strong_only ? strong : on_beat) && ((mask >> util_pitch_class(motive_pitch(motive, i))) & 1u)) {
            return true;
        }
    }
    return false;
}

bool motive_random_segments_candidate(const MotiveRandomOptions *options, const SpelledKey *key, FillRng *rng, Motive *out,
                                      bool *found) {
    Setup setup;
    MotiveRandomPreferences defaults;
    const MotiveRandomPreferences *preferences = options->preferences;
    if (preferences == NULL) {
        motive_random_preferences_default(&defaults);
        preferences = &defaults;
    }
    *found = false;
    motive_random_draw_setup(preferences, &options->meter, rng, &setup);
    /* A drawn plan in a modal key: its V and IV are the mode's approach and predominant chords, which hold its color note (R-732). */
    const PitchMaterial *modal = key != NULL && pitch_material_is_modal_key(NULL, key->material) ? key->material : NULL;
    for (size_t s = 0; modal != NULL && preferences->plan_count == 0 && s < setup.plan.count; s++) {
        setup.plan.degrees[s] = composer_modal_plan_degree(options->knowledge, modal, setup.plan.degrees[s]);
    }
    draw_chord_colors(options, preferences, key, rng, &setup);
    if (setup.grid.ending == ENDING_OPEN) {
        /* An open ending is on degree 2, 5 or 7 of the key: the last chord needs a tone there (not so for IV and vi). */
        bool tone = false;
        for (long t = 0; t < 3; t++) {
            long step = motive_random_step_of(setup.plan.degrees[setup.plan.count - 1] - 1 + 2 * t);
            tone = tone || step == 1 || step == 4 || step == 6;
        }
        if (!tone) {
            return true;
        }
    }
    for (int attempt = 0; attempt < SEGMENT_TRIES_PER_SETUP; attempt++) {
        Layout layout;
        Melody melody;
        Feature feature;
        const char *fault = NULL;
        if (!motive_random_draw_layout(preferences, &setup, rng, &layout, &feature) || !motive_random_draw_melody(preferences, key, &layout, rng, feature, &melody) ||
            !motive_random_build(options, key, &layout.rhythm, &melody, out) ||
            !set_plan_and_segments(options, &layout, out) || !tones_as_drawn(out, &layout, &melody)) {
            continue;
        }
        if (preferences->contrast_fidelity_min.num > 0 && !contrast_is_close(options, key, preferences, &layout, &melody)) {
            continue;
        }
        if (!motive_melodic_fault(out, &fault)) {
            return false;
        }
        if (fault == NULL) {
            /* In a modal key the first tries want its characteristic step on a strong beat, the next ones on any beat, up to color_note
               tries; later ones keep any motif. */
            if ((unsigned)attempt < preferences->color_note && composer_key_color_mask(key) != 0 &&
                !color_on_beat(out, key, (unsigned)attempt < (preferences->color_note + 1) / 2)) {
                continue;
            }
            *found = true;
            return true;
        }
    }
    return true;
}
