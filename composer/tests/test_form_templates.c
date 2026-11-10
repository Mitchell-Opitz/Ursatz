#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_state.h>

/*
 * Form templates, every template and size, in C major 4/4 and A minor 3/4, with a motive invented and with the given
 * one (tests/fixtures/motive_d_minor.json; in 3/4 its last note ends on the barline).
 *
 *   structure <template>  the plan tiles the form with the table's sections in order (letters, bars, pairings,
 *                         cadences, the section each return names, the layers), and sizes not in the table are refused
 *   compose <template>    seeds 1 to 5: how many runs complete and how many theme sections form_hear runs on; neither
 *                         is below its baseline; the runs that do not complete are printed
 *   smoke <template>      the smallest size in C major 4/4, invented: one of seeds 1 to 5 composes and is heard (quick)
 *   heard <template>      seeds 1 to 5: how many sections are heard as the form they ask for is printed with grow's
 *                         measured shares (docs/grow.md) and is not below its baseline
 */

#define SEEDS 5

typedef struct {
    const char *name;
    int tonic;
    KeyStructureMode mode;
    int beats;
    bool given;
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C major 4/4, invented", 0, KEY_STRUCTURE_MODE_MAJOR, 4, false},
    {"A minor 3/4, invented", 9, KEY_STRUCTURE_MODE_MINOR, 3, false},
    {"C major 4/4, given", 0, KEY_STRUCTURE_MODE_MAJOR, 4, true},
    {"A minor 3/4, given", 9, KEY_STRUCTURE_MODE_MINOR, 3, true},
};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

/* A size of a template: bars, or the count of variations or layers. */
typedef struct {
    size_t bars;
    size_t count;
} Size;

#define P PLAN_THEME_BEGINNING_PRESENTATION
#define AN PLAN_THEME_BEGINNING_ANTECEDENT
#define CBI PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA
#define CONT PLAN_THEME_ENDING_CONTINUATION
#define CONS PLAN_THEME_ENDING_CONSEQUENT
#define NONE FORM_TEMPLATE_NO_RETURN

/* What a section of the table must be. */
typedef struct {
    const char *letter;
    size_t bars;
    PlanThemeBeginning beginning;
    PlanThemeEnding ending;
    CadenceType cadence;
    size_t returns;
    unsigned layers;
} Expected;

#define MAX_EXPECTED FORM_TEMPLATE_MAX_SECTIONS

static const unsigned BASS = PLAN_LAYER_BIT(PLAN_LANE_BASS);
static const unsigned ACCOMPANIED = PLAN_LAYER_BIT(PLAN_LANE_BASS) | PLAN_LAYER_BIT(PLAN_LANE_INNER_VOICE);
static const unsigned WITH_MELODY =
    PLAN_LAYER_BIT(PLAN_LANE_BASS) | PLAN_LAYER_BIT(PLAN_LANE_INNER_VOICE) | PLAN_LAYER_BIT(PLAN_LANE_MELODY);

static void set(Expected *e, const char *letter, size_t bars, PlanThemeBeginning beginning, PlanThemeEnding ending,
                CadenceType cadence, size_t returns, unsigned layers) {
    e->letter = letter;
    e->bars = bars;
    e->beginning = beginning;
    e->ending = ending;
    e->cadence = cadence;
    e->returns = returns;
    e->layers = layers;
}

/* The table, written out here so the test does not read it from the code it checks. */
static size_t expected_sections(FormTemplateKind kind, Size size, Expected *out) {
    static char letters[MAX_EXPECTED][PLAN_SECTION_LETTER_MAX];
    size_t n = 0;
    switch (kind) {
    case FORM_TEMPLATE_BINARY: {
        size_t half = size.bars / 2;
        set(&out[n++], "A", half, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n++], "B", half, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        break;
    }
    case FORM_TEMPLATE_ROUNDED_BINARY: {
        size_t second = size.bars == 16 ? 4 : 8;
        set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n++], "B", second, CBI, CONT, CADENCE_TYPE_HALF, NONE, 0);
        set(&out[n++], "A'", second, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, 0, 0);
        break;
    }
    case FORM_TEMPLATE_TERNARY: {
        size_t third = size.bars / 3;
        set(&out[n++], "A", third, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n++], "B", third, P, CONT, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        set(&out[n++], "A", third, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, 0, 0);
        break;
    }
    case FORM_TEMPLATE_VARIATIONS:
        set(&out[n++], "T", 8, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, NONE, 0);
        for (size_t k = 1; k <= size.count; k++) {
            letters[n][0] = 'V';
            letters[n][1] = (char)('0' + k);
            letters[n][2] = '\0';
            set(&out[n], letters[n], 8, AN, CONS, CADENCE_TYPE_PERFECT_AUTHENTIC, 0, 0);
            n++;
        }
        break;
    case FORM_TEMPLATE_LOOP:
        if (size.bars == 8) {
            set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        } else {
            set(&out[n++], "A", 8, P, CONT, CADENCE_TYPE_IMPERFECT_AUTHENTIC, NONE, 0);
            set(&out[n++], "B", 8, P, CONT, CADENCE_TYPE_HALF, NONE, 0);
        }
        break;
    case FORM_TEMPLATE_LAYERED: {
        static const unsigned *const BUILD_UP[3] = {&BASS, &ACCOMPANIED, &WITH_MELODY};
        for (size_t k = 1; k <= size.count; k++) {
            letters[n][0] = 'L';
            letters[n][1] = (char)('0' + k);
            letters[n][2] = '\0';
            unsigned layers = k == size.count ? PLAN_LAYERS_ALL : *BUILD_UP[k - 1];
            set(&out[n], letters[n], 8, P, CONT, CADENCE_TYPE_HALF, k == 1 ? NONE : 0, layers);
            n++;
        }
        break;
    }
    }
    return n;
}

/* Every size of each template: bars for the fixed ones, the count for variations and layered. */
static size_t sizes_of(FormTemplateKind kind, Size *out) {
    size_t n = 0;
    switch (kind) {
    case FORM_TEMPLATE_BINARY:
        out[n++] = (Size){16, 0};
        out[n++] = (Size){8, 0};
        break;
    case FORM_TEMPLATE_ROUNDED_BINARY:
        out[n++] = (Size){16, 0};
        out[n++] = (Size){24, 0};
        break;
    case FORM_TEMPLATE_TERNARY:
        out[n++] = (Size){24, 0};
        out[n++] = (Size){12, 0};
        break;
    case FORM_TEMPLATE_VARIATIONS:
        for (size_t count = 1; count <= 8; count++) {
            out[n++] = (Size){8 * (1 + count), count};
        }
        break;
    case FORM_TEMPLATE_LOOP:
        out[n++] = (Size){8, 0};
        out[n++] = (Size){16, 0};
        break;
    case FORM_TEMPLATE_LAYERED:
        for (size_t count = 2; count <= 4; count++) {
            out[n++] = (Size){8 * count, count};
        }
        break;
    }
    return n;
}

/*
 * The sizes the compose and heard sweeps run: every size, except that variations run 1, 2 and 5 variations (16, 24 and
 * 48 bars). Past that a run only repeats what the shortest ones show, and its stuck runs make the sweep take minutes.
 */
static size_t sweep_sizes_of(FormTemplateKind kind, Size *out) {
    if (kind != FORM_TEMPLATE_VARIATIONS) {
        return sizes_of(kind, out);
    }
    static const size_t COUNTS[] = {1, 2, 5};
    size_t n = 0;
    for (size_t i = 0; i < sizeof(COUNTS) / sizeof(COUNTS[0]); i++) {
        out[n++] = (Size){8 * (1 + COUNTS[i]), COUNTS[i]};
    }
    return n;
}

/* ------------------------------------------------------------- the plan */

static Motive scenario_motive(const Scenario *scenario) {
    Motive motive;
    MotiveError error;
    char path[1024];
    snprintf(path, sizeof(path), "%s/motive_d_minor.json", COMPOSER_FIXTURE_DIR);
    if (motive_load_file(path, MOTIVE_FORMAT_JSON, NULL, &motive, &error) != MOTIVE_OK) {
        fprintf(stderr, "cannot load %s: %s\n", path, error.message);
        exit(2);
    }
    motive_set_meter(&motive, (PlanMeter){scenario->beats, 4, 0});
    AnalysisKey motive_key_value = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    motive_set_key(&motive, &motive_key_value, MOTIVE_SKETCH_KEY_GIVEN);
    if (scenario->beats == 3) {
        /* The half note at beat 3 ends on the barline as a quarter. */
        MotiveSketchNote *last = &motive.notes[motive.note_count - 1];
        assert(rational_make(1, 4, &last->duration) == RATIONAL_OK);
        assert(rational_make(3, 4, &motive.length) == RATIONAL_OK);
    }
    return motive;
}

static PieceRequest scenario_request(const Scenario *scenario, FormTemplateKind kind, Size size,
                                     const Motive *motive) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form_template_name(kind);
    request.key = plan_key_of_pitch_class(scenario->tonic, scenario->mode);
    request.meter.beats_per_bar = scenario->beats;
    request.meter.beat_unit = 4;
    request.bars = size.bars;
    request.count = size.count;
    request.motive = scenario->given ? motive : NULL;
    return request;
}

static const PlanConstraint *find(const PiecePlan *plan, NodeId id, PlanConstraintKind kind, size_t nth) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
        if (constraint->kind == kind && nth-- == 0) {
            return constraint;
        }
    }
    return NULL;
}

static long bars_of(Rational time, int beats) {
    /* time is in whole notes; a bar is beats quarters. */
    long quarters_num = time.num * 4;
    assert(quarters_num % (long)time.den == 0);
    long quarters = quarters_num / (long)time.den;
    assert(quarters % beats == 0);
    return quarters / beats;
}

static void check_structure(const PiecePlan *plan, const Scenario *scenario, FormTemplateKind kind, Size size) {
    Expected expected[MAX_EXPECTED];
    size_t count = expected_sections(kind, size, expected);
    NodeId form = plan_root(plan);
    assert(plan_node_child_count(plan, form) == count + 1);
    NodeId sections[MAX_EXPECTED];
    long at = 0;
    for (size_t i = 0; i < count; i++) {
        NodeId section = plan_node_child_at(plan, form, i);
        sections[i] = section;
        PlanSpan span = plan_node_span(plan, section);
        assert(plan_node_kind(plan, section) == PLAN_NODE_SECTION);
        assert(plan_node_state(plan, section) == PLAN_STATE_CONSTRAINED);
        assert(bars_of(span.start, scenario->beats) == at);
        assert(bars_of(span.length, scenario->beats) == (long)expected[i].bars);
        at += (long)expected[i].bars;

        const PlanConstraint *letter = find(plan, section, PLAN_CONSTRAINT_SECTION_LETTER, 0);
        const PlanConstraint *pairing = find(plan, section, PLAN_CONSTRAINT_THEME_PAIRING, 0);
        const PlanConstraint *cadence = find(plan, section, PLAN_CONSTRAINT_CADENCE, 0);
        const PlanConstraint *key = find(plan, section, PLAN_CONSTRAINT_KEY, 0);
        const PlanConstraint *back = find(plan, section, PLAN_CONSTRAINT_RETURN, 0);
        const PlanConstraint *layers = find(plan, section, PLAN_CONSTRAINT_LAYERS, 0);
        assert(letter && strcmp(letter->params.letter, expected[i].letter) == 0);
        assert(pairing && pairing->params.theme_pairing.beginning == expected[i].beginning &&
               pairing->params.theme_pairing.ending == expected[i].ending);
        assert(find(plan, section, PLAN_CONSTRAINT_ARCHETYPE, 0) != NULL);
        assert(cadence && cadence->params.cadence == expected[i].cadence);
        /* An antecedent's weak cadence comes second. */
        const PlanConstraint *weak = find(plan, section, PLAN_CONSTRAINT_CADENCE, 1);
        assert((expected[i].beginning == AN) == (weak != NULL));
        assert(!weak || weak->params.cadence == CADENCE_TYPE_HALF);
        assert(key && analysis_key_tonic_pitch_class(&key->params.key) == scenario->tonic && key->params.key.mode == scenario->mode);
        if (expected[i].returns == NONE) {
            assert(back == NULL);
        } else {
            assert(back && back->params.section_return.section == sections[expected[i].returns]);
        }
        assert(expected[i].layers == 0 ? layers == NULL : (layers && layers->params.layers.lanes == expected[i].layers));
        /* A layered form carries an accompaniment in every layer. */
        assert((kind == FORM_TEMPLATE_LAYERED) == (find(plan, section, PLAN_CONSTRAINT_ACCOMPANIMENT, 0) != NULL));
        /* A section made from m0 other than as m0 itself names m0; a returning one takes its source's motive. */
        const PlanConstraint *motive = find(plan, section, PLAN_CONSTRAINT_MOTIVE_REF, 0);
        if (expected[i].returns != NONE && kind != FORM_TEMPLATE_VARIATIONS) {
            const PlanConstraint *source = find(plan, sections[expected[i].returns], PLAN_CONSTRAINT_MOTIVE_REF, 0);
            assert((motive == NULL) == (source == NULL));
        }
        if (motive) {
            const char *name = plan_node_name(plan, motive->params.motive.motive);
            assert(name && strcmp(name, "m0") == 0);
        }
        if (strcmp(expected[i].letter, "B") == 0) {
            assert(motive && motive->params.motive.has_step && motive->params.motive.step.kind == MOTIVE_RELATION_INVERT &&
                   motive->params.motive.step.value[1] != 0);
        }
        if (expected[i].letter[0] == 'V') {
            int k = atoi(expected[i].letter + 1);
            assert(motive && motive->params.motive.has_step &&
                   motive->params.motive.step.kind ==
                       (k % 2 == 1 ? MOTIVE_RELATION_DIMINISH : MOTIVE_RELATION_INTERVAL_CHANGE));
        }
    }
    /* The material, after the sections: m0 Given, or open with a motive to invent. */
    NodeId material = plan_node_child_at(plan, form, count);
    assert(find(plan, material, PLAN_CONSTRAINT_MATERIAL, 0) != NULL);
    assert(bars_of(plan_node_span(plan, material).start, scenario->beats) == at);
    NodeId m0 = PLAN_NODE_NONE;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && name && strcmp(name, "m0") == 0) {
            m0 = id;
        }
    }
    assert(m0 != PLAN_NODE_NONE);
    LaneSpanId melody;
    assert(plan_node_lane_spans(plan, m0, &melody, 1) == 1);
    if (scenario->given) {
        assert(plan_lane_span_state(plan, melody) == PLAN_STATE_GIVEN);
    } else {
        assert(plan_lane_span_state(plan, melody) == PLAN_STATE_CONSTRAINED);
        assert(plan_lane_span_constraint_at(plan, melody, 0)->kind == PLAN_CONSTRAINT_INVENT_MOTIVE);
    }
}

/* Sizes not in the table, and counts for the templates that take none, are refused. */
static void check_refused(FormTemplateKind kind, const Scenario *scenario, const Motive *motive) {
    static const Size FIXED_BAD[] = {{4, 0}, {12, 0}, {20, 0}, {32, 0}, {16, 2}};
    static const Size VARIATIONS_BAD[] = {{8, 0}, {12, 0}, {80, 0}, {0, 9}, {24, 3}, {20, 0}};
    static const Size LAYERED_BAD[] = {{8, 0}, {40, 0}, {0, 1}, {0, 5}, {24, 2}, {12, 0}};
    const Size *bad = kind == FORM_TEMPLATE_VARIATIONS ? VARIATIONS_BAD
                      : kind == FORM_TEMPLATE_LAYERED  ? LAYERED_BAD
                                                       : FIXED_BAD;
    size_t count = kind == FORM_TEMPLATE_VARIATIONS ? sizeof(VARIATIONS_BAD) / sizeof(Size)
                   : kind == FORM_TEMPLATE_LAYERED  ? sizeof(LAYERED_BAD) / sizeof(Size)
                                                    : sizeof(FIXED_BAD) / sizeof(Size);
    for (size_t i = 0; i < count; i++) {
        Size size = bad[i];
        bool in_table = false;
        Size good[16];
        for (size_t g = 0; g < sizes_of(kind, good); g++) {
            in_table = in_table || (good[g].bars == size.bars && (size.count == 0 || good[g].count == size.count));
        }
        if (in_table) {
            continue;
        }
        PieceRequest request = scenario_request(scenario, kind, size, motive);
        PiecePlan *plan = NULL;
        assert(piece_build_plan(&request, &plan) == PIECE_ERR_BAD_SIZE && plan == NULL);
    }
}

static void test_structure(FormTemplateKind kind) {
    Size sizes[16];
    size_t size_count = sizes_of(kind, sizes);
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        Motive motive = scenario_motive(&SCENARIOS[s]);
        for (size_t z = 0; z < size_count; z++) {
            PieceRequest request = scenario_request(&SCENARIOS[s], kind, sizes[z], &motive);
            PiecePlan *plan = NULL;
            PieceResult built = piece_build_plan(&request, &plan);
            if (built != PIECE_OK) {
                fprintf(stderr, "%s %zu bars, %s: %s\n", form_template_name(kind), sizes[z].bars, SCENARIOS[s].name,
                        piece_result_name(built));
            }
            assert(built == PIECE_OK);
            check_structure(plan, &SCENARIOS[s], kind, sizes[z]);
            plan_destroy(plan);
            /* The default size and count are a size in the table. */
            request.bars = 0;
            request.count = 0;
            assert(piece_build_plan(&request, &plan) == PIECE_OK);
            plan_destroy(plan);
        }
        check_refused(kind, &SCENARIOS[s], &motive);
    }
    printf("%s: structure of %zu sizes in %zu scenarios as the table says\n", form_template_name(kind), size_count,
           SCENARIO_COUNT);
}

/* ------------------------------------------------------------ composing */

/* What the grow tests measured, heard as requested out of 40 (docs/grow.md), for the themes the templates use. */
static unsigned grow_measured(PlanThemeBeginning beginning, PlanThemeEnding ending, size_t bars) {
    if (bars != 8) {
        return 0; /* every theme of 4 bars: no theme heard */
    }
    if (beginning == P && ending == CONT) {
        return 26;
    }
    if (beginning == CBI && ending == CONT) {
        return 39;
    }
    if (beginning == AN && ending == CONT) {
        return 30;
    }
    return 0; /* a period: 0 of 40 */
}

typedef struct {
    size_t runs;
    size_t completed;
    size_t sections;
    size_t heard;            /* form_hear ran on the section */
    size_t as_requested;
    unsigned long expected;  /* sum over the sections of grow's measured share, in fortieths */
} Tally;

static void compose_all(FormTemplateKind kind, Tally *total) {
    Size sizes[16];
    size_t size_count = sweep_sizes_of(kind, sizes);
    memset(total, 0, sizeof(*total));
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        Motive motive = scenario_motive(&SCENARIOS[s]);
        for (size_t z = 0; z < size_count; z++) {
            Expected expected[MAX_EXPECTED];
            size_t count = expected_sections(kind, sizes[z], expected);
            Tally tally;
            memset(&tally, 0, sizeof(tally));
            for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                PieceRequest request = scenario_request(&SCENARIOS[s], kind, sizes[z], &motive);
                PiecePlan *plan = NULL;
                assert(piece_build_plan(&request, &plan) == PIECE_OK);
                ComposeConfig config;
                ComposeOutcome outcome;
                memset(&config, 0, sizeof(config));
                config.seed = seed;
                PieceResult ran = compose_run(plan, &config, &outcome);
                tally.runs++;
                tally.sections += count;
                for (size_t i = 0; i < count; i++) {
                    tally.expected += grow_measured(expected[i].beginning, expected[i].ending, expected[i].bars);
                }
                if (ran == PIECE_OK) {
                    tally.completed++;
                    assert(outcome.section_count == count);
                    for (size_t i = 0; i < count; i++) {
                        tally.heard += outcome.sections[i].heard_result == FORM_HEAR_OK;
                        tally.as_requested += outcome.sections[i].heard_as_requested;
                    }
                } else {
                    assert(ran == PIECE_ERR_STUCK);
                    printf("  %s %zu bars, %s, seed %lu: stuck at %s\n", form_template_name(kind), sizes[z].bars,
                           SCENARIOS[s].name, seed, compose_stage_name(outcome.stuck_stage));
                }
                compose_outcome_destroy(&outcome);
                plan_destroy(plan);
            }
            printf("%s %zu bars, %s: %zu of %zu runs complete, %zu of %zu sections heard as requested\n",
                   form_template_name(kind), sizes[z].bars, SCENARIOS[s].name, tally.completed, tally.runs,
                   tally.as_requested, tally.sections);
            total->runs += tally.runs;
            total->completed += tally.completed;
            total->sections += tally.sections;
            total->heard += tally.heard;
            total->as_requested += tally.as_requested;
            total->expected += tally.expected;
        }
    }
}

/*
 * What the sweeps measured when this was written (seeds 1 to 5, the four scenarios, the sizes of sweep_sizes_of; the
 * runs are deterministic, so the numbers repeat). The tests fail when a number falls below its baseline and pass when
 * it holds or rises; raise a baseline when a change lifts it. The causes of the stuck runs and of the shares being
 * below grow's are in docs/piece.md. The rounded binary one was 28, 84, 20 until the presentation drew its repetition by
 * weight and a sequence wrote its chords whole (P34 B1); it is 25, 75, 19.
 */
typedef struct {
    FormTemplateKind kind;
    size_t completed;    /* runs that complete */
    size_t heard;        /* sections form_hear ran on */
    size_t as_requested; /* sections heard as the form they ask for */
} Baseline;

static const Baseline BASELINES[] = {
    {FORM_TEMPLATE_BINARY, 20, 40, 12},     {FORM_TEMPLATE_ROUNDED_BINARY, 25, 75, 19},
    {FORM_TEMPLATE_TERNARY, 27, 81, 7},     {FORM_TEMPLATE_VARIATIONS, 45, 165, 0},
    {FORM_TEMPLATE_LOOP, 14, 18, 12},       {FORM_TEMPLATE_LAYERED, 30, 90, 12},
};

static const Baseline *baseline_of(FormTemplateKind kind) {
    for (size_t i = 0; i < sizeof(BASELINES) / sizeof(BASELINES[0]); i++) {
        if (BASELINES[i].kind == kind) {
            return &BASELINES[i];
        }
    }
    assert(!"a baseline for every template");
    return NULL;
}

static int test_compose(FormTemplateKind kind) {
    Tally total;
    compose_all(kind, &total);
    const Baseline *baseline = baseline_of(kind);
    printf("%s: %zu of %zu runs complete (baseline %zu); %zu sections heard (baseline %zu)\n", form_template_name(kind),
           total.completed, total.runs, baseline->completed, total.heard, baseline->heard);
    return total.completed >= baseline->completed && total.heard >= baseline->heard ? 0 : 1;
}

static int test_heard(FormTemplateKind kind) {
    Tally total;
    compose_all(kind, &total);
    const Baseline *baseline = baseline_of(kind);
    /* As many as grow's shares give for the same themes: expected / 40, rounded down. For comparison only. */
    size_t grow_floor = (size_t)(total.expected / 40);
    printf("%s: %zu of %zu sections heard as requested (%.0f%%; baseline %zu); grow's measured shares give %zu\n",
           form_template_name(kind), total.as_requested, total.sections,
           total.sections ? 100.0 * (double)total.as_requested / (double)total.sections : 0.0, baseline->as_requested,
           grow_floor);
    return total.as_requested >= baseline->as_requested ? 0 : 1;
}

/*
 * A quick check that the compose run works for the template: the smallest size in C major 4/4 with an invented motive,
 * the first of seeds 1 to SEEDS that completes (some seeds get stuck, see docs/piece.md), every section heard.
 */
static int test_smoke(FormTemplateKind kind) {
    Size sizes[16];
    size_t size_count = sizes_of(kind, sizes);
    Size smallest = sizes[0];
    for (size_t i = 1; i < size_count; i++) {
        if (sizes[i].bars < smallest.bars) {
            smallest = sizes[i];
        }
    }
    Motive motive = scenario_motive(&SCENARIOS[0]);
    Expected expected[MAX_EXPECTED];
    size_t count = expected_sections(kind, smallest, expected);
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PieceRequest request = scenario_request(&SCENARIOS[0], kind, smallest, &motive);
        PiecePlan *plan = NULL;
        assert(piece_build_plan(&request, &plan) == PIECE_OK);
        ComposeConfig config;
        ComposeOutcome outcome;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        PieceResult ran = compose_run(plan, &config, &outcome);
        size_t heard = 0;
        if (ran == PIECE_OK) {
            assert(outcome.section_count == count);
            for (size_t i = 0; i < count; i++) {
                heard += outcome.sections[i].heard_result == FORM_HEAR_OK;
            }
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
        if (ran == PIECE_OK && heard == count) {
            printf("%s: %zu bars composed with seed %lu, %zu sections heard\n", form_template_name(kind),
                   smallest.bars, seed, heard);
            return 0;
        }
    }
    printf("%s: no seed of 1 to %d composed %zu bars\n", form_template_name(kind), SEEDS, smallest.bars);
    return 1;
}

int main(int argc, char **argv) {
    FormTemplateKind kind;
    if (argc != 3 || !form_template_from_name(argv[2], &kind)) {
        fprintf(stderr, "usage: test_form_templates structure|compose|heard|smoke <template>\n");
        return 2;
    }
    if (strcmp(argv[1], "structure") == 0) {
        test_structure(kind);
        return 0;
    }
    if (strcmp(argv[1], "compose") == 0) {
        return test_compose(kind);
    }
    if (strcmp(argv[1], "heard") == 0) {
        return test_heard(kind);
    }
    if (strcmp(argv[1], "smoke") == 0) {
        return test_smoke(kind);
    }
    fprintf(stderr, "usage: test_form_templates structure|compose|heard|smoke <template>\n");
    return 2;
}
