#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/analysis_load.h>
#include <galant_prototypes.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

/*
 * A plan written as JSON and read back is the same plan: tree, spans, states,
 * constraints and lane payloads. An analysis document loads as a Realized
 * plan with its skipped and trimmed elements reported. Malformed input is
 * refused with a path and no crash.
 */

static PlanSpan bars(long start, long length) {
    PlanSpan span;
    assert(plan_time_make(start, 1, &span.start) == PLAN_OK);
    assert(plan_time_make(length, 1, &span.length) == PLAN_OK);
    return span;
}

static NodeId add(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, const char *name) {
    NodeId id = PLAN_NODE_NONE;
    assert(plan_add_node(plan, parent, kind, span, name, &id) == PLAN_OK);
    return id;
}

static LaneSpanId add_lane(PiecePlan *plan, NodeId owner, PlanLaneKind lane, int voice, PlanSpan span,
                           const PlanLanePayload *payload) {
    LaneSpanId id = PLAN_LANE_SPAN_NONE;
    assert(plan_add_lane_span(plan, owner, lane, voice, span, payload, &id) == PLAN_OK);
    return id;
}

static PlanLanePayload harmony(const char *roman, PlanHarmonicFunction function) {
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    strncpy(payload.harmony.roman_numeral, roman, PLAN_ROMAN_NUMERAL_MAX - 1);
    payload.harmony.function = function;
    return payload;
}

/* The 8-bar sentence from test_piece_plan_sentence, with a mix of states and constraints. */
static PiecePlan *build_sentence_plan(void) {
    PiecePlan *plan = plan_create();
    assert(plan != NULL);
    NodeId form = add(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 8), "form");
    NodeId section = add(plan, form, PLAN_NODE_SECTION, bars(0, 8), "A");
    NodeId phrase = add(plan, section, PLAN_NODE_PHRASE, bars(0, 8), "sentence");
    NodeId presentation = add(plan, phrase, PLAN_NODE_SUBPHRASE, bars(0, 4), "presentation");
    NodeId basic_idea = add(plan, presentation, PLAN_NODE_IDEA, bars(0, 2), "basic idea");
    NodeId m1 = add(plan, basic_idea, PLAN_NODE_MOTIVE, bars(0, 1), "m1");
    NodeId repetition = add(plan, presentation, PLAN_NODE_IDEA, bars(2, 2), NULL);
    NodeId continuation = add(plan, phrase, PLAN_NODE_SUBPHRASE, bars(4, 4), "continuation");
    NodeId fragment = add(plan, continuation, PLAN_NODE_MOTIVE, bars(4, 1), "fragment");

    PlanLanePayload meter;
    memset(&meter, 0, sizeof(meter));
    meter.meter.beats_per_bar = 4;
    meter.meter.beat_unit = 4;
    meter.meter.bars_per_hyperbar = 4;
    LaneSpanId meter_span = add_lane(plan, form, PLAN_LANE_METER, 0, bars(0, 8), &meter);

    PlanLanePayload key;
    memset(&key, 0, sizeof(key));
    key.key.tonic_pitch_class = 3;
    key.key.mode = PLAN_MODE_MINOR;
    add_lane(plan, section, PLAN_LANE_KEY, 0, bars(0, 8), &key);

    PlanLanePayload tonic = harmony("i", PLAN_FUNCTION_TONIC);
    PlanLanePayload dominant = harmony("V", PLAN_FUNCTION_DOMINANT);
    add_lane(plan, basic_idea, PLAN_LANE_HARMONY, 0, bars(0, 2), &tonic);
    LaneSpanId harmony_two = add_lane(plan, repetition, PLAN_LANE_HARMONY, 0, bars(2, 2), &dominant);

    PlanNote notes[2];
    memset(notes, 0, sizeof(notes));
    notes[0].midi_pitch = 64;
    assert(plan_time_make(0, 1, &notes[0].offset) == PLAN_OK);
    assert(plan_time_make(1, 2, &notes[0].duration) == PLAN_OK);
    notes[1].midi_pitch = 67;
    assert(plan_time_make(1, 2, &notes[1].offset) == PLAN_OK);
    assert(plan_time_make(1, 4, &notes[1].duration) == PLAN_OK);
    PlanLanePayload melody;
    memset(&melody, 0, sizeof(melody));
    melody.notes.notes = notes;
    melody.notes.note_count = 2;
    LaneSpanId melody_span = add_lane(plan, m1, PLAN_LANE_MELODY, 0, bars(0, 1), &melody);
    PlanLanePayload bass = melody;
    add_lane(plan, basic_idea, PLAN_LANE_BASS, 0, bars(0, 2), &bass);
    add_lane(plan, basic_idea, PLAN_LANE_INNER_VOICE, 2, bars(0, 2), &bass);

    PlanConstraint length = {PLAN_CONSTRAINT_LENGTH_BARS, {0}};
    length.params.bars = 8;
    PlanConstraint cadence = {PLAN_CONSTRAINT_CADENCE, {0}};
    cadence.params.cadence = PLAN_CADENCE_PAC;
    PlanConstraint archetype = {PLAN_CONSTRAINT_ARCHETYPE, {0}};
    archetype.params.archetype = PLAN_ARCHETYPE_SENTENCE;
    PlanConstraint motive_ref = {PLAN_CONSTRAINT_MOTIVE_REF, {0}};
    motive_ref.params.motive.motive = m1;
    motive_ref.params.motive.transform = PLAN_TRANSFORM_TRANSPOSED;
    motive_ref.params.motive.semitones = -2;
    PlanConstraint key_constraint = {PLAN_CONSTRAINT_KEY, {0}};
    key_constraint.params.key.tonic_pitch_class = 7;
    key_constraint.params.key.mode = PLAN_MODE_MAJOR;
    assert(plan_node_add_constraint(plan, phrase, &length) == PLAN_OK);
    assert(plan_node_add_constraint(plan, phrase, &cadence) == PLAN_OK);
    assert(plan_node_add_constraint(plan, phrase, &archetype) == PLAN_OK);
    assert(plan_node_add_constraint(plan, fragment, &motive_ref) == PLAN_OK);
    assert(plan_lane_span_add_constraint(plan, harmony_two, &key_constraint) == PLAN_OK);
    assert(plan_lane_span_add_constraint(plan, harmony_two, &motive_ref) == PLAN_OK);
    PlanConstraint embellish = {PLAN_CONSTRAINT_EMBELLISH, {0}};
    assert(plan_lane_span_add_constraint(plan, harmony_two, &embellish) == PLAN_OK);
    PlanConstraint schema = {PLAN_CONSTRAINT_SCHEMA, {0}};
    schema.params.schema.schema = GALANT_SCHEMA_ROMANESCA;
    schema.params.schema.stage = 2;
    assert(plan_lane_span_add_constraint(plan, harmony_two, &schema) == PLAN_OK);
    schema.params.schema.stage = 9;
    assert(plan_lane_span_add_constraint(plan, harmony_two, &schema) == PLAN_ERR_BAD_PAYLOAD);

    /* The constraints a grown theme carries. */
    PlanConstraint material = {PLAN_CONSTRAINT_MATERIAL, {0}};
    assert(plan_node_add_constraint(plan, section, &material) == PLAN_OK);
    PlanConstraint shape = {PLAN_CONSTRAINT_HARMONY_SHAPE, {0}};
    shape.params.harmony_shape.kind = PLAN_HARMONY_SHAPE_PHRASE_MODEL;
    shape.params.harmony_shape.starts_on = PLAN_HARMONY_ANCHOR_TONIC;
    shape.params.harmony_shape.ends_on = PLAN_HARMONY_ANCHOR_DOMINANT;
    shape.params.harmony_shape.max_cells_per_chord = 2;
    shape.params.harmony_shape.split_at_bars = true;
    assert(plan_lane_span_add_constraint(plan, harmony_two, &shape) == PLAN_OK);
    shape.params.harmony_shape.max_cells_per_chord = -1;
    assert(plan_lane_span_add_constraint(plan, harmony_two, &shape) == PLAN_ERR_BAD_PAYLOAD);
    PlanConstraint pairing = {PLAN_CONSTRAINT_THEME_PAIRING, {0}};
    pairing.params.theme_pairing.beginning = PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA;
    pairing.params.theme_pairing.ending = PLAN_THEME_ENDING_CADENTIAL;
    assert(plan_node_add_constraint(plan, phrase, &pairing) == PLAN_OK);
    pairing.params.theme_pairing.ending = (PlanThemeEnding)7;
    assert(plan_node_add_constraint(plan, phrase, &pairing) == PLAN_ERR_BAD_PAYLOAD);

    assert(plan_node_set_state(plan, m1, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_node_set_state(plan, repetition, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, meter_span, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, melody_span, PLAN_STATE_REALIZED) == PLAN_OK);
    return plan;
}

static bool same_span(PlanSpan a, PlanSpan b) {
    return plan_time_compare(a.start, b.start) == 0 && plan_time_compare(a.length, b.length) == 0;
}

static bool same_constraint(const PlanConstraint *a, const PlanConstraint *b) {
    if (a->kind != b->kind) {
        return false;
    }
    switch (a->kind) {
    case PLAN_CONSTRAINT_LENGTH_BARS:
        return a->params.bars == b->params.bars;
    case PLAN_CONSTRAINT_CADENCE:
        return a->params.cadence == b->params.cadence;
    case PLAN_CONSTRAINT_MOTIVE_REF:
        return a->params.motive.motive == b->params.motive.motive &&
               a->params.motive.transform == b->params.motive.transform &&
               a->params.motive.semitones == b->params.motive.semitones;
    case PLAN_CONSTRAINT_KEY:
        return a->params.key.tonic_pitch_class == b->params.key.tonic_pitch_class &&
               a->params.key.mode == b->params.key.mode;
    case PLAN_CONSTRAINT_ARCHETYPE:
        return a->params.archetype == b->params.archetype;
    case PLAN_CONSTRAINT_EMBELLISH:
        return true;
    case PLAN_CONSTRAINT_SCHEMA:
        return a->params.schema.schema == b->params.schema.schema && a->params.schema.stage == b->params.schema.stage;
    case PLAN_CONSTRAINT_MATERIAL:
        return true;
    case PLAN_CONSTRAINT_HARMONY_SHAPE:
        return a->params.harmony_shape.kind == b->params.harmony_shape.kind &&
               a->params.harmony_shape.starts_on == b->params.harmony_shape.starts_on &&
               a->params.harmony_shape.ends_on == b->params.harmony_shape.ends_on &&
               a->params.harmony_shape.max_cells_per_chord == b->params.harmony_shape.max_cells_per_chord &&
               a->params.harmony_shape.split_at_bars == b->params.harmony_shape.split_at_bars;
    case PLAN_CONSTRAINT_THEME_PAIRING:
        return a->params.theme_pairing.beginning == b->params.theme_pairing.beginning &&
               a->params.theme_pairing.ending == b->params.theme_pairing.ending;
    case PLAN_CONSTRAINT_ACCOMPANIMENT:
        return a->params.accompaniment.pattern == b->params.accompaniment.pattern &&
               a->params.accompaniment.texture == b->params.accompaniment.texture &&
               a->params.accompaniment.low == b->params.accompaniment.low &&
               a->params.accompaniment.high == b->params.accompaniment.high;
    case PLAN_CONSTRAINT_VOICING:
        return a->params.voicing.write_soprano == b->params.voicing.write_soprano;
    case PLAN_CONSTRAINT_COUNTERPOINT:
        return a->params.counterpoint.species == b->params.counterpoint.species &&
               a->params.counterpoint.above == b->params.counterpoint.above;
    }
    return false;
}

static bool same_payload(PlanLaneKind lane, const PlanLanePayload *a, const PlanLanePayload *b) {
    switch (lane) {
    case PLAN_LANE_METER:
        return a->meter.beats_per_bar == b->meter.beats_per_bar && a->meter.beat_unit == b->meter.beat_unit &&
               a->meter.bars_per_hyperbar == b->meter.bars_per_hyperbar;
    case PLAN_LANE_KEY:
        return a->key.tonic_pitch_class == b->key.tonic_pitch_class && a->key.mode == b->key.mode;
    case PLAN_LANE_HARMONY:
        return strcmp(a->harmony.roman_numeral, b->harmony.roman_numeral) == 0 &&
               a->harmony.function == b->harmony.function;
    case PLAN_LANE_BASS:
    case PLAN_LANE_MELODY:
    case PLAN_LANE_INNER_VOICE:
        if (a->notes.note_count != b->notes.note_count) {
            return false;
        }
        for (size_t i = 0; i < a->notes.note_count; i++) {
            const PlanNote *x = &a->notes.notes[i];
            const PlanNote *y = &b->notes.notes[i];
            if (x->midi_pitch != y->midi_pitch || plan_time_compare(x->offset, y->offset) != 0 ||
                plan_time_compare(x->duration, y->duration) != 0) {
                return false;
            }
        }
        return true;
    }
    return false;
}

static bool plans_equal(const PiecePlan *a, const PiecePlan *b) {
    if (plan_node_count(a) != plan_node_count(b) || plan_lane_span_count(a) != plan_lane_span_count(b)) {
        return false;
    }
    for (NodeId id = 1; id <= plan_node_count(a); id++) {
        const char *name_a = plan_node_name(a, id);
        const char *name_b = plan_node_name(b, id);
        if (plan_node_kind(a, id) != plan_node_kind(b, id) || plan_node_parent(a, id) != plan_node_parent(b, id) ||
            !same_span(plan_node_span(a, id), plan_node_span(b, id)) || (name_a == NULL) != (name_b == NULL) ||
            (name_a && strcmp(name_a, name_b) != 0) || plan_node_state(a, id) != plan_node_state(b, id) ||
            plan_node_child_count(a, id) != plan_node_child_count(b, id) ||
            plan_node_constraint_count(a, id) != plan_node_constraint_count(b, id)) {
            return false;
        }
        for (size_t i = 0; i < plan_node_child_count(a, id); i++) {
            if (plan_node_child_at(a, id, i) != plan_node_child_at(b, id, i)) {
                return false;
            }
        }
        for (size_t i = 0; i < plan_node_constraint_count(a, id); i++) {
            if (!same_constraint(plan_node_constraint_at(a, id, i), plan_node_constraint_at(b, id, i))) {
                return false;
            }
        }
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(a); id++) {
        PlanLaneKind lane = plan_lane_span_lane(a, id);
        if (lane != plan_lane_span_lane(b, id) || plan_lane_span_voice(a, id) != plan_lane_span_voice(b, id) ||
            plan_lane_span_owner(a, id) != plan_lane_span_owner(b, id) ||
            !same_span(plan_lane_span_span(a, id), plan_lane_span_span(b, id)) ||
            plan_lane_span_state(a, id) != plan_lane_span_state(b, id) ||
            !same_payload(lane, plan_lane_span_payload(a, id), plan_lane_span_payload(b, id)) ||
            plan_lane_span_constraint_count(a, id) != plan_lane_span_constraint_count(b, id)) {
            return false;
        }
        for (size_t i = 0; i < plan_lane_span_constraint_count(a, id); i++) {
            if (!same_constraint(plan_lane_span_constraint_at(a, id, i), plan_lane_span_constraint_at(b, id, i))) {
                return false;
            }
        }
    }
    return true;
}

static void expect_ok(PlanResult result, const PlanJsonError *error) {
    if (result != PLAN_OK) {
        fprintf(stderr, "result %d at '%s': %s\n", (int)result, error->path, error->message);
    }
    assert(result == PLAN_OK);
}

static void test_round_trip(void) {
    PiecePlan *plan = build_sentence_plan();
    PlanJsonError error;

    char *text = plan_to_json(plan, &error);
    assert(text != NULL);
    assert(text[strlen(text) - 1] == '\n');
    assert(strstr(text, "\"schema_version\": 1") != NULL);

    PiecePlan *parsed = NULL;
    expect_ok(plan_from_json(text, &parsed, &error), &error);
    assert(parsed != NULL);
    assert(plans_equal(plan, parsed));
    assert(plan_node_state(parsed, 6) == PLAN_STATE_GIVEN);
    assert(plan_lane_span_state(parsed, 1) == PLAN_STATE_GIVEN);

    /* Writing the parsed plan gives the same bytes. */
    char *again = plan_to_json(parsed, &error);
    assert(again != NULL);
    assert(strcmp(text, again) == 0);

    /* Through a file. */
    assert(plan_write_json_file(plan, "test_plan_json_roundtrip.json", &error) == PLAN_OK);
    PiecePlan *from_file = NULL;
    assert(plan_read_json_file("test_plan_json_roundtrip.json", &from_file, &error) == PLAN_OK);
    assert(plans_equal(plan, from_file));

    /* A Given item stays refusing writes after the round trip. */
    PlanConstraint length = {PLAN_CONSTRAINT_LENGTH_BARS, {0}};
    length.params.bars = 1;
    assert(plan_node_add_constraint(parsed, 6, &length) == PLAN_ERR_GIVEN_LOCKED);

    free(text);
    free(again);
    plan_destroy(plan);
    plan_destroy(parsed);
    plan_destroy(from_file);
}

static void test_empty_plan(void) {
    PiecePlan *plan = plan_create();
    PlanJsonError error;
    char *text = plan_to_json(plan, &error);
    assert(text != NULL);
    PiecePlan *parsed = NULL;
    assert(plan_from_json(text, &parsed, &error) == PLAN_OK);
    assert(plan_node_count(parsed) == 0);
    assert(plan_lane_span_count(parsed) == 0);
    free(text);
    plan_destroy(plan);
    plan_destroy(parsed);
}

#define SPAN_0_1 "\"span\":{\"start\":{\"num\":0,\"den\":1},\"end\":{\"num\":1,\"den\":1}}"
#define COMPOSER_ID_1 "\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":[]}"

static void expect_error(const char *text, PlanResult expected, const char *expected_path) {
    PiecePlan *plan = (PiecePlan *)0x1;
    PlanJsonError error;
    memset(&error, 0, sizeof(error));
    assert(plan_from_json(text, &plan, &error) == expected);
    assert(plan == NULL);
    assert(error.message[0] != '\0');
    if (expected_path) {
        if (strcmp(error.path, expected_path) != 0) {
            fprintf(stderr, "expected path '%s', got '%s' (%s)\n", expected_path, error.path, error.message);
        }
        assert(strcmp(error.path, expected_path) == 0);
    }
}

static void test_malformed_input(void) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(plan_from_json(NULL, &plan, &error) == PLAN_ERR_NULL);

    expect_error("not json", PLAN_ERR_PARSE, "");
    expect_error("{\"schema_version\":1,", PLAN_ERR_PARSE, "");
    expect_error("", PLAN_ERR_PARSE, "");
    expect_error("[]", PLAN_ERR_FORMAT, "");
    expect_error("{}", PLAN_ERR_FORMAT, "schema_version");
    expect_error("{\"schema_version\":2,\"form\":null}", PLAN_ERR_FORMAT, "schema_version");
    expect_error("{\"schema_version\":1}", PLAN_ERR_FORMAT, "form");
    expect_error("{\"schema_version\":1,\"form\":5}", PLAN_ERR_FORMAT, "form");

    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"bogus\",\"label\":null," SPAN_0_1
                 ",\"children\":[]," COMPOSER_ID_1 "}}",
                 PLAN_ERR_FORMAT, "form.kind");
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"frozen\",\"constraints\":[]}}}",
                 PLAN_ERR_FORMAT, "form.composer.state");
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null,"
                 "\"span\":{\"start\":{\"num\":0,\"den\":0},\"end\":{\"num\":1,\"den\":1}},\"children\":[]," COMPOSER_ID_1
                 "}}",
                 PLAN_ERR_FORMAT, "form.span.start.den");
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null,"
                 "\"span\":{\"start\":{\"num\":1,\"den\":1},\"end\":{\"num\":1,\"den\":1}},\"children\":[]," COMPOSER_ID_1
                 "}}",
                 PLAN_ERR_FORMAT, "form.span");
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[]}}",
                 PLAN_ERR_FORMAT, "form.composer");
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":5,\"state\":\"constrained\",\"constraints\":[]}}}",
                 PLAN_ERR_FORMAT, "form.composer.id");
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":"
                 "[{\"kind\":\"sparkle\"}]}}}",
                 PLAN_ERR_FORMAT, "form.composer.constraints[0].kind");
    /* The constraints of a grown theme: a name that is not one is refused where it stands. */
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":"
                 "[{\"kind\":\"harmony_shape\",\"shape\":\"waltz\"}]}}}",
                 PLAN_ERR_FORMAT, "form.composer.constraints[0].shape");
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":"
                 "[{\"kind\":\"harmony_shape\",\"shape\":\"prolongation\",\"starts_on\":\"mediant\"}]}}}",
                 PLAN_ERR_FORMAT, "form.composer.constraints[0].starts_on");
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":"
                 "[{\"kind\":\"theme_pairing\",\"beginning\":\"presentation\"}]}}}",
                 PLAN_ERR_FORMAT, "form.composer.constraints[0].ending");
    {
        /* A pairing and a shape with their defaults leave out what is not asked for. */
        PiecePlan *parsed_pairing = NULL;
        assert(plan_from_json("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                              ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":"
                              "[{\"kind\":\"theme_pairing\",\"beginning\":\"antecedent\",\"ending\":\"consequent\"},"
                              "{\"kind\":\"harmony_shape\",\"shape\":\"cadential\"},{\"kind\":\"material\"}]}}}",
                              &parsed_pairing, &error) == PLAN_OK);
        const PlanConstraint *read = plan_node_constraint_at(parsed_pairing, 1, 0);
        assert(read && read->params.theme_pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT &&
               read->params.theme_pairing.ending == PLAN_THEME_ENDING_CONSEQUENT);
        read = plan_node_constraint_at(parsed_pairing, 1, 1);
        assert(read && read->params.harmony_shape.kind == PLAN_HARMONY_SHAPE_CADENTIAL &&
               read->params.harmony_shape.starts_on == PLAN_HARMONY_ANCHOR_ANY &&
               read->params.harmony_shape.max_cells_per_chord == 0 && !read->params.harmony_shape.split_at_bars);
        assert(plan_node_constraint_at(parsed_pairing, 1, 2)->kind == PLAN_CONSTRAINT_MATERIAL);
        plan_destroy(parsed_pairing);
    }
    /* A section outside its parent's span. */
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[{\"kind\":\"section\",\"label\":null,"
                 "\"span\":{\"start\":{\"num\":0,\"den\":1},\"end\":{\"num\":2,\"den\":1}},\"children\":[],"
                 "\"composer\":{\"id\":2,\"state\":\"constrained\",\"constraints\":[]}}]," COMPOSER_ID_1 "}}",
                 PLAN_ERR_FORMAT, "form.children[0]");
    /* A child before its parent in id order. */
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[{\"kind\":\"section\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":[]}}],"
                 "\"composer\":{\"id\":2,\"state\":\"constrained\",\"constraints\":[]}}}",
                 PLAN_ERR_FORMAT, NULL);
    /* A lane span with no composer object. */
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[]," COMPOSER_ID_1 "},\"keys\":[{\"label\":\"C major\"}]}",
                 PLAN_ERR_FORMAT, "keys[0].composer");

    /* Unknown fields are ignored. */
    PiecePlan *parsed = NULL;
    assert(plan_from_json("{\"schema_version\":1,\"extra\":[1,2],\"form\":null}", &parsed, &error) == PLAN_OK);
    plan_destroy(parsed);

    assert(plan_read_json_file("no/such/file.json", &parsed, &error) == PLAN_ERR_IO);
    assert(parsed == NULL);
}

static bool report_has(const AnalysisLoadReport *report, const char *path_prefix, const char *message_part) {
    for (size_t i = 0; i < report->count; i++) {
        if (strncmp(report->notes[i].path, path_prefix, strlen(path_prefix)) == 0 &&
            strstr(report->notes[i].message, message_part) != NULL) {
            return true;
        }
    }
    return false;
}

static void test_load_analysis_document(void) {
    PiecePlan *plan = NULL;
    AnalysisLoadReport report = {NULL, 0};
    PlanJsonError error;
    assert(plan_read_analysis_document_file(COMPOSER_FIXTURE_DIR "/analysis_small.json", &plan, &report, &error) ==
           PLAN_OK);

    /* A Form root over the piece, two bars long, named for the source file. */
    assert(plan_node_count(plan) == 1);
    NodeId root = plan_root(plan);
    assert(plan_node_kind(plan, root) == PLAN_NODE_FORM);
    assert(strcmp(plan_node_name(plan, root), "small.mid") == 0);
    PlanTime two;
    assert(plan_time_make(2, 1, &two) == PLAN_OK);
    assert(plan_time_compare(plan_node_span(plan, root).length, two) == 0);
    assert(plan_node_state(plan, root) == PLAN_STATE_REALIZED);

    /* One key, and the three chords that map. */
    assert(plan_lane_span_count(plan) == 4);
    const PlanLanePayload *key = plan_lane_span_payload(plan, 1);
    assert(plan_lane_span_lane(plan, 1) == PLAN_LANE_KEY);
    assert(key->key.tonic_pitch_class == 0 && key->key.mode == PLAN_MODE_MAJOR);
    const PlanLanePayload *dominant = plan_lane_span_payload(plan, 3);
    assert(plan_lane_span_lane(plan, 3) == PLAN_LANE_HARMONY);
    assert(strcmp(dominant->harmony.roman_numeral, "V") == 0);
    assert(dominant->harmony.function == PLAN_FUNCTION_UNKNOWN);

    /* The V ran to bar 3/2 but the next chord starts at 1, so it is cut to 1/2..1. */
    PlanTime one;
    assert(plan_time_make(1, 1, &one) == PLAN_OK);
    PlanTime dominant_end;
    assert(plan_span_end(plan_lane_span_span(plan, 3), &dominant_end) == PLAN_OK);
    assert(plan_time_compare(dominant_end, one) == 0);

    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        assert(plan_lane_span_state(plan, id) == PLAN_STATE_REALIZED);
        assert(plan_lane_span_owner(plan, id) == root);
    }

    /* Nothing is dropped silently. */
    assert(report.count == 3);
    assert(report_has(&report, "chords[3]", "skipped"));
    assert(report_has(&report, "chords[1]", "trimmed"));
    assert(report_has(&report, "phrases", "not loaded"));

    /* Realized material can be regenerated: a write is accepted. */
    PlanLanePayload tonic = harmony("I", PLAN_FUNCTION_TONIC);
    assert(plan_lane_span_write(plan, 2, &tonic) == PLAN_OK);

    /* The loaded plan survives a plan JSON round trip. */
    char *text = plan_to_json(plan, &error);
    PiecePlan *again = NULL;
    assert(text != NULL);
    assert(plan_from_json(text, &again, &error) == PLAN_OK);
    assert(plans_equal(plan, again));
    free(text);
    plan_destroy(again);

    analysis_load_report_destroy(&report);
    plan_destroy(plan);
}

static void test_analysis_document_errors(void) {
    PiecePlan *plan = NULL;
    AnalysisLoadReport report = {NULL, 0};
    PlanJsonError error;
    assert(plan_from_analysis_document("nope", &plan, &report, &error) == PLAN_ERR_PARSE);
    assert(plan == NULL);
    assert(plan_from_analysis_document("{\"schema_version\":1,\"keys\":[],\"chords\":[]}", &plan, &report, &error) ==
           PLAN_ERR_FORMAT);
    assert(plan == NULL && report.count == 0);
    assert(plan_from_analysis_document("{\"schema_version\":9}", &plan, NULL, &error) == PLAN_ERR_FORMAT);
    assert(strcmp(error.path, "schema_version") == 0);
    assert(plan_from_analysis_document("{\"schema_version\":1,\"keys\":[{\"label\":\"C major\"}]}", &plan, NULL,
                                       &error) == PLAN_ERR_FORMAT);
    assert(strcmp(error.path, "keys[0].span") == 0);
}

int main(void) {
    test_round_trip();
    test_empty_plan();
    test_malformed_input();
    test_load_analysis_document();
    test_analysis_document_errors();
    return 0;
}
