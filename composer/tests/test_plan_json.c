#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <plan/section_json.h>
#include <theory/galant/galant_prototypes.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include "support/test_support.h"
#include "ursatz_test.h"
#include "support/test_knowledge.h"

/*
 * A plan written as JSON and read back is the same plan: tree, spans, states,
 * constraints and lane payloads. Malformed input is refused with a path and no crash.
 */

static PlanSpan bars(long start, long length) {
    PlanSpan span;
    assert(rational_make(start, 1, &span.start) == RATIONAL_OK);
    assert(rational_make(length, 1, &span.length) == RATIONAL_OK);
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

static PlanLanePayload harmony(const char *roman, int function) {
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    strncpy(payload.harmony.roman_numeral, roman, PLAN_ROMAN_NUMERAL_MAX - 1);
    payload.harmony.has_function = function >= 0;
    payload.harmony.function = function >= 0 ? (HarmonicFunction)function : HARMONIC_FUNCTION_TONIC;
    return payload;
}

/* The 8-bar sentence from test_piece_plan_sentence, with a mix of states and constraints. tone_labels is how many labels
   of a motive_tones constraint its first motive node carries (0 for none). */
static PiecePlan *build_sentence_plan_with_tones(size_t tone_labels) {
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
    key.key = test_key(3, "minor");
    add_lane(plan, section, PLAN_LANE_KEY, 0, bars(0, 8), &key);

    PlanLanePayload tonic = harmony("i", HARMONIC_FUNCTION_TONIC);
    PlanLanePayload dominant = harmony("V", HARMONIC_FUNCTION_DOMINANT);
    add_lane(plan, basic_idea, PLAN_LANE_HARMONY, 0, bars(0, 2), &tonic);
    LaneSpanId harmony_two = add_lane(plan, repetition, PLAN_LANE_HARMONY, 0, bars(2, 2), &dominant);

    PlanNote notes[2];
    memset(notes, 0, sizeof(notes));
    notes[0].midi = 64;
    notes[0].spelled = false;
    assert(rational_make(0, 1, &notes[0].offset) == RATIONAL_OK);
    assert(rational_make(1, 2, &notes[0].duration) == RATIONAL_OK);
    notes[1].midi = 67;
    notes[1].spelled = false;
    assert(rational_make(1, 2, &notes[1].offset) == RATIONAL_OK);
    assert(rational_make(1, 4, &notes[1].duration) == RATIONAL_OK);
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
    cadence.params.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    PlanConstraint motive_ref;
    memset(&motive_ref, 0, sizeof(motive_ref));
    motive_ref.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    motive_ref.params.motive.motive = m1;
    motive_ref.params.motive.has_step = true;
    motive_ref.params.motive.step = (MotiveRelationStep){.kind = MOTIVE_RELATION_TRANSPOSE, .value = {0, -2}};
    PlanConstraint key_constraint = {PLAN_CONSTRAINT_KEY, {0}};
    key_constraint.params.key = test_key(7, "major");
    assert(plan_node_add_constraint(plan, phrase, &length) == PLAN_OK);
    assert(plan_node_add_constraint(plan, phrase, &cadence) == PLAN_OK);
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
    SectionDescription described;
    section_description_init(&described);
    described.has_role = true;
    described.role = SECTION_ROLE_MATERIAL;
    snprintf(described.letter, sizeof(described.letter), "A");
    described.has_pairing = true;
    described.pairing.beginning = SECTION_BEGINNING_COMPOUND_BASIC_IDEA;
    described.pairing.ending = SECTION_ENDING_CADENTIAL;
    described.has_cadence = true;
    described.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    assert(plan_node_set_section(plan, section, &described) == PLAN_OK);
    assert(plan_node_set_section(plan, phrase, &described) == PLAN_ERR_BAD_KIND);
    PlanConstraint shape = {PLAN_CONSTRAINT_HARMONY_SHAPE, {0}};
    shape.params.harmony_shape.kind = PLAN_HARMONY_SHAPE_PHRASE_MODEL;
    shape.params.harmony_shape.starts_on = PLAN_HARMONY_ANCHOR_TONIC;
    shape.params.harmony_shape.ends_on = PLAN_HARMONY_ANCHOR_DOMINANT;
    shape.params.harmony_shape.max_cells_per_chord = 2;
    shape.params.harmony_shape.split_at_bars = true;
    assert(plan_lane_span_add_constraint(plan, harmony_two, &shape) == PLAN_OK);
    shape.params.harmony_shape.max_cells_per_chord = -1;
    assert(plan_lane_span_add_constraint(plan, harmony_two, &shape) == PLAN_ERR_BAD_PAYLOAD);
    described.pairing.ending = (SectionEnding)7;
    assert(plan_node_set_section(plan, section, &described) == PLAN_ERR_BAD_PAYLOAD);
    described.pairing.ending = SECTION_ENDING_CADENTIAL;

    if (tone_labels > 0) {
        PlanConstraint tones;
        memset(&tones, 0, sizeof(tones));
        tones.kind = PLAN_CONSTRAINT_MOTIVE_TONES;
        PlanMotiveTones *body = &tones.params.motive_tones;
        snprintf(body->chords[0].numeral, sizeof(body->chords[0].numeral), "i");
        assert(rational_make(0, 1, &body->chords[0].onset) == RATIONAL_OK && rational_make(1, 2, &body->chords[0].length) == RATIONAL_OK);
        snprintf(body->chords[1].numeral, sizeof(body->chords[1].numeral), "bVII65");
        assert(rational_make(1, 2, &body->chords[1].onset) == RATIONAL_OK && rational_make(1, 2, &body->chords[1].length) == RATIONAL_OK);
        body->chord_count = 2;
        for (size_t k = 0; k < tone_labels; k++) {
            snprintf(body->labels[k], sizeof(body->labels[k]), "%s", k == 1 ? "passing" : "chord");
        }
        body->label_count = tone_labels;
        assert(plan_node_add_constraint(plan, m1, &tones) == PLAN_OK);
    }
    assert(plan_node_set_state(plan, m1, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_node_set_state(plan, repetition, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, meter_span, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, melody_span, PLAN_STATE_REALIZED) == PLAN_OK);
    return plan;
}

static PiecePlan *build_sentence_plan(void) {
    return build_sentence_plan_with_tones(0);
}

/* The sentence plan with the descriptions of its section nodes cleared: what a plan of a format before 5 reads as. */
static PiecePlan *build_plan_without_descriptions(void) {
    PiecePlan *plan = build_sentence_plan();
    SectionDescription empty;
    section_description_init(&empty);
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (plan_has_node(plan, id) && plan_node_kind(plan, id) == PLAN_NODE_SECTION) {
            assert(plan_node_set_section(plan, id, &empty) == PLAN_OK);
        }
    }
    return plan;
}

static bool same_span(PlanSpan a, PlanSpan b) {
    return rational_cmp(a.start, b.start) == 0 && rational_cmp(a.length, b.length) == 0;
}

/* A plan read from a format before 5 has no description to compare: those tests turn the comparison off. */
static bool compare_sections = true;

/* Two section nodes have the same description when it writes the same JSON. */
static bool same_section(const PiecePlan *a, const PiecePlan *b, NodeId id) {
    const SectionDescription *x = plan_node_section(a, id);
    const SectionDescription *y = plan_node_section(b, id);
    if (!compare_sections) {
        return true;
    }
    if (!x || !y) {
        return x == y;
    }
    cJSON *left = section_to_json(x, SECTION_USE_PLAN);
    cJSON *right = section_to_json(y, SECTION_USE_PLAN);
    bool same = left && right && cJSON_Compare(left, right, true);
    cJSON_Delete(left);
    cJSON_Delete(right);
    return same;
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
               a->params.motive.has_step == b->params.motive.has_step &&
               memcmp(&a->params.motive.step, &b->params.motive.step, sizeof(a->params.motive.step)) == 0 &&
               strcmp(a->params.motive.segment, b->params.motive.segment) == 0 && a->params.motive.steps == b->params.motive.steps;
    case PLAN_CONSTRAINT_KEY:
        return a->params.key.tonic_letter == b->params.key.tonic_letter &&
               a->params.key.tonic_accidental == b->params.key.tonic_accidental && a->params.key.material == b->params.key.material &&
               harmony_key_mode(&a->params.key) == harmony_key_mode(&b->params.key);
    case PLAN_CONSTRAINT_EMBELLISH:
        return true;
    case PLAN_CONSTRAINT_SCHEMA:
        return a->params.schema.schema == b->params.schema.schema && a->params.schema.stage == b->params.schema.stage;
    case PLAN_CONSTRAINT_HARMONY_SHAPE:
        return a->params.harmony_shape.kind == b->params.harmony_shape.kind &&
               a->params.harmony_shape.starts_on == b->params.harmony_shape.starts_on &&
               a->params.harmony_shape.ends_on == b->params.harmony_shape.ends_on &&
               a->params.harmony_shape.max_cells_per_chord == b->params.harmony_shape.max_cells_per_chord &&
               a->params.harmony_shape.split_at_bars == b->params.harmony_shape.split_at_bars &&
               a->params.harmony_shape.pedal == b->params.harmony_shape.pedal;
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
    case PLAN_CONSTRAINT_INVENT_MOTIVE:
        return true;
    case PLAN_CONSTRAINT_MOTIVE_TONES: {
        const PlanMotiveTones *x = &a->params.motive_tones;
        const PlanMotiveTones *y = &b->params.motive_tones;
        if (x->chord_count != y->chord_count || x->label_count != y->label_count) {
            return false;
        }
        for (size_t i = 0; i < x->chord_count; i++) {
            if (strcmp(x->chords[i].numeral, y->chords[i].numeral) != 0 ||
                rational_cmp(x->chords[i].onset, y->chords[i].onset) != 0 ||
                rational_cmp(x->chords[i].length, y->chords[i].length) != 0) {
                return false;
            }
        }
        for (size_t i = 0; i < x->label_count; i++) {
            if (strcmp(x->labels[i], y->labels[i]) != 0) {
                return false;
            }
        }
        return true;
    }
    case PLAN_CONSTRAINT_STYLE: {
        const PlanStyle *x = &a->params.style;
        const PlanStyle *y = &b->params.style;
        if (strcmp(x->path, y->path) != 0 || x->weight_numerator != y->weight_numerator ||
            x->weight_denominator != y->weight_denominator || x->without_count != y->without_count ||
            strcmp(x->blend_path, y->blend_path) != 0) {
            return false;
        }
        for (size_t i = 0; i < x->without_count; i++) {
            if (strcmp(x->without[i], y->without[i]) != 0) {
                return false;
            }
        }
        for (int d = 0; d < 4; d++) {
            if (x->blend_numerator[d] != y->blend_numerator[d] || x->blend_denominator[d] != y->blend_denominator[d]) {
                return false;
            }
        }
        return true;
    }
    case PLAN_CONSTRAINT_SOURCE: {
        const PlanSource *x = &a->params.source;
        const PlanSource *y = &b->params.source;
        return strcmp(x->piece, y->piece) == 0 && x->start_measure == y->start_measure &&
               x->start_beat == y->start_beat && x->end_measure == y->end_measure && x->end_beat == y->end_beat &&
               x->score_numerator == y->score_numerator && x->score_denominator == y->score_denominator;
    }
    case PLAN_CONSTRAINT_PICKUP:
        return a->params.pickup.num == b->params.pickup.num && a->params.pickup.den == b->params.pickup.den;
    }
    return false;
}

static bool same_payload(PlanLaneKind lane, const PlanLanePayload *a, const PlanLanePayload *b) {
    switch (lane) {
    case PLAN_LANE_METER:
        return a->meter.beats_per_bar == b->meter.beats_per_bar && a->meter.beat_unit == b->meter.beat_unit &&
               a->meter.bars_per_hyperbar == b->meter.bars_per_hyperbar;
    case PLAN_LANE_KEY:
        return a->key.tonic_letter == b->key.tonic_letter && a->key.tonic_accidental == b->key.tonic_accidental && a->key.material == b->key.material &&
               harmony_key_mode(&a->key) == harmony_key_mode(&b->key);
    case PLAN_LANE_HARMONY:
        return strcmp(a->harmony.roman_numeral, b->harmony.roman_numeral) == 0 &&
               a->harmony.has_function == b->harmony.has_function &&
               (!a->harmony.has_function || a->harmony.function == b->harmony.function);
    case PLAN_LANE_BASS:
    case PLAN_LANE_MELODY:
    case PLAN_LANE_INNER_VOICE:
        if (a->notes.note_count != b->notes.note_count) {
            return false;
        }
        for (size_t i = 0; i < a->notes.note_count; i++) {
            const PlanNote *x = &a->notes.notes[i];
            const PlanNote *y = &b->notes.notes[i];
            if (x->midi != y->midi || rational_cmp(x->offset, y->offset) != 0 ||
                rational_cmp(x->duration, y->duration) != 0 || x->spelled != y->spelled ||
                (x->spelled && (x->letter != y->letter || x->accidental != y->accidental))) {
                return false;
            }
        }
        return true;
    }
    return false;
}

static bool plans_equal(const PiecePlan *a, const PiecePlan *b) {
    if (plan_node_count(a) != plan_node_count(b) || plan_lane_span_count(a) != plan_lane_span_count(b) ||
        strcmp(plan_analyzer_version(a), plan_analyzer_version(b)) != 0) {
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
        if (!same_section(a, b, id)) {
            return false;
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
    assert(strstr(text, "\"format_version\": 5") != NULL);

    PiecePlan *parsed = NULL;
    expect_ok(plan_from_json(test_knowledge(), text, &parsed, &error), &error);
    assert(parsed != NULL);
    assert(plans_equal(plan, parsed));
    assert(plan_node_state(parsed, 6) == PLAN_STATE_GIVEN);
    assert(plan_lane_span_state(parsed, 1) == PLAN_STATE_GIVEN);

    /* Writing the parsed plan gives the same bytes. */
    char *again = plan_to_json(parsed, &error);
    assert(again != NULL);
    assert(strcmp(text, again) == 0);

    /* A Given item stays refusing writes after the round trip. */
    PlanConstraint length = {PLAN_CONSTRAINT_LENGTH_BARS, {0}};
    length.params.bars = 1;
    assert(plan_node_add_constraint(parsed, 6, &length) == PLAN_ERR_GIVEN_LOCKED);

    free(text);
    free(again);
    plan_destroy(plan);
    plan_destroy(parsed);
}

/*
 * The tones of a motive (R-215): the constraint round-trips with the base harmony and the labels, a plan without it is
 * written as it always was, and a label count that is not the note count is refused when the plan is read.
 */
static void test_motive_tones(void) {
    PlanJsonError error;
    PiecePlan *plain = build_sentence_plan();
    char *plain_text = plan_to_json(plain, &error);
    assert(plain_text && strstr(plain_text, "motive_tones") == NULL);

    PiecePlan *plan = build_sentence_plan_with_tones(2);
    char *text = plan_to_json(plan, &error);
    assert(text && strstr(text, "\"kind\": \"motive_tones\"") != NULL && strstr(text, "\"numeral\": \"bVII65\"") != NULL &&
           strstr(text, "\"passing\"") != NULL);
    PiecePlan *parsed = NULL;
    expect_ok(plan_from_json(test_knowledge(), text, &parsed, &error), &error);
    assert(plans_equal(plan, parsed));
    const PlanConstraint *tones = plan_node_constraint_at(parsed, 6, 0);
    assert(tones && tones->kind == PLAN_CONSTRAINT_MOTIVE_TONES && tones->params.motive_tones.label_count == 2 &&
           strcmp(tones->params.motive_tones.labels[1], "passing") == 0 && tones->params.motive_tones.chord_count == 2);
    char *again = plan_to_json(parsed, &error);
    assert(again && strcmp(text, again) == 0);

    /* Three labels for the two notes of the node. */
    PiecePlan *mismatch = build_sentence_plan_with_tones(3);
    char *bad = plan_to_json(mismatch, &error);
    assert(bad);
    PiecePlan *refused = (PiecePlan *)0x1;
    assert(plan_from_json(test_knowledge(), bad, &refused, &error) == PLAN_ERR_FORMAT && refused == NULL);
    assert(strstr(error.message, "motive_tones on 6: 3 labels for 2 notes") != NULL);

    /* A constraint with no chord or no label is not valid. */
    PlanConstraint empty;
    memset(&empty, 0, sizeof(empty));
    empty.kind = PLAN_CONSTRAINT_MOTIVE_TONES;
    PiecePlan *scratch = build_sentence_plan();
    assert(plan_node_add_constraint(scratch, 7, &empty) == PLAN_ERR_BAD_PAYLOAD);

    free(plain_text);
    free(text);
    free(again);
    free(bad);
    plan_destroy(plain);
    plan_destroy(plan);
    plan_destroy(parsed);
    plan_destroy(mismatch);
    plan_destroy(scratch);
}

static char *replace_all(const char *text, const char *from, const char *to);

/*
 * Plan files saved before relation steps write a motive ref as "transform" and the members it uses (docs/plan-format.md).
 * Each old name reads to the step of the table in docs/modules/motive-relations.md, and is written as "step"; an
 * unknown transform or an unparsable step fails the read with its text in the message.
 */
typedef struct {
    const char *members;       /* what the old file had, in place of the step */
    bool has_step;
    MotiveRelationKind kind;
    long value[3];
    long den0;
    const char *text;          /* the step as written now */
} LegacyCase;

static char *replace_all(const char *text, const char *from, const char *to) {
    size_t from_length = strlen(from);
    size_t count = 0;
    for (const char *at = text; (at = strstr(at, from)) != NULL; at += from_length) {
        count++;
    }
    assert(count > 0);
    char *out = malloc(strlen(text) + count * (strlen(to) + 1) + 1);
    assert(out);
    char *write = out;
    const char *at = text;
    for (const char *found; (found = strstr(at, from)) != NULL; at = found + from_length) {
        memcpy(write, at, (size_t)(found - at));
        write += found - at;
        memcpy(write, to, strlen(to));
        write += strlen(to);
    }
    strcpy(write, at);
    return out;
}

static void test_old_transforms(void) {
    static const LegacyCase CASES[] = {
        {"\"transform\": \"none\", \"semitones\": 0", false, MOTIVE_RELATION_EXACT, {0, 0, 0}, 0, NULL},
        {"\"transform\": \"restated\", \"semitones\": 0", true, MOTIVE_RELATION_EXACT, {0, 0, 0}, 0, "exact"},
        {"\"transform\": \"transposed\", \"semitones\": 3", true, MOTIVE_RELATION_TRANSPOSE, {0, 3, 0}, 0, "transpose by=3"},
        {"\"transform\": \"transposed\", \"semitones\": 0, \"tonal\": true, \"steps\": -2", true, MOTIVE_RELATION_TRANSPOSE,
         {1, -2, 0}, 0, "transpose tonal=1 by=-2"},
        {"\"transform\": \"transposed\", \"semitones\": 0", true, MOTIVE_RELATION_TRANSPOSE, {0, 0, 0}, 0, "transpose"},
        {"\"transform\": \"inverted\", \"semitones\": 0", true, MOTIVE_RELATION_INVERT, {-1, 0, 0}, 0, "invert"},
        {"\"transform\": \"inverted\", \"semitones\": 0, \"tonal\": true, \"axis\": 62", true, MOTIVE_RELATION_INVERT,
         {62, 1, 0}, 0, "invert axis=62 tonal=1"},
        {"\"transform\": \"retrograde\", \"semitones\": 0", true, MOTIVE_RELATION_RETROGRADE, {0, 0, 0}, 0, "retrograde"},
        {"\"transform\": \"augmented\", \"semitones\": 0, \"ratio_num\": 3, \"ratio_den\": 2", true, MOTIVE_RELATION_AUGMENT,
         {3, 0, 0}, 2, "augment ratio=3:2"},
        {"\"transform\": \"diminished\", \"semitones\": 0, \"ratio_num\": 1, \"ratio_den\": 2", true,
         MOTIVE_RELATION_DIMINISH, {1, 0, 0}, 2, "diminish ratio=1:2"},
        {"\"transform\": \"fragmented\", \"semitones\": 0, \"position\": \"trailing\", \"length\": 3", true,
         MOTIVE_RELATION_FRAGMENT, {2, 3, 0}, 0, "fragment position=2 length=3"},
        {"\"transform\": \"interval_changed\", \"semitones\": 0, \"interval\": 2, \"amount\": -1, \"keep_contour\": true", true,
         MOTIVE_RELATION_INTERVAL_CHANGE, {2, -1, 1}, 0, "interval-change interval=2 amount=-1 keep-contour=1"},
        {"\"transform\": \"liquidated\", \"semitones\": 0, \"drop\": 2, \"keep_rhythm\": true", true,
         MOTIVE_RELATION_LIQUIDATE, {2, 1, 0}, 0, "liquidate drop=2 keep-rhythm=1"}};
    PiecePlan *plan = build_sentence_plan();
    PlanJsonError error;
    char *base = plan_to_json(plan, &error);
    assert(base && strstr(base, "\"step\": \"transpose by=-2\"") != NULL);
    assert(strstr(base, "\"transform\"") == NULL);

    for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
        const LegacyCase *c = &CASES[i];
        char *old = replace_all(base, "\"step\": \"transpose by=-2\"", c->members);
        PiecePlan *read = NULL;
        expect_ok(plan_from_json(test_knowledge(), old, &read, &error), &error);
        size_t found = 0;
        for (NodeId id = 1; id <= plan_node_count(read); id++) {
            for (size_t k = 0; k < plan_node_constraint_count(read, id); k++) {
                const PlanConstraint *constraint = plan_node_constraint_at(read, id, k);
                if (constraint->kind != PLAN_CONSTRAINT_MOTIVE_REF) {
                    continue;
                }
                const PlanMotiveRef *ref = &constraint->params.motive;
                assert(ref->has_step == c->has_step);
                if (c->has_step) {
                    assert(ref->step.kind == c->kind && ref->step.value[0] == c->value[0] &&
                           ref->step.value[1] == c->value[1] && ref->step.value[2] == c->value[2] &&
                           ref->step.value[3] == 0 && ref->step.den[0] == c->den0);
                }
                found++;
            }
        }
        assert(found > 0);

        /* Written as a step (or not at all), and read back equal. */
        char *written = plan_to_json(read, &error);
        assert(written && strstr(written, "\"transform\"") == NULL);
        if (c->has_step) {
            char expected[200];
            snprintf(expected, sizeof(expected), "\"step\": \"%s\"", c->text);
            assert(strstr(written, expected) != NULL);
        } else {
            assert(strstr(written, "\"step\"") == NULL);
        }
        PiecePlan *again = NULL;
        expect_ok(plan_from_json(test_knowledge(), written, &again, &error), &error);
        assert(plans_equal(read, again));
        free(old);
        free(written);
        plan_destroy(read);
        plan_destroy(again);
    }

    /* A name that is not a transform, and a step that is not a relation step, are refused with their text. */
    char *unknown = replace_all(base, "\"step\": \"transpose by=-2\"", "\"transform\": \"sideways\"");
    PiecePlan *refused = NULL;
    assert(plan_from_json(test_knowledge(), unknown, &refused, &error) == PLAN_ERR_FORMAT);
    assert(refused == NULL && strstr(error.message, "sideways") != NULL);
    free(unknown);
    char *bad_step = replace_all(base, "\"step\": \"transpose by=-2\"", "\"step\": \"transpose sideways=1\"");
    assert(plan_from_json(test_knowledge(), bad_step, &refused, &error) == PLAN_ERR_FORMAT);
    assert(refused == NULL && strstr(error.message, "transpose sideways=1") != NULL);
    free(bad_step);
    free(base);
    plan_destroy(plan);
}

static void test_empty_plan(void) {
    PiecePlan *plan = plan_create();
    PlanJsonError error;
    char *text = plan_to_json(plan, &error);
    assert(text != NULL);
    PiecePlan *parsed = NULL;
    assert(plan_from_json(test_knowledge(), text, &parsed, &error) == PLAN_OK);
    assert(plan_node_count(parsed) == 0);
    assert(plan_lane_span_count(parsed) == 0);
    free(text);
    plan_destroy(plan);
    plan_destroy(parsed);
}

#define SPAN_0_1 "\"span\":{\"start\":{\"num\":0,\"den\":1},\"end\":{\"num\":1,\"den\":1}}"
#define COMPOSER_ID_1 "\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":[]}"
/* A plan of format 1 whose form has one section with these constraints (id 2). */
#define OLD_SECTION(CONSTRAINTS)                                                                                       \
    "{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1                                     \
    ",\"children\":[{\"kind\":\"section\",\"label\":\"A\"," SPAN_0_1                                                  \
    ",\"children\":[],\"composer\":{\"id\":2,\"state\":\"constrained\",\"constraints\":" CONSTRAINTS "}}],"        \
    "\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":[]}}}"

static void expect_error(const char *text, PlanResult expected, const char *expected_path) {
    PiecePlan *plan = (PiecePlan *)0x1;
    PlanJsonError error;
    memset(&error, 0, sizeof(error));
    assert(plan_from_json(test_knowledge(), text, &plan, &error) == expected);
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
    assert(plan_from_json(test_knowledge(), NULL, &plan, &error) == PLAN_ERR_NULL);

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
    /* The attributes of a section (formats 1 to 4 wrote them as constraints) are read into its description; the form node is not a
       section, so it has none of them. */
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":"
                 "[{\"kind\":\"theme_pairing\",\"beginning\":\"presentation\",\"ending\":\"continuation\"}]}}}",
                 PLAN_ERR_FORMAT, "form.composer.constraints[0].kind");
    expect_error(OLD_SECTION("[{\"kind\":\"theme_pairing\",\"beginning\":\"presentation\"}]"), PLAN_ERR_FORMAT,
                 "form.children[0].composer.constraints[0].ending");
    {
        /* A pairing and a shape with their defaults leave out what is not asked for. */
        PiecePlan *parsed_pairing = NULL;
        assert(plan_from_json(test_knowledge(),
                              OLD_SECTION("[{\"kind\":\"theme_pairing\",\"beginning\":\"antecedent\",\"ending\":\"consequent\"},"
                                          "{\"kind\":\"harmony_shape\",\"shape\":\"cadential\"},{\"kind\":\"material\"},"
                                          "{\"kind\":\"archetype\",\"type\":\"period\"}]"),
                              &parsed_pairing, &error) == PLAN_OK);
        const SectionDescription *described = plan_node_section(parsed_pairing, 2);
        assert(described && described->has_pairing && described->pairing.beginning == SECTION_BEGINNING_ANTECEDENT &&
               described->pairing.ending == SECTION_ENDING_CONSEQUENT && described->has_role &&
               described->role == SECTION_ROLE_MATERIAL);
        const PlanConstraint *read = plan_node_constraint_at(parsed_pairing, 2, 0);
        assert(plan_node_constraint_count(parsed_pairing, 2) == 1 && read &&
               read->params.harmony_shape.kind == PLAN_HARMONY_SHAPE_CADENTIAL &&
               read->params.harmony_shape.starts_on == PLAN_HARMONY_ANCHOR_ANY &&
               read->params.harmony_shape.max_cells_per_chord == 0 && !read->params.harmony_shape.split_at_bars);
        plan_destroy(parsed_pairing);
    }
    /* The constraints of a piece plan. */
    expect_error(OLD_SECTION("[{\"kind\":\"return\",\"section\":1,\"variation\":\"remix\"}]"), PLAN_ERR_FORMAT,
                 "form.children[0].composer.constraints[0].variation");
    expect_error(OLD_SECTION("[{\"kind\":\"return\",\"section\":1,\"variation\":\"exact\"}]"), PLAN_ERR_FORMAT,
                 "form.children[0].composer.constraints[0]");
    expect_error(OLD_SECTION("[{\"kind\":\"layers\",\"lanes\":[\"bass\",\"drums\"]}]"), PLAN_ERR_FORMAT,
                 "form.children[0].composer.constraints[0].lanes[1]");
    expect_error(OLD_SECTION("[{\"kind\":\"layers\",\"lanes\":[]}]"), PLAN_ERR_FORMAT,
                 "form.children[0].composer.constraints[0].lanes");
    expect_error(OLD_SECTION("[{\"kind\":\"section_letter\",\"letter\":\"ABCD\"}]"), PLAN_ERR_FORMAT,
                 "form.children[0].composer.constraints[0].letter");
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
    assert(plan_from_json(test_knowledge(), "{\"schema_version\":1,\"extra\":[1,2],\"form\":null}", &parsed, &error) == PLAN_OK);
    plan_destroy(parsed);

}

/* ------------------------------------------------------------ format versions 1 and 2 */

/* The MIDI file of the plan, or NULL when it cannot be rendered (a plan with unwritten lanes). */
static unsigned char *midi_of(const PiecePlan *plan, size_t *size) {
    RenderedPlan rendered;
    unsigned char *data = NULL;
    if (plan_export_render(plan, &rendered, NULL) != PLAN_EXPORT_OK) {
        return NULL;
    }
    PlanExportOptions options = plan_export_options_default();
    PlanExportResult exported = rendered_plan_midi_bytes(test_knowledge(), &rendered, &options, &data, size, NULL);
    rendered_plan_destroy(&rendered);
    assert(exported == PLAN_EXPORT_OK);
    return data;
}

/*
 * A plan file of any version reads, writes as version 4, reads back equal and renders the same MIDI. The version 4 text
 * is kept as plan_written_<name> for the schema test.
 */
static void check_plan_file(const char *path, const char *name) {
    char *text = test_must_read_file(path, NULL);
    PiecePlan *first = NULL;
    PiecePlan *second = NULL;
    PlanJsonError error;
    assert(plan_from_json(test_knowledge(), text, &first, &error) == PLAN_OK);
    char *written = plan_to_json(first, &error);
    assert(written != NULL);
    assert(strstr(written, "\"format\": \"ursatz-composer-plan\"") != NULL);
    assert(strstr(written, "\"format_version\": 5") != NULL);
    assert(strstr(written, "schema_version") == NULL && strstr(written, "tonic_pitch_class") == NULL);
    assert(strstr(written, "\"ticks_per_quarter\"") == NULL && strstr(written, "\"voice_leading\"") == NULL);
    assert(plan_from_json(test_knowledge(), written, &second, &error) == PLAN_OK);
    assert(plans_equal(first, second));
    char *again = plan_to_json(second, &error);
    assert(again != NULL && strcmp(written, again) == 0);

    size_t size_first = 0;
    size_t size_second = 0;
    unsigned char *midi_first = midi_of(first, &size_first);
    unsigned char *midi_second = midi_of(second, &size_second);
    assert((midi_first == NULL) == (midi_second == NULL));
    if (midi_first) {
        assert(size_first == size_second && memcmp(midi_first, midi_second, size_first) == 0);
    }
    char file_name[256];
    snprintf(file_name, sizeof(file_name), "plan_written_%s", name);
    test_write_text(file_name, written);

    free(midi_first);
    free(midi_second);
    free(again);
    free(written);
    free(text);
    plan_destroy(first);
    plan_destroy(second);
}

static void test_plan_files(void) {
    /* The plans saved before version 2. */
    static const char *const FIXTURES[] = {"sentence_c_major.json", "sentence_c_to_g.json", "period_a_minor.json"};
    for (size_t i = 0; i < sizeof(FIXTURES) / sizeof(FIXTURES[0]); i++) {
        char path[512];
        snprintf(path, sizeof(path), COMPOSER_FIXTURE_DIR "/%s", FIXTURES[i]);
        check_plan_file(path, FIXTURES[i]);
    }
    /* The plans the command line writes (the snapshots), whatever version they are in. */
    static const char *const SNAPSHOTS[] = {"grow-period", "grow-sentence", "piece-loop-and-ending",
                                            "piece-progression-fill", "piece-regenerate", "piece-theme-fill",
                                            "theme-chords", "theme-fast-minor", "theme-one-motive",
                                            "theme-period-contrast"};
    for (size_t i = 0; i < sizeof(SNAPSHOTS) / sizeof(SNAPSHOTS[0]); i++) {
        char path[512];
        char name[128];
        snprintf(path, sizeof(path), COMPOSER_SNAPSHOT_DIR "/%s.plan.json", SNAPSHOTS[i]);
        snprintf(name, sizeof(name), "%s.plan.json", SNAPSHOTS[i]);
        check_plan_file(path, name);
    }
}

/* A version 4 text turned into the version 1 text the same plan had, member by member. */
static char *as_version_one(const char *v4) {
    char *v3 = test_plan_text_as_format_three(v4);
    assert(v3 != NULL);
    char *step = replace_all(v3, "\"format\": \"ursatz-composer-plan\"", "\"schema_version\": 1");
    free(v3);
    char *next = replace_all(step, "\"format_version\": 3", "\"ticks\": null");
    free(step);
    step = replace_all(next, "\"material\": \"omt.material.major\"", "\"mode\": \"major\"");
    free(next);
    next = replace_all(step, "\"material\": \"omt.material.natural-minor\"", "\"mode\": \"minor\"");
    free(step);
    step = replace_all(next, "\"tonic\": \"D#\"", "\"tonic_pitch_class\": 3");
    free(next);
    next = replace_all(step, "\"tonic\": \"G\"", "\"tonic_pitch_class\": 7");
    free(step);
    return next;
}

static void test_version_one_values(void) {
    compare_sections = false;
    PiecePlan *plan = build_plan_without_descriptions();
    PlanJsonError error;
    char *v2 = plan_to_json(plan, &error);
    assert(v2 != NULL);
    assert(strstr(v2, "\"tonic\": \"D#\"") != NULL && strstr(v2, "\"tonic\": \"G\"") != NULL);
    assert(strstr(v2, "\"composer_version\"") != NULL && strstr(v2, "\"analyzer_version\"") == NULL);

    /* Version 1: tonic pitch classes are spelled as the library spells their keys; a version 1 analyzer_version was the
       Composer's own version and is ignored. */
    char *v1 = as_version_one(v2);
    char *with_analyzer = replace_all(v1, "\"composer_version\"", "\"analyzer_version\"");
    PiecePlan *old = NULL;
    assert(plan_from_json(test_knowledge(), with_analyzer, &old, &error) == PLAN_OK);
    assert(plans_equal(plan, old));
    assert(strcmp(plan_analyzer_version(old), "") == 0);
    plan_destroy(old);

    /* Version 1 wrote a chord with no function as "unknown", and always wrote the member. */
    char *unknown = replace_all(v1, "\"function\": \"tonic\"", "\"function\": \"unknown\"");
    assert(plan_from_json(test_knowledge(), unknown, &old, &error) == PLAN_OK);
    plan_destroy(old);
    free(unknown);
    char *no_function = replace_all(v1, "\"function\": \"tonic\"", "\"fn\": \"tonic\"");
    assert(plan_from_json(test_knowledge(), no_function, &old, &error) == PLAN_ERR_FORMAT && strstr(error.path, "function") != NULL);
    free(no_function);

    /* Version 2: the function is absent for none, a tonic keeps its spelling, an Analyzer version is kept. */
    char *flat = replace_all(v2, "\"tonic\": \"G\"", "\"tonic\": \"Bb\"");
    assert(plan_from_json(test_knowledge(), flat, &old, &error) == PLAN_OK);
    assert(!plans_equal(plan, old));
    plan_destroy(old);
    free(flat);
    char *analyzed = replace_all(v2, "\"composer_version\"", "\"analyzer_version\": \"9.8.7\", \"composer_version\"");
    assert(plan_from_json(test_knowledge(), analyzed, &old, &error) == PLAN_OK);
    assert(strcmp(plan_analyzer_version(old), "9.8.7") == 0);
    char *kept = plan_to_json(old, &error);
    assert(kept != NULL && strstr(kept, "\"analyzer_version\": \"9.8.7\"") != NULL);
    free(kept);
    plan_destroy(old);
    free(analyzed);

    free(with_analyzer);
    free(v1);
    free(v2);
    plan_destroy(plan);
    compare_sections = true;
}

static void test_spelling_round_trip(void) {
    PiecePlan *plan = plan_create();
    NodeId form = add(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 1), "form");
    PlanLanePayload key;
    memset(&key, 0, sizeof(key));
    key.key.tonic_letter = PITCH_LETTER_F;
    key.key.tonic_accidental = 1;
    key.key.material = test_materials()->major;
    add_lane(plan, form, PLAN_LANE_KEY, 0, bars(0, 1), &key);
    Rational quarter;
    assert(rational_make(1, 4, &quarter) == RATIONAL_OK);
    PlanNote notes[2] = {plan_note(66, (Rational){0, 1}, quarter), plan_note(65, quarter, quarter)};
    notes[1].spelled = true;
    notes[1].letter = PITCH_LETTER_E;
    notes[1].accidental = 1;
    PlanLanePayload melody;
    memset(&melody, 0, sizeof(melody));
    melody.notes.notes = notes;
    melody.notes.note_count = 2;
    add_lane(plan, form, PLAN_LANE_MELODY, 0, bars(0, 1), &melody);

    PlanJsonError error;
    char *text = plan_to_json(plan, &error);
    assert(text != NULL);
    assert(strstr(text, "\"tonic\": \"F#\"") != NULL && strstr(text, "\"spelling\": \"E#\"") != NULL);
    PiecePlan *parsed = NULL;
    assert(plan_from_json(test_knowledge(), text, &parsed, &error) == PLAN_OK);
    const PlanLanePayload *read_key = plan_lane_span_payload(parsed, 1);
    assert(read_key->key.tonic_letter == PITCH_LETTER_F && read_key->key.tonic_accidental == 1);
    const PlanLanePayload *read_melody = plan_lane_span_payload(parsed, 2);
    assert(!read_melody->notes.notes[0].spelled && read_melody->notes.notes[1].spelled);
    assert(read_melody->notes.notes[1].letter == PITCH_LETTER_E && read_melody->notes.notes[1].accidental == 1);
    assert(plans_equal(plan, parsed));
    plan_destroy(parsed);

    char *bad_tonic = replace_all(text, "\"tonic\": \"F#\"", "\"tonic\": \"H\"");
    assert(plan_from_json(test_knowledge(), bad_tonic, &parsed, &error) == PLAN_ERR_FORMAT && strstr(error.path, "tonic") != NULL);
    free(bad_tonic);
    char *bad_spelling = replace_all(text, "\"spelling\": \"E#\"", "\"spelling\": \"E#4\"");
    assert(plan_from_json(test_knowledge(), bad_spelling, &parsed, &error) == PLAN_ERR_FORMAT && strstr(error.path, "spelling") != NULL);
    free(bad_spelling);
    free(text);
    plan_destroy(plan);
}

static void test_format_errors(void) {
    PiecePlan *plan = NULL;
    PlanJsonError error;
    assert(plan_from_json(test_knowledge(), "{\"format\":\"other\",\"format_version\":2,\"form\":null}", &plan, &error) == PLAN_ERR_FORMAT);
    assert(strcmp(error.path, "format") == 0);
    assert(plan_from_json(test_knowledge(), "{\"format\":\"ursatz-composer-plan\",\"form\":null}", &plan, &error) == PLAN_ERR_FORMAT);
    assert(strcmp(error.path, "format_version") == 0);
    assert(plan_from_json(test_knowledge(), "{\"format\":\"ursatz-composer-plan\",\"format_version\":6,\"form\":null}", &plan, &error) ==
           PLAN_ERR_FORMAT);
    assert(strcmp(error.path, "format_version") == 0);
    assert(plan_from_json(test_knowledge(), "{\"format\":\"ursatz-composer-plan\",\"format_version\":2,\"form\":null}", &plan, &error) == PLAN_OK);
    plan_destroy(plan);
    assert(plan_from_json(test_knowledge(), "{\"format\":\"ursatz-composer-plan\",\"format_version\":2,\"metadata\":{\"analyzer_version\":4},"
                          "\"form\":null}", &plan, &error) == PLAN_ERR_FORMAT);
    assert(strcmp(error.path, "metadata.analyzer_version") == 0);
}

/* A version 4 text turned into the version 2 text the same plan had: keys name a mode, major or minor, where they name a material. */
static char *as_version_two(const char *v4) {
    char *v3 = test_plan_text_as_format_three(v4);
    assert(v3 != NULL);
    char *step = replace_all(v3, "\"format_version\": 3", "\"format_version\": 2");
    free(v3);
    char *next = replace_all(step, "\"material\": \"omt.material.major\"", "\"mode\": \"major\"");
    free(step);
    step = replace_all(next, "\"material\": \"omt.material.natural-minor\"", "\"mode\": \"minor\"");
    free(next);
    return step;
}

/* Formats 1 and 2 read into materials: the major and the natural minor of the snapshot, the same keys the format 3 text holds. */
static void test_older_formats_read_into_materials(void) {
    compare_sections = false;
    PiecePlan *plan = build_plan_without_descriptions();
    PlanJsonError error;
    char *v3 = plan_to_json(plan, &error);
    assert(v3 != NULL && strstr(v3, "\"mode\"") == NULL && strstr(v3, "omt.material.") != NULL);
    char *v2 = as_version_two(v3);
    char *v1 = as_version_one(v3);
    assert(strstr(v2, "omt.material.") == NULL && strstr(v2, "\"mode\"") != NULL);
    const PitchMaterial *minor = knowledge_snapshot_material(test_knowledge(), "omt.material.natural-minor");
    assert(minor);
    char *texts[2] = {v2, v1};
    for (size_t i = 0; i < 2; i++) {
        PiecePlan *old = NULL;
        assert(plan_from_json(test_knowledge(), texts[i], &old, &error) == PLAN_OK);
        assert(plans_equal(plan, old));
        char *again = plan_to_json(old, &error);
        assert(again != NULL && strcmp(again, v3) == 0);
        free(again);
        plan_destroy(old);
    }
    /* a minor key reads as the natural minor, and a mode that is neither word is refused at its path */
    char *minor_text = replace_all(v2, "\"mode\": \"major\"", "\"mode\": \"minor\"");
    PiecePlan *read = NULL;
    assert(plan_from_json(test_knowledge(), minor_text, &read, &error) == PLAN_OK);
    LaneSpanId key_span;
    assert(plan_lane_find(read, PLAN_LANE_KEY, 0, plan_node_span(read, plan_root(read)), &key_span, 1) == 1);
    assert(plan_lane_span_payload(read, key_span)->key.material == minor);
    plan_destroy(read);
    free(minor_text);
    char *dorian = replace_all(v2, "\"mode\": \"major\"", "\"mode\": \"dorian\"");
    assert(plan_from_json(test_knowledge(), dorian, &read, &error) == PLAN_ERR_FORMAT && strstr(error.path, "mode") != NULL);
    free(dorian);
    free(v1);
    free(v2);
    free(v3);
    plan_destroy(plan);
    compare_sections = true;
}

/* A plan with the key spans given, over 4 bars each: a span is a key, or a relation and an anchor not yet resolved (key.material NULL). */
typedef struct {
    int tonic;
    const char *material; /* "major" or "minor"; NULL for a span named only by its relation */
    const char *relation; /* NULL for none */
    PlanKeyAnchor anchor;
    bool resolved;
} KeySpanCase;

static PiecePlan *key_spans_plan(const KeySpanCase *cases, size_t count) {
    PiecePlan *plan = plan_create();
    assert(plan != NULL);
    NodeId form = add(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, (long)(4 * count)), "form");
    for (size_t i = 0; i < count; i++) {
        PlanLanePayload payload;
        memset(&payload, 0, sizeof(payload));
        if (cases[i].material) {
            payload.key = test_key(cases[i].tonic, cases[i].material);
        }
        LaneSpanId id = add_lane(plan, form, PLAN_LANE_KEY, 0, bars((long)(4 * i), 4), &payload);
        if (cases[i].relation) {
            assert(plan_lane_span_set_key_relation(plan, id, cases[i].relation, cases[i].anchor, cases[i].resolved) == PLAN_OK);
        }
    }
    return plan;
}

/* The key of span `index` of the key lane, in time order. */
static AnalysisKey key_span_at(const PiecePlan *plan, size_t index) {
    LaneSpanId ids[8];
    PlanSpan everything = bars(0, 1000);
    assert(plan_lane_find(plan, PLAN_LANE_KEY, 0, everything, ids, 8) > index);
    return plan_lane_span_payload(plan, ids[index])->key;
}

/* Reads the text and resolves it, returning the result; *out is the plan read. */
static PlanResult read_resolved(const char *text, PiecePlan **out, PlanJsonError *error) {
    assert(plan_from_json(test_knowledge(), text, out, error) == PLAN_OK);
    return plan_resolve_key_spans(*out, test_knowledge(), error);
}

/* A key span named only by a relation and an anchor is written as that, read back unresolved and resolved against its anchor. */
static void test_relation_named_spans(void) {
    PlanJsonError error;
    const KeySpanCase named[] = {{0, "major", NULL, PLAN_KEY_ANCHOR_HOME, false},
                                 {0, NULL, "omt.relation.dominant", PLAN_KEY_ANCHOR_HOME, false}};
    PiecePlan *plan = key_spans_plan(named, 2);
    char *text = plan_to_json(plan, &error);
    assert(text != NULL && strstr(text, "\"relation\": \"omt.relation.dominant\"") != NULL &&
           strstr(text, "\"anchor\": \"home\"") != NULL);
    PiecePlan *read = NULL;
    assert(plan_from_json(test_knowledge(), text, &read, &error) == PLAN_OK);
    const PlanKeyRelation *relation = plan_lane_span_key_relation(read, 2);
    assert(relation && !relation->resolved && relation->anchor == PLAN_KEY_ANCHOR_HOME &&
           strcmp(relation->relation, "omt.relation.dominant") == 0);
    char *unchanged = plan_to_json(read, &error);
    assert(unchanged != NULL && strcmp(unchanged, text) == 0);
    free(unchanged);
    plan_destroy(read);

    /* resolved, it is the plan of the key the relation reaches, with the relation beside it */
    const KeySpanCase explicit_keys[] = {{0, "major", NULL, PLAN_KEY_ANCHOR_HOME, false},
                                         {7, "major", "omt.relation.dominant", PLAN_KEY_ANCHOR_HOME, true}};
    PiecePlan *explicit_plan = key_spans_plan(explicit_keys, 2);
    char *explicit_text = plan_to_json(explicit_plan, &error);
    assert(explicit_text != NULL);
    assert(read_resolved(text, &read, &error) == PLAN_OK);
    char *resolved_text = plan_to_json(read, &error);
    assert(resolved_text != NULL && strcmp(resolved_text, explicit_text) == 0);
    plan_destroy(read);
    /* a plan with every key named is untouched, and resolving twice changes nothing */
    assert(plan_from_json_resolved(test_knowledge(), explicit_text, &read, &error) == PLAN_OK);
    assert(plan_resolve_key_spans(read, test_knowledge(), &error) == PLAN_OK);
    char *again = plan_to_json(read, &error);
    assert(again != NULL && strcmp(again, explicit_text) == 0);
    plan_destroy(read);
    free(again);
    free(resolved_text);
    free(explicit_text);
    plan_destroy(explicit_plan);
    free(text);
    plan_destroy(plan);
}

/* The anchor "previous" is the span before; "home" is the first span. */
static void test_relation_anchors(void) {
    PlanJsonError error;
    const KeySpanCase chain[] = {{0, "major", NULL, PLAN_KEY_ANCHOR_HOME, false},
                                 {0, NULL, "omt.relation.dominant", PLAN_KEY_ANCHOR_PREVIOUS, false},
                                 {0, NULL, "omt.relation.dominant", PLAN_KEY_ANCHOR_PREVIOUS, false},
                                 {0, NULL, "omt.relation.dominant", PLAN_KEY_ANCHOR_HOME, false}};
    PiecePlan *plan = key_spans_plan(chain, 4);
    char *text = plan_to_json(plan, &error);
    PiecePlan *read = NULL;
    assert(text != NULL && read_resolved(text, &read, &error) == PLAN_OK);
    AnalysisKey second = key_span_at(read, 1);
    AnalysisKey third = key_span_at(read, 2);
    AnalysisKey fourth = key_span_at(read, 3);
    assert(analysis_key_tonic_pitch_class(&second) == 7 && analysis_key_tonic_pitch_class(&third) == 2 &&
           analysis_key_tonic_pitch_class(&fourth) == 7);
    assert(second.material == third.material && harmony_key_mode(&second) == HARMONY_MODE_MAJOR);
    plan_destroy(read);
    free(text);
    plan_destroy(plan);
}

/* A relation that cannot be resolved is refused with its message at its span, and the plan stays as it was. */
static void test_relation_refusals(void) {
    PlanJsonError error;
    struct {
        KeySpanCase first;
        const char *relation;
        PlanKeyAnchor anchor;
        const char *message;
    } cases[] = {
        {{0, "major", NULL, PLAN_KEY_ANCHOR_HOME, false}, "omt.relation.subtonic", PLAN_KEY_ANCHOR_HOME,
         "relation 'subtonic' from C major: the triad on degree 7 is not major or minor"},
        {{0, "major", NULL, PLAN_KEY_ANCHOR_HOME, false}, "omt.relation.no-such-relation", PLAN_KEY_ANCHOR_HOME, "no relation has the id"},
        {{0, "minor", NULL, PLAN_KEY_ANCHOR_HOME, false}, "omt.relation.two-steps-brighter", PLAN_KEY_ANCHOR_PREVIOUS,
         "composes in"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const KeySpanCase spans[] = {cases[i].first, {0, NULL, cases[i].relation, cases[i].anchor, false}};
        PiecePlan *plan = key_spans_plan(spans, 2);
        char *text = plan_to_json(plan, &error);
        PiecePlan *read = NULL;
        assert(text != NULL && read_resolved(text, &read, &error) == PLAN_ERR_FORMAT);
        assert(strstr(error.message, cases[i].message) != NULL && strcmp(error.path, "keys[1].composer.payload.relation") == 0);
        const PlanKeyRelation *relation = plan_lane_span_key_relation(read, 2);
        assert(relation && !relation->resolved);
        plan_destroy(read);
        assert(plan_from_json_resolved(test_knowledge(), text, &read, &error) == PLAN_ERR_FORMAT && read == NULL);
        free(text);
        plan_destroy(plan);
    }
    /* the first span has no anchor to be taken from */
    const KeySpanCase first[] = {{0, NULL, "omt.relation.dominant", PLAN_KEY_ANCHOR_HOME, false}};
    PiecePlan *plan = key_spans_plan(first, 1);
    char *text = plan_to_json(plan, &error);
    PiecePlan *read = NULL;
    assert(text != NULL && read_resolved(text, &read, &error) == PLAN_ERR_FORMAT);
    assert(strcmp(error.path, "keys[0].composer.payload.anchor") == 0);
    plan_destroy(read);
    free(text);
    plan_destroy(plan);
    /* a relation member without its anchor is malformed */
    PiecePlan *half = NULL;
    assert(plan_from_json(test_knowledge(),
                          "{\"format\":\"ursatz-composer-plan\",\"format_version\":3,\"form\":null,\"keys\":[{\"span\":{\"start\":{\"num\":0,\"den\":1},"
                          "\"end\":{\"num\":4,\"den\":1}},\"composer\":{\"id\":1,\"owner\":1,\"lane\":\"key\",\"voice\":0,\"state\":\"given\","
                          "\"constraints\":[],\"payload\":{\"relation\":\"omt.relation.dominant\"}}}]}",
                          &half, &error) == PLAN_ERR_FORMAT);
}

int main(void) {
    test_round_trip();
    test_old_transforms();
    test_motive_tones();
    test_empty_plan();
    test_malformed_input();
    test_plan_files();
    test_version_one_values();
    test_older_formats_read_into_materials();
    test_relation_named_spans();
    test_relation_anchors();
    test_relation_refusals();
    test_spelling_round_trip();
    test_format_errors();
    return 0;
}
