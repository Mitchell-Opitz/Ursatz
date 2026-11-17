#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_figure.h>
#include <ursatz_composer/form_return.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan_keys.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/return_hear.h>
#include "support/test_oracles.h"
#include "support/test_composer.h"
#include "ursatz_test.h"
#include "support/test_knowledge.h"

/*
 * The figurate return (form_figure.h, R-497, R-498): tests/fixtures/forms/figure-form.json (a theme and three
 * variations: neighbor tones in eighths; a scale in sixteenths over a reharmonized copy with the root changed at bar 3;
 * a broken chord in sixteenths, the climax), invented motive, seeds 1 to 10.
 *
 *   smoke  C major 2/4, seeds 1 and 2 (quick)
 *   parallel  tests/fixtures/forms/parallel-form.json (a theme, V1 in the parallel mode, V2 in the home key): C major 2/4,
 *          A minor 2/4, C major 6/8 and A minor 3/4: at least 9 of 10 seeds compose in each row; V1's key is the home
 *          tonic in the other mode, V1 is copied, its figure keeps to its kind, its melody fits its chords and holds
 *          only notes of its key's scale (natural and harmonic minor in minor), and the key check passes and counts no change.
 *   sweep  C major 2/4, A minor 2/4, C major 3/4, C major 4/4 and C major 6/8: at least 9 of 10 seeds compose in each
 *          row; in each composed plan the figures keep to their kind (form_figure_check), every phrase and idea of a
 *          variation has the name and length of the theme's at the same offset, V1 has the theme's chords, V2's melody
 *          fits its chords, every cell start holds a tone of its chord, and the plan round-trips through plan JSON.
 *          The counts of raised climaxes, of V2 runs with bar 3 changed and of figurate returns heard are printed
 *          (composed, not heard: R-408).
 */

#define SEEDS 10
#define COMPOSED_LEAST 9
#define MAX_SECTIONS 8

typedef struct {
    const char *name;
    int pitch_class;
    KeyStructureMode mode;
    unsigned beats;
    unsigned unit;
} Row;

static FormSpec *load_form_named(const char *file) {
    char path[1024];
    FormSpec *form = NULL;
    FormSpecError error;
    snprintf(path, sizeof(path), "%s/forms/%s", COMPOSER_FIXTURE_DIR, file);
    char *text = test_must_read_file(path, NULL);
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "%s: %s: %s\n", path, error.path, error.message);
        exit(2);
    }
    free(text);
    return form;
}

static FormSpec *load_form(void) {
    return load_form_named("figure-form.json");
}

static bool equal(Rational a, Rational b) {
    return rational_cmp(a, b) == 0;
}

static Rational minus(Rational a, Rational b) {
    Rational out;
    assert(rational_sub(a, b, &out) == RATIONAL_OK);
    return out;
}

/* The phrases and ideas under two sections have the same names and lengths at the same offsets. */
static bool same_shape(const PiecePlan *plan, NodeId a, NodeId b, NodeId root_a, NodeId root_b) {
    if (plan_node_child_count(plan, a) != plan_node_child_count(plan, b)) {
        return false;
    }
    for (size_t i = 0; i < plan_node_child_count(plan, a); i++) {
        NodeId x = plan_node_child_at(plan, a, i);
        NodeId y = plan_node_child_at(plan, b, i);
        const char *xn = plan_node_name(plan, x);
        const char *yn = plan_node_name(plan, y);
        if (plan_node_kind(plan, x) != plan_node_kind(plan, y) || !xn || !yn || strcmp(xn, yn) != 0 ||
            !equal(plan_node_span(plan, x).length, plan_node_span(plan, y).length) ||
            !equal(minus(plan_node_span(plan, x).start, plan_node_span(plan, root_a).start),
                   minus(plan_node_span(plan, y).start, plan_node_span(plan, root_b).start)) ||
            !same_shape(plan, x, y, root_a, root_b)) {
            return false;
        }
    }
    return true;
}

/* The highest melody pitch of a section. */
static int highest(const PiecePlan *plan, NodeId section) {
    PlanSpan span = plan_node_span(plan, section);
    LaneSpanId ids[64];
    size_t spans = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, 64);
    int top = -1;
    assert(spans <= 64);
    for (size_t s = 0; s < spans; s++) {
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[s])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            top = notes->notes[n].midi > top ? notes->notes[n].midi : top;
        }
    }
    return top;
}

typedef struct {
    size_t composed;
    size_t raised;
    size_t bar_changed;
    size_t heard;
    size_t returns;
} Counts;

static void check_plan(PiecePlan *plan, const ComposeOutcome *outcome, Counts *counts) {
    NodeId sections[MAX_SECTIONS];
    NodeId root = plan_root(plan);
    size_t count = 0;
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId child = plan_node_child_at(plan, root, i);
        if (!plan_node_find_constraint(plan, child, PLAN_CONSTRAINT_MATERIAL)) {
            assert(count < MAX_SECTIONS);
            sections[count++] = child;
        }
    }
    assert(count == 4);
    NodeId theme = sections[0];
    for (size_t v = 1; v < count; v++) {
        char reason[256] = "";
        size_t changed = 0;
        size_t total = 0;
        if (!form_figure_check(plan, sections[v], reason, sizeof(reason))) {
            fprintf(stderr, "variation %zu: %s\n", v, reason);
            assert(false);
        }
        assert(same_shape(plan, theme, sections[v], theme, sections[v]));
        assert(form_return_melody_fits(plan, sections[v]));
        assert(form_return_chords_changed(plan, sections[v], &changed, &total));
        if (v == 1) {
            assert(changed == 0);
        }
        const PlanReturn *ignored = NULL;
        (void)ignored;
        counts->returns++;
        counts->heard += return_heard_find(&outcome->returns, theme, sections[v]) != NULL;
    }
    bool bar_changed = false;
    assert(form_return_bar_changed(plan, sections[2], 3, &bar_changed));
    counts->bar_changed += bar_changed;
    counts->raised += highest(plan, sections[3]) > highest(plan, theme);
    /* The plan round-trips through plan JSON. */
    char *json = plan_to_json(plan, NULL);
    PiecePlan *again = NULL;
    assert(json && plan_from_json(json, &again, NULL) == PLAN_OK);
    char *second = plan_to_json(again, NULL);
    assert(second && strcmp(json, second) == 0);
    free(json);
    free(second);
    plan_destroy(again);
}

/* The key of the section: its one key lane span. */
static AnalysisKey key_of(const PiecePlan *plan, NodeId section) {
    LaneSpanId ids[2];
    assert(plan_lane_find(plan, PLAN_LANE_KEY, 0, plan_node_span(plan, section), ids, 2) == 1);
    return plan_lane_span_payload(plan, ids[0])->key;
}

/* Every melody note of the section is in the scale of the key: in minor the natural minor's and the harmonic minor's (the figures use the raised seventh over V and vii only). */
static bool melody_in_scale(const PiecePlan *plan, NodeId section, AnalysisKey key) {
    static const bool MAJOR[12] = {true, false, true, false, true, true, false, true, false, true, false, true};
    static const bool MINOR[12] = {true, false, true, true, false, true, false, true, true, false, true, true};
    LaneSpanId ids[64];
    size_t spans = plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, section), ids, 64);
    assert(spans <= 64);
    for (size_t s = 0; s < spans; s++) {
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[s])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            int degree = ((notes->notes[n].midi - analysis_key_tonic_pitch_class(&key)) % 12 + 12) % 12;
            if (!(key.mode == KEY_STRUCTURE_MODE_MAJOR ? MAJOR[degree] : MINOR[degree])) {
                return false;
            }
        }
    }
    return true;
}

static void check_parallel_plan(PiecePlan *plan) {
    NodeId sections[MAX_SECTIONS];
    NodeId root = plan_root(plan);
    size_t count = 0;
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId child = plan_node_child_at(plan, root, i);
        if (!plan_node_find_constraint(plan, child, PLAN_CONSTRAINT_MATERIAL)) {
            assert(count < MAX_SECTIONS);
            sections[count++] = child;
        }
    }
    assert(count == 3);
    AnalysisKey home = key_of(plan, sections[0]);
    AnalysisKey other = key_of(plan, sections[1]);
    assert(analysis_key_tonic_pitch_class(&other) == analysis_key_tonic_pitch_class(&home) && other.mode != home.mode);
    AnalysisKey last = key_of(plan, sections[2]);
    assert(analysis_key_equals(&last, &home));
    char reason[256] = "";
    assert(form_return_copies(plan, sections[1]));
    if (!form_figure_check(plan, sections[1], reason, sizeof(reason))) {
        fprintf(stderr, "variation 1: %s\n", reason);
        assert(false);
    }
    assert(form_figure_check(plan, sections[2], reason, sizeof(reason)));
    assert(form_return_melody_fits(plan, sections[1]));
    assert(melody_in_scale(plan, sections[1], other));
    PieceKeyCheck keys;
    memset(&keys, 0, sizeof(keys));
    assert(test_piece_check_keys(plan, &keys) && keys.ok && keys.changes == 0);
    char *json = plan_to_json(plan, NULL);
    PiecePlan *again = NULL;
    assert(json && plan_from_json(json, &again, NULL) == PLAN_OK);
    char *second = plan_to_json(again, NULL);
    assert(second && strcmp(json, second) == 0);
    free(json);
    free(second);
    plan_destroy(again);
}

static int run_row(const FormSpec *form, const Row *row, unsigned long seeds, bool parallel) {
    Counts counts;
    memset(&counts, 0, sizeof(counts));
    for (unsigned long seed = 1; seed <= seeds; seed++) {
        PieceRequest request;
        PiecePlan *plan = NULL;
        ComposeOutcome outcome;
        ComposeConfig config;
        memset(&request, 0, sizeof(request));
        request.form = form->name;
        request.key = test_key_mode(row->pitch_class, row->mode);
        request.knowledge = test_knowledge();
        request.meter.beats_per_bar = row->beats;
        request.meter.beat_unit = row->unit;
        assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
        memset(&config, 0, sizeof(config));
        config.knowledge = test_knowledge();
        config.seed = seed;
        config.form = form;
        if (compose_run(plan, &config, &outcome) == PIECE_OK) {
            if (parallel) {
                check_parallel_plan(plan);
            } else {
                check_plan(plan, &outcome, &counts);
            }
            counts.composed++;
        } else {
            if (getenv("DBG")) { NodeId r = plan_root(plan); for (size_t q = 0; q < plan_node_child_count(plan, r); q++) { NodeId c = plan_node_child_at(plan, r, q); printf("DBGSEC %zu children %zu state %d handles %d copies %d\n", q, plan_node_child_count(plan, c), (int)plan_node_state(plan, c), (int)form_return_handles(plan, c), (int)form_return_copies(plan, c)); } } if (0 && outcome.records[COMPOSE_STAGE_HARMONY]) { char *j = fill_record_to_json(outcome.records[COMPOSE_STAGE_HARMONY]); printf("%s\n", j); free(j); char *pj = plan_to_json(plan, NULL); printf("PLAN %s\n", pj); free(pj); }
            printf("  %s seed %lu: not composed (stuck at %s)\n", row->name, seed,
                   outcome.stuck ? compose_stage_name(outcome.stuck_stage) : "-");
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    if (parallel) {
        printf("%s: %zu of %lu seeds compose (parallel mode)\n", row->name, counts.composed, seeds);
    } else {
        printf("%s: %zu of %lu seeds compose; V3 raised %zu, V2 bar 3 changed %zu, figurate returns heard %zu of %zu\n",
               row->name, counts.composed, seeds, counts.raised, counts.bar_changed, counts.heard, counts.returns);
    }
    if (seeds == SEEDS && counts.composed < COMPOSED_LEAST) {
        fprintf(stderr, "%s: at least %d of %d seeds should compose\n", row->name, COMPOSED_LEAST, SEEDS);
        return 1;
    }
    return counts.composed > 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    static const Row ROWS[] = {
        {"C major 2/4", 0, KEY_STRUCTURE_MODE_MAJOR, 2, 4}, {"A minor 2/4", 9, KEY_STRUCTURE_MODE_MINOR, 2, 4},
        {"C major 3/4", 0, KEY_STRUCTURE_MODE_MAJOR, 3, 4}, {"C major 4/4", 0, KEY_STRUCTURE_MODE_MAJOR, 4, 4},
        {"C major 6/8", 0, KEY_STRUCTURE_MODE_MAJOR, 6, 8},
    };
    const char *mode = argc > 1 ? argv[1] : "smoke";
    FormSpec *form = load_form();
    int failed = 0;
    if (strcmp(mode, "smoke") == 0) {
        failed = run_row(form, &ROWS[0], 2, false);
    } else if (strcmp(mode, "sweep") == 0) {
        for (size_t r = 0; r < sizeof(ROWS) / sizeof(ROWS[0]); r++) {
            failed |= run_row(form, &ROWS[r], SEEDS, false);
        }
    } else if (strcmp(mode, "parallel") == 0) {
        static const Row PARALLEL[] = {
            {"C major 2/4", 0, KEY_STRUCTURE_MODE_MAJOR, 2, 4}, {"A minor 2/4", 9, KEY_STRUCTURE_MODE_MINOR, 2, 4},
            {"C major 6/8", 0, KEY_STRUCTURE_MODE_MAJOR, 6, 8}, {"A minor 3/4", 9, KEY_STRUCTURE_MODE_MINOR, 3, 4},
        };
        FormSpec *parallel = load_form_named("parallel-form.json");
        for (size_t r = 0; r < sizeof(PARALLEL) / sizeof(PARALLEL[0]); r++) {
            failed |= run_row(parallel, &PARALLEL[r], SEEDS, true);
        }
        form_spec_destroy(parallel);
    } else {
        fprintf(stderr, "usage: test_form_figure smoke|sweep|parallel\n");
        failed = 2;
    }
    form_spec_destroy(form);
    return failed;
}
