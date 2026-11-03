#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/midi_pipeline.h>
#include <ursatz_analyzer/version.h>
#include <ursatz_composer/analysis_load.h>
#include <galant-schemas/galant_prototypes.h>

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
    key.key = plan_key_of_pitch_class(3, KEY_STRUCTURE_MODE_MINOR);
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
    PlanConstraint archetype = {PLAN_CONSTRAINT_ARCHETYPE, {0}};
    archetype.params.archetype = PLAN_ARCHETYPE_SENTENCE;
    PlanConstraint motive_ref = {PLAN_CONSTRAINT_MOTIVE_REF, {0}};
    motive_ref.params.motive.motive = m1;
    motive_ref.params.motive.has_step = true;
    motive_ref.params.motive.step = (MotiveRelationStep){.kind = MOTIVE_RELATION_TRANSPOSE, .value = {0, -2}};
    PlanConstraint key_constraint = {PLAN_CONSTRAINT_KEY, {0}};
    key_constraint.params.key = plan_key_of_pitch_class(7, KEY_STRUCTURE_MODE_MAJOR);
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

static bool same_span(PlanSpan a, PlanSpan b) {
    return rational_cmp(a.start, b.start) == 0 && rational_cmp(a.length, b.length) == 0;
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
               memcmp(&a->params.motive.step, &b->params.motive.step, sizeof(a->params.motive.step)) == 0;
    case PLAN_CONSTRAINT_KEY:
        return analysis_key_tonic_pitch_class(&a->params.key) == analysis_key_tonic_pitch_class(&b->params.key) &&
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
    case PLAN_CONSTRAINT_RETURN:
        return a->params.section_return.section == b->params.section_return.section &&
               a->params.section_return.variation == b->params.section_return.variation;
    case PLAN_CONSTRAINT_LAYERS:
        return a->params.layers.lanes == b->params.layers.lanes;
    case PLAN_CONSTRAINT_SECTION_LETTER:
        return strcmp(a->params.letter, b->params.letter) == 0;
    case PLAN_CONSTRAINT_INVENT_MOTIVE:
        return true;
    case PLAN_CONSTRAINT_PIECE_PART:
        return a->params.piece_part.part == b->params.piece_part.part &&
               strcmp(a->params.piece_part.id, b->params.piece_part.id) == 0;
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
    case PLAN_CONSTRAINT_FREE_PHRASE:
    case PLAN_CONSTRAINT_HOLD:
    case PLAN_CONSTRAINT_SEAM:
        return true;
    default:
        break;
    }
    return false;
}

static bool same_payload(PlanLaneKind lane, const PlanLanePayload *a, const PlanLanePayload *b) {
    switch (lane) {
    case PLAN_LANE_METER:
        return a->meter.beats_per_bar == b->meter.beats_per_bar && a->meter.beat_unit == b->meter.beat_unit &&
               a->meter.bars_per_hyperbar == b->meter.bars_per_hyperbar;
    case PLAN_LANE_KEY:
        return analysis_key_tonic_pitch_class(&a->key) == analysis_key_tonic_pitch_class(&b->key) && a->key.mode == b->key.mode;
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
            if (x->midi != y->midi || rational_cmp(x->offset, y->offset) != 0 ||
                rational_cmp(x->duration, y->duration) != 0) {
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
    assert(strstr(text, "\"format_version\": 2") != NULL);

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
    expect_ok(plan_from_json(text, &parsed, &error), &error);
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
    assert(plan_from_json(bad, &refused, &error) == PLAN_ERR_FORMAT && refused == NULL);
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

/*
 * The constraints of a piece plan: a section's letter, the section it returns and how, the lanes a layer sounds, and a
 * motive to invent. They round-trip, and malformed ones are refused.
 */
static void test_section_constraints(void) {
    PiecePlan *plan = plan_create();
    NodeId form = add(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 13), "rounded-binary");
    NodeId a = add(plan, form, PLAN_NODE_SECTION, bars(0, 4), "A");
    NodeId b = add(plan, form, PLAN_NODE_SECTION, bars(4, 4), "B");
    NodeId a2 = add(plan, form, PLAN_NODE_SECTION, bars(8, 4), "A'");
    NodeId phrase = add(plan, a, PLAN_NODE_PHRASE, bars(0, 4), "phrase");
    NodeId material = add(plan, form, PLAN_NODE_SECTION, bars(12, 1), "material");
    NodeId material_phrase = add(plan, material, PLAN_NODE_PHRASE, bars(12, 1), "material");
    NodeId material_idea = add(plan, material_phrase, PLAN_NODE_IDEA, bars(12, 1), "material");
    NodeId m0 = add(plan, material_idea, PLAN_NODE_MOTIVE, bars(12, 1), "m0");
    PlanNote placeholder = plan_note(60, rational_ratio(0, 1), rational_ratio(1, 1));
    PlanLanePayload melody;
    memset(&melody, 0, sizeof(melody));
    melody.notes.notes = &placeholder;
    melody.notes.note_count = 1;
    LaneSpanId open = add_lane(plan, m0, PLAN_LANE_MELODY, 0, bars(12, 1), &melody);

    static const char *const LETTERS[] = {"A", "B", "A'"};
    NodeId sections[] = {a, b, a2};
    for (size_t i = 0; i < 3; i++) {
        PlanConstraint letter = {PLAN_CONSTRAINT_SECTION_LETTER, {0}};
        strcpy(letter.params.letter, LETTERS[i]);
        assert(plan_node_add_constraint(plan, sections[i], &letter) == PLAN_OK);
    }
    PlanConstraint back = {PLAN_CONSTRAINT_RETURN, {0}};
    back.params.section_return.section = a;
    back.params.section_return.variation = PLAN_RETURN_ORNAMENT;
    assert(plan_node_add_constraint(plan, a2, &back) == PLAN_OK);
    PlanConstraint layers = {PLAN_CONSTRAINT_LAYERS, {0}};
    layers.params.layers.lanes = PLAN_LAYER_BIT(PLAN_LANE_BASS) | PLAN_LAYER_BIT(PLAN_LANE_INNER_VOICE);
    assert(plan_node_add_constraint(plan, b, &layers) == PLAN_OK);
    PlanConstraint invent = {PLAN_CONSTRAINT_INVENT_MOTIVE, {0}};
    assert(plan_lane_span_add_constraint(plan, open, &invent) == PLAN_OK);
    /* A piece structure's sections name their part and id (R-169), and a free phrase is marked (R-170). */
    static const int PARTS[] = {1, 1, 2};
    static const char *const IDS[] = {"a", "b", "turn"};
    for (size_t i = 0; i < 3; i++) {
        PlanConstraint part = {PLAN_CONSTRAINT_PIECE_PART, {0}};
        part.params.piece_part.part = PARTS[i];
        strcpy(part.params.piece_part.id, IDS[i]);
        assert(plan_node_add_constraint(plan, sections[i], &part) == PLAN_OK);
    }
    PlanConstraint free_phrase = {PLAN_CONSTRAINT_FREE_PHRASE, {0}};
    assert(plan_node_add_constraint(plan, b, &free_phrase) == PLAN_OK);
    PlanConstraint bad_part = {PLAN_CONSTRAINT_PIECE_PART, {0}};
    assert(plan_node_add_constraint(plan, a, &bad_part) == PLAN_ERR_BAD_PAYLOAD); /* no id */
    strcpy(bad_part.params.piece_part.id, "x");
    bad_part.params.piece_part.part = PLAN_PIECE_PART_COUNT;
    assert(plan_node_add_constraint(plan, a, &bad_part) == PLAN_ERR_BAD_PAYLOAD);
    bad_part.params.piece_part.part = -1;
    assert(plan_node_add_constraint(plan, a, &bad_part) == PLAN_ERR_BAD_PAYLOAD);
    memset(bad_part.params.piece_part.id, 'x', PLAN_PIECE_ID_MAX); /* not terminated */
    bad_part.params.piece_part.part = 0;
    assert(plan_node_add_constraint(plan, a, &bad_part) == PLAN_ERR_BAD_PAYLOAD);

    /* A return names a section; a layer sounds a lane there is; a letter is one to three characters. */
    back.params.section_return.section = phrase;
    assert(plan_node_add_constraint(plan, a2, &back) == PLAN_ERR_BAD_PAYLOAD);
    back.params.section_return.section = 99;
    assert(plan_node_add_constraint(plan, a2, &back) == PLAN_ERR_BAD_PAYLOAD);
    back.params.section_return.section = a;
    back.params.section_return.variation = (PlanReturnVariation)9;
    assert(plan_node_add_constraint(plan, a2, &back) == PLAN_ERR_BAD_PAYLOAD);
    layers.params.layers.lanes = 0;
    assert(plan_node_add_constraint(plan, b, &layers) == PLAN_ERR_BAD_PAYLOAD);
    layers.params.layers.lanes = 1u << 9;
    assert(plan_node_add_constraint(plan, b, &layers) == PLAN_ERR_BAD_PAYLOAD);
    PlanConstraint letter = {PLAN_CONSTRAINT_SECTION_LETTER, {0}};
    assert(plan_node_add_constraint(plan, a, &letter) == PLAN_ERR_BAD_PAYLOAD);
    memcpy(letter.params.letter, "ABCD", PLAN_SECTION_LETTER_MAX);
    assert(plan_node_add_constraint(plan, a, &letter) == PLAN_ERR_BAD_PAYLOAD);

    PlanJsonError error;
    char *text = plan_to_json(plan, &error);
    assert(text != NULL);
    assert(strstr(text, "\"variation\": \"ornament\"") != NULL);
    assert(strstr(text, "\"letter\": \"A'\"") != NULL);
    assert(strstr(text, "\"invent_motive\"") != NULL);
    assert(strstr(text, "\"piece_part\"") != NULL && strstr(text, "\"part\": \"turnaround\"") != NULL);
    assert(strstr(text, "\"id\": \"turn\"") != NULL && strstr(text, "\"free_phrase\"") != NULL);
    PiecePlan *parsed = NULL;
    expect_ok(plan_from_json(text, &parsed, &error), &error);
    assert(plans_equal(plan, parsed));
    const PlanConstraint *read = plan_node_constraint_at(parsed, b, 1);
    assert(read && read->kind == PLAN_CONSTRAINT_LAYERS &&
           read->params.layers.lanes == (PLAN_LAYER_BIT(PLAN_LANE_BASS) | PLAN_LAYER_BIT(PLAN_LANE_INNER_VOICE)));
    char *again = plan_to_json(parsed, &error);
    assert(again && strcmp(text, again) == 0);
    /* A part that is none of the four is refused. */
    PiecePlan *refused = NULL;
    char *outro = strdup(text);
    char *at = strstr(outro, "\"turnaround\"");
    assert(at);
    memcpy(at, "\"turnaroun_\"", 12);
    assert(plan_from_json(outro, &refused, &error) != PLAN_OK && refused == NULL);
    free(outro);
    free(text);
    free(again);
    plan_destroy(parsed);
    plan_destroy(plan);
}

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
        expect_ok(plan_from_json(old, &read, &error), &error);
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
        expect_ok(plan_from_json(written, &again, &error), &error);
        assert(plans_equal(read, again));
        free(old);
        free(written);
        plan_destroy(read);
        plan_destroy(again);
    }

    /* A name that is not a transform, and a step that is not a relation step, are refused with their text. */
    char *unknown = replace_all(base, "\"step\": \"transpose by=-2\"", "\"transform\": \"sideways\"");
    PiecePlan *refused = NULL;
    assert(plan_from_json(unknown, &refused, &error) == PLAN_ERR_FORMAT);
    assert(refused == NULL && strstr(error.message, "sideways") != NULL);
    free(unknown);
    char *bad_step = replace_all(base, "\"step\": \"transpose by=-2\"", "\"step\": \"transpose sideways=1\"");
    assert(plan_from_json(bad_step, &refused, &error) == PLAN_ERR_FORMAT);
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
    /* The constraints of a piece plan. */
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":"
                 "[{\"kind\":\"return\",\"section\":1,\"variation\":\"remix\"}]}}}",
                 PLAN_ERR_FORMAT, "form.composer.constraints[0].variation");
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":"
                 "[{\"kind\":\"return\",\"section\":1,\"variation\":\"exact\"}]}}}",
                 PLAN_ERR_FORMAT, "form.composer.constraints[0]");
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":"
                 "[{\"kind\":\"layers\",\"lanes\":[\"bass\",\"drums\"]}]}}}",
                 PLAN_ERR_FORMAT, "form.composer.constraints[0].lanes[1]");
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":"
                 "[{\"kind\":\"layers\",\"lanes\":[]}]}}}",
                 PLAN_ERR_FORMAT, "form.composer.constraints[0].lanes");
    expect_error("{\"schema_version\":1,\"form\":{\"kind\":\"form\",\"label\":null," SPAN_0_1
                 ",\"children\":[],\"composer\":{\"id\":1,\"state\":\"constrained\",\"constraints\":"
                 "[{\"kind\":\"section_letter\",\"letter\":\"ABCD\"}]}}}",
                 PLAN_ERR_FORMAT, "form.composer.constraints[0].letter");
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

static size_t lane_count(const PiecePlan *plan, PlanLaneKind lane) {
    size_t count = 0;
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        count += plan_has_lane_span(plan, id) && plan_lane_span_lane(plan, id) == lane;
    }
    return count;
}

/* A piece analyzed by the Analyzer loads as a plan: no document is read. */
static void test_load_analyzer_result(void) {
    ImportedNotes notes;
    memset(&notes, 0, sizeof(notes));
    assert(midi_pipeline_import_file(COMPOSER_FIXTURE_DIR "/mozart545-bar1.mid", &notes) == MIDI_PIPELINE_OK);
    AnalyzerResult *result = NULL;
    assert(analyzer_analyze(&notes, NULL, &result) == ANALYZER_OK);

    PiecePlan *plan = NULL;
    AnalysisLoadReport report = {NULL, 0};
    PlanJsonError error;
    assert(plan_from_analyzer_result(result, &notes, &plan, &report) == PLAN_OK);

    /* A Form root, Realized, over the piece from the start; the plan records the Analyzer that heard it. */
    NodeId root = plan_root(plan);
    assert(plan_node_kind(plan, root) == PLAN_NODE_FORM && plan_node_state(plan, root) == PLAN_STATE_REALIZED);
    assert(plan_node_span(plan, root).start.num == 0);
    assert(strcmp(plan_analyzer_version(plan), ursatz_analyzer_version()) == 0);

    /* The phrases the Analyzer heard are Phrase nodes under one Section. */
    assert(plan_node_count(plan) >= 1);
    for (NodeId id = 2; id <= plan_node_count(plan); id++) {
        assert(plan_node_state(plan, id) == PLAN_STATE_REALIZED);
        assert(plan_node_kind(plan, id) == PLAN_NODE_SECTION || plan_node_kind(plan, id) == PLAN_NODE_PHRASE);
    }

    /* The meter, key and chords, and both outer voices. */
    assert(lane_count(plan, PLAN_LANE_METER) >= 1);
    assert(lane_count(plan, PLAN_LANE_KEY) >= 1);
    assert(lane_count(plan, PLAN_LANE_HARMONY) >= 1);
    assert(lane_count(plan, PLAN_LANE_MELODY) == 1);
    assert(lane_count(plan, PLAN_LANE_BASS) == 1);
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        assert(plan_lane_span_state(plan, id) == PLAN_STATE_REALIZED);
        assert(plan_lane_span_owner(plan, id) == root);
        const PlanLanePayload *payload = plan_lane_span_payload(plan, id);
        if (plan_lane_span_lane(plan, id) == PLAN_LANE_HARMONY) {
            assert(payload->harmony.roman_numeral[0] != '\0' && !payload->harmony.has_function);
        }
        if (plan_lane_span_lane(plan, id) == PLAN_LANE_MELODY) {
            assert(payload->notes.note_count > 0);
        }
    }
    /* Nothing is dropped silently: an area the plan has no place for is listed. */
    assert(report_has(&report, "", ""));

    /* Realized material can be regenerated: a write is accepted. */
    LaneSpanId first_chord = PLAN_LANE_SPAN_NONE;
    assert(plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, root), &first_chord, 1) >= 1);
    PlanLanePayload tonic = harmony("I", HARMONIC_FUNCTION_TONIC);
    assert(plan_lane_span_write(plan, first_chord, &tonic) == PLAN_OK);

    /* The loaded plan survives a plan JSON round trip. */
    char *text = plan_to_json(plan, &error);
    PiecePlan *again = NULL;
    assert(text != NULL);
    assert(strstr(text, "\"composer_version\"") != NULL && strstr(text, "\"analyzer_version\"") != NULL);
    assert(plan_from_json(text, &again, &error) == PLAN_OK);
    assert(plans_equal(plan, again));
    assert(strcmp(plan_analyzer_version(again), ursatz_analyzer_version()) == 0);
    free(text);
    plan_destroy(again);

    analysis_load_report_destroy(&report);
    plan_destroy(plan);
    analyzer_result_destroy(result);
    imported_notes_destroy(&notes);
}

static void test_load_analyzer_result_errors(void) {
    PiecePlan *plan = NULL;
    AnalysisLoadReport report = {NULL, 0};
    ImportedNotes notes;
    memset(&notes, 0, sizeof(notes));
    assert(plan_from_analyzer_result(NULL, &notes, &plan, &report) == PLAN_ERR_NULL);
    assert(plan == NULL && report.count == 0);
    assert(plan_from_analyzer_result(NULL, NULL, NULL, NULL) == PLAN_ERR_NULL);
}

int main(void) {
    test_round_trip();
    test_old_transforms();
    test_motive_tones();
    test_section_constraints();
    test_empty_plan();
    test_malformed_input();
    test_load_analyzer_result();
    test_load_analyzer_result_errors();
    return 0;
}
