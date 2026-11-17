#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/harmony/harmony_key_change.h>
#include <theory/harmony/harmony_related_key.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_perform.h>
#include <ursatz_composer/piece_plan_keys.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>
#include "ursatz_test.h"
#include "support/test_knowledge.h"

/*
 * A composed piece performed in order (R-485, docs/form-performance.md): tests/fixtures/forms/repeat-form.json (A, B, A',
 * 4 bars each, with "repeats": [["A"], ["B", "A'"]]) composed in C major 4/4, seeds 1 to 5, then performed: six sections
 * A, A (repeat), B, A', B (repeat), A' (repeat) at bars 0, 4, 8, 12, 16 and 20 of 24; each section's melody, harmony, bass
 * and inner voice spans equal its source's moved; the performed plan renders 24 bars with twice the notes of the composed
 * one; and repeats do not change composing (the composed plan is the plan of the same form without "repeats").
 *
 *   test_piece_perform parts   a compound form (tests/fixtures/forms/parts-form.json: First and Second, binary pieces of 16
 *                              bars, Second in the region "other" with a motive of its own, then First again) composed in C
 *                              major 4/4, seeds 1 to 10, at least 9 compose: the keys are C major, A minor and C major; the
 *                              performance is 48 bars of the sections A, B, Second A, Second B, First again A and First
 *                              again B; the third part's notes are the first's moved; Second's m0 is not First's.
 *                              piece_region_key with degree 1 is the parallel key.
 *
 *   test_piece_perform part-bars   a part with its own bars (R-495, tests/fixtures/forms/part-bars-form.json: First a minuet
 *                              of the compound's size, Trio a trio of 12 bars in the region "other", then First again) at
 *                              sizes 24 and 12 in C major 3/4, seeds 1 to 5, all compose: the Trio is 12 bars at both sizes
 *                              (six sections of 4 bars with its repeats), the first part and its return are the size's.
 */

#define SEEDS 5
#define PART_SEEDS 10
#define INNER_VOICES 4

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
    request.key = test_key_mode(0, HARMONY_MODE_MAJOR);
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.seed = seed;
    PiecePlan *plan = NULL;
    assert(form_build_plan(form, &request, &plan) == PIECE_OK && plan);
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
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


/* ------------------------------------------------------------- compound forms */

static const char *const PART_NAMES[6] = {"A", "B", "Second A", "Second B", "First again A", "First again B"};

/* The notes of the first lane span of the motive m0 of a plan. */
static const PlanLanePayload *m0_notes(const PiecePlan *plan) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *name = plan_node_name(plan, id);
        if (plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && name && strcmp(name, "m0") == 0) {
            LaneSpanId melody;
            assert(plan_node_lane_spans(plan, id, &melody, 1) == 1);
            return plan_lane_span_payload(plan, melody);
        }
    }
    assert(!"m0");
    return NULL;
}

static bool same_notes(const PlanLanePayload *a, const PlanLanePayload *b) {
    if (a->notes.note_count != b->notes.note_count) {
        return false;
    }
    for (size_t n = 0; n < a->notes.note_count; n++) {
        if (a->notes.notes[n].midi != b->notes.notes[n].midi ||
            rational_cmp(a->notes.notes[n].offset, b->notes.notes[n].offset) != 0 ||
            rational_cmp(a->notes.notes[n].duration, b->notes.notes[n].duration) != 0) {
            return false;
        }
    }
    return true;
}

static void check_parts_performance(const FormSpec *form, unsigned long seed, const PieceParts *parts) {
    PiecePerformance performance;
    assert(piece_perform_compound(form, 0, 0, parts, &performance) == PIECE_OK && performance.plan);
    assert(performance.section_count == 6);
    assert(strcmp(plan_node_name(performance.plan, plan_root(performance.plan)), "parts-form") == 0);
    static const size_t SOURCE_PART[6] = {0, 0, 1, 1, 0, 0};
    for (size_t i = 0; i < 6; i++) {
        const PiecePerformedSection *section = &performance.sections[i];
        const PiecePlan *source_plan = parts->plans[SOURCE_PART[i]];
        assert(strcmp(section->name, PART_NAMES[i]) == 0 && !section->repeat);
        assert(section->part == (i < 2 ? 0u : i < 4 ? 1u : 2u));
        assert(rational_cmp(section->start, whole((long long)i * 8)) == 0);
        assert(rational_cmp(plan_node_span(performance.plan, section->node).length, whole(8)) == 0);
        same_lane(source_plan, section->source, performance.plan, section->node, PLAN_LANE_MELODY, 0);
        same_lane(source_plan, section->source, performance.plan, section->node, PLAN_LANE_HARMONY, 0);
        same_lane(source_plan, section->source, performance.plan, section->node, PLAN_LANE_BASS, 0);
        for (int voice = 0; voice < INNER_VOICES; voice++) {
            same_lane(source_plan, section->source, performance.plan, section->node, PLAN_LANE_INNER_VOICE, voice);
        }
    }
    /* The key lane follows the parts, a span over each performed section: C major, A minor, C major. */
    LaneSpanId ids[8];
    PlanSpan all = {whole(0), whole(48)};
    size_t found = plan_lane_find(performance.plan, PLAN_LANE_KEY, 0, all, ids, 8);
    assert(found == 6);
    static const int TONIC[3] = {0, 9, 0};
    static const HarmonyMode MODE[3] = {HARMONY_MODE_MAJOR, HARMONY_MODE_MINOR, HARMONY_MODE_MAJOR};
    for (size_t k = 0; k < 6; k++) {
        PlanSpan span = plan_lane_span_span(performance.plan, ids[k]);
        const AnalysisKey *key = &plan_lane_span_payload(performance.plan, ids[k])->key;
        assert(rational_cmp(span.start, whole((long long)k * 8)) == 0 && rational_cmp(span.length, whole(8)) == 0);
        assert(analysis_key_tonic_pitch_class(key) == TONIC[k / 2] && harmony_key_mode(key) == MODE[k / 2]);
    }
    Rational end;
    size_t sections;
    render_count(performance.plan, &end, &sections);
    assert(rational_cmp(end, whole(48)) == 0 && sections == 6);
    (void)seed;
    piece_performance_destroy(&performance);
}

static void check_region_keys(void) {
    FormRegion parallel;
    memset(&parallel, 0, sizeof(parallel));
    parallel.major[0] = 1;
    parallel.major_count = 1;
    parallel.minor[0] = 1;
    parallel.minor_count = 1;
    FillRng rng;
    AnalysisKey key;
    fill_rng_seed(&rng, 1);
    AnalysisKey c_major = test_key_mode(0, HARMONY_MODE_MAJOR);
    AnalysisKey a_minor = test_key_mode(9, HARMONY_MODE_MINOR);
    assert(piece_region_key(test_knowledge(), c_major, &parallel, &rng, &key) && analysis_key_tonic_pitch_class(&key) == 0 &&
           harmony_key_mode(&key) == HARMONY_MODE_MINOR);
    assert(piece_region_key(test_knowledge(), a_minor, &parallel, &rng, &key) && analysis_key_tonic_pitch_class(&key) == 9 &&
           harmony_key_mode(&key) == HARMONY_MODE_MAJOR);
    /* Any other degree is the closely related key, as before: 6 of C major is A minor, 3 of A minor is C major. */
    FormRegion other;
    memset(&other, 0, sizeof(other));
    other.major[0] = 6;
    other.major_count = 1;
    other.minor[0] = 3;
    other.minor_count = 1;
    assert(piece_region_key(test_knowledge(), c_major, &other, &rng, &key) && analysis_key_tonic_pitch_class(&key) == 9 &&
           harmony_key_mode(&key) == HARMONY_MODE_MINOR);
    assert(piece_region_key(test_knowledge(), a_minor, &other, &rng, &key) && analysis_key_tonic_pitch_class(&key) == 0 &&
           harmony_key_mode(&key) == HARMONY_MODE_MAJOR);
    /* A degree that is no closely related key is refused. */
    FormRegion none;
    memset(&none, 0, sizeof(none));
    none.major[0] = 7;
    none.major_count = 1;
    assert(!piece_region_key(test_knowledge(), c_major, &none, &rng, &key));
    /* The relation entries give the closely related keys of standard-harmony for every home key and degree (R-596), or both refuse. */
    for (int tonic = 0; tonic < 12; tonic++) {
        for (int mode = 0; mode < 2; mode++) {
            AnalysisKey home = test_key_mode(tonic, mode);
            HarmonyMode home_mode = harmony_key_mode(&home);
            for (int degree = 2; degree <= 7; degree++) {
                FormRegion one;
                memset(&one, 0, sizeof(one));
                one.major[0] = degree;
                one.major_count = 1;
                one.minor[0] = degree;
                one.minor_count = 1;
                HarmonyRelatedKey related;
                bool expected = harmony_related_key_of(home_mode, degree, &related) && harmony_key_is_closely_related(home_mode, &related);
                bool made = piece_region_key(test_knowledge(), home, &one, &rng, &key);
                assert(made == expected);
                if (made) {
                    assert(analysis_key_tonic_pitch_class(&key) == (tonic + harmony_related_key_semitone(home_mode, &related)) % 12 &&
                           harmony_key_mode(&key) == related.mode);
                }
            }
        }
    }
}

static int test_parts(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/forms/parts-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    FormSpec *form = NULL;
    FormSpecError error;
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form_spec_performed(form) && form->part_count == 3);
    check_region_keys();
    int composed = 0;
    for (unsigned long seed = 1; seed <= PART_SEEDS; seed++) {
        PieceRequest request;
        memset(&request, 0, sizeof(request));
        request.form = form->name;
        request.key = test_key_mode(0, HARMONY_MODE_MAJOR);
        request.knowledge = test_knowledge();
        request.meter.beats_per_bar = 4;
        request.meter.beat_unit = 4;
        request.seed = seed;
        ComposeConfig config;
        memset(&config, 0, sizeof(config));
        config.knowledge = test_knowledge();
        config.seed = seed;
        PieceParts parts;
        PieceResult ran = piece_compose_parts(form, &request, &config, &parts);
        if (ran != PIECE_OK) {
            printf("  seed %lu: %s\n", seed, piece_result_name(ran));
            assert(parts.count == 0 && parts.plans[0] == NULL);
            continue;
        }
        composed++;
        assert(parts.count == 3 && parts.plans[0] && parts.plans[1] && parts.plans[2] == NULL);
        assert(analysis_key_tonic_pitch_class(&parts.keys[0]) == 0 && harmony_key_mode(&parts.keys[0]) == HARMONY_MODE_MAJOR);
        assert(analysis_key_tonic_pitch_class(&parts.keys[1]) == 9 && harmony_key_mode(&parts.keys[1]) == HARMONY_MODE_MINOR);
        assert(analysis_key_equals(&parts.keys[2], &parts.keys[0]));
        assert(parts.forms[0] && parts.forms[1] && parts.forms[2] == NULL);
        check_parts_performance(form, seed, &parts);
        /* Second has a motive of its own: its m0 is not First's. */
        assert(!same_notes(m0_notes(parts.plans[0]), m0_notes(parts.plans[1])));
        piece_parts_destroy(&parts);
        piece_parts_destroy(&parts);
    }
    printf("parts-form in C major 4/4: %d of %d seeds compose and perform as First, Second in A minor, First again\n", composed,
           PART_SEEDS);
    assert(composed >= PART_SEEDS - 1);
    /* A form that is not compound, and a size it does not have, are refused. */
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.key = test_key_mode(0, HARMONY_MODE_MAJOR);
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
    PieceParts parts;
    request.bars = 20;
    assert(piece_compose_parts(form, &request, &config, &parts) == PIECE_ERR_BAD_SIZE);
    FormSpec *binary = NULL;
    assert(form_spec_builtin("binary", &binary) == FORM_SPEC_OK);
    request.bars = 16;
    assert(piece_compose_parts(binary, &request, &config, &parts) == PIECE_ERR_BAD_REQUEST);
    assert(piece_compose_parts(NULL, &request, &config, &parts) == PIECE_ERR_NULL);
    form_spec_destroy(binary);
    form_spec_destroy(form);
    free(text);
    return 0;
}

/* A length of bars in 3/4, in whole notes. */
static Rational in_bars(size_t bars) {
    return rational_ratio((long long)bars * 3, 4);
}

/* The performed bars of the sections of a part. */
static void check_part_bars_performance(const FormSpec *form, size_t bars, const PieceParts *parts) {
    PiecePerformance performance;
    assert(piece_perform_compound(form, bars, 0, parts, &performance) == PIECE_OK && performance.plan);
    /* First and its return are the size's bars, the Trio is 12 whatever the size: A A B A1 B A1, 6 sections each. */
    const size_t first = bars / 3;
    size_t in_part[3] = {0, 0, 0};
    for (size_t i = 0; i < performance.section_count; i++) {
        const PiecePerformedSection *section = &performance.sections[i];
        assert(section->part < 3);
        in_part[section->part]++;
        assert(rational_cmp(plan_node_span(performance.plan, section->node).length, in_bars(section->part == 1 ? 4 : first)) == 0);
        /* Its source in the part's own plan is as long: the Trio's plan is composed at 12 bars, the others at the size's. */
        const PiecePlan *source_plan = parts->plans[section->part == 2 ? 0 : section->part];
        assert(rational_cmp(plan_node_span(source_plan, section->source).length, in_bars(section->part == 1 ? 4 : first)) == 0);
    }
    assert(in_part[0] == 6 && in_part[1] == 6 && in_part[2] == 3);
    assert(performance.section_count == 15);
    Rational end;
    size_t sections;
    render_count(performance.plan, &end, &sections);
    assert(rational_cmp(end, in_bars(2 * bars + 24 + bars)) == 0 && sections == 15);
    piece_performance_destroy(&performance);
}

static int test_part_bars(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/forms/part-bars-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
    FormSpec *form = NULL;
    FormSpecError error;
    assert(form_spec_parse(text, &form, &error) == FORM_SPEC_OK && form_spec_performed(form) && form->part_count == 3);
    static const size_t SIZES[2] = {24, 12};
    for (size_t z = 0; z < 2; z++) {
        int composed = 0;
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            PieceRequest request;
            memset(&request, 0, sizeof(request));
            request.form = form->name;
            request.key = test_key_mode(0, HARMONY_MODE_MAJOR);
            request.knowledge = test_knowledge();
            request.meter.beats_per_bar = 3;
            request.meter.beat_unit = 4;
            request.bars = SIZES[z];
            request.seed = seed;
            ComposeConfig config;
            memset(&config, 0, sizeof(config));
            config.knowledge = test_knowledge();
            config.seed = seed;
            PieceParts parts;
            PieceResult ran = piece_compose_parts(form, &request, &config, &parts);
            if (ran != PIECE_OK) {
                printf("  %zu bars, seed %lu: %s\n", SIZES[z], seed, piece_result_name(ran));
                continue;
            }
            composed++;
            assert(parts.count == 3 && parts.plans[0] && parts.plans[1] && parts.plans[2] == NULL);
            check_part_bars_performance(form, SIZES[z], &parts);
            piece_parts_destroy(&parts);
        }
        printf("part-bars-form at %zu bars in C major 3/4: %d of %d seeds compose, the Trio at 12 bars\n", SIZES[z], composed,
               SEEDS);
        assert(composed == SEEDS);
    }
    form_spec_destroy(form);
    free(text);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "parts") == 0) {
        return test_parts();
    }
    if (argc == 2 && strcmp(argv[1], "part-bars") == 0) {
        return test_part_bars();
    }
    if (argc != 1) {
        fprintf(stderr, "usage: test_piece_perform [parts|part-bars]\n");
        return 2;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/forms/repeat-form.json", COMPOSER_FIXTURE_DIR);
    char *text = test_must_read_file(path, NULL);
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
