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
#include <ursatz_composer/piece_perform.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>

/*
 * A composed piece performed in order (R-485, docs/form-performance.md): tests/fixtures/forms/repeat-form.json (A, B, A',
 * 4 bars each, with "repeats": [["A"], ["B", "A'"]]) composed in C major 4/4, seeds 1 to 5, then performed: six sections
 * A, A (repeat), B, A', B (repeat), A' (repeat) at bars 0, 4, 8, 12, 16 and 20 of 24; each section's melody, harmony, bass
 * and inner voice spans equal its source's moved; the performed plan renders 24 bars with twice the notes of the composed
 * one; and repeats do not change composing (the composed plan is the plan of the same form without "repeats").
 */

#define SEEDS 5
#define INNER_VOICES 4

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    assert(text && fread(text, 1, (size_t)size, file) == (size_t)size);
    text[size] = '\0';
    fclose(file);
    return text;
}

/* The document with the first `from` replaced by `to`; the caller frees it. */
static char *replaced(const char *text, const char *from, const char *to) {
    const char *at = strstr(text, from);
    assert(at);
    size_t head = (size_t)(at - text);
    char *out = malloc(strlen(text) + strlen(to) + 1);
    assert(out);
    memcpy(out, text, head);
    strcpy(out + head, to);
    strcpy(out + head + strlen(to), at + strlen(from));
    return out;
}

static Rational whole(long long numerator) {
    return (Rational){numerator, 1};
}

static PiecePlan *compose(const FormSpec *form, unsigned long seed, ComposeOutcome *outcome, PieceResult *ran) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form->name;
    request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.seed = seed;
    PiecePlan *plan = NULL;
    assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.form = form;
    *ran = compose_run(plan, &config, outcome);
    return plan;
}

/* The lane spans of one lane and voice inside a section, in order: their start from the section's, and their payload. */
static void same_lane(const PiecePlan *source_plan, NodeId source, const PiecePlan *performed, NodeId node, PlanLaneKind lane,
                      int voice) {
    LaneSpanId a[32];
    LaneSpanId b[32];
    PlanSpan from = plan_node_span(source_plan, source);
    PlanSpan to = plan_node_span(performed, node);
    size_t na = plan_lane_find(source_plan, lane, voice, from, a, 32);
    size_t nb = plan_lane_find(performed, lane, voice, to, b, 32);
    assert(na == nb && na <= 32);
    for (size_t i = 0; i < na; i++) {
        PlanSpan span_a = plan_lane_span_span(source_plan, a[i]);
        PlanSpan span_b = plan_lane_span_span(performed, b[i]);
        Rational offset_a;
        Rational offset_b;
        assert(rational_sub(span_a.start, from.start, &offset_a) == RATIONAL_OK &&
               rational_sub(span_b.start, to.start, &offset_b) == RATIONAL_OK);
        assert(rational_cmp(offset_a, offset_b) == 0 && rational_cmp(span_a.length, span_b.length) == 0);
        const PlanLanePayload *pa = plan_lane_span_payload(source_plan, a[i]);
        const PlanLanePayload *pb = plan_lane_span_payload(performed, b[i]);
        if (lane == PLAN_LANE_HARMONY) {
            assert(strcmp(pa->harmony.roman_numeral, pb->harmony.roman_numeral) == 0);
        } else {
            assert(pa->notes.note_count == pb->notes.note_count);
            for (size_t n = 0; n < pa->notes.note_count; n++) {
                assert(pa->notes.notes[n].midi == pb->notes.notes[n].midi &&
                       rational_cmp(pa->notes.notes[n].offset, pb->notes.notes[n].offset) == 0 &&
                       rational_cmp(pa->notes.notes[n].duration, pb->notes.notes[n].duration) == 0);
            }
        }
    }
}

static size_t composed_sections(const PiecePlan *plan, NodeId *out) {
    size_t count = 0;
    NodeId root = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId child = plan_node_child_at(plan, root, i);
        if (!plan_node_has_constraint(plan, child, PLAN_CONSTRAINT_MATERIAL)) {
            out[count++] = child;
        }
    }
    return count;
}

static size_t render_count(const PiecePlan *plan, Rational *end, size_t *sections) {
    RenderedPlan rendered;
    assert(plan_render(plan, &rendered) == PLAN_RENDER_OK);
    size_t count = rendered.count;
    *end = rendered.piece_end;
    *sections = rendered.section_count;
    rendered_plan_destroy(&rendered);
    return count;
}

static void check_performance(const FormSpec *form, const PiecePlan *plan) {
    static const char *const NAMES[6] = {"A", "A (repeat)", "B", "A'", "B (repeat)", "A' (repeat)"};
    static const size_t SOURCE[6] = {0, 0, 1, 2, 1, 2};
    PiecePerformPart part = {form, &form->sizes[0], plan, true, NULL};
    PiecePerformance performance;
    NodeId sections[FORM_SPEC_MAX_SECTIONS];
    assert(piece_perform(&part, 1, &performance) == PIECE_OK && performance.plan);
    assert(composed_sections(plan, sections) == 3 && performance.section_count == 6);
    for (size_t i = 0; i < 6; i++) {
        const PiecePerformedSection *section = &performance.sections[i];
        assert(strcmp(section->name, NAMES[i]) == 0 && section->part == 0 && section->source == sections[SOURCE[i]]);
        assert(section->repeat == (i == 1 || i == 4 || i == 5));
        assert(rational_cmp(section->start, whole((long long)i * 4)) == 0);
        PlanSpan span = plan_node_span(performance.plan, section->node);
        assert(rational_cmp(span.start, whole((long long)i * 4)) == 0 && rational_cmp(span.length, whole(4)) == 0);
        assert(strcmp(plan_node_name(performance.plan, section->node), NAMES[i]) == 0);
        same_lane(plan, section->source, performance.plan, section->node, PLAN_LANE_MELODY, 0);
        same_lane(plan, section->source, performance.plan, section->node, PLAN_LANE_HARMONY, 0);
        same_lane(plan, section->source, performance.plan, section->node, PLAN_LANE_BASS, 0);
        for (int voice = 0; voice < INNER_VOICES; voice++) {
            same_lane(plan, section->source, performance.plan, section->node, PLAN_LANE_INNER_VOICE, voice);
        }
        /* The letter, the cadences and the key are the source's. */
        for (size_t c = 0; c < plan_node_constraint_count(plan, section->source); c++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, section->source, c);
            if (constraint->kind == PLAN_CONSTRAINT_SECTION_LETTER) {
                const PlanConstraint *copied = plan_node_find_constraint(performance.plan, section->node, PLAN_CONSTRAINT_SECTION_LETTER);
                assert(copied && strcmp(copied->params.letter, constraint->params.letter) == 0);
            }
        }
    }
    /* The performed plan renders 24 bars and twice the notes of the composed one (each section is played twice). */
    Rational composed_end;
    Rational performed_end;
    size_t composed_sections_count;
    size_t performed_sections_count;
    size_t composed_notes = render_count(plan, &composed_end, &composed_sections_count);
    size_t performed_notes = render_count(performance.plan, &performed_end, &performed_sections_count);
    assert(rational_cmp(composed_end, whole(12)) == 0 && rational_cmp(performed_end, whole(24)) == 0);
    assert(composed_sections_count == 3 && performed_sections_count == 6 && performed_notes == 2 * composed_notes);
    piece_performance_destroy(&performance);
    piece_performance_destroy(&performance);
}

/* A performance of parts in two meters is refused, and so is a plan that does not match its size. */
static void check_refusals(const FormSpec *form, const PiecePlan *plan) {
    PiecePerformance performance;
    PiecePerformPart part = {form, &form->sizes[0], plan, true, NULL};
    assert(piece_perform(NULL, 1, &performance) == PIECE_ERR_NULL && piece_perform(&part, 0, &performance) == PIECE_ERR_NULL);
    FormSpecSize shorter = form->sizes[0];
    shorter.section_count = 2;
    part.size = &shorter;
    assert(piece_perform(&part, 1, &performance) == PIECE_ERR_BAD_REQUEST && performance.plan == NULL);
}

int main(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/forms/repeat-form.json", COMPOSER_FIXTURE_DIR);
    char *text = read_file(path);
    FormSpec *form = NULL;
    FormSpec *plain_form = NULL;
    FormSpecError error;
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form_spec_performed(form));
    assert(form->repeat_count == 2 && form->repeats[0].first == 0 && form->repeats[0].count == 1 &&
           form->repeats[1].first == 1 && form->repeats[1].count == 2);
    char *without = replaced(text, "\"repeats\": [[\"A\"], [\"B\", \"A'\"]],\n  ", "");
    assert(form_spec_parse(without, &plain_form, &error) == FORM_SPEC_OK && !form_spec_performed(plain_form));
    int composed = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        ComposeOutcome outcome;
        ComposeOutcome plain_outcome;
        PieceResult ran;
        PieceResult plain_ran;
        PiecePlan *plan = compose(form, seed, &outcome, &ran);
        PiecePlan *plain = compose(plain_form, seed, &plain_outcome, &plain_ran);
        /* Repeats do not change composing. */
        assert(ran == plain_ran);
        PlanJsonError detail;
        char *json = plan_to_json(plan, &detail);
        char *plain_json = plan_to_json(plain, &detail);
        assert(json && plain_json);
        assert(strcmp(json, plain_json) == 0);
        free(json);
        free(plain_json);
        if (ran == PIECE_OK) {
            check_performance(form, plan);
            if (composed == 0) {
                check_refusals(form, plan);
            }
            composed++;
        } else {
            printf("  seed %lu: %s\n", seed, piece_result_name(ran));
        }
        compose_outcome_destroy(&outcome);
        compose_outcome_destroy(&plain_outcome);
        plan_destroy(plan);
        plan_destroy(plain);
    }
    printf("repeat-form in C major 4/4: %d of %d seeds compose and perform as A A B A' B A'\n", composed, SEEDS);
    assert(composed >= SEEDS - 1);
    form_spec_destroy(form);
    form_spec_destroy(plain_form);
    free(without);
    free(text);
    return 0;
}
