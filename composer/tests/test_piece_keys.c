#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-harmony/harmony_cadence_rules.h>
#include <standard-harmony/harmony_chord.h>
#include <standard-harmony/harmony_pivot.h>
#include <standard-harmony/harmony_related_key.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan_keys.h>

/*
 * Piece plans that change key follow standard-harmony's key-change rules (R-069). Every template, in C major and A
 * minor 4/4 with an invented motive, seeds 1 to 10, asking to modulate:
 *
 *   plan <>            the plans only: the key lane tiles the piece, every key is closely related to the home key, a
 *                      section's KEY constraint is the key it ends in, the cadences pass piece_check_cadences, and a
 *                      template that does not modulate (loop, layered) keeps one key span
 *   compose <template> the composed runs: of those that complete, every key change is opened by a pivot chord
 *                      standard-harmony calls valid, every planned modulation is a modulation confirmed by an
 *                      authentic cadence heard inside the new key, and the last cadence is the strongest. The share
 *                      of runs that complete is printed; below 100% the test fails (WILL_FAIL, docs/piece.md)
 *   smoke              the first of seeds 1 to 5 of binary in C major that composes
 */

#define SEEDS 10

typedef struct {
    const char *name;
    int tonic;
    KeyStructureMode mode;
} Scenario;

static const Scenario SCENARIOS[] = {{"C major", 0, KEY_STRUCTURE_MODE_MAJOR}, {"A minor", 9, KEY_STRUCTURE_MODE_MINOR}};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

static PieceRequest request_of(const Scenario *scenario, FormTemplateKind kind, unsigned long seed) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = kind;
    request.key = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.modulate = true;
    request.seed = seed;
    return request;
}

static HarmonyMode harmony_mode(KeyStructureMode mode) {
    return mode == KEY_STRUCTURE_MODE_MINOR ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
}

/* The closely related key of `home` that is `key`, from standard-harmony's own list. False when it is none. */
static bool related(AnalysisKey home, AnalysisKey key, HarmonyRelatedKey *out) {
    HarmonyMode home_mode = harmony_mode(home.mode);
    int semitone = ((analysis_key_tonic_pitch_class(&key) - analysis_key_tonic_pitch_class(&home)) % 12 + 12) % 12;
    for (int i = 0; i < harmony_related_key_count(home_mode); i++) {
        HarmonyRelatedKey candidate;
        assert(harmony_related_key_at(home_mode, i, &candidate));
        if (candidate.mode == harmony_mode(key.mode) && harmony_related_key_semitone(home_mode, &candidate) == semitone) {
            *out = candidate;
            return harmony_key_is_closely_related(home_mode, &candidate);
        }
    }
    return false;
}

static bool same_key(AnalysisKey a, AnalysisKey b) {
    return analysis_key_tonic_pitch_class(&a) == analysis_key_tonic_pitch_class(&b) && a.mode == b.mode;
}

typedef struct {
    PlanSpan span[32];
    AnalysisKey key[32];
    size_t count;
} KeyLane;

static KeyLane read_lane(const PiecePlan *plan) {
    KeyLane lane;
    LaneSpanId ids[32];
    PlanSpan all = {{0, 1}, {100000, 1}};
    lane.count = plan_lane_find(plan, PLAN_LANE_KEY, 0, all, ids, 32);
    assert(lane.count >= 1 && lane.count <= 32);
    for (size_t i = 0; i < lane.count; i++) {
        lane.span[i] = plan_lane_span_span(plan, ids[i]);
        lane.key[i] = plan_lane_span_payload(plan, ids[i])->key;
        assert(plan_lane_span_state(plan, ids[i]) == PLAN_STATE_GIVEN);
    }
    return lane;
}

/* The key lane tiles the plan from 0 to the end of the form, and neighbours differ (they are one span otherwise). */
static void check_lane_tiles(const PiecePlan *plan, const KeyLane *lane) {
    Rational at = {0, 1};
    for (size_t i = 0; i < lane->count; i++) {
        assert(rational_cmp(lane->span[i].start, at) == 0);
        assert(plan_span_end(lane->span[i], &at) == PLAN_OK);
        assert(i == 0 || !same_key(lane->key[i], lane->key[i - 1]));
    }
    Rational form_end;
    assert(plan_span_end(plan_node_span(plan, plan_root(plan)), &form_end) == PLAN_OK);
    assert(rational_cmp(at, form_end) == 0);
}

static AnalysisKey key_at(const KeyLane *lane, Rational time) {
    for (size_t i = 0; i < lane->count; i++) {
        Rational end;
        assert(plan_span_end(lane->span[i], &end) == PLAN_OK);
        if (rational_cmp(time, end) < 0) {
            return lane->key[i];
        }
    }
    assert(!"a time inside the form");
    return lane->key[0];
}

static const PlanConstraint *find(const PiecePlan *plan, NodeId id, PlanConstraintKind kind) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
        if (plan_node_constraint_at(plan, id, i)->kind == kind) {
            return plan_node_constraint_at(plan, id, i);
        }
    }
    return NULL;
}

/* The sections of a plan, not the material. */
static size_t sections_of(const PiecePlan *plan, NodeId *out) {
    size_t count = 0;
    NodeId form = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, form); i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        if (!find(plan, child, PLAN_CONSTRAINT_MATERIAL)) {
            out[count++] = child;
        }
    }
    return count;
}

/* ------------------------------------------------------------- the plans */

static void check_plan(const PiecePlan *plan, const Scenario *scenario, FormTemplateKind kind) {
    KeyLane lane = read_lane(plan);
    check_lane_tiles(plan, &lane);
    AnalysisKey home = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    assert(same_key(lane.key[0], home));
    for (size_t i = 0; i < lane.count; i++) {
        HarmonyRelatedKey unused;
        assert(same_key(lane.key[i], home) || related(home, lane.key[i], &unused));
    }
    NodeId sections[FORM_TEMPLATE_MAX_SECTIONS];
    size_t count = sections_of(plan, sections);
    for (size_t s = 0; s < count; s++) {
        /* The KEY constraint is the key the section ends in: the key under its last bar. */
        PlanSpan span = plan_node_span(plan, sections[s]);
        Rational end;
        Rational last_bar;
        assert(plan_span_end(span, &end) == PLAN_OK);
        assert(rational_make(-1, 1, &last_bar) == RATIONAL_OK);
        assert(rational_add(end, last_bar, &last_bar) == RATIONAL_OK);
        const PlanConstraint *key = find(plan, sections[s], PLAN_CONSTRAINT_KEY);
        assert(key && same_key(key->params.key, key_at(&lane, last_bar)));
    }
    PieceCadenceCheck cadences;
    assert(piece_check_cadences(plan, &cadences) && cadences.ok);
    if (kind == FORM_TEMPLATE_LOOP || kind == FORM_TEMPLATE_LAYERED) {
        assert(lane.count == 1);
    }
}

static int test_plans(void) {
    size_t plans = 0;
    size_t modulating = 0;
    for (int k = 0; k < FORM_TEMPLATE_KIND_COUNT; k++) {
        for (size_t s = 0; s < SCENARIO_COUNT; s++) {
            size_t modulated = 0;
            for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                PieceRequest request = request_of(&SCENARIOS[s], (FormTemplateKind)k, seed);
                PiecePlan *plan = NULL;
                assert(piece_build_plan(&request, &plan) == PIECE_OK);
                check_plan(plan, &SCENARIOS[s], (FormTemplateKind)k);
                modulated += read_lane(plan).count > 1;
                plan_destroy(plan);
                /* Without the request to modulate there is one key and the template's cadences. */
                request.modulate = false;
                assert(piece_build_plan(&request, &plan) == PIECE_OK);
                assert(read_lane(plan).count == 1);
                plan_destroy(plan);
                plans++;
            }
            printf("%s, %s: %zu of %d plans change key\n", form_template_name((FormTemplateKind)k), SCENARIOS[s].name,
                   modulated, SEEDS);
            bool can = k != FORM_TEMPLATE_LOOP && k != FORM_TEMPLATE_LAYERED;
            assert(can ? modulated > 0 : modulated == 0);
            modulating += modulated;
        }
    }
    printf("%zu plans, %zu change key\n", plans, modulating);
    return 0;
}

/* ------------------------------------------------------------ the pieces */

/* Whether the chord at the start of a key change is a pivot chord between the two keys, by standard-harmony. */
static bool opens_with_pivot(const PiecePlan *plan, AnalysisKey from, AnalysisKey to, Rational at) {
    PlanSpan around = {at, {1, 1000}};
    LaneSpanId chord;
    HarmonyRelatedKey key;
    HarmonyChord parsed;
    if (plan_lane_find(plan, PLAN_LANE_HARMONY, 0, around, &chord, 1) != 1 ||
        rational_cmp(plan_lane_span_span(plan, chord).start, at) != 0 || !related(from, to, &key) ||
        !harmony_roman_parse(to.mode, plan_lane_span_payload(plan, chord)->harmony.roman_numeral, &parsed) ||
        parsed.inversion != 0 || parsed.seventh) {
        return false;
    }
    HarmonyPivot pivots[HARMONY_PIVOT_MAX];
    int found = harmony_pivot_find(harmony_mode(from.mode), &key, pivots);
    for (int i = 0; i < found; i++) {
        if (pivots[i].new_degree == parsed.degree && harmony_pivot_valid(harmony_mode(from.mode), &key, &pivots[i])) {
            return true;
        }
    }
    return false;
}

static double as_double(Rational time) {
    return (double)time.num / (double)time.den;
}

/* Whether an authentic cadence arrives in [start, end), heard section by section. */
static bool authentic_inside(const PiecePlan *plan, const NodeId *sections, size_t count, Rational start, Rational end) {
    for (size_t s = 0; s < count; s++) {
        FormHeard heard;
        assert(form_hear(plan, plan_node_span(plan, sections[s]), &heard) == FORM_HEAR_OK);
        for (size_t i = 0; i < heard.cadence_count; i++) {
            Rational at = heard.cadences[i].arrival;
            int type = heard.cadences[i].type;
            /* The Analyzer hears an authentic cadence it cannot call perfect or imperfect (no soprano under it, the
               piece's last) as authentic-unspecified; piece_check_keys counts it too. */
            if ((type == CADENCE_TYPE_PERFECT_AUTHENTIC || type == CADENCE_TYPE_IMPERFECT_AUTHENTIC ||
                 type == CADENCE_TYPE_AUTHENTIC_UNSPECIFIED) &&
                rational_cmp(at, start) >= 0 && rational_cmp(at, end) < 0) {
                form_heard_destroy(&heard);
                return true;
            }
        }
        form_heard_destroy(&heard);
    }
    return false;
}

static void check_piece(const PiecePlan *plan, FormTemplateKind kind) {
    KeyLane lane = read_lane(plan);
    NodeId sections[FORM_TEMPLATE_MAX_SECTIONS];
    size_t count = sections_of(plan, sections);
    Rational piece_end;
    assert(plan_span_end(plan_node_span(plan, sections[count - 1]), &piece_end) == PLAN_OK);
    size_t changes = 0;
    for (size_t k = 1; k < lane.count; k++) {
        changes++;
        /* Opened by a pivot chord standard-harmony calls valid. */
        assert(opens_with_pivot(plan, lane.key[k - 1], lane.key[k], lane.span[k].start));
        /* A modulation: an authentic cadence arrives inside the new key's span, before the piece ends. */
        Rational end;
        assert(plan_span_end(lane.span[k], &end) == PLAN_OK);
        if (rational_cmp(end, piece_end) > 0) {
            end = piece_end;
        }
        assert(authentic_inside(plan, sections, count, lane.span[k].start, end));
        assert(as_double(end) > as_double(lane.span[k].start));
    }
    PieceKeyCheck keys;
    assert(piece_check_keys(plan, &keys) && keys.ok);
    assert(keys.changes == changes && keys.modulations == changes);
    /* The last cadence is the strongest of the piece's, by standard-harmony's order. */
    if (kind != FORM_TEMPLATE_LOOP && kind != FORM_TEMPLATE_LAYERED) {
        int last = -1;
        int strongest = -1;
        for (size_t s = 0; s < count; s++) {
            const PlanConstraint *cadence = find(plan, sections[s], PLAN_CONSTRAINT_CADENCE);
            int strength = harmony_cadence_strength(cadence->params.cadence);
            strongest = strength > strongest ? strength : strongest;
            last = strength;
        }
        assert(last == strongest);
        PieceCadenceCheck cadences;
        assert(piece_check_cadences(plan, &cadences) && cadences.ok);
    }
}

typedef struct {
    size_t runs;
    size_t completed;
    size_t changing; /* completed runs that change key */
} Tally;

static bool compose_one(const Scenario *scenario, FormTemplateKind kind, unsigned long seed, Tally *tally) {
    PieceRequest request = request_of(scenario, kind, seed);
    PiecePlan *plan = NULL;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    ComposeConfig config;
    ComposeOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    PieceResult ran = compose_run(plan, &config, &outcome);
    tally->runs++;
    bool done = ran == PIECE_OK;
    if (done) {
        tally->completed++;
        tally->changing += read_lane(plan).count > 1;
        check_piece(plan, kind);
    } else {
        assert(ran == PIECE_ERR_STUCK);
        printf("  %s, %s, seed %lu: stuck at %s%s%s\n", form_template_name(kind), scenario->name, seed,
               compose_stage_name(outcome.stuck_stage), outcome.key_rejected ? ": " : "",
               outcome.key_rejected ? (outcome.key_check.ok ? outcome.cadence_check.reason : outcome.key_check.reason)
                                    : "");
    }
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);
    return done;
}

static int test_compose(FormTemplateKind kind) {
    Tally tally;
    memset(&tally, 0, sizeof(tally));
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            compose_one(&SCENARIOS[s], kind, seed, &tally);
        }
    }
    printf("%s: %zu of %zu runs complete (%.0f%%), %zu of them change key\n", form_template_name(kind),
           tally.completed, tally.runs, 100.0 * (double)tally.completed / (double)tally.runs, tally.changing);
    return tally.completed == tally.runs ? 0 : 1;
}

static int test_smoke(void) {
    Tally tally;
    memset(&tally, 0, sizeof(tally));
    for (unsigned long seed = 1; seed <= 5; seed++) {
        if (compose_one(&SCENARIOS[0], FORM_TEMPLATE_BINARY, seed, &tally) && tally.changing > 0) {
            printf("binary: seed %lu composes, changes key, and passes the checks\n", seed);
            return 0;
        }
    }
    printf("binary: no seed of 1 to 5 composes a piece that changes key\n");
    return 1;
}

int main(int argc, char **argv) {
    FormTemplateKind kind;
    if (argc == 2 && strcmp(argv[1], "plan") == 0) {
        return test_plans();
    }
    if (argc == 2 && strcmp(argv[1], "smoke") == 0) {
        return test_smoke();
    }
    if (argc == 3 && strcmp(argv[1], "compose") == 0 && form_template_from_name(argv[2], &kind)) {
        return test_compose(kind);
    }
    fprintf(stderr, "usage: test_piece_keys plan|smoke|compose <template>\n");
    return 2;
}
