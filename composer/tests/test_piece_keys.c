#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/harmony/harmony_cadence_rules.h>
#include <theory/harmony/harmony_chord.h>
#include <theory/harmony/harmony_pivot.h>
#include <theory/harmony/harmony_related_key.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan_keys.h>
#include "support/test_composer.h"
#include "support/test_knowledge.h"

/*
 * Piece plans that change key follow standard-harmony's key-change rules (R-069). Every template, in C major and A
 * minor 4/4 (3/4 for the forms that list it, the minuet, the trio and the scherzo) with an invented motive, seeds 1 to 10, asking to modulate:
 *
 *   plan <>            the plans only: the key lane tiles the piece, every key is closely related to the home key, a
 *                      section's KEY constraint is the key it ends in, the cadences pass piece_check_cadences, and a
 *                      template that does not modulate (loop, layered) keeps one key span; a form that modulates always
 *                      (the scherzo) changes key without the request; a span in the home tonic's other mode (the figured
 *                      variations' V3, R-499) is not a change of key: it is related to home, has no pivot and is not counted
 *   compose <template> the composed runs: of those that complete, every key change is opened by a pivot chord
 *                      standard-harmony calls valid, every planned modulation is a modulation confirmed by an
 *                      authentic cadence heard inside the new key, and the last cadence is the strongest. The share
 *                      of seeds that complete is printed, each tried up to 16 times from the next seeds as the piece
 *                      entry does; below 100% the test fails (WILL_FAIL, docs/piece.md)
 *   tonic-major        the stand-in form of R-517 (B1 returns B exact in the tonic major; 56 bars), A minor and C major 4/4,
 *                      seeds 1 to 10: the plan's key lane gives B1 A major in A minor and C major in C major, and the
 *                      sections after it home; at least 9 of 10 compose in each; B1's melody is B's moved by the semitones the
 *                      return operator gives, and the section after B1 is in the home key
 *   tonic-major smoke  the same on seed 1 alone, which must compose in each, with no rate (B11 of P56)
 *   smoke              the first of seeds 1 to 5 of binary in C major that composes
 */

#define SEEDS 10
#define SEED_TRIES 16

typedef struct {
    const char *name;
    int tonic;
    KeyStructureMode mode;
} Scenario;

static const Scenario SCENARIOS[] = {{"C major", 0, KEY_STRUCTURE_MODE_MAJOR}, {"A minor", 9, KEY_STRUCTURE_MODE_MINOR}};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

/* The beats of a bar the template's pieces are written in: 4/4, or the form's first meter when it lists meters (the
 * minuet and the trio are in 3/4, R-487). */
static int beats_of(const char *kind) {
    FormSpec *form = NULL;
    assert(form_spec_builtin(kind, &form) == FORM_SPEC_OK);
    int beats = form->meter_count > 0 ? form->meters[0].beats_per_bar : 4;
    form_spec_destroy(form);
    return beats;
}

static PieceRequest request_of(const Scenario *scenario, const char *kind, unsigned long seed) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = kind;
    request.key = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    request.meter.beats_per_bar = beats_of(kind);
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

/* The same tonic in the other mode: a change of mode on one tonic is not a modulation (R-499). */
static bool same_tonic(AnalysisKey a, AnalysisKey b) {
    return analysis_key_tonic_pitch_class(&a) == analysis_key_tonic_pitch_class(&b);
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

/* How many times the lane changes tonic: a span in the parallel mode is not a change of key. */
static size_t tonic_changes(const KeyLane *lane) {
    size_t changes = 0;
    for (size_t k = 1; k < lane->count; k++) {
        changes += !same_tonic(lane->key[k], lane->key[k - 1]);
    }
    return changes;
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

static void check_plan(const PiecePlan *plan, const Scenario *scenario, const char *kind) {
    KeyLane lane = read_lane(plan);
    check_lane_tiles(plan, &lane);
    AnalysisKey home = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    assert(same_key(lane.key[0], home));
    for (size_t i = 0; i < lane.count; i++) {
        HarmonyRelatedKey unused;
        assert(same_key(lane.key[i], home) || same_tonic(lane.key[i], home) || related(home, lane.key[i], &unused));
    }
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
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
    assert(piece_check_cadences(plan, NULL, &cadences) && cadences.ok);
    if (strcmp(kind, "loop") == 0 || strcmp(kind, "layered") == 0) {
        assert(lane.count == 1);
    }
}

/* Whether the built-in form comes in sizes with a plan of its own: the game loop's sections are the request's (P44 B5), and
 * a compound form's parts are forms of their own (R-485). */
static bool has_sizes(const char *name) {
    FormSpec *form = NULL;
    assert(form_spec_builtin(name, &form) == FORM_SPEC_OK);
    bool sized = !form->request_sections && form->part_count == 0;
    form_spec_destroy(form);
    return sized;
}

/* Whether the built-in form has regions to leave home for (the loop, the layered form and the figured variations have none). */
static bool has_regions(const char *name) {
    FormSpec *form = NULL;
    assert(form_spec_builtin(name, &form) == FORM_SPEC_OK);
    bool regions = form->region_count > 0;
    form_spec_destroy(form);
    return regions;
}

/* How many sections of the form's default size are in the home tonic's other mode (R-499). */
static size_t parallel_sections(const char *name) {
    FormSpec *form = NULL;
    assert(form_spec_builtin(name, &form) == FORM_SPEC_OK);
    const FormSpecSize *size = form_spec_default_size(form);
    size_t parallel = 0;
    for (size_t i = 0; size && i < size->section_count; i++) {
        parallel += size->sections[i].parallel;
    }
    form_spec_destroy(form);
    return parallel;
}

/* Whether the built-in form leaves home whether or not the request asks for modulate (R-493). */
static bool always_modulates(const char *name) {
    FormSpec *form = NULL;
    assert(form_spec_builtin(name, &form) == FORM_SPEC_OK);
    bool always = form->modulates;
    form_spec_destroy(form);
    return always;
}

static int test_plans(void) {
    size_t plans = 0;
    size_t modulating = 0;
    for (size_t form = 0; form < form_spec_builtin_count(); form++) {
        const char *k = form_spec_builtin_name(form);
        if (!has_sizes(k)) {
            continue;
        }
        for (size_t s = 0; s < SCENARIO_COUNT; s++) {
            size_t modulated = 0;
            for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                PieceRequest request = request_of(&SCENARIOS[s], k, seed);
                PiecePlan *plan = NULL;
                assert(piece_build_plan(&request, &plan) == PIECE_OK);
                check_plan(plan, &SCENARIOS[s], k);
                KeyLane planned = read_lane(plan);
                modulated += tonic_changes(&planned) > 0;
                plan_destroy(plan);
                /* Without the request to modulate there is one key and the template's cadences, except that a form that
                   modulates always (the scherzo, R-493) leaves home as if it were asked to, and every key check passes. */
                request.modulate = false;
                assert(piece_build_plan(&request, &plan) == PIECE_OK);
                KeyLane unasked = read_lane(plan);
                if (always_modulates(k)) {
                    assert(tonic_changes(&unasked) > 0);
                    check_plan(plan, &SCENARIOS[s], k);
                } else {
                    /* One key, or home and the parallel mode of the sections that are in it: the figured variations'
                       V3 is a span of its own between two home spans (R-499). */
                    assert(tonic_changes(&unasked) == 0 && unasked.count == (parallel_sections(k) > 0 ? 3u : 1u));
                }
                plan_destroy(plan);
                plans++;
            }
            printf("%s, %s: %zu of %d plans change key\n", k, SCENARIOS[s].name,
                   modulated, SEEDS);
            bool can = has_regions(k);
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

/* Whether an authentic cadence heard in `span` arrives in [start, end). */
static bool authentic_in_part(const PiecePlan *plan, PlanSpan span, Rational start, Rational end) {
    FormHeard heard;
    assert(test_form_hear(plan, span, &heard) == FORM_HEAR_OK);
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
    return false;
}

/*
 * Whether an authentic cadence arrives in [start, end), heard as piece_check_keys hears it: each section whole, and the
 * part of a section after a key change inside it, from the change, in its new key.
 */
static bool authentic_inside(const PiecePlan *plan, const KeyLane *lane, const NodeId *sections, size_t count, Rational start,
                             Rational end) {
    for (size_t s = 0; s < count; s++) {
        PlanSpan whole = plan_node_span(plan, sections[s]);
        Rational whole_end;
        assert(plan_span_end(whole, &whole_end) == PLAN_OK);
        if (authentic_in_part(plan, whole, start, end)) {
            return true;
        }
        for (size_t k = 0; k < lane->count; k++) {
            Rational at = lane->span[k].start;
            Rational negative = {-at.num, at.den};
            PlanSpan part = {at, {0, 1}};
            if (rational_cmp(at, whole.start) > 0 && rational_cmp(at, whole_end) < 0 &&
                rational_add(whole_end, negative, &part.length) == RATIONAL_OK &&
                authentic_in_part(plan, part, start, end)) {
                return true;
            }
        }
    }
    return false;
}

/* Whether a retransition starts at `at`: it opens the home key straight from any region, over the arrival on V, with no
   pivot chord (R-505); V is one in the main and subordinate themes' V region and none in the small rondo's IV or vi. */
static bool opens_retransition(const PiecePlan *plan, const NodeId *sections, size_t count, Rational at) {
    for (size_t s = 0; s < count; s++) {
        const PlanConstraint *function = find(plan, sections[s], PLAN_CONSTRAINT_SECTION_FUNCTION);
        if (function && function->params.section_function.function == PLAN_FUNCTION_RETRANSITION &&
            rational_cmp(plan_node_span(plan, sections[s]).start, at) == 0) {
            return true;
        }
    }
    return false;
}

/* Whether a sequence starts at `at`: a section that returns the one just before it, moved into another key, opens that key
   with its pattern's own first chord and no pivot (the sonata rondo's S after P, vi to ii, R-520). */
static bool opens_sequence(const PiecePlan *plan, const NodeId *sections, size_t count, Rational at) {
    for (size_t s = 0; s < count; s++) {
        const PlanConstraint *back = find(plan, sections[s], PLAN_CONSTRAINT_RETURN);
        Rational source_end;
        if (back && rational_cmp(plan_node_span(plan, sections[s]).start, at) == 0 &&
            plan_span_end(plan_node_span(plan, back->params.section_return.section), &source_end) == PLAN_OK &&
            rational_cmp(source_end, at) == 0) {
            return true;
        }
    }
    return false;
}

static void check_piece(const PiecePlan *plan, const char *kind) {
    KeyLane lane = read_lane(plan);
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    size_t count = sections_of(plan, sections);
    Rational piece_end;
    assert(plan_span_end(plan_node_span(plan, sections[count - 1]), &piece_end) == PLAN_OK);
    size_t changes = 0;
    for (size_t k = 1; k < lane.count; k++) {
        /* A change of mode on one tonic has no pivot and is no modulation: the key check neither counts nor classifies it. */
        if (same_tonic(lane.key[k], lane.key[k - 1])) {
            continue;
        }
        changes++;
        /* Opened by a pivot chord standard-harmony calls valid, except a retransition home (R-505) or a sequence (R-492). Two keys that are not closely related to each other (the
           scherzo's V to IV, a sequence of its pattern, R-492) have no pivot between them: each is related to home. */
        HarmonyRelatedKey adjacent;
        if (related(lane.key[k - 1], lane.key[k], &adjacent) && !opens_retransition(plan, sections, count, lane.span[k].start) &&
            !opens_sequence(plan, sections, count, lane.span[k].start)) {
            assert(opens_with_pivot(plan, lane.key[k - 1], lane.key[k], lane.span[k].start));
        }
        /* A modulation: an authentic cadence arrives inside the new key's span, before the piece ends. */
        Rational end;
        assert(plan_span_end(lane.span[k], &end) == PLAN_OK);
        if (rational_cmp(end, piece_end) > 0) {
            end = piece_end;
        }
        assert(authentic_inside(plan, &lane, sections, count, lane.span[k].start, end));
        assert(as_double(end) > as_double(lane.span[k].start));
    }
    PieceKeyCheck keys;
    assert(test_piece_check_keys(plan, &keys) && keys.ok);
    assert(keys.changes == changes && keys.modulations == changes);
    /* The last cadence is the strongest of the piece's, by standard-harmony's order. */
    if (strcmp(kind, "loop") != 0 && strcmp(kind, "layered") != 0) {
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
        assert(piece_check_cadences(plan, NULL, &cadences) && cadences.ok);
    }
}

typedef struct {
    size_t runs;
    size_t completed;
    size_t changing; /* completed runs that change key */
} Tally;

static bool compose_one(const Scenario *scenario, const char *kind, unsigned long seed, Tally *tally) {
    PieceRequest request = request_of(scenario, kind, seed);
    PiecePlan *plan = NULL;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    ComposeConfig config;
    ComposeOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
    config.seed = seed;
    PieceResult ran = compose_run(plan, &config, &outcome);
    tally->runs++;
    bool done = ran == PIECE_OK;
    if (done) {
        tally->completed++;
        KeyLane composed = read_lane(plan);
        tally->changing += tonic_changes(&composed) > 0;
        check_piece(plan, kind);
    } else {
        assert(ran == PIECE_ERR_STUCK);
        printf("  %s, %s, seed %lu: stuck at %s%s%s\n", kind, scenario->name, seed,
               compose_stage_name(outcome.stuck_stage), outcome.key_rejected ? ": " : "",
               outcome.key_rejected ? (outcome.key_check.ok ? outcome.cadence_check.reason : outcome.key_check.reason)
                                    : "");
    }
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);
    return done;
}

static int test_compose(const char *kind) {
    Tally tally;
    memset(&tally, 0, sizeof(tally));
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            /* A stuck run is tried again from the next seed, as the piece entry does (PIECE_SEED_TRIES). */
            Tally attempt;
            bool done = false;
            for (unsigned long extra = 0; extra < SEED_TRIES && !done; extra++) {
                memset(&attempt, 0, sizeof(attempt));
                done = compose_one(&SCENARIOS[s], kind, seed + extra, &attempt);
            }
            tally.runs++;
            tally.completed += done;
            tally.changing += done ? attempt.changing : 0;
        }
    }
    printf("%s: %zu of %zu runs complete (%.0f%%), %zu of them change key\n", kind,
           tally.completed, tally.runs, 100.0 * (double)tally.completed / (double)tally.runs, tally.changing);
    return tally.completed == tally.runs ? 0 : 1;
}

static int test_smoke(void) {
    Tally tally;
    memset(&tally, 0, sizeof(tally));
    for (unsigned long seed = 1; seed <= 5; seed++) {
        if (compose_one(&SCENARIOS[0], "binary", seed, &tally) && tally.changing > 0) {
            printf("binary: seed %lu composes, changes key, and passes the checks\n", seed);
            return 0;
        }
    }
    printf("binary: no seed of 1 to 5 composes a piece that changes key\n");
    return 1;
}

/* A form that modulates always composes without the request to modulate, changes key, and passes every key check. */
static int test_smoke_always(void) {
    for (unsigned long seed = 1; seed <= 5; seed++) {
        PieceRequest request = request_of(&SCENARIOS[0], "scherzo", seed);
        request.modulate = false;
        PiecePlan *plan = NULL;
        assert(piece_build_plan(&request, &plan) == PIECE_OK);
        ComposeConfig config;
        ComposeOutcome outcome;
        memset(&config, 0, sizeof(config));
        config.knowledge = test_knowledge();
        config.seed = seed;
        PieceResult ran = compose_run(plan, &config, &outcome);
        bool changes = false;
        if (ran == PIECE_OK) {
            changes = read_lane(plan).count > 1;
            check_piece(plan, "scherzo");
            PieceKeyCheck keys;
            assert(test_piece_check_keys(plan, &keys) && keys.ok && keys.changes > 0);
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
        if (ran == PIECE_OK && changes) {
            printf("scherzo: seed %lu composes without modulate, changes key, and passes the checks\n", seed);
            return 0;
        }
    }
    printf("scherzo: no seed of 1 to 5 composes a piece that changes key\n");
    return 1;
}

/* ------------------------------------------------------------- a returning section in the tonic major (R-517) */

#define MAJOR_SEEDS 10
#define MAJOR_COMPOSED_LEAST 9
#define MAJOR_MELODY_MAX 512

/* The stand-in form of R-517: A, T, B, R, A1, T1, B1 and K, 56 bars, B1 returning B exact in the tonic major. */
static const char TONIC_MAJOR_FORM[] =
    "{\"schema\":\"ursatz-form\",\"version\":1,\"name\":\"tonic-major-form\",\"label\":\"Tonic major form\",\"modulates\":true,"
    "\"regions\":[{\"name\":\"subordinate\",\"major\":[5],\"minor\":[3]}],"
    "\"sizes\":[{\"bars\":56,\"default\":true,\"sections\":["
    "{\"name\":\"A\",\"letter\":\"A\",\"role\":\"theme\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"perfect-authentic\",\"closed\":true},"
    "{\"name\":\"T\",\"letter\":\"T\",\"role\":\"transition\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"half\",\"keys\":{\"end\":\"subordinate\"}},"
    "{\"name\":\"B\",\"letter\":\"B\",\"role\":\"subordinate\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"perfect-authentic\",\"keys\":{\"start\":\"subordinate\",\"end\":\"subordinate\"},\"motive\":\"invert tonal=1\"},"
    "{\"name\":\"R\",\"letter\":\"R\",\"role\":\"retransition\",\"bars\":4,\"cadence\":\"half\"},"
    "{\"name\":\"A1\",\"letter\":\"A\",\"role\":\"return\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"perfect-authentic\",\"closed\":true,\"returns\":\"A\",\"variation\":\"ornament\"},"
    "{\"name\":\"T1\",\"letter\":\"T\",\"role\":\"transition\",\"bars\":4,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"half\"},"
    "{\"name\":\"B1\",\"letter\":\"B\",\"role\":\"subordinate\",\"bars\":8,\"beginning\":\"presentation\",\"ending\":\"continuation\","
    "\"cadence\":\"perfect-authentic\",\"returns\":\"B\",\"variation\":\"exact\",\"mode\":\"major\"},"
    "{\"name\":\"K\",\"letter\":\"K\",\"role\":\"coda\",\"bars\":8,\"cadence\":\"perfect-authentic\"}]}]}";

typedef struct {
    Rational at;
    Rational length;
    int pitch;
} Note;

/* The melody notes of a section, voice 0, with their offsets from the section's start. */
static size_t melody_of(const PiecePlan *plan, NodeId section, Note *out) {
    PlanSpan span = plan_node_span(plan, section);
    LaneSpanId ids[MAJOR_MELODY_MAX];
    size_t spans = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, MAJOR_MELODY_MAX);
    size_t count = 0;
    for (size_t i = 0; i < spans; i++) {
        PlanSpan lane_span = plan_lane_span_span(plan, ids[i]);
        const PlanLanePayload *payload = plan_lane_span_payload(plan, ids[i]);
        Rational from;
        assert(rational_sub(lane_span.start, span.start, &from) == RATIONAL_OK);
        for (size_t n = 0; n < payload->notes.note_count; n++) {
            assert(count < MAJOR_MELODY_MAX);
            out[count].length = payload->notes.notes[n].duration;
            out[count].pitch = payload->notes.notes[n].midi;
            assert(rational_add(from, payload->notes.notes[n].offset, &out[count].at) == RATIONAL_OK);
            count++;
        }
    }
    return count;
}

/* The key difference in semitones from -5 to 6 a section returned in another key is moved by (return_key_shift). */
static int shift_between(AnalysisKey from, AnalysisKey to) {
    int step = ((analysis_key_tonic_pitch_class(&to) - analysis_key_tonic_pitch_class(&from)) % 12 + 12) % 12;
    return step > 6 ? step - 12 : step;
}

static int test_tonic_major(bool smoke) {
    FormSpec *form = NULL;
    FormSpecError error;
    int failed = 0;
    if (form_spec_parse(TONIC_MAJOR_FORM, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "tonic major form: %s: %s\n", error.path, error.message);
        return 2;
    }
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        const AnalysisKey home = plan_key_of_pitch_class(SCENARIOS[s].tonic, SCENARIOS[s].mode);
        const bool minor = SCENARIOS[s].mode == KEY_STRUCTURE_MODE_MINOR;
        const AnalysisKey major = plan_key_of_pitch_class(SCENARIOS[s].tonic, KEY_STRUCTURE_MODE_MAJOR);
        size_t composed = 0;
        for (unsigned long seed = 1; seed <= (smoke ? 1 : MAJOR_SEEDS); seed++) {
            PieceRequest request;
            PiecePlan *plan = NULL;
            NodeId sections[FORM_SPEC_MAX_SECTIONS];
            memset(&request, 0, sizeof(request));
            request.form = form->name;
            request.key = home;
            request.meter.beats_per_bar = 4;
            request.meter.beat_unit = 4;
            request.modulate = true;
            request.seed = seed;
            assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
            assert(sections_of(plan, sections) == 8);
            /* The plan's key lane: B returns B1 in the tonic major (A major in A minor, home in C major), K and A1 are home. */
            KeyLane lane = read_lane(plan);
            check_lane_tiles(plan, &lane);
            const AnalysisKey at_b = key_at(&lane, plan_node_span(plan, sections[2]).start);
            const AnalysisKey at_b1 = key_at(&lane, plan_node_span(plan, sections[6]).start);
            assert(same_key(at_b1, minor ? major : home));
            assert(same_key(key_at(&lane, plan_node_span(plan, sections[7]).start), home));
            assert(same_key(key_at(&lane, plan_node_span(plan, sections[4]).start), home));
            assert(!same_key(at_b, home));
            ComposeConfig config;
            ComposeOutcome outcome;
            memset(&config, 0, sizeof(config));
            config.knowledge = test_knowledge();
            config.seed = seed;
            config.form = form;
            if (compose_run(plan, &config, &outcome) == PIECE_OK) {
                static Note b[MAJOR_MELODY_MAX];
                static Note b1[MAJOR_MELODY_MAX];
                composed++;
                size_t bn = melody_of(plan, sections[2], b);
                size_t b1n = melody_of(plan, sections[6], b1);
                const int shift = shift_between(at_b, at_b1);
                assert(bn > 0 && bn == b1n);
                for (size_t i = 0; i < bn; i++) {
                    assert(rational_cmp(b[i].at, b1[i].at) == 0 && rational_cmp(b[i].length, b1[i].length) == 0 &&
                           b[i].pitch + shift == b1[i].pitch);
                }
                /* The section after B1 is in the home key again, with a change of mode on one tonic and no pivot. */
                KeyLane composed_lane = read_lane(plan);
                assert(same_key(key_at(&composed_lane, plan_node_span(plan, sections[7]).start), home));
            }
            compose_outcome_destroy(&outcome);
            plan_destroy(plan);
        }
        printf("%s 4/4: %zu of %d seeds compose, B1 in %s\n", SCENARIOS[s].name, composed, smoke ? 1 : MAJOR_SEEDS,
               minor ? "the tonic major" : "the home key");
        if (smoke) {
            /* Seed 1 alone has no rate to hold, but it must compose, so that the checks of a composed piece ran. */
            if (composed < 1) {
                fprintf(stderr, "%s: seed 1 should compose\n", SCENARIOS[s].name);
                failed = 1;
            }
        } else if (composed < MAJOR_COMPOSED_LEAST) {
            fprintf(stderr, "%s: at least %d of %d seeds should compose\n", SCENARIOS[s].name, MAJOR_COMPOSED_LEAST, MAJOR_SEEDS);
            failed = 1;
        }
    }
    form_spec_destroy(form);
    return failed;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "plan") == 0) {
        return test_plans();
    }
    if (argc == 2 && strcmp(argv[1], "tonic-major") == 0) {
        return test_tonic_major(false);
    }
    if (argc == 3 && strcmp(argv[1], "tonic-major") == 0 && strcmp(argv[2], "smoke") == 0) {
        return test_tonic_major(true);
    }
    if (argc == 2 && strcmp(argv[1], "smoke") == 0) {
        return test_smoke() | test_smoke_always();
    }
    for (size_t form = 0; argc == 3 && strcmp(argv[1], "compose") == 0 && form < form_spec_builtin_count(); form++) {
        if (strcmp(argv[2], form_spec_builtin_name(form)) == 0) {
            return test_compose(form_spec_builtin_name(form));
        }
    }
    fprintf(stderr, "usage: test_piece_keys plan|smoke|tonic-major [smoke]|compose <template>\n");
    return 2;
}
