#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/plan_state.h>

/*
 * The three repetitions of a presentation (B1 of P34): over seeds 1 to 30 of an 8-bar sentence in C major 4/4 with a 1-bar
 * motive each of exact, dominant form and sequence occurs; a dominant form opens on V and keeps the basic idea's rhythm; a
 * sequence is the basic idea a step up in the key with its chord a step up (I to ii); a basic idea of one cell only repeats
 * exact.
 */

typedef enum { EXACT, DOMINANT_FORM, SEQUENCE } Kind;

static NodeId find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

static Motive make_motive(const char *text, int beats, int unit) {
    Motive motive;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = beats;
    options.meter.beat_unit = unit;
    assert(motive_from_notes_text(text, &options, &motive, NULL) == MOTIVE_OK);
    return motive;
}

static GrowRequest make_request(const Motive *motive, int tonic, KeyStructureMode mode, int beats, int unit, size_t bars) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = plan_key_of_pitch_class(tonic, mode);
    request.meter.beats_per_bar = beats;
    request.meter.beat_unit = unit;
    request.bars = bars;
    request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    return request;
}

/* How the plan has the repetition made from the basic idea: the relation its motive node asks for. */
static Kind repetition_kind(const PiecePlan *plan) {
    NodeId node = find_node(plan, "m1");
    assert(node != PLAN_NODE_NONE);
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind != PLAN_CONSTRAINT_MOTIVE_REF || !constraint->params.motive.has_step) {
            continue;
        }
        switch (constraint->params.motive.step.kind) {
        case MOTIVE_RELATION_EXACT:
            return EXACT;
        case MOTIVE_RELATION_DOMINANT_FORM:
            return DOMINANT_FORM;
        case MOTIVE_RELATION_TRANSPOSE:
            assert(constraint->params.motive.step.value[0] == 1 && constraint->params.motive.step.value[1] == 1);
            return SEQUENCE;
        default:
            break;
        }
    }
    assert(!"the repetition's motive has a relation");
    return EXACT;
}

/* The notes written on the motive node `name`: onsets, durations and pitches. */
static size_t written_notes(const PiecePlan *plan, const char *name, const PlanNote **notes) {
    LaneSpanId span = PLAN_LANE_SPAN_NONE;
    assert(plan_node_lane_spans(plan, find_node(plan, name), &span, 1) == 1);
    const PlanNotes *held = &plan_lane_span_payload(plan, span)->notes;
    *notes = held->notes;
    return held->note_count;
}

static bool same_rhythm(const PlanNote *a, const PlanNote *b, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (rational_cmp(a[i].offset, b[i].offset) != 0 || rational_cmp(a[i].duration, b[i].duration) != 0) {
            return false;
        }
    }
    return true;
}

/* The scale step of a pitch in C major, counting the octaves. */
static int step_of(int midi) {
    static const int STEP[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    return midi / 12 * 7 + STEP[midi % 12];
}

/* The first chord of the idea. */
static const char *first_chord(const PiecePlan *plan, const char *idea) {
    LaneSpanId chord = PLAN_LANE_SPAN_NONE;
    assert(plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, find_node(plan, idea)), &chord, 1) >= 1);
    return plan_lane_span_payload(plan, chord)->harmony.roman_numeral;
}

static void test_three_kinds(void) {
    Motive motive = make_motive("E4:q G4:e F4:e D4:q C4:q", 4, 4);
    GrowRequest request = make_request(&motive, 0, KEY_STRUCTURE_MODE_MAJOR, 4, 4, 8);
    size_t seen[3] = {0, 0, 0};
    for (unsigned long seed = 1; seed <= 30; seed++) {
        PiecePlan *plan = NULL;
        GrowOutcome outcome;
        GrowConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        config.basic = &motive;
        assert(grow_build_plan(&request, &plan) == GROW_OK);
        assert(grow_run(plan, &config, &outcome) == GROW_OK);
        Kind kind = repetition_kind(plan);
        seen[kind]++;

        const PlanNote *basic;
        const PlanNote *repeated;
        size_t count = written_notes(plan, "m0", &basic);
        assert(written_notes(plan, "m1", &repeated) == count);
        if (kind == DOMINANT_FORM) {
            /* The chords go from the tonic to V and back; the notes keep the rhythm. */
            const char *numeral = first_chord(plan, "repetition");
            assert(numeral[0] == 'V' && numeral[1] != 'I');
            assert(same_rhythm(basic, repeated, count));
        } else if (kind == SEQUENCE) {
            /* The notes a step up in the key, the chord a step up: I to ii. */
            assert(same_rhythm(basic, repeated, count));
            for (size_t n = 0; n < count; n++) {
                assert((step_of(repeated[n].midi) - step_of(basic[n].midi) + 70) % 7 == 1);
            }
            assert(strcmp(first_chord(plan, "basic idea"), "I") == 0);
            assert(strcmp(first_chord(plan, "repetition"), "ii") == 0);
        } else {
            for (size_t n = 0; n < count; n++) {
                assert(repeated[n].midi == basic[n].midi);
            }
            assert(same_rhythm(basic, repeated, count));
        }
        grow_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    assert(seen[EXACT] > 0 && seen[DOMINANT_FORM] > 0 && seen[SEQUENCE] > 0);
    printf("exact %zu, dominant form %zu, sequence %zu of 30\n", seen[EXACT], seen[DOMINANT_FORM], seen[SEQUENCE]);
}

/* A 4-bar theme in 2/4 has a basic idea of one cell: the repetition can only be exact. */
static void test_one_cell(void) {
    Motive motive = make_motive("A4:q C5:e Bb4:e", 2, 4);
    GrowRequest request = make_request(&motive, 5, KEY_STRUCTURE_MODE_MAJOR, 2, 4, 4);
    for (unsigned long seed = 1; seed <= 10; seed++) {
        PiecePlan *plan = NULL;
        GrowOutcome outcome;
        GrowConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        config.basic = &motive;
        assert(grow_build_plan(&request, &plan) == GROW_OK);
        assert(grow_run(plan, &config, &outcome) == GROW_OK);
        assert(repetition_kind(plan) == EXACT);
        grow_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
}

int main(void) {
    test_three_kinds();
    test_one_cell();
    printf("test_grow_repetitions: ok\n");
    return 0;
}
