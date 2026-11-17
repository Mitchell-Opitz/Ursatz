#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/harmony/harmony_cadence_rules.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/piece.h>

#include "util/plan_util.h"
#include "support/test_composer.h"

/*
 * Piece structures (R-169, R-170): the checks, the default cadences, and the plans and composed runs of structures
 * laid out in beats (4/4, C major, seeds 1 to 10).
 *
 *   checks    no body, parts out of order, a duplicate id, a bad pickup, too many sections, a short length;
 *             the default cadences and the default ending
 *   plan      the sections start where the beats say, with their parts, cadences and pickups
 *   compose   intro 2 bars, body 8 bars (a motif sentence) and 6 beats (empty), turnaround 1 bar: every section is
 *             written and the cadences are heard as planned in at least 8 of the 10 seeds
 *   pickup    a body section with a quarter-note pickup composes and keeps its pickup
 *   free      a free phrase of 3 bars and one of 10 beats: their ideas and their cadences
 *   functions  a game loop with a transition body section and a coda ending (P51 B6, R-508), C major 4/4 and A minor 3/4:
 *             the loop and the ending each compose 9 of 10 or more; T ends on V and its `fragment 2` is a liquidation as
 *             B2's measure reads it; K has three phrases of 4, 2 and 2 bars; each refusal is PIECE_ERR_BAD_REQUEST
 *   minor_half_free  a game loop in D minor in 4/4 whose body ends in a free phrase of 20 beats ending in a half cadence
 *             builds and composes, every section written (P44 B6, R-418); the same loop was stuck at the seam stage
 */

#define SEEDS 10

typedef struct {
    PieceSection sections[PIECE_STRUCTURE_MAX_SECTIONS + 2];
    PieceRequest request;
} Fixture;

static void fixture_init(Fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    fixture->request.meter.beats_per_bar = 4;
    fixture->request.meter.beat_unit = 4;
    fixture->request.form = "game-loop";
    fixture->request.sections = fixture->sections;
}

static PieceSection *add_section(Fixture *fixture, const char *id, const char *letter, PiecePart part, size_t beats,
                                 PieceFillKind fill) {
    PieceSection *section = &fixture->sections[fixture->request.section_count++];
    memset(section, 0, sizeof(*section));
    snprintf(section->id, sizeof(section->id), "%s", id);
    snprintf(section->letter, sizeof(section->letter), "%s", letter);
    section->part = part;
    section->beats = beats;
    section->pickup = (Rational){0, 1};
    section->fill = fill;
    return section;
}

/* The intro 2 bars, the body 8 bars motif sentence plus 6 beats empty, the turnaround 1 bar (all 4/4). */
static void standard_structure(Fixture *fixture) {
    fixture_init(fixture);
    PieceSection *body = add_section(fixture, "intro", "I", PIECE_PART_INTRO, 8, PIECE_FILL_EMPTY);
    (void)body;
    PieceSection *a = add_section(fixture, "a", "A", PIECE_PART_BODY, 32, PIECE_FILL_MOTIF);
    a->has_into = true;
    a->into = PLAN_ARCHETYPE_SENTENCE;
    add_section(fixture, "b", "B", PIECE_PART_BODY, 6, PIECE_FILL_EMPTY);
    add_section(fixture, "t", "T", PIECE_PART_TURNAROUND, 4, PIECE_FILL_EMPTY);
}

/* The default ending and the default cadences of the game loop's document (forms/game-loop.json). */
static bool default_ending(PieceRequest *request, size_t capacity) {
    FormSpec *form = NULL;
    assert(form_spec_builtin("game-loop", &form) == FORM_SPEC_OK);
    bool added = piece_default_ending(form, request, capacity);
    form_spec_destroy(form);
    return added;
}

static void default_cadences(PieceRequest *request) {
    FormSpec *form = NULL;
    assert(form_spec_builtin("game-loop", &form) == FORM_SPEC_OK);
    piece_default_cadences(form, request);
    form_spec_destroy(form);
}

static const PlanConstraint *node_constraint(const PiecePlan *plan, NodeId id, PlanConstraintKind kind) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, id); i++) {
        if (plan_node_constraint_at(plan, id, i)->kind == kind) {
            return plan_node_constraint_at(plan, id, i);
        }
    }
    return NULL;
}

/* The section nodes of the piece (not the material or the seam), in time order. */
static size_t piece_sections(const PiecePlan *plan, NodeId *out, size_t max) {
    size_t count = 0;
    NodeId form = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, form) && count < max; i++) {
        NodeId child = plan_node_child_at(plan, form, i);
        if (!node_constraint(plan, child, PLAN_CONSTRAINT_MATERIAL) && !node_constraint(plan, child, PLAN_CONSTRAINT_SEAM)) {
            out[count++] = child;
        }
    }
    return count;
}

static void test_checks(void) {
    Fixture fixture;
    standard_structure(&fixture);
    assert(piece_request_check(&fixture.request) == PIECE_OK);

    /* No body. */
    fixture_init(&fixture);
    add_section(&fixture, "intro", "I", PIECE_PART_INTRO, 8, PIECE_FILL_EMPTY);
    assert(piece_request_check(&fixture.request) == PIECE_ERR_BAD_REQUEST);

    /* Parts out of order: the turnaround before the body. */
    fixture_init(&fixture);
    add_section(&fixture, "t", "T", PIECE_PART_TURNAROUND, 4, PIECE_FILL_EMPTY);
    add_section(&fixture, "a", "A", PIECE_PART_BODY, 16, PIECE_FILL_EMPTY);
    assert(piece_request_check(&fixture.request) == PIECE_ERR_BAD_REQUEST);

    /* A duplicate id. */
    fixture_init(&fixture);
    add_section(&fixture, "a", "A", PIECE_PART_BODY, 16, PIECE_FILL_EMPTY);
    add_section(&fixture, "a", "B", PIECE_PART_BODY, 16, PIECE_FILL_EMPTY);
    assert(piece_request_check(&fixture.request) == PIECE_ERR_BAD_REQUEST);

    /* A length under one beat. */
    fixture_init(&fixture);
    add_section(&fixture, "a", "A", PIECE_PART_BODY, 0, PIECE_FILL_EMPTY);
    assert(piece_request_check(&fixture.request) == PIECE_ERR_BAD_REQUEST);

    /* Pickups: a quarter is fine, a bar is not, a triplet sixteenth is not a multiple of 1/32. */
    fixture_init(&fixture);
    PieceSection *section = add_section(&fixture, "a", "A", PIECE_PART_BODY, 16, PIECE_FILL_EMPTY);
    assert(rational_make(1, 4, &section->pickup) == RATIONAL_OK);
    assert(piece_request_check(&fixture.request) == PIECE_OK);
    assert(rational_make(1, 1, &section->pickup) == RATIONAL_OK);
    assert(piece_request_check(&fixture.request) == PIECE_ERR_BAD_REQUEST);
    assert(rational_make(1, 12, &section->pickup) == RATIONAL_OK);
    assert(piece_request_check(&fixture.request) == PIECE_ERR_BAD_REQUEST);

    /* Too many sections, and too many bars. */
    fixture_init(&fixture);
    for (size_t i = 0; i < PIECE_STRUCTURE_MAX_SECTIONS + 1; i++) {
        char id[PIECE_SECTION_ID_MAX];
        snprintf(id, sizeof(id), "s%zu", i);
        add_section(&fixture, id, "A", PIECE_PART_BODY, 4, PIECE_FILL_EMPTY);
    }
    assert(piece_request_check(&fixture.request) == PIECE_ERR_BAD_REQUEST);
    fixture_init(&fixture);
    add_section(&fixture, "a", "A", PIECE_PART_BODY, 4 * (PIECE_STRUCTURE_MAX_BARS + 1), PIECE_FILL_EMPTY);
    assert(piece_request_check(&fixture.request) == PIECE_ERR_BAD_REQUEST);
}

static void test_default_cadences(void) {
    Fixture fixture;
    standard_structure(&fixture);
    PieceSection *explicit_cadence = &fixture.sections[2];
    explicit_cadence->has_cadence = true;
    explicit_cadence->cadence = CADENCE_TYPE_DECEPTIVE;
    assert(default_ending(&fixture.request, PIECE_STRUCTURE_MAX_SECTIONS + 2));
    assert(fixture.request.section_count == 5);
    default_cadences(&fixture.request);
    /* The intro leads into the body (HC), the middle of the body is open (IAC), the loop leads back (HC). */
    assert(fixture.sections[0].cadence == CADENCE_TYPE_HALF);
    assert(fixture.sections[1].cadence == CADENCE_TYPE_IMPERFECT_AUTHENTIC);
    assert(fixture.sections[2].cadence == CADENCE_TYPE_DECEPTIVE); /* a cadence asked for is kept */
    assert(fixture.sections[3].cadence == CADENCE_TYPE_HALF);
    /* The ending closes. */
    assert(fixture.sections[4].part == PIECE_PART_ENDING);
    assert(fixture.sections[4].cadence == CADENCE_TYPE_PERFECT_AUTHENTIC);
    assert(fixture.sections[4].beats == 8);
    assert(strcmp(fixture.sections[4].id, "end") == 0);
    assert(strcmp(fixture.sections[4].letter, "E") == 0);
    /* A structure with an ending is left alone. */
    assert(default_ending(&fixture.request, PIECE_STRUCTURE_MAX_SECTIONS + 2));
    assert(fixture.request.section_count == 5);
}

static void test_plan(void) {
    Fixture fixture;
    standard_structure(&fixture);
    PieceSection *pickup = &fixture.sections[2];
    assert(rational_make(1, 4, &pickup->pickup) == RATIONAL_OK);
    PiecePlan *plan;
    assert(piece_build_plan(&fixture.request, &plan) == PIECE_OK);
    NodeId sections[8];
    size_t count = piece_sections(plan, sections, 8);
    assert(count == 4);
    static const size_t BEATS[4] = {8, 32, 6, 4};
    static const char *const IDS[4] = {"intro", "a", "b", "t"};
    static const int PARTS[4] = {PIECE_PART_INTRO, PIECE_PART_BODY, PIECE_PART_BODY, PIECE_PART_TURNAROUND};
    static const CadenceType CADENCES[4] = {CADENCE_TYPE_HALF, CADENCE_TYPE_IMPERFECT_AUTHENTIC, CADENCE_TYPE_IMPERFECT_AUTHENTIC, CADENCE_TYPE_HALF};
    size_t start = 0;
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = plan_node_span(plan, sections[i]);
        Rational want_start;
        Rational want_length;
        assert(rational_make((long)start, 4, &want_start) == RATIONAL_OK);
        assert(rational_make((long)BEATS[i], 4, &want_length) == RATIONAL_OK);
        assert(rational_cmp(span.start, want_start) == 0);
        assert(rational_cmp(span.length, want_length) == 0);
        const PlanConstraint *part = node_constraint(plan, sections[i], PLAN_CONSTRAINT_PIECE_PART);
        assert(part && part->params.piece_part.part == PARTS[i] && strcmp(part->params.piece_part.id, IDS[i]) == 0);
        const PlanConstraint *cadence = node_constraint(plan, sections[i], PLAN_CONSTRAINT_CADENCE);
        assert(cadence && cadence->params.cadence == CADENCES[i]);
        assert(node_constraint(plan, sections[i], PLAN_CONSTRAINT_KEY));
        assert(node_constraint(plan, sections[i], PLAN_CONSTRAINT_SECTION_LETTER));
        /* Only the second body section has a pickup. */
        const PlanConstraint *constraint = node_constraint(plan, sections[i], PLAN_CONSTRAINT_PICKUP);
        assert((constraint != NULL) == (i == 2));
        /* Only a motif section of 8 whole bars on a barline is a theme. */
        assert((node_constraint(plan, sections[i], PLAN_CONSTRAINT_ARCHETYPE) != NULL) == (i == 1));
        start += BEATS[i];
    }
    /* Only the turnaround, the loop's end, is held for the seam stage. */
    for (size_t i = 0; i < count; i++) {
        assert((node_constraint(plan, sections[i], PLAN_CONSTRAINT_HOLD) != NULL) == (i == 3));
    }
    /* The seam is the bar after the loop's end (50 beats), open and empty. */
    NodeId form = plan_root(plan);
    NodeId seam = plan_node_child_at(plan, form, count);
    assert(node_constraint(plan, seam, PLAN_CONSTRAINT_SEAM));
    assert(!node_constraint(plan, seam, PLAN_CONSTRAINT_MATERIAL) && !node_constraint(plan, seam, PLAN_CONSTRAINT_HOLD));
    assert(plan_node_child_count(plan, seam) == 0 && plan_node_state(plan, seam) == PLAN_STATE_CONSTRAINED);
    Rational end;
    assert(plan_span_end(plan_node_span(plan, sections[3]), &end) == PLAN_OK);
    assert(rational_cmp(plan_node_span(plan, seam).start, end) == 0);
    assert(rational_cmp(plan_node_span(plan, seam).length, (Rational){1, 1}) == 0);
    /* The material starts on the first barline at or after the seam's end (54 beats: bar 14, beat 56). */
    NodeId material = plan_node_child_at(plan, form, count + 1);
    assert(node_constraint(plan, material, PLAN_CONSTRAINT_MATERIAL));
    Rational barline;
    assert(rational_make(56, 4, &barline) == RATIONAL_OK);
    assert(rational_cmp(plan_node_span(plan, material).start, barline) == 0);
    plan_destroy(plan);

    /* The ending is not laid out. */
    assert(default_ending(&fixture.request, PIECE_STRUCTURE_MAX_SECTIONS + 2));
    assert(piece_build_plan(&fixture.request, &plan) == PIECE_OK);
    assert(piece_sections(plan, sections, 8) == 4);
    plan_destroy(plan);
}

/* Whether the harmony, bass and melody lanes over the section are written (none left Constrained, each lane there). */
static bool section_written(const PiecePlan *plan, NodeId section) {
    PlanSpan span = plan_node_span(plan, section);
    static const PlanLaneKind LANES[3] = {PLAN_LANE_HARMONY, PLAN_LANE_BASS, PLAN_LANE_MELODY};
    for (size_t lane = 0; lane < 3; lane++) {
        LaneSpanId ids[128];
        size_t found = plan_lane_find(plan, LANES[lane], 0, span, ids, 128);
        if (found == 0 || found > 128) {
            return false;
        }
        for (size_t i = 0; i < found; i++) {
            if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED) {
                return false;
            }
        }
    }
    return plan_node_state(plan, section) != PLAN_STATE_CONSTRAINED;
}

/* Whether the cadence the section was planned to end in is the one heard at the end of its chords. */
static bool cadence_heard(const PiecePlan *plan, NodeId section) {
    const PlanConstraint *cadence = node_constraint(plan, section, PLAN_CONSTRAINT_CADENCE);
    const PlanConstraint *key = node_constraint(plan, section, PLAN_CONSTRAINT_KEY);
    CadenceType heard;
    bool at_end;
    return cadence && key &&
           harmony_hear_cadence(plan, plan_node_span(plan, section), key->params.key.mode, cadence->params.cadence,
                                &heard, &at_end) &&
           at_end && heard == cadence->params.cadence;
}

typedef struct {
    size_t completed;       /* the runs that completed */
    size_t written;         /* of them, those with every section written */
    size_t cadences_heard;  /* of them, those with every cadence heard as planned */
} Sweep;

/* Composes the fixture with seeds 1 to SEEDS. Every section the plan lays out must carry its id and part. */
static Sweep compose_seeds(Fixture *fixture) {
    Sweep sweep;
    memset(&sweep, 0, sizeof(sweep));
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PiecePlan *plan;
        ComposeConfig config;
        ComposeOutcome outcome;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        assert(piece_build_plan(&fixture->request, &plan) == PIECE_OK);
        PieceResult ran = compose_run(plan, &config, &outcome);
        if (ran == PIECE_OK) {
            NodeId sections[PIECE_STRUCTURE_MAX_SECTIONS];
            size_t count = piece_sections(plan, sections, PIECE_STRUCTURE_MAX_SECTIONS);
            bool written = true;
            bool heard = true;
            assert(outcome.section_count == count);
            for (size_t i = 0; i < count; i++) {
                const PlanConstraint *part = node_constraint(plan, sections[i], PLAN_CONSTRAINT_PIECE_PART);
                assert(outcome.sections[i].section == sections[i]);
                assert(outcome.sections[i].has_part && outcome.sections[i].part == part->params.piece_part.part);
                assert(strcmp(outcome.sections[i].id, part->params.piece_part.id) == 0);
                written = written && section_written(plan, sections[i]);
                heard = heard && cadence_heard(plan, sections[i]);
            }
            sweep.completed++;
            sweep.written += written;
            sweep.cadences_heard += written && heard;
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    return sweep;
}

static void test_compose(void) {
    Fixture fixture;
    standard_structure(&fixture);
    Sweep sweep = compose_seeds(&fixture);
    printf("composed %zu of %d, written %zu, cadences heard as planned %zu\n", sweep.completed, SEEDS, sweep.written,
           sweep.cadences_heard);
    /* One seed of the ten was rejected since the tritone is refused (P25 B5); ten of ten again since the presentation
       draws its repetition by weight (P34 B1). */
    assert(sweep.completed == SEEDS);
    assert(sweep.written == SEEDS);
    assert(sweep.cadences_heard >= 8);
}

/* A body of one 2-bar motif section, then a section with a quarter-note pickup, then a turnaround of 6 beats. */
static void test_pickup(void) {
    Fixture fixture;
    fixture_init(&fixture);
    add_section(&fixture, "a", "A", PIECE_PART_BODY, 8, PIECE_FILL_MOTIF);
    PieceSection *pickup = add_section(&fixture, "b", "B", PIECE_PART_BODY, 12, PIECE_FILL_EMPTY);
    assert(rational_make(1, 4, &pickup->pickup) == RATIONAL_OK);
    add_section(&fixture, "t", "T", PIECE_PART_TURNAROUND, 6, PIECE_FILL_EMPTY);
    PiecePlan *plan;
    assert(piece_build_plan(&fixture.request, &plan) == PIECE_OK);
    NodeId sections[4];
    assert(piece_sections(plan, sections, 4) == 3);
    const PlanConstraint *constraint = node_constraint(plan, sections[1], PLAN_CONSTRAINT_PICKUP);
    assert(constraint && constraint->params.pickup.num == 1 && constraint->params.pickup.den == 4);
    assert(!node_constraint(plan, sections[0], PLAN_CONSTRAINT_PICKUP));
    /* The pickup is the section's own: it does not move the section. */
    PlanSpan span = plan_node_span(plan, sections[1]);
    assert(span.start.num == 2 && span.start.den == 1 && span.length.num == 3 && span.length.den == 1);
    plan_destroy(plan);
    Sweep sweep = compose_seeds(&fixture);
    printf("pickup: composed %zu of %d, written %zu\n", sweep.completed, SEEDS, sweep.written);
    assert(sweep.completed == SEEDS && sweep.written == SEEDS);
}

/* The ideas of the free phrase section: the idea nodes under its phrase, in order. */
static size_t section_ideas(const PiecePlan *plan, NodeId section, PlanSpan *spans, size_t max) {
    size_t count = 0;
    assert(plan_node_child_count(plan, section) == 1);
    NodeId phrase = plan_node_child_at(plan, section, 0);
    assert(plan_node_kind(plan, phrase) == PLAN_NODE_PHRASE);
    assert(plan_node_span(plan, phrase).length.num == plan_node_span(plan, section).length.num);
    for (size_t i = 0; i < plan_node_child_count(plan, phrase) && count < max; i++) {
        NodeId idea = plan_node_child_at(plan, phrase, i);
        assert(plan_node_kind(plan, idea) == PLAN_NODE_IDEA);
        spans[count++] = plan_node_span(plan, idea);
    }
    return count;
}

/* A free phrase of `beats` beats after an intro of 2 bars, its ideas one to a bar, the part bar joining the one next. */
static void free_phrase(size_t beats, size_t expected_ideas, const size_t *idea_beats) {
    Fixture fixture;
    fixture_init(&fixture);
    add_section(&fixture, "a", "A", PIECE_PART_BODY, 8, PIECE_FILL_EMPTY);
    PieceSection *phrase = add_section(&fixture, "f", "F", PIECE_PART_BODY, beats, PIECE_FILL_MOTIF);
    phrase->has_into = true;
    phrase->into = PLAN_ARCHETYPE_PERIOD; /* a free phrase has no archetype: this is read only for a theme */
    PiecePlan *plan;
    assert(piece_build_plan(&fixture.request, &plan) == PIECE_OK);
    NodeId sections[2];
    assert(piece_sections(plan, sections, 2) == 2);
    assert(node_constraint(plan, sections[1], PLAN_CONSTRAINT_FREE_PHRASE));
    assert(!node_constraint(plan, sections[1], PLAN_CONSTRAINT_ARCHETYPE));
    plan_destroy(plan);

    size_t completed = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        ComposeConfig config;
        ComposeOutcome outcome;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        assert(piece_build_plan(&fixture.request, &plan) == PIECE_OK);
        if (compose_run(plan, &config, &outcome) == PIECE_OK) {
            piece_sections(plan, sections, 2);
            PlanSpan spans[8];
            size_t count = section_ideas(plan, sections[1], spans, 8);
            assert(count == expected_ideas);
            Rational position = plan_node_span(plan, sections[1]).start;
            for (size_t i = 0; i < count; i++) {
                Rational want;
                assert(rational_cmp(spans[i].start, position) == 0);
                assert(rational_make((long)idea_beats[i], 4, &want) == RATIONAL_OK);
                assert(rational_cmp(spans[i].length, want) == 0);
                assert(rational_add(position, spans[i].length, &position) == RATIONAL_OK);
            }
            /* The section is written and ends in the cadence it was planned to. */
            assert(section_written(plan, sections[1]));
            assert(cadence_heard(plan, sections[1]));
            completed++;
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    printf("free phrase of %zu beats: composed %zu of %d\n", beats, completed, SEEDS);
    assert(completed == SEEDS);
}

static void test_free(void) {
    static const size_t THREE_BARS[3] = {4, 4, 4};
    static const size_t TEN_BEATS[2] = {4, 6};
    free_phrase(12, 3, THREE_BARS);
    free_phrase(10, 2, TEN_BEATS);
}

/* A body A of 32 beats and B of 20 beats, a free phrase that ends in the loop's half cadence, in D minor in 4/4. */
static void test_minor_half_free(void) {
    Fixture fixture;
    fixture_init(&fixture);
    fixture.request.key = plan_key_of_pitch_class(2, KEY_STRUCTURE_MODE_MINOR);
    PieceSection *a = add_section(&fixture, "a", "A", PIECE_PART_BODY, 32, PIECE_FILL_MOTIF);
    a->has_into = true;
    a->into = PLAN_ARCHETYPE_SENTENCE;
    PieceSection *b = add_section(&fixture, "b", "B", PIECE_PART_BODY, 20, PIECE_FILL_MOTIF);
    b->has_into = true;
    b->into = PLAN_ARCHETYPE_PERIOD; /* a free phrase has no archetype: this is read only for a theme */
    default_cadences(&fixture.request);
    assert(piece_request_check(&fixture.request) == PIECE_OK);

    PiecePlan *plan = NULL;
    assert(piece_build_plan(&fixture.request, &plan) == PIECE_OK);
    NodeId sections[PIECE_STRUCTURE_MAX_SECTIONS];
    size_t count = piece_sections(plan, sections, PIECE_STRUCTURE_MAX_SECTIONS);
    assert(count == 2);
    assert(node_constraint(plan, sections[1], PLAN_CONSTRAINT_FREE_PHRASE));
    const PlanConstraint *cadence = node_constraint(plan, sections[1], PLAN_CONSTRAINT_CADENCE);
    assert(cadence && cadence->params.cadence == CADENCE_TYPE_HALF);

    ComposeConfig config;
    ComposeOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = 1;
    PieceResult ran = compose_run(plan, &config, &outcome);
    assert(ran == PIECE_OK);
    for (size_t i = 0; i < count; i++) {
        assert(section_written(plan, sections[i]));
    }
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);
    printf("D minor 4/4 loop, B a free phrase of 20 beats ending HC: composed, every section written\n");
}


/* ------------------------------------------------------------- the transition and the coda (P51 B6, R-508) */

typedef struct {
    const char *name;
    int tonic;
    KeyStructureMode mode;
    int beats;
    Motive motive;
} FunctionRow;

/* Intro 4 bars; body A 8 bars, T 8 bars (transition), B 8 bars with a motive of its own; turnaround 4 bars; ending K 8 bars (coda). */
static void function_structure(Fixture *fixture, FunctionRow *row) {
    fixture_init(fixture);
    fixture->request.key = plan_key_of_pitch_class(row->tonic, row->mode);
    fixture->request.meter.beats_per_bar = row->beats;
    size_t bar = (size_t)row->beats;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter = fixture->request.meter;
    options.has_key = true;
    options.key = fixture->request.key;
    assert(motive_from_degrees_text(row->beats == 4 ? "1:q 3:e 2:e 5:h" : "1:q 3:e 2:e 5:q", &options, &row->motive, NULL) ==
           MOTIVE_OK);
    add_section(fixture, "intro", "I", PIECE_PART_INTRO, 4 * bar, PIECE_FILL_EMPTY);
    add_section(fixture, "a", "A", PIECE_PART_BODY, 8 * bar, PIECE_FILL_MOTIF);
    PieceSection *t = add_section(fixture, "t", "T", PIECE_PART_BODY, 8 * bar, PIECE_FILL_MOTIF);
    t->function = PIECE_FUNCTION_TRANSITION;
    PieceSection *b = add_section(fixture, "b", "B", PIECE_PART_BODY, 8 * bar, PIECE_FILL_MOTIF);
    b->motive = &row->motive;
    add_section(fixture, "turn", "R", PIECE_PART_TURNAROUND, 4 * bar, PIECE_FILL_EMPTY);
    PieceSection *k = add_section(fixture, "k", "K", PIECE_PART_ENDING, 8 * bar, PIECE_FILL_MOTIF);
    k->function = PIECE_FUNCTION_CODA;
}

/* The motive reference of the motive node an idea's melody span names, or NULL (R-512: the plan, not the notes). */
static const PlanMotiveRef *idea_motive_ref(const PiecePlan *plan, NodeId idea) {
    LaneSpanId melody;
    if (idea == PLAN_NODE_NONE || !plan_melody_span_of(plan, idea, &melody)) {
        return NULL;
    }
    const PlanConstraint *span_ref = plan_lane_span_find_constraint(plan, melody, PLAN_CONSTRAINT_MOTIVE_REF);
    if (!span_ref) {
        return NULL;
    }
    const PlanConstraint *node_ref = plan_node_find_constraint(plan, span_ref->params.motive.motive, PLAN_CONSTRAINT_MOTIVE_REF);
    return node_ref ? &node_ref->params.motive : NULL;
}

/* The harmony span of a section that starts latest. */
static LaneSpanId last_chord(const PiecePlan *plan, NodeId section) {
    LaneSpanId ids[64];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, section), ids, 64);
    assert(found > 0 && found <= 64);
    LaneSpanId last = ids[0];
    for (size_t i = 1; i < found; i++) {
        if (rational_cmp(plan_lane_span_span(plan, ids[i]).start, plan_lane_span_span(plan, last).start) > 0) {
            last = ids[i];
        }
    }
    return last;
}

/* T of the loop version: a transition ending on V or V7, its continuation liquidated (fragment 2 by 1 step or more on a segment). */
static bool transition_ok(const PiecePlan *plan, NodeId t) {
    const PlanConstraint *function = node_constraint(plan, t, PLAN_CONSTRAINT_SECTION_FUNCTION);
    if (!function || function->params.section_function.function != PLAN_FUNCTION_TRANSITION) {
        return false;
    }
    const char *numeral = plan_lane_span_payload(plan, last_chord(plan, t))->harmony.roman_numeral;
    const PlanMotiveRef *residue = idea_motive_ref(plan, test_find_named(plan, t, "fragment 2"));
    return (strcmp(numeral, "V") == 0 || strcmp(numeral, "V7") == 0) && residue && residue->steps >= 1 &&
           residue->segment[0] != '\0';
}

/* K of the ending version: the coda function, three phrases of 4, 2 and 2 bars, no archetype. */
static bool coda_ok(const PiecePlan *plan, NodeId k) {
    const PlanConstraint *function = node_constraint(plan, k, PLAN_CONSTRAINT_SECTION_FUNCTION);
    if (!function || function->params.section_function.function != PLAN_FUNCTION_CODA ||
        node_constraint(plan, k, PLAN_CONSTRAINT_ARCHETYPE) || plan_node_child_count(plan, k) != 3) {
        return false;
    }
    PlanSpan whole = plan_node_span(plan, k);
    Rational bar;
    assert(rational_div(whole.length, (Rational){8, 1}, &bar) == RATIONAL_OK);
    static const long BARS[3] = {4, 2, 2};
    Rational expected = whole.start;
    for (size_t p = 0; p < 3; p++) {
        PlanSpan span = plan_node_span(plan, plan_node_child_at(plan, k, p));
        Rational length;
        assert(rational_mul_int(bar, BARS[p], &length) == RATIONAL_OK);
        if (rational_cmp(span.start, expected) != 0 || rational_cmp(span.length, length) != 0) {
            return false;
        }
        assert(rational_add(span.start, span.length, &expected) == RATIONAL_OK);
    }
    return true;
}

static void function_refusals(const FunctionRow *row, const PiecePlan *loop_plan) {
    Fixture fixture;
    FunctionRow copy = *row;
    PiecePlan *plan = NULL;
    size_t bar = (size_t)row->beats;
    static const size_t T = 2, B = 3, TURN = 4, K = 5;

    /* A transition on the turnaround or the ending, a coda on any part but the ending. */
    function_structure(&fixture, &copy);
    fixture.sections[TURN].function = PIECE_FUNCTION_TRANSITION;
    assert(piece_request_check(&fixture.request) == PIECE_ERR_BAD_REQUEST);
    function_structure(&fixture, &copy);
    fixture.sections[K].function = PIECE_FUNCTION_TRANSITION;
    assert(piece_request_check(&fixture.request) == PIECE_ERR_BAD_REQUEST);
    function_structure(&fixture, &copy);
    fixture.sections[B].function = PIECE_FUNCTION_CODA;
    assert(piece_request_check(&fixture.request) == PIECE_ERR_BAD_REQUEST);
    function_structure(&fixture, &copy);
    fixture.sections[TURN].function = PIECE_FUNCTION_CODA;
    assert(piece_request_check(&fixture.request) == PIECE_ERR_BAD_REQUEST);
    function_structure(&fixture, &copy);
    assert(piece_request_check(&fixture.request) == PIECE_OK);
    assert(strcmp(piece_function_name(PIECE_FUNCTION_TRANSITION), "transition") == 0 &&
           strcmp(piece_function_name(PIECE_FUNCTION_CODA), "coda") == 0);
    PieceFunction named;
    assert(piece_function_from_name("coda", &named) && named == PIECE_FUNCTION_CODA);
    assert(!piece_function_from_name("none", &named) && !piece_function_from_name("retransition", &named));

    /* A transition that is no sentence-or-motif theme section with a half cadence: a perfect cadence, a period, a theme fill, a
       construction, a length that is no theme's. */
    function_structure(&fixture, &copy);
    fixture.sections[T].has_cadence = true;
    fixture.sections[T].cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    assert(piece_build_plan(&fixture.request, &plan) == PIECE_ERR_BAD_REQUEST && !plan);
    function_structure(&fixture, &copy);
    fixture.sections[T].has_into = true;
    fixture.sections[T].into = PLAN_ARCHETYPE_PERIOD;
    assert(piece_build_plan(&fixture.request, &plan) == PIECE_ERR_BAD_REQUEST && !plan);
    function_structure(&fixture, &copy);
    fixture.sections[T].fill = PIECE_FILL_PROGRESSION;
    assert(piece_build_plan(&fixture.request, &plan) == PIECE_ERR_BAD_REQUEST && !plan);
    function_structure(&fixture, &copy);
    fixture.sections[T].has_construction = true;
    fixture.sections[T].construction_auto = true;
    assert(piece_build_plan(&fixture.request, &plan) == PIECE_ERR_BAD_REQUEST && !plan);
    function_structure(&fixture, &copy);
    fixture.sections[T].beats = 6 * bar + 1;
    assert(piece_build_plan(&fixture.request, &plan) == PIECE_ERR_BAD_REQUEST && !plan);

    /* A coda that is not 8 whole bars with no pickup, a perfect authentic cadence and an empty or motif fill. */
    PiecePlan *ending = NULL;
    function_structure(&fixture, &copy);
    fixture.sections[K].beats = 4 * bar;
    assert(piece_build_ending(loop_plan, &fixture.request, &ending) == PIECE_ERR_BAD_REQUEST && !ending);
    function_structure(&fixture, &copy);
    assert(rational_make(1, 4, &fixture.sections[K].pickup) == RATIONAL_OK);
    assert(piece_build_ending(loop_plan, &fixture.request, &ending) == PIECE_ERR_BAD_REQUEST && !ending);
    function_structure(&fixture, &copy);
    fixture.sections[K].has_cadence = true;
    fixture.sections[K].cadence = CADENCE_TYPE_HALF;
    assert(piece_build_ending(loop_plan, &fixture.request, &ending) == PIECE_ERR_BAD_REQUEST && !ending);
    function_structure(&fixture, &copy);
    fixture.sections[K].fill = PIECE_FILL_PROGRESSION;
    assert(piece_build_ending(loop_plan, &fixture.request, &ending) == PIECE_ERR_BAD_REQUEST && !ending);
}

static void test_functions(void) {
    static FunctionRow rows[2];
    rows[0] = (FunctionRow){.name = "C major 4/4", .tonic = 0, .mode = KEY_STRUCTURE_MODE_MAJOR, .beats = 4};
    rows[1] = (FunctionRow){.name = "A minor 3/4", .tonic = 9, .mode = KEY_STRUCTURE_MODE_MINOR, .beats = 3};
    for (size_t r = 0; r < 2; r++) {
        FunctionRow *row = &rows[r];
        Fixture fixture;
        function_structure(&fixture, row);
        assert(piece_request_check(&fixture.request) == PIECE_OK);
        size_t loops = 0;
        size_t endings = 0;
        PiecePlan *kept = NULL;
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            PiecePlan *loop_plan = NULL;
            PiecePlan *ending = NULL;
            ComposeConfig config;
            ComposeOutcome outcome;
            memset(&config, 0, sizeof(config));
            config.seed = seed;
            assert(piece_build_plan(&fixture.request, &loop_plan) == PIECE_OK);
            PieceResult ran = compose_run(loop_plan, &config, &outcome);
            compose_outcome_destroy(&outcome);
            if (ran != PIECE_OK) {
                fprintf(stderr, "%s seed %lu: the loop does not compose\n", row->name, seed);
                plan_destroy(loop_plan);
                continue;
            }
            NodeId sections[PIECE_STRUCTURE_MAX_SECTIONS];
            size_t count = piece_sections(loop_plan, sections, PIECE_STRUCTURE_MAX_SECTIONS);
            assert(count == 5);
            assert(transition_ok(loop_plan, sections[2]));
            assert(!node_constraint(loop_plan, sections[1], PLAN_CONSTRAINT_SECTION_FUNCTION));
            loops++;
            assert(piece_build_ending(loop_plan, &fixture.request, &ending) == PIECE_OK);
            config.seed = seed + 20;
            ran = compose_run(ending, &config, &outcome);
            compose_outcome_destroy(&outcome);
            if (ran == PIECE_OK) {
                count = piece_sections(ending, sections, PIECE_STRUCTURE_MAX_SECTIONS);
                assert(count == 5 && coda_ok(ending, sections[count - 1]));
                endings++;
            } else {
                fprintf(stderr, "%s seed %lu: the ending does not compose\n", row->name, seed);
            }
            plan_destroy(ending);
            if (kept) {
                plan_destroy(loop_plan);
            } else {
                kept = loop_plan;
            }
        }
        printf("%s: the loop composed %zu of %d, the ending %zu of %d\n", row->name, loops, SEEDS, endings, SEEDS);
        assert(loops >= 9 && endings >= 9);
        function_refusals(row, kept);
        plan_destroy(kept);
    }
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "checks") == 0) {
        test_checks();
        test_default_cadences();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "plan") == 0) {
        test_plan();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "compose") == 0) {
        test_compose();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "pickup") == 0) {
        test_pickup();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "free") == 0) {
        test_free();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "minor_half_free") == 0) {
        test_minor_half_free();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "functions") == 0) {
        test_functions();
        return 0;
    }
    fprintf(stderr, "usage: test_piece_structure checks|plan|compose|pickup|free|minor_half_free|functions\n");
    return 2;
}
