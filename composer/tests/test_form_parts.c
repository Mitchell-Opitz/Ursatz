#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

#include "util/plan_util.h"

/*
 * The parts of larger forms (P51, R-503 to R-509), one case per branch. Each composes a 32-bar form built here in the eight
 * rows of key and meter, seeds 1 to 10, and at least 9 of 10 compose in each row.
 *
 * transition (B2, R-504): A theme 8 PAC closed, T transition 8 HC keys end `subordinate`, S subordinate 8 antecedent and
 * consequent PAC in `subordinate`, C codetta 8 presentation and continuation PAC in `subordinate`; region major [5] minor [3].
 * In every composed run T carries the transition function, T's second half is in the region's key (its key lane span starts
 * at T's half), T's last chord is that key's V or V7, and T's continuation is liquidated: its `fragment 2` idea is made by
 * liquidation steps on a segment (1 step or more) and its `cadential` idea by at least as many; the codetta's `fragment 2` has
 * none, so only the transition liquidates. The liquidation is read from the plan, not counted in notes: a residue is regrown to
 * fill its bar (grow-continuation.md), so its span often holds as many notes as fragment 1 or more (R-512).
 *
 * independent (B3, R-504): the same form with T `independent`. In every composed run T's `basic idea` melody differs from A's,
 * the material holds two invented motives (m0 and `independent T`, each of two notes or more), T's motive reference names the
 * second, and the composed plan round-trips through plan JSON byte for byte, as the plan built before it does.
 */

#define SEEDS 10
#define COMPOSED_LEAST 9

typedef struct {
    const char *name;
    int tonic;
    KeyStructureMode mode;
    int beats;
    int unit;
} Row;

static const Row ROWS[] = {{"C major 4/4", 0, KEY_STRUCTURE_MODE_MAJOR, 4, 4}, {"A minor 4/4", 9, KEY_STRUCTURE_MODE_MINOR, 4, 4},
                           {"C major 3/4", 0, KEY_STRUCTURE_MODE_MAJOR, 3, 4}, {"A minor 3/4", 9, KEY_STRUCTURE_MODE_MINOR, 3, 4},
                           {"G major 2/4", 7, KEY_STRUCTURE_MODE_MAJOR, 2, 4}, {"D minor 2/4", 2, KEY_STRUCTURE_MODE_MINOR, 2, 4},
                           {"C major 6/8", 0, KEY_STRUCTURE_MODE_MAJOR, 6, 8}, {"D minor 6/8", 2, KEY_STRUCTURE_MODE_MINOR, 6, 8}};
#define ROW_COUNT (sizeof(ROWS) / sizeof(ROWS[0]))

/* The 32-bar form; the transition's line is the member that differs between cases. */
static const char *const FORM_HEAD =
    "{\"schema\": \"ursatz-form\", \"version\": 1, \"name\": \"form-parts\", \"label\": \"Form parts\", \"modulates\": true,"
    " \"regions\": [{\"name\": \"subordinate\", \"major\": [5], \"minor\": [3]}],"
    " \"sizes\": [{\"bars\": 32, \"default\": true, \"sections\": ["
    "{\"name\": \"A\", \"letter\": \"A\", \"role\": \"theme\", \"bars\": 8, \"beginning\": \"presentation\","
    " \"ending\": \"continuation\", \"cadence\": \"perfect-authentic\", \"closed\": true},";
static const char *const FORM_TAIL =
    ",{\"name\": \"S\", \"letter\": \"S\", \"role\": \"subordinate\", \"bars\": 8, \"beginning\": \"antecedent\","
    " \"ending\": \"consequent\", \"cadence\": \"perfect-authentic\", \"keys\": {\"start\": \"subordinate\","
    " \"end\": \"subordinate\"}},"
    "{\"name\": \"C\", \"letter\": \"C\", \"role\": \"codetta\", \"bars\": 8, \"beginning\": \"presentation\","
    " \"ending\": \"continuation\", \"cadence\": \"perfect-authentic\", \"keys\": {\"start\": \"subordinate\","
    " \"end\": \"subordinate\"}}]}]}";
static const char *const INDEPENDENT =
    "{\"name\": \"T\", \"letter\": \"T\", \"role\": \"transition\", \"bars\": 8, \"beginning\": \"presentation\","
    " \"ending\": \"continuation\", \"cadence\": \"half\", \"independent\": true, \"keys\": {\"end\": \"subordinate\"}}";
static const char *const TRANSITION =
    "{\"name\": \"T\", \"letter\": \"T\", \"role\": \"transition\", \"bars\": 8, \"beginning\": \"presentation\","
    " \"ending\": \"continuation\", \"cadence\": \"half\", \"keys\": {\"end\": \"subordinate\"}}";

static FormSpec *load_form(const char *transition) {
    size_t size = strlen(FORM_HEAD) + strlen(transition) + strlen(FORM_TAIL) + 1;
    char *text = malloc(size);
    assert(text);
    snprintf(text, size, "%s%s%s", FORM_HEAD, transition, FORM_TAIL);
    FormSpec *form = NULL;
    FormSpecError error;
    if (form_spec_parse(text, &form, &error) != FORM_SPEC_OK) {
        fprintf(stderr, "form: %s: %s\n", error.path, error.message);
        exit(2);
    }
    free(text);
    return form;
}

static PiecePlan *compose(const FormSpec *form, const Row *row, unsigned long seed, ComposeOutcome *outcome,
                          PieceResult *ran) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form->name;
    request.key = plan_key_of_pitch_class(row->tonic, row->mode);
    request.meter.beats_per_bar = (unsigned)row->beats;
    request.meter.beat_unit = (unsigned)row->unit;
    request.seed = seed;
    request.modulate = true;
    PiecePlan *plan = NULL;
    PieceResult built = form_build_plan(form, &request, &plan);
    if (built != PIECE_OK) {
        fprintf(stderr, "form_build_plan: %s\n", piece_result_name(built));
        exit(2);
    }
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.form = form;
    *ran = compose_run(plan, &config, outcome);
    return plan;
}

static Rational add_time(Rational a, Rational b) {
    Rational out;
    assert(rational_add(a, b, &out) == RATIONAL_OK);
    return out;
}

/* The sections of the piece, in order (the material nodes are not sections). */
static size_t piece_sections(const PiecePlan *plan, NodeId *sections) {
    size_t count = 0;
    NodeId root = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId child = plan_node_child_at(plan, root, i);
        if (!plan_node_find_constraint(plan, child, PLAN_CONSTRAINT_MATERIAL)) {
            sections[count++] = child;
        }
    }
    return count;
}

/* The first node under `node` (any depth) with this name, or PLAN_NODE_NONE. */
static NodeId find_named(const PiecePlan *plan, NodeId node, const char *name) {
    for (size_t i = 0; i < plan_node_child_count(plan, node); i++) {
        NodeId child = plan_node_child_at(plan, node, i);
        if (strcmp(plan_node_name(plan, child), name) == 0) {
            return child;
        }
        NodeId inner = find_named(plan, child, name);
        if (inner != PLAN_NODE_NONE) {
            return inner;
        }
    }
    return PLAN_NODE_NONE;
}

/* The motive reference of the motive node an idea's melody span names (the idea's own made motive), or NULL. */
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

/* The liquidation steps an idea is made with, 0 for an idea that is not liquidated. */
static size_t idea_steps(const PiecePlan *plan, NodeId idea) {
    const PlanMotiveRef *ref = idea_motive_ref(plan, idea);
    return ref ? ref->steps : 0;
}

/* The tonic pitch class of the region the subordinate material is in: the fifth in major, the third (III) in minor. */
static int region_tonic(const Row *row) {
    return (row->tonic + (row->mode == KEY_STRUCTURE_MODE_MAJOR ? 7 : 3)) % 12;
}

/* One composed run of the transition case. */
static void check_transition(const PiecePlan *plan, const Row *row, unsigned long seed) {
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    assert(piece_sections(plan, sections) == 4);
    NodeId t = sections[1];
    PlanSectionFunction function;
    assert(plan_section_function_of(plan, t, &function) && function.function == PLAN_FUNCTION_TRANSITION);
    PlanSpan span = plan_node_span(plan, t);
    Rational bar = {row->beats, row->unit};
    Rational half = {0, 1};
    for (int i = 0; i < 4; i++) {
        half = add_time(half, bar);
    }
    Rational middle = add_time(span.start, half);
    Rational end = add_time(span.start, span.length);

    /* The second half is in the region's key: a key span starts at the half. */
    LaneSpanId keys[8];
    PlanSpan second = {middle, half};
    size_t found = plan_lane_find(plan, PLAN_LANE_KEY, 0, second, keys, 8);
    assert(found > 0);
    bool starts_at_half = false;
    for (size_t i = 0; i < found && i < 8; i++) {
        PlanSpan k = plan_lane_span_span(plan, keys[i]);
        if (rational_cmp(k.start, middle) == 0) {
            starts_at_half = true;
            if (analysis_key_tonic_pitch_class(&plan_lane_span_payload(plan, keys[i])->key) != region_tonic(row)) {
                fprintf(stderr, "%s seed %lu: T's second half is not in the region\n", row->name, seed);
                assert(false);
            }
        }
    }
    assert(starts_at_half);

    /* The last chord is the region key's V or V7 (the roman numeral is in that key). */
    LaneSpanId chords[16];
    found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, chords, 16);
    assert(found > 0);
    LaneSpanId last = chords[0];
    for (size_t i = 0; i < found && i < 16; i++) {
        if (rational_cmp(plan_lane_span_span(plan, chords[i]).start, plan_lane_span_span(plan, last).start) > 0) {
            last = chords[i];
        }
    }
    const char *numeral = plan_lane_span_payload(plan, last)->harmony.roman_numeral;
    assert(rational_cmp(plan_lane_span_span(plan, last).start, end) < 0);
    if (strcmp(numeral, "V") != 0 && strcmp(numeral, "V7") != 0) {
        fprintf(stderr, "%s seed %lu: T ends on %s\n", row->name, seed, numeral);
    }
    assert(strcmp(numeral, "V") == 0 || strcmp(numeral, "V7") == 0);

    /* The liquidation, read from the plan: fragment 2 is a liquidation by steps on a segment, the cadential idea at least as far. */
    NodeId fragment_2 = find_named(plan, t, "fragment 2");
    const PlanMotiveRef *residue = idea_motive_ref(plan, fragment_2);
    if (!residue || residue->steps < 1 || residue->segment[0] == '\0' ||
        idea_steps(plan, find_named(plan, t, "cadential")) < residue->steps) {
        fprintf(stderr, "%s seed %lu: T's continuation is not liquidated\n", row->name, seed);
    }
    assert(residue && residue->steps >= 1 && residue->segment[0] != '\0');
    assert(idea_steps(plan, find_named(plan, t, "cadential")) >= residue->steps);
    /* Only the transition liquidates: the codetta's continuation is the form's usual one. */
    assert(idea_steps(plan, find_named(plan, sections[3], "fragment 2")) == 0);
}

/* The pitches of an idea's melody span, in order; the count, 0 when the idea has none. */
static size_t idea_pitches(const PiecePlan *plan, NodeId idea, int *out, size_t room) {
    LaneSpanId melody;
    if (idea == PLAN_NODE_NONE || !plan_melody_span_of(plan, idea, &melody)) {
        return 0;
    }
    const PlanLanePayload *payload = plan_lane_span_payload(plan, melody);
    size_t count = payload->notes.note_count < room ? payload->notes.note_count : room;
    for (size_t i = 0; i < count; i++) {
        out[i] = payload->notes.notes[i].midi;
    }
    return count;
}

/* The motive node of the material with this name, or PLAN_NODE_NONE, and its invented notes (2 or more). */
static bool invented_motive(const PiecePlan *plan, const char *name, NodeId *node) {
    *node = find_named(plan, plan_root(plan), name);
    LaneSpanId melody;
    return *node != PLAN_NODE_NONE && plan_node_kind(plan, *node) == PLAN_NODE_MOTIVE && plan_melody_span_of(plan, *node, &melody) &&
           plan_lane_span_payload(plan, melody)->notes.note_count >= 2;
}

static void check_round_trip(const PiecePlan *plan, const Row *row, unsigned long seed) {
    PlanJsonError error;
    char *text = plan_to_json(plan, &error);
    assert(text);
    PiecePlan *again = NULL;
    assert(plan_from_json(text, &again, &error) == PLAN_OK);
    char *second = plan_to_json(again, &error);
    assert(second);
    if (strcmp(text, second) != 0) {
        fprintf(stderr, "%s seed %lu: the plan does not round-trip\n", row->name, seed);
        assert(false);
    }
    free(second);
    free(text);
    plan_destroy(again);
}

/* One composed run of the independent case. */
static void check_independent(const PiecePlan *plan, const Row *row, unsigned long seed) {
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    assert(piece_sections(plan, sections) == 4);
    NodeId own;
    NodeId m0;
    assert(invented_motive(plan, "m0", &m0) && invented_motive(plan, "independent T", &own));

    /* T's section asks for the motive of its own, as it is. */
    const PlanConstraint *ref = plan_node_find_constraint(plan, sections[1], PLAN_CONSTRAINT_MOTIVE_REF);
    assert(ref && ref->params.motive.motive == own && !ref->params.motive.has_step);
    assert(!plan_node_find_constraint(plan, sections[0], PLAN_CONSTRAINT_MOTIVE_REF));

    /* T's basic idea is not A's. */
    int a[64];
    int t[64];
    size_t a_count = idea_pitches(plan, find_named(plan, sections[0], "basic idea"), a, 64);
    size_t t_count = idea_pitches(plan, find_named(plan, sections[1], "basic idea"), t, 64);
    bool same = a_count == t_count;
    for (size_t i = 0; same && i < a_count; i++) {
        same = a[i] == t[i];
    }
    if (a_count == 0 || t_count == 0 || same) {
        fprintf(stderr, "%s seed %lu: T's basic idea is A's (%zu and %zu notes)\n", row->name, seed, a_count, t_count);
    }
    assert(a_count > 0 && t_count > 0 && !same);
    check_round_trip(plan, row, seed);
}

static int run_rows(const char *label, const FormSpec *form, void (*check)(const PiecePlan *, const Row *, unsigned long)) {
    int status = 0;
    for (size_t r = 0; r < ROW_COUNT; r++) {
        int composed = 0;
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            ComposeOutcome outcome;
            PieceResult ran;
            PiecePlan *plan = compose(form, &ROWS[r], seed, &outcome, &ran);
            if (ran == PIECE_OK) {
                check(plan, &ROWS[r], seed);
                composed++;
            } else {
                printf("  seed %lu: %s at %s\n", seed, piece_result_name(ran), compose_stage_name(outcome.stuck_stage));
            }
            compose_outcome_destroy(&outcome);
            plan_destroy(plan);
        }
        printf("%s in %s: %d of %d seeds compose\n", label, ROWS[r].name, composed, SEEDS);
        if (composed < COMPOSED_LEAST) {
            fprintf(stderr, "%s in %s: %d composed, at least %d expected\n", label, ROWS[r].name, composed, COMPOSED_LEAST);
            status = 1;
        }
    }
    return status;
}

int main(void) {
    FormSpec *form = load_form(TRANSITION);
    int status = run_rows("transition", form, check_transition);
    form_spec_destroy(form);
    form = load_form(INDEPENDENT);
    status |= run_rows("independent", form, check_independent);
    form_spec_destroy(form);
    if (status == 0) {
        printf("test_form_parts: ok\n");
    }
    return status;
}
