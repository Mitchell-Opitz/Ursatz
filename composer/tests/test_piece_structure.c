#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-harmony/harmony_cadence_rules.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/piece_structure.h>

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
 */

#define SEEDS 10

typedef struct {
    PieceSection sections[PIECE_STRUCTURE_MAX_SECTIONS + 2];
    PieceStructure structure;
} Fixture;

static void fixture_init(Fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->structure.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    fixture->structure.meter.beats_per_bar = 4;
    fixture->structure.meter.beat_unit = 4;
    fixture->structure.sections = fixture->sections;
}

static PieceSection *add_section(Fixture *fixture, const char *id, const char *letter, PiecePart part, size_t beats,
                                 PieceFillKind fill) {
    PieceSection *section = &fixture->sections[fixture->structure.section_count++];
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
    assert(piece_structure_check(&fixture.structure) == PIECE_OK);

    /* No body. */
    fixture_init(&fixture);
    add_section(&fixture, "intro", "I", PIECE_PART_INTRO, 8, PIECE_FILL_EMPTY);
    assert(piece_structure_check(&fixture.structure) == PIECE_ERR_BAD_REQUEST);

    /* Parts out of order: the turnaround before the body. */
    fixture_init(&fixture);
    add_section(&fixture, "t", "T", PIECE_PART_TURNAROUND, 4, PIECE_FILL_EMPTY);
    add_section(&fixture, "a", "A", PIECE_PART_BODY, 16, PIECE_FILL_EMPTY);
    assert(piece_structure_check(&fixture.structure) == PIECE_ERR_BAD_REQUEST);

    /* A duplicate id. */
    fixture_init(&fixture);
    add_section(&fixture, "a", "A", PIECE_PART_BODY, 16, PIECE_FILL_EMPTY);
    add_section(&fixture, "a", "B", PIECE_PART_BODY, 16, PIECE_FILL_EMPTY);
    assert(piece_structure_check(&fixture.structure) == PIECE_ERR_BAD_REQUEST);

    /* A length under one beat. */
    fixture_init(&fixture);
    add_section(&fixture, "a", "A", PIECE_PART_BODY, 0, PIECE_FILL_EMPTY);
    assert(piece_structure_check(&fixture.structure) == PIECE_ERR_BAD_REQUEST);

    /* Pickups: a quarter is fine, a bar is not, a triplet sixteenth is not a multiple of 1/32. */
    fixture_init(&fixture);
    PieceSection *section = add_section(&fixture, "a", "A", PIECE_PART_BODY, 16, PIECE_FILL_EMPTY);
    assert(rational_make(1, 4, &section->pickup) == RATIONAL_OK);
    assert(piece_structure_check(&fixture.structure) == PIECE_OK);
    assert(rational_make(1, 1, &section->pickup) == RATIONAL_OK);
    assert(piece_structure_check(&fixture.structure) == PIECE_ERR_BAD_REQUEST);
    assert(rational_make(1, 12, &section->pickup) == RATIONAL_OK);
    assert(piece_structure_check(&fixture.structure) == PIECE_ERR_BAD_REQUEST);

    /* Too many sections, and too many bars. */
    fixture_init(&fixture);
    for (size_t i = 0; i < PIECE_STRUCTURE_MAX_SECTIONS + 1; i++) {
        char id[PIECE_SECTION_ID_MAX];
        snprintf(id, sizeof(id), "s%zu", i);
        add_section(&fixture, id, "A", PIECE_PART_BODY, 4, PIECE_FILL_EMPTY);
    }
    assert(piece_structure_check(&fixture.structure) == PIECE_ERR_BAD_REQUEST);
    fixture_init(&fixture);
    add_section(&fixture, "a", "A", PIECE_PART_BODY, 4 * (PIECE_STRUCTURE_MAX_BARS + 1), PIECE_FILL_EMPTY);
    assert(piece_structure_check(&fixture.structure) == PIECE_ERR_BAD_REQUEST);
}

static void test_default_cadences(void) {
    Fixture fixture;
    standard_structure(&fixture);
    PieceSection *explicit_cadence = &fixture.sections[2];
    explicit_cadence->has_cadence = true;
    explicit_cadence->cadence = CADENCE_TYPE_DECEPTIVE;
    assert(piece_structure_default_ending(&fixture.structure, PIECE_STRUCTURE_MAX_SECTIONS + 2));
    assert(fixture.structure.section_count == 5);
    piece_structure_default_cadences(&fixture.structure);
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
    assert(piece_structure_default_ending(&fixture.structure, PIECE_STRUCTURE_MAX_SECTIONS + 2));
    assert(fixture.structure.section_count == 5);
}

static void test_plan(void) {
    Fixture fixture;
    standard_structure(&fixture);
    PieceSection *pickup = &fixture.sections[2];
    assert(rational_make(1, 4, &pickup->pickup) == RATIONAL_OK);
    PiecePlan *plan;
    assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_OK);
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
    assert(piece_structure_default_ending(&fixture.structure, PIECE_STRUCTURE_MAX_SECTIONS + 2));
    assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_OK);
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
        assert(piece_structure_build_plan(&fixture->structure, &plan) == PIECE_OK);
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
    assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_OK);
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
    assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_OK);
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
        assert(piece_structure_build_plan(&fixture.structure, &plan) == PIECE_OK);
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
    fprintf(stderr, "usage: test_piece_structure checks|plan|compose|pickup|free\n");
    return 2;
}
