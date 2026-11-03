#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_structure.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

/*
 * The Analyzer check (analyzer_check.h, docs/analyzer-check.md): a realized section is analyzed in memory by the
 * Ursatz Analyzer and compared with the plan.
 *
 *   contradiction  a plan whose half cadence is written as an authentic one is a contradiction
 *   default        without analyzer_check nothing is checked and the outcome holds no check
 *   smoke          a sentence in C major 4/4, seed 1, composed with the check: every planned cadence and the key heard
 *   planned        the sweep: sentence and period (grown, 8 bars) and rounded binary (24 bars), seeds 1 to 10, C major
 *                  4/4 and A minor 3/4, composed with the check: every accepted run has every planned cadence and its
 *                  key heard as planned, and the completion share is not below its baseline (R-075)
 *   same           the same seed gives byte-identical output with the check on
 *   candidates     compose_candidates: ranked by agreement, score and seed, the same inputs the same order
 *   piece          analyzer_check_piece (R-172): a piece structure (intro, a sentence, a free phrase, a turnaround) read as one
 *                  fragment, sections that are not whole bars included; its cadences and key are the planned items, the
 *                  compose run with analyzer_check keeps the one check, and a plan with nothing written is refused
 */

#define SEEDS 10

typedef enum { SENTENCE, PERIOD, ROUNDED_BINARY } Shape;

typedef struct {
    const char *name;
    int tonic;
    PlanKeyMode mode;
    int beats;
    const char *motive; /* grown shapes only */
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C major 4/4", 0, PLAN_MODE_MAJOR, 4, "E4:q G4:e F4:e D4:q C4:q"},
    {"A minor 3/4", 9, PLAN_MODE_MINOR, 3, "C5:q E5:e D5:e B4:q"},
};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

static const char *const SHAPE_NAMES[] = {"sentence", "period", "rounded binary"};

/*
 * The completed runs the check accepts, of SEEDS for each scenario, by shape and scenario: the baselines (R-075),
 * measured. A minor in 3/4 is where the Analyzer most often hears the cadences elsewhere than planned (a period) and a
 * rounded binary in A minor 3/4 does not complete without the check either (docs/piece.md).
 */
static const size_t BASELINE[3][SCENARIO_COUNT] = {{9, 7}, {10, 0}, {9, 0}};

static NodeId find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

/* Builds the plan of a shape in a scenario; *motive_out is the motive a grown shape keeps pointing at. */
static PiecePlan *plan_of(Shape shape, const Scenario *scenario, Motive *motive_out) {
    PiecePlan *plan = NULL;
    PlanMeter meter = {scenario->beats, 4, 0};
    PlanKey key = {scenario->tonic, scenario->mode};
    if (shape == ROUNDED_BINARY) {
        PieceRequest request;
        memset(&request, 0, sizeof(request));
        request.form = FORM_TEMPLATE_ROUNDED_BINARY;
        request.key = key;
        request.meter = meter;
        request.bars = 24;
        assert(piece_build_plan(&request, &plan) == PIECE_OK);
        return plan;
    }
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter = meter;
    assert(motive_from_notes_text(scenario->motive, &options, motive_out, NULL) == MOTIVE_OK);
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive_out;
    request.key = key;
    request.meter = meter;
    request.bars = 8;
    request.pairing.beginning = shape == PERIOD ? PLAN_THEME_BEGINNING_ANTECEDENT : PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = shape == PERIOD ? PLAN_THEME_ENDING_CONSEQUENT : PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = PLAN_CADENCE_PAC;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    return plan;
}

static PieceResult compose(PiecePlan *plan, unsigned long seed, bool check, ComposeOutcome *outcome) {
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.analyzer_check = check;
    return compose_run(plan, &config, outcome);
}

/* ------------------------------------------------------------ contradiction */

static const AnalyzerCheckItem *cadence_item(const AnalyzerCheck *check) {
    for (size_t i = 0; i < check->item_count; i++) {
        if (check->items[i].kind == ANALYZER_CHECK_CADENCE) {
            return &check->items[i];
        }
    }
    return NULL;
}

static int test_contradiction(void) {
    for (unsigned long seed = 1; seed <= 5; seed++) {
        Motive motive;
        PiecePlan *plan = plan_of(SENTENCE, &SCENARIOS[0], &motive);
        ComposeOutcome outcome;
        if (compose(plan, seed, false, &outcome) != PIECE_OK) {
            compose_outcome_destroy(&outcome);
            plan_destroy(plan);
            continue;
        }
        NodeId theme = find_node(plan, "theme");
        AnalyzerCheck check;
        assert(analyzer_check_section(plan, theme, &check) == ANALYZER_CHECK_OK);
        const AnalyzerCheckItem *item = cadence_item(&check);
        assert(item && item->bar == 7 && strcmp(item->planned, "perfect-authentic") == 0);
        bool heard_as_planned = item->agreed;
        printf("seed %lu: planned PAC at bar 8, heard %s\n", seed, item->heard);
        analyzer_check_destroy(&check);
        if (!heard_as_planned) {
            compose_outcome_destroy(&outcome);
            plan_destroy(plan);
            continue;
        }

        /* The cadence is now planned as a half cadence while an authentic one is written: a contradiction. */
        for (size_t i = plan_node_constraint_count(plan, theme); i > 0; i--) {
            if (plan_node_constraint_at(plan, theme, i - 1)->kind == PLAN_CONSTRAINT_CADENCE) {
                assert(plan_node_remove_constraint(plan, theme, i - 1) == PLAN_OK);
            }
        }
        PlanConstraint half;
        memset(&half, 0, sizeof(half));
        half.kind = PLAN_CONSTRAINT_CADENCE;
        half.params.cadence = PLAN_CADENCE_HC;
        assert(plan_node_add_constraint(plan, theme, &half) == PLAN_OK);
        assert(analyzer_check_section(plan, theme, &check) == ANALYZER_CHECK_OK);
        item = cadence_item(&check);
        assert(item && strcmp(item->planned, "half") == 0 && !item->agreed);
        assert(strcmp(item->heard, "half") != 0);
        assert(check.contradicts && check.agreed < check.planned);
        analyzer_check_destroy(&check);
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
        printf("a half cadence written as an authentic one is a contradiction\n");
        return 0;
    }
    fputs("no seed from 1 to 5 composed a sentence heard as planned\n", stderr);
    return 1;
}

/* ------------------------------------------------------------ default */

static int test_default(void) {
    Motive motive;
    PiecePlan *plan = plan_of(SENTENCE, &SCENARIOS[0], &motive);
    ComposeOutcome outcome;
    assert(compose(plan, 1, false, &outcome) == PIECE_OK);
    assert(outcome.analyzer_checks == NULL && outcome.analyzer_check_count == 0);
    assert(outcome.analyzer_planned == 0 && outcome.analyzer_agreed == 0 && !outcome.analyzer_rejected);
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);
    printf("without the check nothing is analyzed\n");
    return 0;
}

/* ------------------------------------------------------------ planned */

/* Whether every theme section of a plan composed with the check is heard as planned: the outcome's and a new check's. */
static void expect_heard_as_planned(const PiecePlan *plan, const ComposeOutcome *outcome) {
    assert(outcome->analyzer_check_count == outcome->section_count && outcome->analyzer_checks);
    assert(!outcome->analyzer_rejected);
    size_t planned = 0;
    size_t agreed = 0;
    for (size_t i = 0; i < outcome->section_count; i++) {
        const AnalyzerCheck *kept = &outcome->analyzer_checks[i];
        assert(!kept->contradicts);
        AnalyzerCheck again;
        assert(analyzer_check_section(plan, outcome->sections[i].section, &again) == ANALYZER_CHECK_OK);
        assert(!again.contradicts && again.planned == kept->planned && again.agreed == kept->agreed);
        for (size_t k = 0; k < again.item_count; k++) {
            assert(again.items[k].kind != ANALYZER_CHECK_VIOLATION);
            assert(again.items[k].agreed);
        }
        planned += kept->planned;
        agreed += kept->agreed;
        analyzer_check_destroy(&again);
    }
    assert(planned == outcome->analyzer_planned && agreed == outcome->analyzer_agreed && planned == agreed);
    assert(planned > 0);
}

/* One seed; true when the run completed (and then it was accepted by the check). */
static bool run_once(Shape shape, const Scenario *scenario, unsigned long seed) {
    Motive motive;
    PiecePlan *plan = plan_of(shape, scenario, &motive);
    ComposeOutcome outcome;
    PieceResult ran = compose(plan, seed, true, &outcome);
    assert(ran == PIECE_OK || ran == PIECE_ERR_STUCK);
    if (ran == PIECE_OK) {
        expect_heard_as_planned(plan, &outcome);
    }
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);
    return ran == PIECE_OK;
}

static int test_smoke(void) {
    assert(run_once(SENTENCE, &SCENARIOS[0], 1));
    printf("a sentence in C major 4/4, seed 1: every planned cadence and the key heard\n");
    return 0;
}

static int test_planned(void) {
    int status = 0;
    for (int shape = SENTENCE; shape <= ROUNDED_BINARY; shape++) {
        for (size_t s = 0; s < SCENARIO_COUNT; s++) {
            size_t completed = 0;
            for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                completed += run_once((Shape)shape, &SCENARIOS[s], seed);
            }
            printf("%s, %s: %zu of %d runs complete with the check (baseline %zu)\n", SHAPE_NAMES[shape],
                   SCENARIOS[s].name, completed, SEEDS, BASELINE[shape][s]);
            status |= completed < BASELINE[shape][s];
        }
    }
    return status;
}

/* ------------------------------------------------------------ same */

static int test_same(void) {
    char *texts[2];
    size_t agreed[2];
    for (int run = 0; run < 2; run++) {
        Motive motive;
        PiecePlan *plan = plan_of(SENTENCE, &SCENARIOS[0], &motive);
        ComposeOutcome outcome;
        assert(compose(plan, 1, true, &outcome) == PIECE_OK);
        texts[run] = plan_to_json(plan, NULL);
        agreed[run] = outcome.analyzer_agreed;
        assert(texts[run]);
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    assert(strcmp(texts[0], texts[1]) == 0 && agreed[0] == agreed[1]);
    free(texts[0]);
    free(texts[1]);
    printf("the same seed gives the same plan with the check on\n");
    return 0;
}

/* ------------------------------------------------------------ candidates */

static void expect_ranked(const ComposeCandidates *candidates) {
    for (size_t i = 0; i + 1 < candidates->count; i++) {
        const ComposeCandidate *a = &candidates->items[i];
        const ComposeCandidate *b = &candidates->items[i + 1];
        unsigned long long a_planned = a->planned ? a->planned : 1;
        unsigned long long a_agreed = a->planned ? a->agreed : 1;
        unsigned long long b_planned = b->planned ? b->planned : 1;
        unsigned long long b_agreed = b->planned ? b->agreed : 1;
        if (a_agreed * b_planned != b_agreed * a_planned) {
            assert(a_agreed * b_planned > b_agreed * a_planned);
        } else if ((unsigned long long)a->score.numerator * b->score.denominator !=
                   (unsigned long long)b->score.numerator * a->score.denominator) {
            assert((unsigned long long)a->score.numerator * b->score.denominator >
                   (unsigned long long)b->score.numerator * a->score.denominator);
        } else {
            assert(a->seed < b->seed);
        }
    }
}

static int test_candidates(void) {
    for (int check = 0; check < 2; check++) {
        Motive motive;
        PiecePlan *plan = plan_of(PERIOD, &SCENARIOS[0], &motive);
        char *before = plan_to_json(plan, NULL);
        ComposeConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = 1;
        config.analyzer_check = check != 0;
        ComposeCandidates first;
        ComposeCandidates second;
        assert(compose_candidates(plan, &config, 6, &first) == PIECE_OK);
        assert(compose_candidates(plan, &config, 6, &second) == PIECE_OK);
        char *after = plan_to_json(plan, NULL);
        assert(strcmp(before, after) == 0); /* the plan given is not changed */
        assert(first.tried == 6 && first.count > 0 && first.count <= 6);
        assert(first.count == second.count);
        expect_ranked(&first);
        for (size_t i = 0; i < first.count; i++) {
            assert(first.items[i].seed == second.items[i].seed && first.items[i].seed >= 1 && first.items[i].seed <= 6);
            char *a = plan_to_json(first.items[i].plan, NULL);
            char *b = plan_to_json(second.items[i].plan, NULL);
            assert(strcmp(a, b) == 0);
            free(a);
            free(b);
            assert(first.items[i].agreed == first.items[i].outcome.analyzer_agreed);
            assert(check ? first.items[i].planned > 0 : first.items[i].planned == 0);
        }
        printf("%s: %zu of 6 candidates complete, best seed %lu, agreement %zu/%zu\n", check ? "with the check" : "no check",
               first.count, first.items[0].seed, first.items[0].agreed, first.items[0].planned);
        compose_candidates_destroy(&first);
        compose_candidates_destroy(&second);
        free(before);
        free(after);
        plan_destroy(plan);
    }
    return 0;
}

/* ------------------------------------------------------------- the whole piece */

typedef struct {
    PieceSection sections[PIECE_STRUCTURE_MAX_SECTIONS];
    PieceStructure structure;
} PieceFixture;

static void piece_section(PieceFixture *fixture, const char *id, PiecePart part, size_t beats, PieceFillKind fill) {
    PieceSection *section = &fixture->sections[fixture->structure.section_count++];
    memset(section, 0, sizeof(*section));
    snprintf(section->id, sizeof(section->id), "%s", id);
    snprintf(section->letter, sizeof(section->letter), "%c", id[0] - 32);
    section->part = part;
    section->beats = beats;
    section->pickup = (Rational){0, 1};
    section->fill = fill;
}

/* The intro 2 bars, a sentence of 8 bars, a free phrase of 4 and a turnaround of 2, D minor 4/4, and the ending a run adds. */
static void piece_fixture(PieceFixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->structure.key.tonic_pitch_class = 2;
    fixture->structure.key.mode = PLAN_MODE_MINOR;
    fixture->structure.meter.beats_per_bar = 4;
    fixture->structure.meter.beat_unit = 4;
    fixture->structure.sections = fixture->sections;
    piece_section(fixture, "intro", PIECE_PART_INTRO, 8, PIECE_FILL_EMPTY);
    piece_section(fixture, "a", PIECE_PART_BODY, 32, PIECE_FILL_MOTIF);
    fixture->sections[1].has_into = true;
    fixture->sections[1].into = PLAN_ARCHETYPE_SENTENCE;
    piece_section(fixture, "b", PIECE_PART_BODY, 16, PIECE_FILL_EMPTY);
    piece_section(fixture, "t", PIECE_PART_TURNAROUND, 8, PIECE_FILL_EMPTY);
}

static int test_piece(void) {
    AnalyzerCheck check;
    assert(analyzer_check_piece(NULL, &check) == ANALYZER_CHECK_ERR_NULL);
    PieceFixture fixture;
    piece_fixture(&fixture);
    PiecePlan *plan = NULL;
    assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_OK);
    assert(analyzer_check_piece(plan, &check) == ANALYZER_CHECK_ERR_NOT_WRITTEN && check.item_count == 0);
    plan_destroy(plan);
    size_t checked = 0;
    for (unsigned long seed = 1; seed <= 12 && checked < 3; seed++) {
        ComposeConfig config;
        ComposeOutcome outcome;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_OK);
        if (compose_run(plan, &config, &outcome) != PIECE_OK) {
            compose_outcome_destroy(&outcome);
            plan_destroy(plan);
            continue;
        }
        assert(analyzer_check_piece(plan, &check) == ANALYZER_CHECK_OK);
        /* A cadence for each of the four sections, and the key. */
        size_t cadences = 0;
        size_t keys = 0;
        for (size_t i = 0; i < check.item_count; i++) {
            cadences += check.items[i].kind == ANALYZER_CHECK_CADENCE;
            keys += check.items[i].kind == ANALYZER_CHECK_KEY;
        }
        assert(cadences == 4 && keys == 1 && check.planned == 5 && check.agreed <= check.planned);
        assert(check.contradicts == (check.agreed != check.planned));
        analyzer_check_destroy(&check);
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
        checked++;
    }
    assert(checked > 0);
    /* A run with analyzer_check keeps the one check of the whole piece, and the planned items are in its score. */
    size_t kept = 0;
    for (unsigned long seed = 1; seed <= 12 && kept == 0; seed++) {
        ComposeConfig config;
        ComposeOutcome outcome;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        config.analyzer_check = true;
        assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_OK);
        if (compose_run(plan, &config, &outcome) == PIECE_OK) {
            assert(outcome.analyzer_check_count == 1 && outcome.analyzer_planned == 5 && outcome.analyzer_agreed == 5);
            assert(!outcome.analyzer_checks[0].contradicts);
            kept++;
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    assert(kept > 0);
    return 0;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "";
    if (strcmp(mode, "contradiction") == 0) {
        return test_contradiction();
    }
    if (strcmp(mode, "default") == 0) {
        return test_default();
    }
    if (strcmp(mode, "smoke") == 0) {
        return test_smoke();
    }
    if (strcmp(mode, "planned") == 0) {
        return test_planned();
    }
    if (strcmp(mode, "same") == 0) {
        return test_same();
    }
    if (strcmp(mode, "candidates") == 0) {
        return test_candidates();
    }
    if (strcmp(mode, "piece") == 0) {
        return test_piece();
    }
    fputs("usage: test_analyzer_check contradiction|default|smoke|planned|same|candidates|piece\n", stderr);
    return 2;
}
