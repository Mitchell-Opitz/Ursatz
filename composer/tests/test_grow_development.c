#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/plan_state.h>

/*
 * The development setting (B3 of P34, ch. XI): the 2-bar two-segment motif of test_grow_segment_fragments in an 8-bar
 * sentence, seeds 1 to 10 for each of melody, balanced and theme. The continuation's fragments keep more of the motif the nearer
 * the setting is to melody: the share of note pairs (a note's duration and its interval to the next note, within an idea)
 * that quote the motif is ordered melody > balanced > theme in mean. A melody never draws a sequence repetition; a theme
 * always draws fragment 2 as a sequence.
 */

#define SEEDS 10
#define MOTIF_NOTES "C4:q D4:e E4:e G4:q E4:q G4:q E4:e D4:e C4:h"

static NodeId find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

static Motive make_motive(void) {
    Motive motive;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = 4;
    options.meter.beat_unit = 4;
    options.has_features = true;
    options.feature_count = 2;
    strcpy(options.features[0].label, "a");
    options.features[0].first = 0;
    options.features[0].last = 4;
    strcpy(options.features[1].label, "b");
    options.features[1].first = 5;
    options.features[1].last = 8;
    assert(motive_from_notes_text(MOTIF_NOTES, &options, &motive, NULL) == MOTIVE_OK);
    return motive;
}

static PiecePlan *grow(const Motive *motive, GrowDevelopment development, unsigned long seed) {
    GrowRequest request;
    GrowConfig config;
    GrowOutcome outcome;
    PiecePlan *plan = NULL;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.bars = 8;
    request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.basic = motive;
    config.context.development = development;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    assert(grow_run(plan, &config, &outcome) == GROW_OK);
    grow_outcome_destroy(&outcome);
    return plan;
}

static size_t written_notes(const PiecePlan *plan, const char *name, const PlanNote **notes) {
    LaneSpanId span = PLAN_LANE_SPAN_NONE;
    assert(plan_node_lane_spans(plan, find_node(plan, name), &span, 1) == 1);
    const PlanNotes *held = &plan_lane_span_payload(plan, span)->notes;
    *notes = held->notes;
    return held->note_count;
}

/* The relation the motive node's motive_ref asks for, or NULL for none. */
static const PlanMotiveRef *ref_of(const PiecePlan *plan, const char *name) {
    NodeId node = find_node(plan, name);
    assert(node != PLAN_NODE_NONE);
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
            return &constraint->params.motive;
        }
    }
    return NULL;
}

/* Whether the repetition (m1) is a sequence: the basic idea transposed a step. */
static bool is_sequence(const PiecePlan *plan, const char *name) {
    const PlanMotiveRef *ref = ref_of(plan, name);
    return ref && ref->has_step && ref->step.kind == MOTIVE_RELATION_TRANSPOSE;
}

/*
 * The pairs of the notes of one idea that quote the motif, and all its pairs: the idea is laid against the motif at each
 * start, and the most pairs that have the motif's duration and interval to the next note count. An exact fragment quotes a
 * passage of the motif whole; a sequence moves a step and loses the pairs whose interval changes.
 */
static void count_pairs(const PlanNote *motif, size_t motif_count, const PlanNote *notes, size_t count, size_t *matched,
                        size_t *total) {
    size_t best = 0;
    for (size_t start = 0; start + 1 < motif_count; start++) {
        size_t same = 0;
        for (size_t i = 0; i + 1 < count && start + i + 1 < motif_count; i++) {
            same += rational_cmp(notes[i].duration, motif[start + i].duration) == 0 &&
                            notes[i + 1].midi - notes[i].midi == motif[start + i + 1].midi - motif[start + i].midi
                        ? 1
                        : 0;
        }
        best = same > best ? same : best;
    }
    *matched += best;
    *total += count > 1 ? count - 1 : 0;
}

/*
 * The share of the note pairs of the continuation's two fragments (m2 and m3) that quote the motif. The cadential idea is
 * left out: liquidation shortens it, which would raise a share of pairs; its steps are counted below.
 */
static double matched_share(const PiecePlan *plan) {
    const PlanNote *motif;
    size_t motif_count = written_notes(plan, "m0", &motif);
    size_t matched = 0;
    size_t total = 0;
    for (int k = 2; k <= 3; k++) {
        char name[4];
        const PlanNote *notes;
        snprintf(name, sizeof(name), "m%d", k);
        size_t count = written_notes(plan, name, &notes);
        count_pairs(motif, motif_count, notes, count, &matched, &total);
    }
    return total > 0 ? (double)matched / (double)total : 0.0;
}

static void test_ordered_by_setting(void) {
    static const GrowDevelopment ORDER[] = {GROW_DEVELOP_MELODY, GROW_DEVELOP_BALANCED, GROW_DEVELOP_THEME};
    static const char *const NAMES[] = {"melody", "balanced", "theme"};
    Motive motive = make_motive();
    double mean[3];
    for (int s = 0; s < 3; s++) {
        double sum = 0;
        size_t sequences = 0;
        size_t fragment_sequences = 0;
        size_t least_steps = SEEDS;
        size_t most_steps = 0;
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            PiecePlan *plan = grow(&motive, ORDER[s], seed);
            sum += matched_share(plan);
            sequences += is_sequence(plan, "m1") ? 1 : 0;
            fragment_sequences += is_sequence(plan, "m3") ? 1 : 0;
            size_t steps = ref_of(plan, "m4")->steps;
            least_steps = steps < least_steps ? steps : least_steps;
            most_steps = steps > most_steps ? steps : most_steps;
            plan_destroy(plan);
        }
        mean[s] = sum / SEEDS;
        printf("%-8s: matched share %.3f, sequence repetitions %zu, sequence fragments %zu of %d\n", NAMES[s], mean[s], sequences,
               fragment_sequences, SEEDS);
        if (ORDER[s] == GROW_DEVELOP_MELODY) {
            assert(sequences == 0);
            assert(most_steps <= 1);
        }
        if (ORDER[s] == GROW_DEVELOP_THEME) {
            assert(fragment_sequences == SEEDS);
            /* A residue that keeps a segment may take one step when two keep none (the widened draw, R-345). */
            assert(least_steps >= 1);
        }
    }
    assert(mean[0] > mean[1]);
    assert(mean[1] > mean[2]);
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    test_ordered_by_setting();
    return 0;
}
