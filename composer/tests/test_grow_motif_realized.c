#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/given.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_state.h>

#include "support/test_support.h"
#include "util/plan_util.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Realized motive notes (B1 of P35, R-326 to R-328): the 6/8 Test Motif (tests/fixtures/motifs) is the basic motive of an
 * 8-bar sentence in C major over given chords. The basic idea is the motif's rhythm and, but for its last note, its own
 * pitches (the escape tone kept); every idea that carries a motive node has that node's rhythm and its notes within
 * MOVE_LIMIT semitones of the node's pitches, moved by one octave for the whole idea.
 */

#define SEEDS 10
/* How far from its motive's pitch (moved by one octave) a note of an idea may be: R-328 moves a clash to the nearest chord tone, two semitones in a triad. */
#define MOVE_LIMIT 2
#define CHORDS 9

static NodeId find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

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

/* The notes of the melody span a node owns, written in the plan. */
static const PlanNotes *melody_of(const PiecePlan *plan, NodeId node) {
    LaneSpanId span;
    assert(plan_melody_span_of(plan, node, &span));
    return &plan_lane_span_payload(plan, span)->notes;
}

/* The motive_ref of the melody span of an idea, or NULL. */
static const PlanMotiveRef *ref_of(const PiecePlan *plan, LaneSpanId span) {
    for (size_t i = 0; i < plan_lane_span_constraint_count(plan, span); i++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, span, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
            return &constraint->params.motive;
        }
    }
    return NULL;
}

static int distance(int a, int b) {
    return a > b ? a - b : b - a;
}

/* Whether one octave shift of the node's notes puts every note of the idea within MOVE_LIMIT semitones of it. */
static bool within_one_shift(const PlanNotes *idea, const PlanNotes *node) {
    static const int SHIFTS[] = {0, -12, 12, -24, 24};
    for (size_t s = 0; s < sizeof(SHIFTS) / sizeof(SHIFTS[0]); s++) {
        bool all = true;
        for (size_t n = 0; all && n < node->note_count; n++) {
            all = distance(idea->notes[n].midi, node->notes[n].midi + SHIFTS[s]) <= MOVE_LIMIT;
        }
        if (all) {
            return true;
        }
    }
    return false;
}

static PiecePlan *compose(const Motive *motive, unsigned long seed, GivenChord *chords) {
    static const char *const NUMERALS[CHORDS] = {"I", "vi", "IV", "V", "I", "vi", "IV", "V", "I"};
    /* Bars of 6/8 are six eighths: chords I vi IV V I vi in bars 0 to 5, IV then V from the second dotted quarter of bar 6, I in bar 7. */
    static const long long STARTS[CHORDS] = {0, 6, 12, 18, 24, 30, 36, 39, 42};
    static const long long LENGTHS[CHORDS] = {6, 6, 6, 6, 6, 6, 3, 3, 6};
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
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    GrowResult result = grow_run(plan, &config, &outcome);
    if (result != GROW_OK) {
        fprintf(stderr, "seed %lu: stuck at %s\n", seed, grow_stage_name(outcome.stuck_stage));
    }
    assert(result == GROW_OK && !outcome.stuck);
    grow_outcome_destroy(&outcome);
    return plan;
}

int main(void) {
    Motive motive = load_motive();
    assert(motive.note_count == 10);
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        GivenChord chords[CHORDS];
        PiecePlan *plan = compose(&motive, seed, chords);
        NodeId basic = find_node(plan, "basic idea");
        assert(basic != PLAN_NODE_NONE);
        const PlanNotes *notes = melody_of(plan, basic);
        assert(notes->note_count == motive.note_count);
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(rational_cmp(notes->notes[n].offset, motive.notes[n].onset) == 0);
            assert(rational_cmp(notes->notes[n].duration, motive.notes[n].duration) == 0);
        }
        /* Notes 1 to 9 are the motif's pitches as written, the escape tone (note 4) among them; note 10 may bend. */
        for (size_t n = 0; n < 9; n++) {
            if (notes->notes[n].midi != motive.notes[n].pitch) {
                fprintf(stderr, "seed %lu: note %zu is %d, the motif has %d\n", seed, n + 1, notes->notes[n].midi,
                         motive.notes[n].pitch);
            }
            assert(notes->notes[n].midi == motive.notes[n].pitch);
        }
        assert(notes->notes[3].midi == 71);
        assert(notes->notes[9].midi == 79 || distance(notes->notes[9].midi, 79) <= 2);

        /* Every idea that carries a motive node says that node's rhythm, in notes within MOVE_LIMIT semitones of its pitches. */
        size_t carrying = 0;
        for (NodeId idea = 1; idea <= plan_node_count(plan); idea++) {
            LaneSpanId span;
            LaneSpanId node_span;
            if (plan_node_kind(plan, idea) != PLAN_NODE_IDEA || !plan_melody_span_of(plan, idea, &span)) {
                continue;
            }
            const PlanMotiveRef *ref = ref_of(plan, span);
            if (!ref || !plan_has_node(plan, ref->motive) || !plan_melody_span_of(plan, ref->motive, &node_span)) {
                continue;
            }
            const PlanNotes *carried = melody_of(plan, ref->motive);
            const PlanNotes *written = melody_of(plan, idea);
            assert(written->note_count >= carried->note_count);
            for (size_t n = 0; n < carried->note_count; n++) {
                assert(rational_cmp(written->notes[n].offset, carried->notes[n].offset) == 0);
                assert(rational_cmp(written->notes[n].duration, carried->notes[n].duration) == 0);
            }
            if (!within_one_shift(written, carried)) {
                fprintf(stderr, "seed %lu: idea %s is not its motive within %d semitones of one octave\n", seed,
                        plan_node_name(plan, idea) ? plan_node_name(plan, idea) : "?", MOVE_LIMIT);
            }
            assert(within_one_shift(written, carried));
            carrying++;
        }
        assert(carrying >= 4);
        plan_destroy(plan);
    }
    return 0;
}
