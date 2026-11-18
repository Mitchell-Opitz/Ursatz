#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/given.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_state.h>

#include "support/test_support.h"
#include "util/plan_util.h"
#include "ursatz_test.h"
#include "support/test_knowledge.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The melody settings (R-411, R-413) on the Test Motif sentence of test_grow_motif_realized: with the default
 * MelodyFillParams the melody notes are exactly those of a run with GrowConfig.melody NULL, and weights that count
 * the shaping rules differently give a different melody for some seeds.
 *
 * Measured (B1 of P43), seeds 1 to 8 against the defaults: weights.central_range 20 with weights.waves 0 changes none of
 * the melodies (the shaping rules rank candidates that all hold them alike), so the setting the test uses is
 * weights.against_bass 20, which changes all 8.
 */

#define SEEDS 8
#define MEASURED_CLOSING 8 /* of 8 seeds; the defaults' 6/8 sentence already ends on a note 2 or more times the mean */
#define MEASURED_FIRST 4
#define CHORDS 9

static Motive load_motive(void) {
    char *text = test_must_read_file(COMPOSER_FIXTURE_DIR "/motifs/test-motif-6-8.json", NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(text);
    assert(motive_from_json(text, test_motive_options(), &motive, &error) == MOTIVE_OK);
    free(text);
    return motive;
}

/* The same sentence as test_grow_motif_realized: 8 bars of 6/8 in C major over I vi IV V I vi IV V I. */
static PiecePlan *compose(const Motive *motive, unsigned long seed, const MelodyFillParams *melody) {
    static const char *const NUMERALS[CHORDS] = {"I", "vi", "IV", "V", "I", "vi", "IV", "V", "I"};
    static const long long STARTS[CHORDS] = {0, 6, 12, 18, 24, 30, 36, 39, 42};
    static const long long LENGTHS[CHORDS] = {6, 6, 6, 6, 6, 6, 3, 3, 6};
    GivenChord chords[CHORDS];
    for (size_t i = 0; i < CHORDS; i++) {
        chords[i].numeral = NUMERALS[i];
        chords[i].onset = rational_ratio(STARTS[i], 8);
        chords[i].length = rational_ratio(LENGTHS[i], 8);
    }
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = test_key(0, "major");
    request.meter.beats_per_bar = 6;
    request.meter.beat_unit = 8;
    request.bars = 8;
    request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    request.chords = chords;
    request.chord_count = CHORDS;
    PiecePlan *plan = NULL;
    GrowOutcome outcome;
    GrowConfig config;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
    config.seed = seed;
    config.basic = motive;
    config.melody = melody;
    assert(grow_build_plan_checked(&request, &plan, NULL) == GROW_OK);
    GrowResult result = grow_run(plan, &config, &outcome);
    if (result != GROW_OK) {
        fprintf(stderr, "seed %lu: stuck at %s\n", seed, grow_stage_name(outcome.stuck_stage));
    }
    assert(result == GROW_OK && !outcome.stuck);
    grow_outcome_destroy(&outcome);
    return plan;
}

static uint64_t mix(uint64_t hash, long long value) {
    hash ^= (uint64_t)value;
    return hash * 1099511628211ull;
}

/* A hash of every melody note of the plan (pitch, onset, duration), node by node; the count of notes goes in too. */
static uint64_t melody_hash(const PiecePlan *plan, size_t *notes_out) {
    uint64_t hash = 14695981039346656037ull;
    size_t total = 0;
    for (NodeId node = 1; node <= plan_node_count(plan); node++) {
        LaneSpanId span;
        if (!plan_melody_span_of(plan, node, &span)) {
            continue;
        }
        const PlanNotes *notes = &plan_lane_span_payload(plan, span)->notes;
        hash = mix(hash, (long long)node);
        for (size_t n = 0; n < notes->note_count; n++) {
            hash = mix(hash, notes->notes[n].midi);
            hash = mix(hash, notes->notes[n].offset.num);
            hash = mix(hash, notes->notes[n].offset.den);
            hash = mix(hash, notes->notes[n].duration.num);
            hash = mix(hash, notes->notes[n].duration.den);
            total++;
        }
    }
    *notes_out = total;
    return hash;
}


/*
 * Closure (phase L4, R-478): the last melody note over the mean note value of the notes before it, from the start of the
 * plan, which is the phrase's mean for a theme of one phrase.
 */
static double final_over_mean(const PiecePlan *plan) {
    double last_onset = -1;
    double last_duration = 0;
    size_t count = 0;
    for (NodeId node = 1; node <= plan_node_count(plan); node++) {
        LaneSpanId span;
        if (!plan_melody_span_of(plan, node, &span)) {
            continue;
        }
        double start = (double)plan_lane_span_span(plan, span).start.num / (double)plan_lane_span_span(plan, span).start.den;
        const PlanNotes *notes = &plan_lane_span_payload(plan, span)->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            double onset = start + (double)notes->notes[n].offset.num / (double)notes->notes[n].offset.den;
            count++;
            if (onset > last_onset) {
                last_onset = onset;
                last_duration = (double)notes->notes[n].duration.num / (double)notes->notes[n].duration.den;
            }
        }
    }
    assert(count > 1);
    return last_duration / (last_onset / (double)(count - 1));
}

/* The duration of the last note of the antecedent of a period, in whole notes. */
static double antecedent_last_note(const PiecePlan *plan) {
    NodeId antecedent = PLAN_NODE_NONE;
    for (NodeId node = 1; node <= plan_node_count(plan); node++) {
        const char *name = plan_node_name(plan, node);
        if (name && strcmp(name, "antecedent") == 0) {
            antecedent = node;
        }
    }
    assert(antecedent != PLAN_NODE_NONE);
    LaneSpanId melodies[8];
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, antecedent), melodies, 8);
    assert(found > 0);
    const PlanNotes *notes = &plan_lane_span_payload(plan, melodies[found - 1])->notes;
    return (double)notes->notes[notes->note_count - 1].duration.num / (double)notes->notes[notes->note_count - 1].duration.den;
}

/* The period of test_grow_period (C major 4/4, an antecedent and a consequent, 8 bars) with a melody setting. */
static PiecePlan *compose_period(unsigned long seed, const MelodyFillParams *melody) {
    Motive motive;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.knowledge = test_knowledge();
    options.has_meter = true;
    options.meter.beats_per_bar = 4;
    options.meter.beat_unit = 4;
    assert(motive_from_notes_text("E4:q G4:e F4:e D4:q C4:q", &options, &motive, NULL) == MOTIVE_OK);
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = &motive;
    request.key = test_key(0, "major");
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.bars = 8;
    request.pairing.beginning = PLAN_THEME_BEGINNING_ANTECEDENT;
    request.pairing.ending = PLAN_THEME_ENDING_CONSEQUENT;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    PiecePlan *plan = NULL;
    GrowOutcome outcome;
    GrowConfig config;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
    config.seed = seed;
    config.melody = melody;
    assert(grow_build_plan_checked(&request, &plan, NULL) == GROW_OK);
    GrowResult result = grow_run(plan, &config, &outcome);
    assert(result == GROW_OK && !outcome.stuck);
    grow_outcome_destroy(&outcome);
    return plan;
}

/*
 * Rhythm repetition (phase L4, R-479, R-483): the longest run of whole bars of 2 or more notes in the theme's melody that
 * share one rhythm (onsets and durations within the bar). Only the idea nodes are read: the plan's motive and material
 * nodes have melody spans of their own after the theme. Times are in 128ths of a whole note, the finest the operator writes.
 */
#define BAR_UNITS 96 /* a 6/8 bar */
#define MAX_MELODY_NOTES 512
#define MAX_BAR_NOTES 64

typedef struct {
    long long onset;
    long long duration;
} UnitNote;

static int compare_units(const void *a, const void *b) {
    long long x = ((const UnitNote *)a)->onset;
    long long y = ((const UnitNote *)b)->onset;
    return (x > y) - (x < y);
}

static size_t longest_rhythm_run(const PiecePlan *plan) {
    static UnitNote list[MAX_MELODY_NOTES];
    size_t count = 0;
    for (NodeId node = 1; node <= plan_node_count(plan); node++) {
        LaneSpanId span;
        if (plan_node_kind(plan, node) != PLAN_NODE_IDEA || !plan_melody_span_of(plan, node, &span)) {
            continue;
        }
        Rational start = plan_lane_span_span(plan, span).start;
        const PlanNotes *notes = &plan_lane_span_payload(plan, span)->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(count < MAX_MELODY_NOTES && 128 % start.den == 0 && 128 % notes->notes[n].offset.den == 0 &&
                   128 % notes->notes[n].duration.den == 0);
            list[count].onset =
                start.num * (128 / start.den) + notes->notes[n].offset.num * (128 / notes->notes[n].offset.den);
            list[count].duration = notes->notes[n].duration.num * (128 / notes->notes[n].duration.den);
            count++;
        }
    }
    assert(count > 0);
    qsort(list, count, sizeof(list[0]), compare_units);
    size_t longest = 0;
    size_t run = 0;
    long long previous_onset[MAX_BAR_NOTES];
    long long previous_duration[MAX_BAR_NOTES];
    size_t previous_count = 0;
    for (long long bar = 0; bar * BAR_UNITS <= list[count - 1].onset; bar++) {
        long long onset[MAX_BAR_NOTES];
        long long duration[MAX_BAR_NOTES];
        size_t in_bar = 0;
        for (size_t i = 0; i < count; i++) {
            if (list[i].onset >= bar * BAR_UNITS && list[i].onset < (bar + 1) * BAR_UNITS && in_bar < MAX_BAR_NOTES) {
                onset[in_bar] = list[i].onset - bar * BAR_UNITS;
                duration[in_bar] = list[i].duration;
                in_bar++;
            }
        }
        if (in_bar < 2) {
            run = 0;
            previous_count = 0;
            continue;
        }
        bool same = in_bar == previous_count;
        for (size_t i = 0; same && i < in_bar; i++) {
            same = onset[i] == previous_onset[i] && duration[i] == previous_duration[i];
        }
        run = same ? run + 1 : 1;
        longest = run > longest ? run : longest;
        previous_count = in_bar;
        memcpy(previous_onset, onset, in_bar * sizeof(onset[0]));
        memcpy(previous_duration, duration, in_bar * sizeof(duration[0]));
    }
    return longest;
}

/*
 * Leaps and register (phase L4, R-481). leaps: the intervals of 5 semitones or more between neighbouring notes of the
 * theme's melody (idea nodes only, in time order). register_move: the largest move of the median pitch between
 * consecutive ideas, in semitones.
 */
typedef struct {
    double onset;
    int pitch;
} PitchNote;

static int compare_pitch_notes(const void *a, const void *b) {
    double x = ((const PitchNote *)a)->onset;
    double y = ((const PitchNote *)b)->onset;
    return (x > y) - (x < y);
}

static int compare_ints(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

static size_t leaps_of(const PiecePlan *plan) {
    static PitchNote list[MAX_MELODY_NOTES];
    size_t count = 0;
    for (NodeId node = 1; node <= plan_node_count(plan); node++) {
        LaneSpanId span;
        if (plan_node_kind(plan, node) != PLAN_NODE_IDEA || !plan_melody_span_of(plan, node, &span)) {
            continue;
        }
        Rational start = plan_lane_span_span(plan, span).start;
        const PlanNotes *notes = &plan_lane_span_payload(plan, span)->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(count < MAX_MELODY_NOTES);
            list[count].onset = (double)start.num / (double)start.den +
                                (double)notes->notes[n].offset.num / (double)notes->notes[n].offset.den;
            list[count].pitch = notes->notes[n].midi;
            count++;
        }
    }
    qsort(list, count, sizeof(list[0]), compare_pitch_notes);
    size_t leaps = 0;
    for (size_t i = 1; i < count; i++) {
        int move = list[i].pitch - list[i - 1].pitch;
        leaps += move >= 5 || move <= -5;
    }
    return leaps;
}

static int register_move(const PiecePlan *plan) {
    PitchNote ideas[16];
    size_t idea_count = 0;
    for (NodeId node = 1; node <= plan_node_count(plan); node++) {
        LaneSpanId span;
        if (plan_node_kind(plan, node) != PLAN_NODE_IDEA || !plan_melody_span_of(plan, node, &span)) {
            continue;
        }
        const PlanNotes *notes = &plan_lane_span_payload(plan, span)->notes;
        int pitches[MAX_BAR_NOTES];
        Rational start = plan_lane_span_span(plan, span).start;
        assert(notes->note_count > 0 && notes->note_count <= MAX_BAR_NOTES && idea_count < 16);
        for (size_t n = 0; n < notes->note_count; n++) {
            pitches[n] = notes->notes[n].midi;
        }
        qsort(pitches, notes->note_count, sizeof(pitches[0]), compare_ints);
        ideas[idea_count].onset = (double)start.num / (double)start.den;
        ideas[idea_count].pitch = (pitches[(notes->note_count - 1) / 2] + pitches[notes->note_count / 2]) / 2;
        idea_count++;
    }
    qsort(ideas, idea_count, sizeof(ideas[0]), compare_pitch_notes);
    int largest = 0;
    for (size_t i = 1; i < idea_count; i++) {
        int move = ideas[i].pitch - ideas[i - 1].pitch;
        move = move < 0 ? -move : move;
        largest = move > largest ? move : largest;
    }
    return largest;
}

int main(void) {
    Motive motive = load_motive();
    MelodyFillParams defaults = melody_fill_params_default();
    MelodyFillParams shaped = melody_fill_params_default();
    shaped.weights.against_bass = 20;
    int differing = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        size_t absent_notes;
        size_t default_notes;
        size_t shaped_notes;
        PiecePlan *absent = compose(&motive, seed, NULL);
        PiecePlan *with_defaults = compose(&motive, seed, &defaults);
        PiecePlan *with_weights = compose(&motive, seed, &shaped);
        uint64_t absent_hash = melody_hash(absent, &absent_notes);
        uint64_t default_hash = melody_hash(with_defaults, &default_notes);
        uint64_t shaped_hash = melody_hash(with_weights, &shaped_notes);
        assert(absent_notes > 0);
        if (absent_notes != default_notes || absent_hash != default_hash) {
            fprintf(stderr, "seed %lu: the default settings change the melody\n", seed);
        }
        assert(absent_notes == default_notes && absent_hash == default_hash);
        differing += shaped_hash != absent_hash;
        plan_destroy(absent);
        plan_destroy(with_defaults);
        plan_destroy(with_weights);
    }
    printf("test_melody_preferences: %d of %d seeds differ with against_bass 20\n", differing, SEEDS);
    assert(differing >= 1);
    /* Seeds are reproducible: the same settings and seed compose the same melody again. */
    size_t first_notes;
    size_t second_notes;
    PiecePlan *first = compose(&motive, 1, &shaped);
    PiecePlan *second = compose(&motive, 1, &shaped);
    assert(melody_hash(first, &first_notes) == melody_hash(second, &second_notes) && first_notes == second_notes);
    plan_destroy(first);
    plan_destroy(second);

    /* Closure on the last cadence (R-478): the settled final note is longer against the phrase's own note values. */
    MelodyFillParams closing = melody_fill_params_default();
    MelodyFillParams written_zero = melody_fill_params_default();
    closing.closure_final = 20;
    written_zero.closure_final = 0;
    written_zero.closure_first = 0;
    int long_default = 0;
    int long_closing = 0;
    int closing_differs = 0;
    double ratio_default = 0;
    double ratio_closing = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        size_t absent_notes;
        size_t zero_notes;
        size_t closing_notes;
        PiecePlan *absent = compose(&motive, seed, NULL);
        PiecePlan *zero = compose(&motive, seed, &written_zero);
        PiecePlan *with_closure = compose(&motive, seed, &closing);
        uint64_t absent_hash = melody_hash(absent, &absent_notes);
        assert(melody_hash(zero, &zero_notes) == absent_hash && zero_notes == absent_notes);
        closing_differs += melody_hash(with_closure, &closing_notes) != absent_hash;
        ratio_default += final_over_mean(absent);
        ratio_closing += final_over_mean(with_closure);
        long_default += final_over_mean(absent) >= 2.0 - 1e-9;
        long_closing += final_over_mean(with_closure) >= 2.0 - 1e-9;
        plan_destroy(absent);
        plan_destroy(zero);
        plan_destroy(with_closure);
    }
    printf("test_melody_preferences: closure_final 20: a final note of 2 or more times the mean in %d of %d seeds "
           "(defaults %d), mean ratio %.2f (defaults %.2f), %d melodies differ\n",
           long_closing, SEEDS, long_default, ratio_closing / SEEDS, ratio_default / SEEDS, closing_differs);
    assert(long_closing >= MEASURED_CLOSING && closing_differs >= 1);

    /* Closure on an earlier cadence: the antecedent of a period ends on a longer note. */
    MelodyFillParams first_closing = melody_fill_params_default();
    first_closing.closure_first = 20;
    int antecedent_longer = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PiecePlan *without = compose_period(seed, &defaults);
        PiecePlan *with_closure = compose_period(seed, &first_closing);
        antecedent_longer += antecedent_last_note(with_closure) > antecedent_last_note(without) + 1e-9;
        plan_destroy(without);
        plan_destroy(with_closure);
    }
    printf("test_melody_preferences: closure_first 20: the antecedent's last note is longer in %d of %d seeds\n",
           antecedent_longer, SEEDS);
    assert(antecedent_longer >= MEASURED_FIRST);

    /* Rhythm repetition (R-479): with a cap of 3 no run of bars with one rhythm passes 3; a cap of 0 is the defaults. */
    MelodyFillParams capped = melody_fill_params_default();
    MelodyFillParams cap_zero = melody_fill_params_default();
    capped.rhythm_repeat_bars = 3;
    cap_zero.rhythm_repeat_bars = 0;
    size_t longest_off = 0;
    size_t longest_on = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        size_t absent_notes;
        size_t zero_notes;
        PiecePlan *absent = compose(&motive, seed, NULL);
        PiecePlan *zero = compose(&motive, seed, &cap_zero);
        PiecePlan *with_cap = compose(&motive, seed, &capped);
        assert(melody_hash(zero, &zero_notes) == melody_hash(absent, &absent_notes) && zero_notes == absent_notes);
        size_t run_off = longest_rhythm_run(absent);
        size_t run_on = longest_rhythm_run(with_cap);
        if (run_on > 3) {
            fprintf(stderr, "seed %lu: a run of %zu bars with one rhythm under a cap of 3\n", seed, run_on);
        }
        assert(run_on <= 3);
        longest_off = run_off > longest_off ? run_off : longest_off;
        longest_on = run_on > longest_on ? run_on : longest_on;
        plan_destroy(absent);
        plan_destroy(zero);
        plan_destroy(with_cap);
    }
    printf("test_melody_preferences: rhythm_repeat_bars 3: the longest run of bars with one rhythm is %zu (0: %zu)\n",
           longest_on, longest_off);

    /* Leaps (R-481): leap_weight 20 leaves fewer leaps over the melody; 0 is the defaults. */
    MelodyFillParams leap_on = melody_fill_params_default();
    MelodyFillParams leap_off = melody_fill_params_default();
    leap_on.leap_weight = 20;
    leap_off.leap_weight = 0;
    size_t leaps_default = 0;
    size_t leaps_weighted = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        size_t absent_notes;
        size_t zero_notes;
        PiecePlan *absent = compose(&motive, seed, NULL);
        PiecePlan *zero = compose(&motive, seed, &leap_off);
        PiecePlan *weighted = compose(&motive, seed, &leap_on);
        assert(melody_hash(zero, &zero_notes) == melody_hash(absent, &absent_notes) && zero_notes == absent_notes);
        leaps_default += leaps_of(absent);
        leaps_weighted += leaps_of(weighted);
        plan_destroy(absent);
        plan_destroy(zero);
        plan_destroy(weighted);
    }
    printf("test_melody_preferences: leap_weight 20: %zu leaps over seeds 1 to 8 (defaults %zu)\n", leaps_weighted,
           leaps_default);
    assert(leaps_weighted < leaps_default);

    /* Register (R-481): register_weight 20 keeps each idea nearer the one before; 0 is the defaults. */
    MelodyFillParams register_on = melody_fill_params_default();
    MelodyFillParams register_off = melody_fill_params_default();
    register_on.register_weight = 20;
    register_off.register_weight = 0;
    int moves_default = 0;
    int moves_weighted = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        size_t absent_notes;
        size_t zero_notes;
        PiecePlan *absent = compose_period(seed, NULL);
        PiecePlan *zero = compose_period(seed, &register_off);
        PiecePlan *weighted = compose_period(seed, &register_on);
        assert(melody_hash(zero, &zero_notes) == melody_hash(absent, &absent_notes) && zero_notes == absent_notes);
        moves_default += register_move(absent);
        moves_weighted += register_move(weighted);
        plan_destroy(absent);
        plan_destroy(zero);
        plan_destroy(weighted);
    }
    printf("test_melody_preferences: register_weight 20: the largest move of the median pitch between ideas sums to %d "
           "over seeds 1 to 8 (defaults %d)\n",
           moves_weighted, moves_default);
    assert(moves_weighted < moves_default);
    return 0;
}
