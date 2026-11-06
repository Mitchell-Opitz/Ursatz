#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_state.h>

#include "support/test_support.h"
#include "util/plan_util.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * A motive the strict rules cannot continue (B1 of P35, R-338): the D minor arpeggio (tests/fixtures/motifs) as the basic
 * motive of an 8-bar sentence over chords the harmony stage writes. Some of its ideas leave the generated notes no pitch the
 * strict melodic, bass and climax rules allow, so the melody operator composes them relaxed; every seed composes, and the basic
 * idea keeps the arpeggio's four notes within two semitones, moved by one octave.
 */

#define SEEDS 5

static NodeId find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

int main(void) {
    static const int ARPEGGIO[4] = {62, 65, 69, 74};
    static const int SHIFTS[] = {0, -12, 12, -24, 24};
    char *text = test_read_file(COMPOSER_FIXTURE_DIR "/motifs/motive-d-minor-arpeggio.json", NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(text);
    assert(motive_from_json(text, NULL, &motive, &error) == MOTIVE_OK);
    free(text);
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        GrowRequest request;
        memset(&request, 0, sizeof(request));
        request.motive = &motive;
        request.key = plan_key_of_pitch_class(2, KEY_STRUCTURE_MODE_MINOR);
        request.meter.beats_per_bar = 4;
        request.meter.beat_unit = 4;
        request.bars = 8;
        request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
        request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
        request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
        PiecePlan *plan = NULL;
        GrowOutcome outcome;
        GrowConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        config.basic = &motive;
        assert(grow_build_plan(&request, &plan) == GROW_OK);
        GrowResult result = grow_run(plan, &config, &outcome);
        if (result != GROW_OK) {
            fprintf(stderr, "seed %lu: stuck at %s\n", seed, grow_stage_name(outcome.stuck_stage));
        }
        assert(result == GROW_OK && !outcome.stuck);
        grow_outcome_destroy(&outcome);
        LaneSpanId span;
        NodeId basic = find_node(plan, "basic idea");
        assert(basic != PLAN_NODE_NONE && plan_melody_span_of(plan, basic, &span));
        const PlanNotes *notes = &plan_lane_span_payload(plan, span)->notes;
        assert(notes->note_count >= 4);
        bool placed = false;
        for (size_t s = 0; s < sizeof(SHIFTS) / sizeof(SHIFTS[0]) && !placed; s++) {
            placed = true;
            for (size_t n = 0; n < 4; n++) {
                int apart = notes->notes[n].midi - (ARPEGGIO[n] + SHIFTS[s]);
                placed = placed && (apart < 0 ? -apart : apart) <= 2;
            }
        }
        if (!placed) {
            fprintf(stderr, "seed %lu: the basic idea is %d %d %d %d\n", seed, notes->notes[0].midi, notes->notes[1].midi,
                    notes->notes[2].midi, notes->notes[3].midi);
        }
        assert(placed);
        plan_destroy(plan);
    }
    return 0;
}
