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
#define CHORDS 9

static Motive load_motive(void) {
    char *text = test_read_file(COMPOSER_FIXTURE_DIR "/motifs/test-motif-6-8.json", NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(text);
    assert(motive_from_json(text, NULL, &motive, &error) == MOTIVE_OK);
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
    request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
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
    config.seed = seed;
    config.basic = motive;
    config.melody = melody;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
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
    return 0;
}
